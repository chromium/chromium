// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/global_shortcut_listener_linux.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/dbus/properties/types.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/utils/check_for_service_and_start.h"
#include "components/dbus/xdg/request.h"
#include "components/dbus/xdg/systemd.h"
#include "crypto/sha2.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/linux/xdg_shortcut.h"

namespace extensions {

using DbusShortcut = DbusStruct<DbusString, DbusDictionary>;
using DbusShortcuts = DbusArray<DbusShortcut>;

GlobalShortcutListenerLinux::GlobalShortcutListenerLinux(
    scoped_refptr<dbus::Bus> bus)
    : bus_(std::move(bus)) {
  if (!bus_) {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SESSION;
    options.dbus_task_runner = dbus_thread_linux::GetTaskRunner();
    bus_ = base::MakeRefCounted<dbus::Bus>(options);
  }

  global_shortcuts_proxy_ = bus_->GetObjectProxy(
      kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));

  global_shortcuts_proxy_->ConnectToSignal(
      kGlobalShortcutsInterface, kSignalActivated,
      base::BindRepeating(&GlobalShortcutListenerLinux::OnActivatedSignal,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&GlobalShortcutListenerLinux::OnSignalConnected,
                     weak_ptr_factory_.GetWeakPtr()));

  dbus_xdg::SetSystemdScopeUnitNameForXdgPortal(
      bus_.get(),
      base::BindOnce(&GlobalShortcutListenerLinux::OnSystemdUnitStarted,
                     weak_ptr_factory_.GetWeakPtr()));
}

GlobalShortcutListenerLinux::~GlobalShortcutListenerLinux() {
  // Normally GlobalShortcutListener outlives the browser process, so this
  // destructor won't normally get called. It's okay for the sessions not to be
  // closed explicitly, but this destructor is left here for testing purposes,
  // and in case this object ever does need to be destructed.
  for (auto& entry : session_map_) {
    dbus::MethodCall method_call(kSessionInterface, kMethodCloseSession);
    entry.second->session_proxy->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::DoNothing());
  }
  session_map_.clear();

  dbus_thread_linux::GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&dbus::Bus::ShutdownAndBlock, bus_));
}

void GlobalShortcutListenerLinux::OnSystemdUnitStarted(
    dbus_xdg::SystemdUnitStatus) {
  // Intentionally ignoring the status.
  dbus_utils::CheckForServiceAndStart(
      bus_.get(), kPortalServiceName,
      base::BindOnce(&GlobalShortcutListenerLinux::OnServiceStarted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GlobalShortcutListenerLinux::OnServiceStarted(
    std::optional<bool> service_started) {
  service_started_ = service_started.value_or(false);

  if (!*service_started_) {
    session_map_.clear();
    return;
  }

  for (auto& pair : session_map_) {
    CreateSession(pair);
  }
}

void GlobalShortcutListenerLinux::CreateSession(SessionMapPair& pair) {
  CHECK(!bus_->GetConnectionName().empty());

  const SessionKey& session_key = pair.first;
  SessionContext& session_context = *pair.second;

  std::string session_token = session_key.GetTokenKey();
  std::string session_path_str = base::nix::XdgDesktopPortalSessionPath(
      bus_->GetConnectionName(), session_token);
  dbus::ObjectPath session_path(session_path_str);
  session_context.session_proxy =
      bus_->GetObjectProxy(kPortalServiceName, session_path);
  session_context.bus = bus_;

  session_context.request = std::make_unique<dbus_xdg::Request>(
      bus_, global_shortcuts_proxy_, kGlobalShortcutsInterface,
      kMethodCreateSession, DbusParameters(),
      MakeDbusDictionary("session_handle_token", DbusString(session_token)),
      base::BindOnce(&GlobalShortcutListenerLinux::OnCreateSession,
                     weak_ptr_factory_.GetWeakPtr(), session_key));
}

void GlobalShortcutListenerLinux::StartListening() {}

void GlobalShortcutListenerLinux::StopListening() {}

bool GlobalShortcutListenerLinux::RegisterAcceleratorImpl(
    const ui::Accelerator& accelerator) {
  // Shortcut registration is now handled in OnCommandsChanged()
  return false;
}

void GlobalShortcutListenerLinux::UnregisterAcceleratorImpl(
    const ui::Accelerator& accelerator) {
  // Shortcut unregistration is now handled per extension
}

void GlobalShortcutListenerLinux::UnregisterAccelerators(Observer* observer) {
  std::vector<SessionKey> remove;
  for (const auto& [key, context] : session_map_) {
    if (context->observer == observer) {
      remove.push_back(key);
    }
  }
  for (const auto& key : remove) {
    session_map_.erase(key);
  }
}

bool GlobalShortcutListenerLinux::IsRegistrationHandledExternally() const {
  return true;
}

void GlobalShortcutListenerLinux::OnCommandsChanged(
    const ExtensionId& extension_id,
    const std::string& profile_id,
    const CommandMap& commands,
    Observer* observer) {
  // If starting the service failed, there's no need to add the command list.
  if (!service_started_.value_or(true)) {
    return;
  }

  SessionKey session_key = {extension_id, profile_id};
  auto it = session_map_.find(session_key);
  if (it != session_map_.end()) {
    auto& session_context = *it->second;
    session_context.commands = commands;

    // BindShortcuts can only be called once per session.
    if (session_context.bind_shortcuts_called) {
      // If BindShortcuts was already called then recreate the session.
      dbus::MethodCall method_call(kSessionInterface, kMethodCloseSession);
      session_context.session_proxy->CallMethod(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
          base::BindOnce(&GlobalShortcutListenerLinux::RecreateSessionOnClosed,
                         weak_ptr_factory_.GetWeakPtr(), session_key));
    }
    return;
  }

  it = session_map_.emplace_hint(
      it, session_key, std::make_unique<SessionContext>(observer, commands));

  if (service_started_.has_value()) {
    CreateSession(*it);
  }
}

void GlobalShortcutListenerLinux::OnCreateSession(
    const SessionKey& session_key,
    base::expected<DbusDictionary, dbus_xdg::ResponseError> results) {
  if (!results.has_value()) {
    LOG(ERROR) << "Failed to call CreateSession (error code "
               << static_cast<int>(results.error()) << ").";
    session_map_.erase(session_key);
    return;
  }

  auto session_it = session_map_.find(session_key);
  if (session_it == session_map_.end()) {
    LOG(ERROR) << "Unknown session path.";
    return;
  }
  const auto& session_context = session_it->second;

  auto* session_handle = results->GetAs<DbusString>("session_handle");
  if (!session_handle ||
      session_context->session_proxy->object_path().value() !=
          session_handle->value()) {
    LOG(ERROR) << "Expected session handle does not match.";
    session_map_.erase(session_key);
    return;
  }

  // Check the list of registered shortcuts using ListShortcuts so that
  // BindShortcuts can be avoided if the registered shortcuts are the same,
  // otherwise a settings window will open each time the extension is loaded
  // (likely on browser start).
  session_context->request = std::make_unique<dbus_xdg::Request>(
      bus_, global_shortcuts_proxy_, kGlobalShortcutsInterface,
      kMethodListShortcuts,
      DbusObjectPath(session_context->session_proxy->object_path()),
      DbusDictionary(),
      base::BindOnce(&GlobalShortcutListenerLinux::OnListShortcuts,
                     weak_ptr_factory_.GetWeakPtr(), session_key));
}

void GlobalShortcutListenerLinux::OnListShortcuts(
    const SessionKey& session_key,
    base::expected<DbusDictionary, dbus_xdg::ResponseError> results) {
  if (!results.has_value()) {
    LOG(ERROR) << "Failed to call ListShortcuts (error code "
               << static_cast<int>(results.error()) << ").";
    session_map_.erase(session_key);
    return;
  }

  auto session_it = session_map_.find(session_key);
  if (session_it == session_map_.end()) {
    LOG(ERROR) << "Unknown session path.";
    return;
  }
  const auto& session_context = session_it->second;

  auto* shortcuts = results->GetAs<DbusShortcuts>("shortcuts");
  if (!shortcuts) {
    LOG(ERROR) << "No shortcuts in ListShortcuts response.";
    session_map_.erase(session_key);
    return;
  }

  std::set<std::string> registered_shortcut_ids;
  for (const DbusShortcut& shortcut : shortcuts->value()) {
    const DbusString& id = std::get<0>(shortcut.value());
    registered_shortcut_ids.insert(id.value());
  }

  // Only call BindShortcuts if necessary since it opens a settings window.
  // The GlobalShortcuts interface doesn't provide a way to unregister
  // shortcuts, so only check for new shortcuts that need registration.
  for (const auto& command : session_context->commands) {
    const std::string& id = command.first;
    if (!base::Contains(registered_shortcut_ids, id)) {
      BindShortcuts(*session_it->second);
      return;
    }
  }
}

void GlobalShortcutListenerLinux::BindShortcuts(
    SessionContext& session_context) {
  dbus::MethodCall method_call(kGlobalShortcutsInterface, kMethodBindShortcuts);
  dbus::MessageWriter writer(&method_call);

  writer.AppendObjectPath(session_context.session_proxy->object_path());

  DbusShortcuts shortcuts;
  for (const auto& cmd_pair : session_context.commands) {
    const auto& command = cmd_pair.second;

    auto props = MakeDbusDictionary(
        "description", DbusString(base::UTF16ToUTF8(command.description())));
    if (command.accelerator().key_code()) {
      props.PutAs(
          "preferred_trigger",
          DbusString(ui::AcceleratorToXdgShortcut(command.accelerator())));
    }
    shortcuts.value().push_back(
        MakeDbusStruct(DbusString(command.command_name()), std::move(props)));
  }

  DbusString empty_parent_window;
  session_context.request = std::make_unique<dbus_xdg::Request>(
      bus_, global_shortcuts_proxy_, kGlobalShortcutsInterface,
      kMethodBindShortcuts,
      MakeDbusParameters(
          DbusObjectPath(session_context.session_proxy->object_path()),
          std::move(shortcuts), std::move(empty_parent_window)),
      DbusDictionary(),
      base::BindOnce(&GlobalShortcutListenerLinux::OnBindShortcuts,
                     weak_ptr_factory_.GetWeakPtr()));
  session_context.bind_shortcuts_called = true;
}

void GlobalShortcutListenerLinux::OnBindShortcuts(
    base::expected<DbusDictionary, dbus_xdg::ResponseError> results) {
  if (!results.has_value()) {
    LOG(ERROR) << "Failed to call BindShortcuts (error code "
               << static_cast<int>(results.error()) << ").";
    return;
  }

  // Shortcuts successfully bound. The signal also includes information about
  // the bound shortcuts, but it's currently not needed.
}

void GlobalShortcutListenerLinux::RecreateSessionOnClosed(
    const SessionKey& session_key,
    dbus::Response* response) {
  auto session_it = session_map_.find(session_key);
  if (session_it == session_map_.end()) {
    return;
  }
  CreateSession(*session_it);
}

void GlobalShortcutListenerLinux::OnActivatedSignal(dbus::Signal* signal) {
  dbus::MessageReader reader(signal);
  dbus::ObjectPath session_handle;
  std::string shortcut_id;
  uint64_t timestamp;

  if (!reader.PopObjectPath(&session_handle) ||
      !reader.PopString(&shortcut_id) || !reader.PopUint64(&timestamp)) {
    LOG(ERROR) << "Failed to parse Activated signal.";
    return;
  }

  // Find the corresponding accelerator
  for (const auto& [session_key, session_context] : session_map_) {
    if (session_context->session_proxy->object_path() == session_handle) {
      session_context->observer->ExecuteCommand(session_key.extension_id,
                                                shortcut_id);
      break;
    }
  }
}

void GlobalShortcutListenerLinux::OnSignalConnected(
    const std::string& interface_name,
    const std::string& signal_name,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to connect to signal: " << interface_name << "."
               << signal_name;
  }
}

std::string GlobalShortcutListenerLinux::SessionKey::GetTokenKey() const {
  return kSessionTokenPrefix +
         base::HexEncode(crypto::SHA256HashString(extension_id + profile_id))
             .substr(0, 32);
}

GlobalShortcutListenerLinux::SessionContext::SessionContext(
    Observer* observer,
    const CommandMap& commands)
    : observer(observer), commands(commands) {}

GlobalShortcutListenerLinux::SessionContext::~SessionContext() {
  if (session_proxy) {
    bus->RemoveObjectProxy(kPortalServiceName, session_proxy->object_path(),
                           base::DoNothing());
  }
}

}  // namespace extensions
