// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_WAYLAND_SERVER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_WAYLAND_SERVER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/borealis/infra/expected.h"
#include "chromeos/ash/components/dbus/vm_launch/launch.pb.h"

class Profile;

namespace guest_os {

class GuestOsSecurityDelegate;

// Holds references to the wayland servers created for GuestOS VMs. There is one
// instance of the server per-capability set, where capability-sets loosely
// correlate to VM types, i.e. there is one server and one capability set for
// all instances of Crostini VMs, but a different server+set for Borealis.
class GuestOsWaylandServer {
 public:
  // Stores details of the wayland server stored in a Guest OS cache. This
  // object controls the lifetime of that server, and deleting it shuts down the
  // server.
  class ServerDetails {
   public:
    ServerDetails(base::WeakPtr<GuestOsSecurityDelegate> security_delegate,
                  base::FilePath path);
    ~ServerDetails();

    // No copying or moving
    ServerDetails(ServerDetails&&) = delete;
    ServerDetails(const ServerDetails&) = delete;
    ServerDetails* operator=(ServerDetails&&) = delete;
    ServerDetails* operator=(const ServerDetails&) = delete;

    // This may be nullptr during shutdown.
    GuestOsSecurityDelegate* security_delegate() const {
      return security_delegate_.get();
    }

    const base::FilePath& server_path() const { return server_path_; }

   private:
    // The capability-set is owned by Exo, we hold a weak reference in case we
    // need to delete it before Exo does (on shutdown).
    base::WeakPtr<GuestOsSecurityDelegate> security_delegate_;
    base::FilePath server_path_;
  };

  // Enumerates the reasons why a wayland server might not be created.
  enum class ServerFailure {
    kUnknownVmType,
    kUndefinedSecurityDelegate,
    kFailedToSpawn,
    kRejected,
  };

  // When a server is requested, the response will either be a handle to that
  // server's details, or a failure.
  using Result = borealis::Expected<ServerDetails*, ServerFailure>;

  // Creates a wayland server as per the |request|, and responds with the
  // relevant details in the |response_callback|. This API is used by e.g.
  // dbus.
  static void StartServer(
      const vm_tools::launch::StartWaylandServerRequest& request,
      base::OnceCallback<
          void(borealis::Expected<vm_tools::launch::StartWaylandServerResponse,
                                  std::string>)> response_callback);

  explicit GuestOsWaylandServer(Profile* profile);

  ~GuestOsWaylandServer();

  // Gets details for the wayland server for the given |vm_type|, returning
  // those as a Result via |callback|. Spawns the server if needed, otherwise
  // re-uses the previous one. Servers will persist for the entire chrome
  // session, and are removed at logout.
  void Get(vm_tools::launch::VmType vm_type,
           base::OnceCallback<void(Result)> callback);

  void SetCapabilityFactoryForTesting(
      vm_tools::launch::VmType vm_type,
      base::RepeatingCallback<void(
          base::OnceCallback<void(std::unique_ptr<GuestOsSecurityDelegate>)>)>
          factory);

  // Used in tests to skip actually trying to allocate a server socket via exo.
  void OverrideServerForTesting(
      vm_tools::launch::VmType vm_type,
      base::WeakPtr<GuestOsSecurityDelegate> security_delegate,
      base::FilePath path);

 private:
  class DelegateHolder;

  Profile* profile_;

  base::flat_map<vm_tools::launch::VmType, std::unique_ptr<DelegateHolder>>
      delegate_holders_;
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_WAYLAND_SERVER_H_
