// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_wayland_server.h"

#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_wayland_interface.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace guest_os {

namespace {

using ResponseType =
    borealis::Expected<vm_tools::launch::StartWaylandServerResponse,
                       std::string>;

void OnWaylandServerStarted(
    base::OnceCallback<void(ResponseType)> response_callback,
    borealis::BorealisCapabilities* capabilities,
    const base::FilePath& path) {
  if (!capabilities || path.empty()) {
    std::move(response_callback)
        .Run(ResponseType::Unexpected("Wayland server creation failed"));
    return;
  }
  vm_tools::launch::StartWaylandServerResponse response;
  response.mutable_server()->set_path(path.AsUTF8Unsafe());
  std::move(response_callback).Run(ResponseType(std::move(response)));
}

}  // namespace

void GuestOsWaylandServer::StartServer(
    const vm_tools::launch::StartWaylandServerRequest& request,
    base::OnceCallback<void(ResponseType)> response_callback) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile || ash::ProfileHelper::GetUserIdHashFromProfile(profile) !=
                      request.owner_id()) {
    std::move(response_callback)
        .Run(ResponseType::Unexpected("Invalid owner_id"));
    return;
  }

  switch (request.vm_type()) {
    case vm_tools::launch::VmType::BOREALIS:
      borealis::BorealisService::GetForProfile(profile)
          ->WaylandInterface()
          .GetWaylandServer(base::BindOnce(&OnWaylandServerStarted,
                                           std::move(response_callback)));
      break;
    default:
      std::move(response_callback)
          .Run(ResponseType::Unexpected(
              "Not implemented for the given vm_type"));
      break;
  }
}

}  // namespace guest_os
