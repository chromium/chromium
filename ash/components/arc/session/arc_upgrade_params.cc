// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_upgrade_params.h"

#include "ash/components/arc/arc_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"

namespace arc {
namespace {

UpgradeParams::PackageCacheMode GetPackagesCacheMode() {
  // Set packages cache mode coming from autotests.
  const std::string packages_cache_mode_string =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ash::switches::kArcPackagesCacheMode);
  if (packages_cache_mode_string == kPackagesCacheModeSkipCopy)
    return UpgradeParams::PackageCacheMode::SKIP_SETUP_COPY_ON_INIT;
  if (packages_cache_mode_string == kPackagesCacheModeCopy)
    return UpgradeParams::PackageCacheMode::COPY_ON_INIT;

  VLOG_IF(2, !packages_cache_mode_string.empty())
      << "Invalid packages cache mode switch " << packages_cache_mode_string;
  return UpgradeParams::PackageCacheMode::DEFAULT;
}

}  // namespace

UpgradeParams::UpgradeParams()
    : skip_boot_completed_broadcast(
          !base::FeatureList::IsEnabled(arc::kBootCompletedBroadcastFeature)),
      packages_cache_mode(GetPackagesCacheMode()),
      skip_gms_core_cache(base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kArcDisableGmsCoreCache)),
      skip_tts_cache(base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kArcDisableTtsCache)),
      skip_dexopt_cache(base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kArcDisableDexOptCache)),
      enable_priority_app_lmk_delay(
          base::FeatureList::IsEnabled(kPriorityAppLmkDelay)),
      priority_app_lmk_delay_second(kPriorityAppLmkDelaySecond.Get()),
      priority_app_lmk_delay_list(kPriorityAppLmkDelayList.Get()),
      enable_lmk_perceptible_min_state_update(
          base::FeatureList::IsEnabled(kLmkPerceptibleMinStateUpdate)) {}

UpgradeParams::UpgradeParams(const UpgradeParams& other) = default;
UpgradeParams::UpgradeParams(UpgradeParams&& other) = default;
UpgradeParams& UpgradeParams::operator=(UpgradeParams&& other) = default;
UpgradeParams::~UpgradeParams() = default;

}  // namespace arc
