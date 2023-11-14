// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"

#include <utility>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/geolocation/system_geolocation_source.h"
#include "components/prefs/pref_service.h"

namespace ash {

class SystemGeolocationSourceTests : public AshTestBase {
 protected:
  SystemGeolocationSourceTests()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  // AshTestBase:
  void SetUp() override { AshTestBase::SetUp(); }

  void SetUserPref(GeolocationAccessLevel access_level) {
    Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
        prefs::kUserGeolocationAccessLevel, static_cast<int>(access_level));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SystemGeolocationSourceTests, PermissionUpdate) {
  scoped_feature_list_.InitWithFeatures(
      {features::kCrosPrivacyHubV0, features::kCrosPrivacyHub}, {});

  SystemGeolocationSource source;
  base::test::TestFuture<device::LocationSystemPermissionStatus> status;

  source.RegisterPermissionUpdateCallback(status.GetRepeatingCallback());

  // Initial value should be to allow.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed, status.Take());

  // Change user settings to "Blocked for all" and check that the callback is
  // called.
  SetUserPref(GeolocationAccessLevel::kDisallowed);
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied, status.Take());

  // Change user settings to "Only allowed for system services" and check that
  // callback is called.
  SetUserPref(GeolocationAccessLevel::kOnlyAllowedForSystem);
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied, status.Take());

  // Change user settings to "Allowed" and check that the callback is called.
  SetUserPref(GeolocationAccessLevel::kAllowed);
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed, status.Take());
}

TEST_F(SystemGeolocationSourceTests, DisabledInV0) {
  // Disables the geolocation part of the PrivacyHub
  scoped_feature_list_.InitWithFeatures({ash::features::kCrosPrivacyHubV0},
                                        {ash::features::kCrosPrivacyHub});

  SystemGeolocationSource source;
  base::test::TestFuture<device::LocationSystemPermissionStatus> status;

  source.RegisterPermissionUpdateCallback(status.GetRepeatingCallback());

  // The value should always be kAllowed.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed, status.Take());

  // Change user settings to "Blocked for all" and check that the sent value is
  // still `kAllowed`.
  SetUserPref(GeolocationAccessLevel::kDisallowed);
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed, status.Take());

  // Change user settings "Only allowed for system services" and check that the
  // sent value is still `kAllowed`.
  SetUserPref(GeolocationAccessLevel::kOnlyAllowedForSystem);
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed, status.Take());

  // Change user settings to "Allowed" and check that the sent value is still
  // `kAllowed`.
  SetUserPref(GeolocationAccessLevel::kAllowed);
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed, status.Take());
}

}  // namespace ash
