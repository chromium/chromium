// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_wayland_interface.h"

#include "base/callback.h"
#include "chrome/browser/ash/borealis/borealis_capabilities.h"
#include "components/exo/server/wayland_server_controller.h"

namespace borealis {

BorealisWaylandInterface::BorealisWaylandInterface(Profile* profile)
    : profile_(profile), capabilities_(nullptr) {
  (void)profile_;
}

BorealisWaylandInterface::~BorealisWaylandInterface() {
  if (capabilities_ && !server_path_.empty())
    exo::WaylandServerController::Get()->DeleteServer(server_path_);
}

void BorealisWaylandInterface::GetWaylandServer(
    base::OnceCallback<void(BorealisCapabilities*, const base::FilePath&)>
        callback) {
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
    base::OnceCallback<void(BorealisCapabilities*, const base::FilePath&)>
        callback,
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
