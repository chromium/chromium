// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/guest_os_wayland_server.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/ash/borealis/borealis_capabilities.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/crostini/crostini_capabilities.h"
#include "chrome/browser/ash/guest_os/guest_os_capabilities.h"
#include "chrome/browser/ash/guest_os/infra/cached_callback.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/dbus/vm_launch/launch.pb.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

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
      case GuestOsWaylandServer::ServerFailure::kUndefinedCapabilities:
        reason = "could not generate capabilities";
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

class GuestOsWaylandServer::CapabilityHolder
    : public CachedCallback<ServerDetails, ServerFailure> {
 public:
  // To create a capability set we allow each VM type to asynchronously build
  // and return the capabilities. Callees can reject the request by returning
  // nullptr instead of the capabilities.
  using CapabilityFactory = base::RepeatingCallback<void(
      base::OnceCallback<void(std::unique_ptr<GuestOsCapabilities>)>)>;

  explicit CapabilityHolder(CapabilityFactory cap_factory)
      : cap_factory_(std::move(cap_factory)) {}

 private:
  static void OnServerCreated(RealCallback callback,
                              base::WeakPtr<GuestOsCapabilities> cap_ptr,
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

  static void OnCapabilitiesCreated(RealCallback callback,
                                    std::unique_ptr<GuestOsCapabilities> caps) {
    if (!caps) {
      std::move(callback).Run(Failure(ServerFailure::kUndefinedCapabilities));
      return;
    }
    GuestOsCapabilities::BuildServer(
        std::move(caps), base::BindOnce(&OnServerCreated, std::move(callback)));
  }

  // CachedCallback overrides.
  void Build(RealCallback callback) override {
    cap_factory_.Run(
        base::BindOnce(&OnCapabilitiesCreated, std::move(callback)));
  }
  ServerFailure Reject() override { return ServerFailure::kRejected; }

  CapabilityFactory cap_factory_;
};

GuestOsWaylandServer::ServerDetails::ServerDetails(
    base::WeakPtr<GuestOsCapabilities> capabilities,
    base::FilePath path)
    : capabilities_(capabilities), server_path_(std::move(path)) {}

GuestOsWaylandServer::ServerDetails::~ServerDetails() {
  // In tests, this is used to avoid dealing with the real server controller.
  if (server_path_.empty())
    return;
  GuestOsCapabilities::MaybeRemoveServer(capabilities_, server_path_);
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
  capability_holders_[vm_tools::launch::BOREALIS] =
      std::make_unique<CapabilityHolder>(base::BindRepeating(
          &borealis::BorealisCapabilities::Build, profile_));
  capability_holders_[vm_tools::launch::TERMINA] =
      std::make_unique<CapabilityHolder>(base::BindRepeating(
          &crostini::CrostiniCapabilities::Build, profile_));
}

GuestOsWaylandServer::~GuestOsWaylandServer() = default;

void GuestOsWaylandServer::Get(vm_tools::launch::VmType vm_type,
                               base::OnceCallback<void(Result)> callback) {
  auto holder_iter = capability_holders_.find(vm_type);
  if (holder_iter == capability_holders_.end()) {
    std::move(callback).Run(Result::Unexpected(ServerFailure::kUnknownVmType));
    return;
  }
  holder_iter->second->Get(std::move(callback));
}

void GuestOsWaylandServer::SetCapabilityFactoryForTesting(
    vm_tools::launch::VmType vm_type,
    CapabilityHolder::CapabilityFactory factory) {
  capability_holders_[vm_type] = std::make_unique<CapabilityHolder>(factory);
}

void GuestOsWaylandServer::OverrideServerForTesting(
    vm_tools::launch::VmType vm_type,
    base::WeakPtr<GuestOsCapabilities> capabilities,
    base::FilePath path) {
  capability_holders_[vm_type]->CacheForTesting(  // IN-TEST
      std::make_unique<ServerDetails>(capabilities, std::move(path)));
}

}  // namespace guest_os
