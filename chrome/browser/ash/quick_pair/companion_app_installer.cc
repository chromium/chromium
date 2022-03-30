// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/quick_pair/companion_app_installer.h"

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/connection_holder.h"
#include "ash/quick_pair/common/logging.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"

using CompanionAppState =
    ash::quick_pair::CompanionAppInstaller::CompanionAppState;

namespace ash {
namespace quick_pair {

CompanionAppInstaller::CompanionAppInstaller() = default;

CompanionAppInstaller::~CompanionAppInstaller() = default;

void CompanionAppInstaller::CheckAppState(
    const std::string& package_name,
    base::OnceCallback<void(CompanionAppState)>
        on_companion_app_state_checked) {
  QP_LOG(INFO) << __func__ << ": package_name:" << package_name;
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    QP_LOG(WARNING) << "Arc Service Manage is null";
    std::move(on_companion_app_state_checked)
        .Run(CompanionAppState::kNotAvailable);
    return;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->app(), IsInstallable);
  if (!instance) {
    QP_LOG(WARNING) << "App Instance is null";
    std::move(on_companion_app_state_checked)
        .Run(CompanionAppState::kNotAvailable);
    return;
  }

  instance->IsInstallable(
      package_name,
      base::BindOnce(&CompanionAppInstaller::OnCheckAppIsInstallable,
                     weak_pointer_factory_.GetWeakPtr(),
                     std::move(on_companion_app_state_checked)));
}

void CompanionAppInstaller::OnCheckAppIsInstallable(
    base::OnceCallback<void(CompanionAppInstaller::CompanionAppState)>
        on_companion_app_state_checked,
    bool is_installable) {
  if (is_installable) {
    std::move(on_companion_app_state_checked)
        .Run(CompanionAppState::kAvailableToDownload);
    return;
  }

  std::move(on_companion_app_state_checked)
      .Run(CompanionAppState::kNotAvailable);
}

}  // namespace quick_pair
}  // namespace ash
