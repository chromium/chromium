// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/campaigns_manager_client_impl.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/version.h"
#include "chrome/browser/ash/login/demo_mode/demo_components.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_dimensions.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

inline constexpr char kCampaignComponentName[] = "growth-campaigns";

}  // namespace

CampaignsManagerClientImpl::CampaignsManagerClientImpl()
    : campaigns_manager_(std::make_unique<growth::CampaignsManager>(
          /*client=*/this,
          g_browser_process->local_state())) {}

CampaignsManagerClientImpl::~CampaignsManagerClientImpl() = default;

void CampaignsManagerClientImpl::LoadCampaignsComponent(
    growth::CampaignComponentLoadedCallback callback) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kGrowthCampaignsPath)) {
    std::move(callback).Run(base::FilePath(command_line->GetSwitchValueASCII(
        ash::switches::kGrowthCampaignsPath)));
    return;
  }

  // Loads campaigns component.
  auto cros_component_manager =
      g_browser_process->platform_part()->cros_component_manager();
  CHECK(cros_component_manager);

  cros_component_manager->Load(
      kCampaignComponentName,
      component_updater::CrOSComponentManager::MountPolicy::kMount,
      component_updater::CrOSComponentManager::UpdatePolicy::kDontForce,
      base::BindOnce(&CampaignsManagerClientImpl::OnComponentDownloaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool CampaignsManagerClientImpl::IsDeviceInDemoMode() const {
  return ash::DemoSession::IsDeviceInDemoMode();
}

bool CampaignsManagerClientImpl::IsCloudGamingDevice() const {
  return ash::demo_mode::IsCloudGamingDevice();
}

bool CampaignsManagerClientImpl::IsFeatureAwareDevice() const {
  return ash::demo_mode::IsFeatureAwareDevice();
}

const std::string& CampaignsManagerClientImpl::GetApplicationLocale() const {
  return g_browser_process->GetApplicationLocale();
}

const base::Version& CampaignsManagerClientImpl::GetDemoModeAppVersion() const {
  auto* demo_session = ash::DemoSession::Get();
  CHECK(demo_session);

  const auto& version = demo_session->components()->app_component_version();
  if (!version.has_value()) {
    // TODO(b/299305911): Add metrics to track the case that version is not
    // available and convert to CHECK if we are confident that it will always
    // available at this point.
    static const base::NoDestructor<base::Version> empty_version;
    return *empty_version;
  }

  return version.value();
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
