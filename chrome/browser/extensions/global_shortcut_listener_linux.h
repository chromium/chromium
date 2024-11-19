// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_LINUX_H_
#define CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_LINUX_H_

#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/global_shortcut_listener.h"
#include "dbus/bus.h"
#include "dbus/object_proxy.h"

namespace extensions {

// Linux-specific implementation of the GlobalShortcutListener class that
// listens for global shortcuts using the org.freedesktop.portal.GlobalShortcuts
// interface.
class GlobalShortcutListenerLinux : public GlobalShortcutListener {
 public:
  explicit GlobalShortcutListenerLinux(scoped_refptr<dbus::Bus> bus);

  GlobalShortcutListenerLinux(const GlobalShortcutListenerLinux&) = delete;
  GlobalShortcutListenerLinux& operator=(const GlobalShortcutListenerLinux&) =
      delete;

  ~GlobalShortcutListenerLinux() override;

 private:
  struct SessionKey {
    ExtensionId extension_id;
    std::string profile_id;

    std::string GetTokenKey() const;

    bool operator<(const SessionKey& other) const {
      return std::tie(extension_id, profile_id) <
             std::tie(other.extension_id, other.profile_id);
    }
  };

  // This helper manages ObjectProxy lifetimes for XDG Requests. This is
  // required to prevent leaks as the only way to unregister a signal registered
  // on an ObjectProxy object is to Detach() the ObjectProxy. On XDG requests,
  // the signal handler is emitted at most once when the request finishes, and
  // is not needed after that.
  class ScopedObjectProxy {
   public:
    ScopedObjectProxy();
    ScopedObjectProxy(scoped_refptr<dbus::Bus> bus,
                      dbus::ObjectProxy* object_proxy);
    ScopedObjectProxy(ScopedObjectProxy&& other) noexcept;
    ScopedObjectProxy& operator=(ScopedObjectProxy&& other) noexcept;
    ~ScopedObjectProxy();

    dbus::ObjectProxy* get() { return object_proxy_; }
    const dbus::ObjectProxy* get() const { return object_proxy_; }

    dbus::ObjectProxy* operator->() { return object_proxy_; }
    const dbus::ObjectProxy* operator->() const { return object_proxy_; }

   private:
    scoped_refptr<dbus::Bus> bus_;
    raw_ptr<dbus::ObjectProxy> object_proxy_ = nullptr;
  };

  struct SessionContext {
    SessionContext(Observer* observer, const CommandMap& commands);
    ~SessionContext();

    ScopedObjectProxy session_proxy;
    // There may be at most one outstanding request at a time. This proxy
    // corresponds to one type of request (CreateSession, BindShortcuts,
    // ListShortcuts). If the proxy is nullptr, then there are no outstanding
    // requests.
    ScopedObjectProxy request_proxy;
    const raw_ptr<Observer> observer;
    CommandMap commands;
    bool bind_shortcuts_called = false;
  };

  using SessionMap =
      base::flat_map<SessionKey, std::unique_ptr<SessionContext>>;
  using SessionMapPair = std::pair<SessionKey, std::unique_ptr<SessionContext>>;

  // GlobalShortcutListener:
  void StartListening() override;
  void StopListening() override;
  bool RegisterAcceleratorImpl(const ui::Accelerator& accelerator) override;
  void UnregisterAcceleratorImpl(const ui::Accelerator& accelerator) override;
  void UnregisterAccelerators(Observer* observer) override;
  bool IsRegistrationHandledExternally() const override;
  void OnCommandsChanged(const ExtensionId& extension_id,
                         const std::string& profile_id,
                         const CommandMap& commands,
                         Observer* observer) override;

  // Callbacks for DBus responses.
  void OnCreateSessionResponse(const SessionKey& session_key,
                               dbus::Response* response);
  void OnListShortcutsResponse(const SessionKey& session_key,
                               dbus::Response* response);
  void OnBindShortcutsResponse(const SessionKey& session_key,
                               dbus::Response* response);

  // Callbacks for DBus signals.
  void OnCreateSessionSignal(const SessionKey& session_key,
                             dbus::Signal* signal);
  void OnListShortcutsSignal(const SessionKey& session_key,
                             dbus::Signal* signal);
  void OnBindShortcutsSignal(dbus::Signal* signal);
  void OnActivatedSignal(dbus::Signal* signal);

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool success);

  void OnServiceStarted(std::optional<bool> service_started);

  void CreateSession(SessionMapPair& pair);

  void BindShortcuts(SessionMapPair& pair);

  // DBus components.
  scoped_refptr<dbus::Bus> bus_;
  raw_ptr<dbus::ObjectProxy> global_shortcuts_proxy_ = nullptr;

  // Whether the GlboalShortcuts service is available, or nullopt if the status
  // is not yet known.
  std::optional<bool> service_started_;

  // One session per extension.
  base::flat_map<SessionKey, std::unique_ptr<SessionContext>> session_map_;

  base::WeakPtrFactory<GlobalShortcutListenerLinux> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_LINUX_H_
