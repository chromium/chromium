// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/campaigns_manager_client_impl.h"

#include <memory>

#include "base/files/file_path.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

inline constexpr char kCampaignComponentName[] = "growth-campaigns";

}  // namespace

CampaignsManagerClientImpl::CampaignsManagerClientImpl()
    : campaigns_manager_(
          std::make_unique<growth::CampaignsManager>(/*client=*/this)) {}

CampaignsManagerClientImpl::~CampaignsManagerClientImpl() = default;

void CampaignsManagerClientImpl::LoadCampaignsComponent(
    growth::CampaignComponentLoadedCallback callback) {
  // Loads campaigns component.
  auto cros_component_manager =
      g_browser_process->platform_part()->cros_component_manager();
  DCHECK(cros_component_manager);

  cros_component_manager->Load(
      kCampaignComponentName,
      component_updater::CrOSComponentManager::MountPolicy::kMount,
      component_updater::CrOSComponentManager::UpdatePolicy::kDontForce,
      base::BindOnce(&CampaignsManagerClientImpl::OnComponentDownloaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CampaignsManagerClientImpl::OnComponentDownloaded(
    growth::CampaignComponentLoadedCallback loaded_callback,
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& path) {
  if (error != component_updater::CrOSComponentManager::Error::NONE) {
    std::move(loaded_callback).Run(absl::nullopt);
    return;
  }

  std::move(loaded_callback).Run(path);
}
