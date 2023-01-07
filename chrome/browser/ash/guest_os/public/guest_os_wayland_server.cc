// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/guest_os_wayland_server.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/ash/borealis/borealis_security_delegate.h"
#include "chrome/browser/ash/crostini/crostini_security_delegate.h"
#include "chrome/browser/ash/guest_os/guest_os_security_delegate.h"
#include "chrome/browser/ash/guest_os/infra/cached_callback.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/dbus/vm_launch/launch.pb.h"

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
  if (server_path_.empty())
    return;
  GuestOsSecurityDelegate::MaybeRemoveServer(security_delegate_, server_path_);
}

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

}  // namespace guest_os
