// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_WAYLAND_SERVER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_WAYLAND_SERVER_H_

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"

class Profile;

namespace exo {
class WaylandServerHandle;
}

namespace vm_tools {

namespace apps {
enum VmType : int;
}

namespace wl {
class ListenOnSocketRequest;
class CloseSocketRequest;
}  // namespace wl

}  // namespace vm_tools

namespace guest_os {

class GuestOsSecurityDelegate;

// Holds references to the wayland servers created by concierge on the vm_wl
// protocol (see go/securer-exo-ids for details). Concierge will create one
// server per-vm-instance.
class GuestOsWaylandServer : public ash::ConciergeClient::Observer {
 public:
  class ScopedServer {
   public:
    ScopedServer(std::unique_ptr<exo::WaylandServerHandle> handle,
                 base::WeakPtr<GuestOsSecurityDelegate> security_delegate);

    // No copying
    ScopedServer(const ScopedServer&) = delete;
    ScopedServer& operator=(const ScopedServer&) = delete;

    ~ScopedServer();

    base::WeakPtr<GuestOsSecurityDelegate> security_delegate() const {
      return security_delegate_;
    }

   private:
    std::unique_ptr<exo::WaylandServerHandle> handle_;
    base::WeakPtr<GuestOsSecurityDelegate> security_delegate_;
  };

  using ResponseCallback = base::OnceCallback<void(std::optional<std::string>)>;

  using ServersByName =
      base::flat_map<std::string, std::unique_ptr<ScopedServer>>;
  using ServersByType = base::flat_map<vm_tools::apps::VmType, ServersByName>;

  // Use the given |socket_fd| as a wayland socket for the VM given by
  // |request|. Invokes the |response_callback| with nullopt on success, or a
  // string description of an error on failure.
  static void ListenOnSocket(const vm_tools::wl::ListenOnSocketRequest& request,
                             base::ScopedFD socket_fd,
                             ResponseCallback callback);

  // Advise that the wayland server for the VM given in |request| is no-longer
  // needed. Invokes the |response_callback| with nullopt on success, or a
  // string description of an error on failure.
  static void CloseSocket(const vm_tools::wl::CloseSocketRequest& request,
                          ResponseCallback callback);

  explicit GuestOsWaylandServer(Profile* profile);

  ~GuestOsWaylandServer() override;

  // Returns a weak handle to the security delegate for the VM with the given
  // |name| and |type|, if one exists, and nullptr otherwise.
  base::WeakPtr<GuestOsSecurityDelegate> GetDelegate(
      vm_tools::apps::VmType type,
      const std::string& name) const;

  void Listen(base::ScopedFD fd,
              vm_tools::apps::VmType type,
              const std::string& name,
              ResponseCallback callback);

  void Close(vm_tools::apps::VmType type,
             const std::string& name,
             ResponseCallback callback);

 private:
  void OnSecurityDelegateCreated(
      base::ScopedFD fd,
      vm_tools::apps::VmType type,
      std::string name,
      ResponseCallback callback,
      std::unique_ptr<GuestOsSecurityDelegate> delegate);

  void OnServerCreated(vm_tools::apps::VmType type,
                       std::string name,
                       ResponseCallback callback,
                       base::WeakPtr<GuestOsSecurityDelegate> delegate,
                       std::unique_ptr<exo::WaylandServerHandle> handle);

  //  ash::ConciergeClient::Observer::
  void ConciergeServiceStarted() override;
  void ConciergeServiceStopped() override;

  raw_ptr<Profile> profile_;

  ServersByType servers_;

  base::WeakPtrFactory<GuestOsWaylandServer> weak_factory_{this};
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_WAYLAND_SERVER_H_
