// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_wayland_interface.h"

#include "base/callback.h"
#include "chrome/browser/ash/borealis/borealis_capabilities.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "components/exo/server/wayland_server_controller.h"

namespace borealis {

BorealisWaylandInterface::BorealisWaylandInterface(Profile* profile)
    : profile_(profile) {}

BorealisWaylandInterface::~BorealisWaylandInterface() {
  if (capabilities_ && !server_path_.empty()) {
    exo::WaylandServerController* controller =
        exo::WaylandServerController::Get();
    // Exo's destructor can run before borealis'. When that happens exo is
    // deleting the server itself and we don't need to. See crbug.com/1295392.
    if (controller)
      controller->DeleteServer(server_path_);
  }
}

void BorealisWaylandInterface::GetWaylandServer(CapabilityCallback callback) {
  // The custom wayland server will be mandatory for borealis going forward, so
  // it is a good place to guard against unauthorized launches.
  BorealisService::GetForProfile(profile_)->Features().IsAllowed(
      base::BindOnce(&BorealisWaylandInterface::OnAllowednessChecked,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BorealisWaylandInterface::OnAllowednessChecked(
    CapabilityCallback callback,
    BorealisFeatures::AllowStatus allowed) {
  if (allowed != BorealisFeatures::AllowStatus::kAllowed) {
    LOG(WARNING) << "Borealis is not allowed: " << allowed;
    std::move(callback).Run(nullptr, {});
    return;
  }

  if (capabilities_) {
    // If there is a current operation in-progress we will just bail out. Its
    // very unlikely that the user can run into this and if they do a retry will
    // work.
    if (server_path_.empty()) {
      std::move(callback).Run(nullptr, {});
      return;
    }
    std::move(callback).Run(capabilities_, server_path_);
    return;
  }
  auto caps = std::make_unique<BorealisCapabilities>();
  server_path_ = {};
  capabilities_ = caps.get();
  exo::WaylandServerController::Get()->CreateServer(
      std::move(caps),
      base::BindOnce(&BorealisWaylandInterface::OnWaylandServerCreated,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BorealisWaylandInterface::OnWaylandServerCreated(
    CapabilityCallback callback,
    bool success,
    const base::FilePath& server_path) {
  if (!success) {
    capabilities_ = nullptr;
    std::move(callback).Run(nullptr, {});
    return;
  }
  server_path_ = server_path;
  std::move(callback).Run(capabilities_, server_path_);
}

}  // namespace borealis
