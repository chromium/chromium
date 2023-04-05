// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/guest_os_wayland_server.h"

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "chrome/browser/ash/borealis/borealis_security_delegate.h"
#include "chrome/browser/ash/crostini/crostini_security_delegate.h"
#include "chrome/browser/ash/guest_os/guest_os_security_delegate.h"
#include "chrome/browser/ash/guest_os/infra/cached_callback.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/dbus/vm_launch/launch.pb.h"
#include "chromeos/ash/components/dbus/vm_wl/wl.pb.h"
#include "components/exo/server/wayland_server_controller.h"
#include "components/exo/server/wayland_server_handle.h"

namespace guest_os {

namespace {

using StartServerResponse =
    borealis::Expected<vm_tools::launch::StartWaylandServerResponse,
                       std::string>;

void OnWaylandServerStarted(
    base::OnceCallback<void(StartServerResponse)> response_callback,
    GuestOsWaylandServer::Result result) {
  if (!result) {
    std::string reason;
    switch (result.Error()) {
      case GuestOsWaylandServer::ServerFailure::kUnknownVmType:
        reason = "requested VM type is not known";
        break;
      case GuestOsWaylandServer::ServerFailure::kUndefinedSecurityDelegate:
        reason = "could not generate security_delegate";
        break;
      case GuestOsWaylandServer::ServerFailure::kFailedToSpawn:
        reason = "could not spawn the server";
        break;
      case GuestOsWaylandServer::ServerFailure::kRejected:
        reason = "request rejected";
        break;
    }
    std::move(response_callback)
        .Run(StartServerResponse::Unexpected(
            "Wayland server creation failed: " + reason));
    return;
  }
  vm_tools::launch::StartWaylandServerResponse response;
  response.mutable_server()->set_path(
      result.Value()->server_path().AsUTF8Unsafe());
  std::move(response_callback).Run(StartServerResponse(std::move(response)));
}

}  // namespace

class GuestOsWaylandServer::DelegateHolder
    : public CachedCallback<ServerDetails, ServerFailure> {
 public:
  // To create a capability set we allow each VM type to asynchronously build
  // and return the security_delegate. Callees can reject the request by
  // returning nullptr instead of the security_delegate.
  using CapabilityFactory = base::RepeatingCallback<void(
      base::OnceCallback<void(std::unique_ptr<GuestOsSecurityDelegate>)>)>;

  explicit DelegateHolder(CapabilityFactory cap_factory)
      : cap_factory_(std::move(cap_factory)) {}

 private:
  static void OnServerCreated(RealCallback callback,
                              base::WeakPtr<GuestOsSecurityDelegate> cap_ptr,
                              bool success,
                              const base::FilePath& path) {
    if (!success) {
      std::move(callback).Run(Failure(ServerFailure::kFailedToSpawn));
      return;
    }
    DCHECK(cap_ptr);
    DCHECK(!path.empty());
    std::move(callback).Run(Success(cap_ptr, path));
  }

  static void OnSecurityDelegateCreated(
      RealCallback callback,
      std::unique_ptr<GuestOsSecurityDelegate> caps) {
    if (!caps) {
      std::move(callback).Run(
          Failure(ServerFailure::kUndefinedSecurityDelegate));
      return;
    }
    GuestOsSecurityDelegate::BuildServer(
        std::move(caps), base::BindOnce(&OnServerCreated, std::move(callback)));
  }

  // CachedCallback overrides.
  void Build(RealCallback callback) override {
    cap_factory_.Run(
        base::BindOnce(&OnSecurityDelegateCreated, std::move(callback)));
  }
  ServerFailure Reject() override { return ServerFailure::kRejected; }

  CapabilityFactory cap_factory_;
};

GuestOsWaylandServer::ServerDetails::ServerDetails(
    base::WeakPtr<GuestOsSecurityDelegate> security_delegate,
    base::FilePath path)
    : security_delegate_(security_delegate), server_path_(std::move(path)) {}

GuestOsWaylandServer::ServerDetails::~ServerDetails() {
  // In tests, this is used to avoid dealing with the real server controller.
  if (server_path_.empty()) {
    return;
  }
  GuestOsSecurityDelegate::MaybeRemoveServer(security_delegate_, server_path_);
}

GuestOsWaylandServer::ScopedServer::ScopedServer(
    std::unique_ptr<exo::WaylandServerHandle> handle,
    base::WeakPtr<GuestOsSecurityDelegate> security_delegate)
    : handle_(std::move(handle)), security_delegate_(security_delegate) {}

GuestOsWaylandServer::ScopedServer::~ScopedServer() = default;

// static
void GuestOsWaylandServer::StartServer(
    const vm_tools::launch::StartWaylandServerRequest& request,
    base::OnceCallback<void(StartServerResponse)> response_callback) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile || ash::ProfileHelper::GetUserIdHashFromProfile(profile) !=
                      request.owner_id()) {
    std::move(response_callback)
        .Run(StartServerResponse::Unexpected("Invalid owner_id"));
    return;
  }

  GuestOsService::GetForProfile(profile)->WaylandServer()->Get(
      request.vm_type(),
      base::BindOnce(&OnWaylandServerStarted, std::move(response_callback)));
}

// static
void GuestOsWaylandServer::ListenOnSocket(
    const vm_tools::wl::ListenOnSocketRequest& request,
    base::ScopedFD socket_fd,
    base::OnceCallback<void(absl::optional<std::string>)> response_callback) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile || ash::ProfileHelper::GetUserIdHashFromProfile(profile) !=
                      request.desc().owner_id()) {
    std::move(response_callback).Run({"Invalid owner_id"});
    return;
  }
  GuestOsService::GetForProfile(profile)->WaylandServer()->Listen(
      std::move(socket_fd), request.desc().type(), request.desc().name(),
      std::move(response_callback));
}

// static
void GuestOsWaylandServer::CloseSocket(
    const vm_tools::wl::CloseSocketRequest& request,
    base::OnceCallback<void(absl::optional<std::string>)> response_callback) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile || ash::ProfileHelper::GetUserIdHashFromProfile(profile) !=
                      request.desc().owner_id()) {
    std::move(response_callback).Run({"Invalid owner_id"});
    return;
  }
  GuestOsService::GetForProfile(profile)->WaylandServer()->Close(
      request.desc().type(), request.desc().name(),
      std::move(response_callback));
}

GuestOsWaylandServer::GuestOsWaylandServer(Profile* profile)
    : profile_(profile) {
  delegate_holders_[vm_tools::launch::BOREALIS] =
      std::make_unique<DelegateHolder>(base::BindRepeating(
          &borealis::BorealisSecurityDelegate::Build, profile_));
  delegate_holders_[vm_tools::launch::TERMINA] =
      std::make_unique<DelegateHolder>(base::BindRepeating(
          &crostini::CrostiniSecurityDelegate::Build, profile_));
}

GuestOsWaylandServer::~GuestOsWaylandServer() = default;

void GuestOsWaylandServer::Get(vm_tools::launch::VmType vm_type,
                               base::OnceCallback<void(Result)> callback) {
  auto holder_iter = delegate_holders_.find(vm_type);
  if (holder_iter == delegate_holders_.end()) {
    std::move(callback).Run(Result::Unexpected(ServerFailure::kUnknownVmType));
    return;
  }
  holder_iter->second->Get(std::move(callback));
}

// Returns a weak handle to the security delegate for the VM with the given
// |name| and |type|, if one exists, and nullptr otherwise.
base::WeakPtr<GuestOsSecurityDelegate> GuestOsWaylandServer::GetDelegate(
    vm_tools::apps::VmType type,
    const std::string& name) const {
  auto type_iter = servers_.find(type);
  if (type_iter == servers_.end()) {
    return nullptr;
  }
  auto name_iter = type_iter->second.find(name);
  if (name_iter == type_iter->second.end()) {
    return nullptr;
  }
  return name_iter->second->security_delegate();
}

void GuestOsWaylandServer::SetCapabilityFactoryForTesting(
    vm_tools::launch::VmType vm_type,
    DelegateHolder::CapabilityFactory factory) {
  delegate_holders_[vm_type] = std::make_unique<DelegateHolder>(factory);
}

void GuestOsWaylandServer::OverrideServerForTesting(
    vm_tools::launch::VmType vm_type,
    base::WeakPtr<GuestOsSecurityDelegate> security_delegate,
    base::FilePath path) {
  delegate_holders_[vm_type]->CacheForTesting(  // IN-TEST
      std::make_unique<ServerDetails>(security_delegate, std::move(path)));
}

void GuestOsWaylandServer::Listen(base::ScopedFD fd,
                                  vm_tools::apps::VmType type,
                                  const std::string& name,
                                  ResponseCallback callback) {
  if (servers_[type].erase(name) > 0) {
    LOG(WARNING) << "Re-binding wayland server for " << name << "(type=" << type
                 << ") while in-use";
  }
  switch (type) {
    case vm_tools::apps::TERMINA:
      crostini::CrostiniSecurityDelegate::Build(
          profile_,
          base::BindOnce(&GuestOsWaylandServer::OnSecurityDelegateCreated,
                         weak_factory_.GetWeakPtr(), std::move(fd), type, name,
                         std::move(callback)));
      return;
    case vm_tools::apps::BOREALIS:
      borealis::BorealisSecurityDelegate::Build(
          profile_,
          base::BindOnce(&GuestOsWaylandServer::OnSecurityDelegateCreated,
                         weak_factory_.GetWeakPtr(), std::move(fd), type, name,
                         std::move(callback)));
      return;
    default:
      // For all other VMs, provide the minimal capability-set.
      OnSecurityDelegateCreated(std::move(fd), type, name, std::move(callback),
                                std::make_unique<GuestOsSecurityDelegate>());
      return;
  }
}

void GuestOsWaylandServer::Close(vm_tools::apps::VmType type,
                                 const std::string& name,
                                 ResponseCallback callback) {
  if (servers_[type].erase(name) == 0) {
    LOG(WARNING) << "Trying to close non-existent server for " << name
                 << "(type=" << type << ")";
  }
  std::move(callback).Run(absl::nullopt);
}

void GuestOsWaylandServer::OnSecurityDelegateCreated(
    base::ScopedFD fd,
    vm_tools::apps::VmType type,
    std::string name,
    ResponseCallback callback,
    std::unique_ptr<GuestOsSecurityDelegate> delegate) {
  if (!delegate) {
    std::move(callback).Run({"Failed to get security privileges"});
    return;
  }
  GuestOsSecurityDelegate::MakeServerWithFd(
      std::move(delegate), std::move(fd),
      base::BindOnce(&GuestOsWaylandServer::OnServerCreated,
                     weak_factory_.GetWeakPtr(), type, std::move(name),
                     std::move(callback)));
}

void GuestOsWaylandServer::OnServerCreated(
    vm_tools::apps::VmType type,
    std::string name,
    ResponseCallback callback,
    base::WeakPtr<GuestOsSecurityDelegate> delegate,
    std::unique_ptr<exo::WaylandServerHandle> handle) {
  if (!handle) {
    std::move(callback).Run({"Failed to create wayland server"});
    return;
  }
  servers_[type].insert_or_assign(
      std::move(name),
      std::make_unique<ScopedServer>(std::move(handle), delegate));
  std::move(callback).Run(absl::nullopt);
}

}  // namespace guest_os
