// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_mode_dimensions.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/version.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace demo_mode {

bool AreDemoDimensionsAccessible() {
  return DemoSession::IsDeviceInDemoMode() ||
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
