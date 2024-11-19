// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/global_shortcut_listener_linux.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "components/dbus/properties/types.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/utils/check_for_service_and_start.h"
#include "crypto/sha2.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/linux/xdg_shortcut.h"

namespace extensions {

namespace {

constexpr char kPortalServiceName[] = "org.freedesktop.portal.Desktop";
constexpr char kPortalObjectPath[] = "/org/freedesktop/portal/desktop";
constexpr char kGlobalShortcutsInterface[] =
    "org.freedesktop.portal.GlobalShortcuts";
constexpr char kSessionInterface[] = "org.freedesktop.portal.Session";
constexpr char kRequestInterface[] = "org.freedesktop.portal.Request";

constexpr char kMethodCreateSession[] = "CreateSession";
constexpr char kMethodListShortcuts[] = "ListShortcuts";
constexpr char kMethodBindShortcuts[] = "BindShortcuts";
constexpr char kMethodCloseSession[] = "Close";
constexpr char kSignalActivated[] = "Activated";
constexpr char kSignalResponse[] = "Response";

constexpr char kSessionTokenPrefix[] = "chromium_";

}  // namespace

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

  dbus_utils::CheckForServiceAndStart(
      bus_.get(), kPortalServiceName,
      base::BindOnce(&GlobalShortcutListenerLinux::OnServiceStarted,
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
  session_context.session_proxy = {
      bus_, bus_->GetObjectProxy(kPortalServiceName, session_path)};

  std::string request_token = base::UnguessableToken::Create().ToString();
  dbus::ObjectPath request_path(base::nix::XdgDesktopPortalRequestPath(
      bus_->GetConnectionName(), request_token));
  session_context.request_proxy = {
      bus_, bus_->GetObjectProxy(kPortalServiceName, request_path)};

  session_context.request_proxy->ConnectToSignal(
      kRequestInterface, kSignalResponse,
      base::BindRepeating(&GlobalShortcutListenerLinux::OnCreateSessionSignal,
                          weak_ptr_factory_.GetWeakPtr(), session_key),
      base::BindOnce(&GlobalShortcutListenerLinux::OnSignalConnected,
                     weak_ptr_factory_.GetWeakPtr()));

  dbus::MethodCall method_call(kGlobalShortcutsInterface, kMethodCreateSession);

  dbus::MessageWriter writer(&method_call);
  DbusDictionary options;
  options.Put("handle_token", MakeDbusVariant(DbusString(request_token)));
  options.Put("session_handle_token",
              MakeDbusVariant(DbusString(session_token)));
  options.Write(&writer);

  global_shortcuts_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&GlobalShortcutListenerLinux::OnCreateSessionResponse,
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
    it->second->commands = commands;
    // If BindShortcuts() was already called, there's no need to check against
    // the list of registered shortcuts again since it should match against the
    // previous value of `commands`, so we can call BindShortcuts() directly.
    // Otherwise, updating `commands` above is sufficient since there's a
    // running asynchronous job that will result in BindShortcuts() getting
    // called.
    if (it->second->bind_shortcuts_called) {
      BindShortcuts(*it);
    }
    return;
  }

  it = session_map_.emplace_hint(
      it, session_key, std::make_unique<SessionContext>(observer, commands));

  if (service_started_.has_value()) {
    CreateSession(*it);
  }
}

void GlobalShortcutListenerLinux::OnCreateSessionResponse(
    const SessionKey& session_key,
    dbus::Response* response) {
  auto it = session_map_.find(session_key);
  if (it == session_map_.end()) {
    LOG(ERROR) << "Session context not found for session_key.";
    return;
  }

  if (!response) {
    LOG(ERROR) << "CreateSession call failed.";
    session_map_.erase(session_key);
    return;
  }

  dbus::MessageReader reader(response);
  dbus::ObjectPath request_handle;
  if (!reader.PopObjectPath(&request_handle)) {
    LOG(ERROR) << "Failed to read request handle.";
    session_map_.erase(session_key);
    return;
  }

  // Check that the request_handle matches our expected request_path
  if (request_handle != it->second->request_proxy->object_path()) {
    LOG(ERROR) << "Request handle does not match expected request path.";
    session_map_.erase(session_key);
    return;
  }

  // Waiting for the Response signal on the Request object
}

void GlobalShortcutListenerLinux::OnCreateSessionSignal(
    const SessionKey& session_key,
    dbus::Signal* signal) {
  dbus::MessageReader reader(signal);

  uint32_t response;
  if (!reader.PopUint32(&response)) {
    LOG(ERROR) << "Failed to read response from signal.";
    session_map_.erase(session_key);
    return;
  }

  if (response != 0) {
    LOG(ERROR) << "CreateSession failed with response code: " << response;
    session_map_.erase(session_key);
    return;
  }

  auto session_it = session_map_.find(session_key);
  if (session_it == session_map_.end()) {
    LOG(ERROR) << "Unknown session path.";
    return;
  }
  const auto& session_context = session_it->second;

  // Check the list of registered shortcuts using ListShortcuts so that
  // BindShortcuts can be avoided if the registered shortcuts are the same,
  // otherwise a settings window will open each time the extension is loaded
  // (likely on browser start).
  dbus::MethodCall method_call(kGlobalShortcutsInterface, kMethodListShortcuts);

  dbus::MessageWriter writer(&method_call);

  writer.AppendObjectPath(session_context->session_proxy->object_path());

  // Prepare handle_token for the list request
  DbusDictionary options;
  std::string list_request_token = base::UnguessableToken::Create().ToString();
  options.Put("handle_token", MakeDbusVariant(DbusString(list_request_token)));
  options.Write(&writer);

  dbus::ObjectPath list_request_path(base::nix::XdgDesktopPortalRequestPath(
      bus_->GetConnectionName(), list_request_token));
  session_context->request_proxy = {
      bus_, bus_->GetObjectProxy(kPortalServiceName, list_request_path)};

  session_context->request_proxy->ConnectToSignal(
      kRequestInterface, kSignalResponse,
      base::BindRepeating(&GlobalShortcutListenerLinux::OnListShortcutsSignal,
                          weak_ptr_factory_.GetWeakPtr(), session_key),
      base::BindOnce(&GlobalShortcutListenerLinux::OnSignalConnected,
                     weak_ptr_factory_.GetWeakPtr()));

  global_shortcuts_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&GlobalShortcutListenerLinux::OnListShortcutsResponse,
                     weak_ptr_factory_.GetWeakPtr(), session_key));
}

void GlobalShortcutListenerLinux::OnListShortcutsResponse(
    const SessionKey& session_key,
    dbus::Response* response) {
  auto it = session_map_.find(session_key);
  if (it == session_map_.end()) {
    LOG(ERROR) << "Session context not found for session_key.";
    return;
  }

  if (!response) {
    LOG(ERROR) << "ListShortcuts call failed.";
    session_map_.erase(session_key);
    return;
  }

  dbus::MessageReader reader(response);
  dbus::ObjectPath request_handle;
  if (!reader.PopObjectPath(&request_handle)) {
    LOG(ERROR) << "Failed to read request handle.";
    session_map_.erase(session_key);
    return;
  }

  // Check that the request_handle matches our expected request_path
  if (request_handle != it->second->request_proxy->object_path()) {
    LOG(ERROR) << "Request handle does not match expected request path.";
    session_map_.erase(session_key);
    return;
  }

  // Waiting for the Response signal on the Request object
}

void GlobalShortcutListenerLinux::OnListShortcutsSignal(
    const SessionKey& session_key,
    dbus::Signal* signal) {
  dbus::MessageReader reader(signal);

  uint32_t response_code;
  if (!reader.PopUint32(&response_code)) {
    LOG(ERROR) << "Failed to read response code from signal.";
    session_map_.erase(session_key);
    return;
  }

  if (response_code != 0) {
    LOG(ERROR) << "ListShortcuts failed with response code: " << response_code;
    session_map_.erase(session_key);
    return;
  }

  auto session_it = session_map_.find(session_key);
  if (session_it == session_map_.end()) {
    LOG(ERROR) << "Unknown session path.";
    return;
  }
  const auto& session_context = session_it->second;

  // Read the response data
  dbus::MessageReader response_dict_reader(nullptr);
  if (!reader.PopArray(&response_dict_reader)) {
    LOG(ERROR) << "Failed to read response data.";
    return;
  }

  std::set<std::string> registered_shortcut_ids;
  while (response_dict_reader.HasMoreData()) {
    dbus::MessageReader response_dict_entry_reader(nullptr);
    if (!response_dict_reader.PopDictEntry(&response_dict_entry_reader)) {
      LOG(ERROR) << "Failed to read entry.";
      return;
    }

    std::string response_key;
    if (response_dict_entry_reader.PopString(&response_key) &&
        response_key == "shortcuts") {
      dbus::MessageReader variant_reader(nullptr);
      if (!response_dict_entry_reader.PopVariant(&variant_reader)) {
        LOG(ERROR) << "Failed to read variant.";
        return;
      }

      dbus::MessageReader array_reader(nullptr);
      if (!variant_reader.PopArray(&array_reader)) {
        LOG(ERROR) << "Failed to read array.";
        return;
      }

      while (array_reader.HasMoreData()) {
        dbus::MessageReader shortcut_reader(nullptr);
        if (!array_reader.PopStruct(&shortcut_reader)) {
          LOG(ERROR) << "Failed to read struct.";
          return;
        }

        std::string shortcut_id;
        if (!shortcut_reader.PopString(&shortcut_id)) {
          LOG(ERROR) << "Failed to read shortcut entry.";
          return;
        }

        registered_shortcut_ids.insert(shortcut_id);
      }
    }
  }

  // Compare with our own set of shortcut IDs
  std::set<std::string> our_shortcut_ids;
  for (const auto& pair : session_context->commands) {
    our_shortcut_ids.insert(pair.first);
  }

  // Only call BindShortcuts if necessary since it opens a settings window.
  if (registered_shortcut_ids != our_shortcut_ids) {
    BindShortcuts(*session_it);
  }
}

void GlobalShortcutListenerLinux::BindShortcuts(SessionMapPair& pair) {
  const SessionKey& session_key = pair.first;
  SessionContext& session_context = *pair.second;

  dbus::MethodCall method_call(kGlobalShortcutsInterface, kMethodBindShortcuts);
  dbus::MessageWriter writer(&method_call);

  writer.AppendObjectPath(session_context.session_proxy->object_path());

  // Write the list of shortcuts
  dbus::MessageWriter shortcuts_writer(nullptr);
  writer.OpenArray("(sa{sv})", &shortcuts_writer);

  for (const auto& cmd_pair : session_context.commands) {
    const auto& command = cmd_pair.second;
    dbus::MessageWriter struct_writer(nullptr);
    shortcuts_writer.OpenStruct(&struct_writer);

    // Shortcut ID
    struct_writer.AppendString(command.command_name());

    // Properties
    DbusDictionary props;
    props.Put(
        "description",
        MakeDbusVariant(DbusString(base::UTF16ToUTF8(command.description()))));
    if (command.accelerator().key_code()) {
      std::string trigger = ui::AcceleratorToXdgShortcut(command.accelerator());
      props.Put("preferred_trigger", MakeDbusVariant(DbusString(trigger)));
    }
    props.Write(&struct_writer);

    shortcuts_writer.CloseContainer(&struct_writer);
  }

  writer.CloseContainer(&shortcuts_writer);

  // Parent window (empty)
  writer.AppendString("");

  DbusDictionary options;
  std::string bind_request_token = base::UnguessableToken::Create().ToString();
  options.Put("handle_token", MakeDbusVariant(DbusString(bind_request_token)));
  options.Write(&writer);

  dbus::ObjectPath bind_request_path(base::nix::XdgDesktopPortalRequestPath(
      bus_->GetConnectionName(), bind_request_token));
  session_context.request_proxy = {
      bus_, bus_->GetObjectProxy(kPortalServiceName, bind_request_path)};

  session_context.request_proxy->ConnectToSignal(
      kRequestInterface, kSignalResponse,
      base::BindRepeating(&GlobalShortcutListenerLinux::OnBindShortcutsSignal,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&GlobalShortcutListenerLinux::OnSignalConnected,
                     weak_ptr_factory_.GetWeakPtr()));

  global_shortcuts_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&GlobalShortcutListenerLinux::OnBindShortcutsResponse,
                     weak_ptr_factory_.GetWeakPtr(), session_key));
  session_context.bind_shortcuts_called = true;
}

void GlobalShortcutListenerLinux::OnBindShortcutsResponse(
    const SessionKey& session_key,
    dbus::Response* response) {
  auto it = session_map_.find(session_key);
  if (it == session_map_.end()) {
    LOG(ERROR) << "Session context not found for session_key.";
    return;
  }

  if (!response) {
    LOG(ERROR) << "BindShortcuts call failed.";
    session_map_.erase(session_key);
    return;
  }

  dbus::MessageReader reader(response);
  dbus::ObjectPath request_handle;
  if (!reader.PopObjectPath(&request_handle)) {
    LOG(ERROR) << "Failed to read request handle.";
    session_map_.erase(session_key);
    return;
  }

  // Check that the request_handle matches our expected request_path
  if (request_handle != it->second->request_proxy->object_path()) {
    LOG(ERROR) << "Request handle does not match expected request path.";
    session_map_.erase(session_key);
    return;
  }

  // Waiting for the Response signal on the Request object
}

void GlobalShortcutListenerLinux::OnBindShortcutsSignal(dbus::Signal* signal) {
  dbus::MessageReader reader(signal);

  uint32_t response;
  if (!reader.PopUint32(&response)) {
    LOG(ERROR) << "Failed to read response from signal.";
    return;
  }

  if (response != 0) {
    LOG(ERROR) << "BindShortcuts failed with response code: " << response;
    return;
  }

  // Shortcuts successfully bound. The signal also includes information about
  // the bound shortcuts, but it's currently not needed.
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

GlobalShortcutListenerLinux::ScopedObjectProxy::ScopedObjectProxy() = default;

GlobalShortcutListenerLinux::ScopedObjectProxy::ScopedObjectProxy(
    scoped_refptr<dbus::Bus> bus,
    dbus::ObjectProxy* object_proxy)
    : bus_(std::move(bus)), object_proxy_(object_proxy) {}

GlobalShortcutListenerLinux::ScopedObjectProxy::ScopedObjectProxy(
    ScopedObjectProxy&& other) noexcept = default;

GlobalShortcutListenerLinux::ScopedObjectProxy&
GlobalShortcutListenerLinux::ScopedObjectProxy::operator=(
    ScopedObjectProxy&& other) noexcept = default;

GlobalShortcutListenerLinux::ScopedObjectProxy::~ScopedObjectProxy() {
  if (!object_proxy_) {
    return;
  }

  CHECK(bus_);

  // Detach() must be called on the DBus task runner.
  bus_->GetDBusTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<dbus::Bus> bus, dbus::ObjectProxy* object_proxy) {
            // `bus` is intentionally unused and only functions to guarantee
            // `object_proxy` is alive while this function runs.
            object_proxy->Detach();
          },
          // Unretained is safe for `object_proxy_` since it's owned by `bus_`
          // which is also bound (as a RefCountedThreadSafe) in the callback.
          std::move(bus_), base::Unretained(object_proxy_)));
}

GlobalShortcutListenerLinux::SessionContext::SessionContext(
    Observer* observer,
    const CommandMap& commands)
    : observer(observer), commands(commands) {}

GlobalShortcutListenerLinux::SessionContext::~SessionContext() = default;

}  // namespace extensions
