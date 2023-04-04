// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_WAYLAND_SERVER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_WAYLAND_SERVER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/borealis/infra/expected.h"
#include "chromeos/ash/components/dbus/vm_launch/launch.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  using ResponseCallback =
      base::OnceCallback<void(absl::optional<std::string>)>;

  using ServersByName =
      base::flat_map<std::string, std::unique_ptr<ScopedServer>>;
  using ServersByType = base::flat_map<vm_tools::apps::VmType, ServersByName>;

  // Creates a wayland server as per the |request|, and responds with the
  // relevant details in the |response_callback|. This API is used by e.g.
  // dbus.
  //
  // TODO(b/270254359): deprecate this method.
  static void StartServer(
      const vm_tools::launch::StartWaylandServerRequest& request,
      base::OnceCallback<
          void(borealis::Expected<vm_tools::launch::StartWaylandServerResponse,
                                  std::string>)> response_callback);

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

  ~GuestOsWaylandServer();

  // Gets details for the wayland server for the given |vm_type|, returning
  // those as a Result via |callback|. Spawns the server if needed, otherwise
  // re-uses the previous one. Servers will persist for the entire chrome
  // session, and are removed at logout.
  void Get(vm_tools::launch::VmType vm_type,
           base::OnceCallback<void(Result)> callback);

  // Returns a weak handle to the security delegate for the VM with the given
  // |name| and |type|, if one exists, and nullptr otherwise.
  base::WeakPtr<GuestOsSecurityDelegate> GetDelegate(
      vm_tools::apps::VmType type,
      const std::string& name) const;

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

  void Listen(base::ScopedFD fd,
              vm_tools::apps::VmType type,
              const std::string& name,
              ResponseCallback callback);

  void Close(vm_tools::apps::VmType type,
             const std::string& name,
             ResponseCallback callback);

 private:
  class DelegateHolder;

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

  Profile* profile_;

  base::flat_map<vm_tools::launch::VmType, std::unique_ptr<DelegateHolder>>
      delegate_holders_;

  ServersByType servers_;

  base::WeakPtrFactory<GuestOsWaylandServer> weak_factory_{this};
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_WAYLAND_SERVER_H_
