/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */
#include "DBusMessage.h"
#include "DBusUtil.h"
#include "utils/log.h"
#include "settings/AdvancedSettings.h"

CDBusMessage::CDBusMessage(const char *destination, const char *object, const char *interface, const char *method)
{
  m_reply = nullptr;
  m_message.reset(dbus_message_new_method_call(destination, object, interface, method));
  if (!m_message)
  {
    // Fails only due to memory allocation failure
    throw std::runtime_error("dbus_message_new_method_call");
  }
  m_haveArgs = false;

  CLog::Log(LOGDEBUG, LOGDBUS, "DBus: Creating message to %s on %s with interface %s and method %s\n", destination, object, interface, method);
}

CDBusMessage::CDBusMessage(const std::string& destination, const std::string& object, const std::string& interface, const std::string& method)
: CDBusMessage(destination.c_str(), object.c_str(), interface.c_str(), method.c_str())
{
}

void DBusMessageDeleter::operator()(DBusMessage* message) const
{
  dbus_message_unref(message);
}

void CDBusMessage::AppendObjectPath(const char *object)
{
  AppendWithType(DBUS_TYPE_OBJECT_PATH, &object);
}

template<>
void CDBusMessage::AppendArgument<bool>(const bool arg)
{
  // dbus_bool_t width might not match C++ bool width
  dbus_bool_t convArg = (arg == true);
  AppendWithType(DBUS_TYPE_BOOLEAN, &convArg);
}

template<>
void CDBusMessage::AppendArgument<std::string>(const std::string arg)
{
  AppendArgument(arg.c_str());
}

void CDBusMessage::AppendArgument(const char **arrayString, unsigned int length)
{
  PrepareArgument();
  DBusMessageIter sub;
  if (!dbus_message_iter_open_container(&m_args, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING_AS_STRING, &sub))
  {
    throw std::runtime_error("dbus_message_iter_open_container");
  }

  for (unsigned int i = 0; i < length; i++)
  {
    if (!dbus_message_iter_append_basic(&sub, DBUS_TYPE_STRING, &arrayString[i]))
    {
      throw std::runtime_error("dbus_message_iter_append_basic");
    }
  }

  if (!dbus_message_iter_close_container(&m_args, &sub))
  {
    throw std::runtime_error("dbus_message_iter_close_container");
  }
}

void CDBusMessage::AppendWithType(int type, const void* value)
{
  PrepareArgument();
  if (!dbus_message_iter_append_basic(&m_args, type, value))
  {
    throw std::runtime_error("dbus_message_iter_append_basic");
  }
}

DBusMessage *CDBusMessage::SendSystem()
{
  return Send(DBUS_BUS_SYSTEM);
}

DBusMessage *CDBusMessage::SendSession()
{
  return Send(DBUS_BUS_SESSION);
}

DBusMessage *CDBusMessage::SendSystem(CDBusError& error)
{
  return Send(DBUS_BUS_SYSTEM, error);
}

DBusMessage *CDBusMessage::SendSession(CDBusError& error)
{
  return Send(DBUS_BUS_SESSION, error);
}

bool CDBusMessage::SendAsyncSystem()
{
  return SendAsync(DBUS_BUS_SYSTEM);
}

bool CDBusMessage::SendAsyncSession()
{
  return SendAsync(DBUS_BUS_SESSION);
}

DBusMessage *CDBusMessage::Send(DBusBusType type)
{
  CDBusError error;
  CDBusConnection con;
  if (!con.Connect(type))
    return nullptr;

  DBusMessage *returnMessage = Send(con, error);

  if (error)
    error.Log();

  return returnMessage;
}

DBusMessage *CDBusMessage::Send(DBusBusType type, CDBusError& error)
{
  CDBusConnection con;
  if (!con.Connect(type, error))
    return nullptr;
  
  DBusMessage *returnMessage = Send(con, error);

  return returnMessage;
}

bool CDBusMessage::SendAsync(DBusBusType type)
{
  CDBusConnection con;
  if (!con.Connect(type))
    return false;

  return dbus_connection_send(con, m_message.get(), nullptr);
}

DBusMessage *CDBusMessage::Send(DBusConnection *con, CDBusError& error)
{
  m_reply.reset(dbus_connection_send_with_reply_and_block(con, m_message.get(), -1, error));
  return m_reply.get();
}

void CDBusMessage::PrepareArgument()
{
  if (!m_haveArgs)
    dbus_message_iter_init_append(m_message.get(), &m_args);

  m_haveArgs = true;
}

bool CDBusMessage::InitializeReplyIter(DBusMessageIter* iter)
{
  if (!m_reply)
  {
    throw std::logic_error("Cannot get reply arguments of message that does not have reply");
  }
  if (!dbus_message_iter_init(m_reply.get(), iter))
  {
    CLog::Log(LOGWARNING, "Tried to obtain reply arguments from message that has zero arguments");
    return false;
  }
  return true;
}

bool CDBusMessage::CheckTypeAndGetValue(DBusMessageIter* iter, int expectType, void* dest)
{
  const int haveType = dbus_message_iter_get_arg_type(iter);
  if (haveType != expectType)
  {
    CLog::Log(LOGDEBUG, "DBus argument type mismatch: expected %d, got %d", expectType, haveType);
    return false;
  }

  dbus_message_iter_get_basic(iter, dest);
  return true;
}
