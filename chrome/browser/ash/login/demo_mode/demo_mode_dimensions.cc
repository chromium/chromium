// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_mode_dimensions.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/version.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/demo_mode/utils/demo_session_utils.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

namespace ash {
namespace demo_mode {

bool AreDemoDimensionsAccessible() {
  return IsDeviceInDemoMode() ||
         DemoSetupController::IsOobeDemoSetupFlowInProgress();
}

std::string Country() {
  DCHECK(AreDemoDimensionsAccessible());
  // TODO(b/328305607): Remove this conversion part once all
  // prefs::kDemoModeCountry are converted.
  const std::string country =
      g_browser_process->local_state()->GetString(prefs::kDemoModeCountry);
  std::string country_uppercase = base::ToUpperASCII(country);
  // if country is in lowercase, convert it to uppercase.
  if (country_uppercase != country) {
    g_browser_process->local_state()->SetString(prefs::kDemoModeCountry,
                                                country_uppercase);
  }
  return g_browser_process->local_state()->GetString(prefs::kDemoModeCountry);
}

std::string RetailerName() {
  DCHECK(AreDemoDimensionsAccessible());
  return g_browser_process->local_state()->GetString(
      prefs::kDemoModeRetailerId);
}

std::string StoreNumber() {
  DCHECK(AreDemoDimensionsAccessible());
  return g_browser_process->local_state()->GetString(prefs::kDemoModeStoreId);
}

bool IsCloudGamingDevice() {
  DCHECK(AreDemoDimensionsAccessible());
  return chromeos::features::IsCloudGamingDeviceEnabled();
}

bool IsFeatureAwareDevice() {
  DCHECK(AreDemoDimensionsAccessible());
  return ash::features::IsFeatureAwareDeviceDemoModeEnabled();
}

base::Version AppVersion() {
  DCHECK(AreDemoDimensionsAccessible());
  return base::Version(
      g_browser_process->local_state()->GetString(prefs::kDemoModeAppVersion));
}

base::Version ResourcesVersion() {
  DCHECK(AreDemoDimensionsAccessible());
  return base::Version(g_browser_process->local_state()->GetString(
      prefs::kDemoModeResourcesVersion));
}

std::string GetChromeOSVersionString() {
  DCHECK(AreDemoDimensionsAccessible());
  // 1. Get Chrome Browser Milestone from version_info. We use the version from
  // the browser since some dev devices may have a locally built Chromium
  // deployed.
  std::string chrome_version = version_info::GetMajorVersionNumber();

  // 2. Get Platform Version.
  std::string platform_version = base::SysInfo::OperatingSystemVersion();
  // Fall back to 0.0.0 if platform version is not available.
  if (platform_version.empty()) {
    LOG(WARNING) << "Could not obtain the Platform Version info.";
    platform_version = "0.0.0";
  }

  // 3. Get Channel from LSB CHROMEOS_RELEASE_TRACK.
  std::string track;
  base::SysInfo::GetLsbReleaseValue("CHROMEOS_RELEASE_TRACK", &track);
  // Fall back to "unknown-channel" if the channel info is not available.
  if (track.empty()) {
    LOG(WARNING) << "Could not obtain the Channel info.";
    track = "unknown-channel";
  }

  // 4. Combine them.
  return base::StringPrintf("R%s-%s_%s", chrome_version, platform_version,
                            track);
}

std::string Board() {
  DCHECK(AreDemoDimensionsAccessible());
  return base::SysInfo::GetLsbReleaseBoard();
}

std::string_view Model() {
  DCHECK(AreDemoDimensionsAccessible());
  // kCustomizationIdKey stores the model name of the device.
  const std::optional<std::string_view> model =
      system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          system::kCustomizationIdKey);

  return model.value_or("");
}

std::string Locale() {
  DCHECK(AreDemoDimensionsAccessible());
  return g_browser_process->local_state()->GetString(
      language::prefs::kApplicationLocale);
}

enterprise_management::DemoModeDimensions GetDemoModeDimensions() {
  enterprise_management::DemoModeDimensions dimensions;
  dimensions.set_country(Country());
  dimensions.set_retailer_name(RetailerName());
  dimensions.set_store_number(StoreNumber());
  if (IsCloudGamingDevice()) {
    dimensions.add_customization_facets(
        enterprise_management::DemoModeDimensions::CLOUD_GAMING_DEVICE);
  }
  if (IsFeatureAwareDevice()) {
    dimensions.add_customization_facets(
        enterprise_management::DemoModeDimensions::FEATURE_AWARE_DEVICE);
  }
  return dimensions;
}

}  // namespace demo_mode
}  // namespace ash
