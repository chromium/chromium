// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/geolocation/system_geolocation_source.h"
#include "components/prefs/pref_service.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"

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

  GeolocationAccessLevel GetUserPref() {
    return static_cast<GeolocationAccessLevel>(
        Shell::Get()->session_controller()->GetActivePrefService()->GetInteger(
            prefs::kUserGeolocationAccessLevel));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class SystemGeolocationSourceTestsGeolocationOn
    : public SystemGeolocationSourceTests {
 protected:
  SystemGeolocationSourceTestsGeolocationOn() {
    scoped_feature_list_.InitWithFeatures({features::kCrosPrivacyHub}, {});
  }
};

class SystemGeolocationSourceTestsGeolocationOff
    : public SystemGeolocationSourceTests {
 protected:
  SystemGeolocationSourceTestsGeolocationOff() {
    // Disables the geolocation part of the PrivacyHub
    scoped_feature_list_.InitWithFeatures({}, {ash::features::kCrosPrivacyHub});
  }
};

TEST_F(SystemGeolocationSourceTestsGeolocationOn, PrefChange) {
  EXPECT_TRUE(ash::features::IsCrosPrivacyHubLocationEnabled());

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

TEST_F(SystemGeolocationSourceTestsGeolocationOff, DisabledInV0) {
  EXPECT_FALSE(ash::features::IsCrosPrivacyHubLocationEnabled());

  SystemGeolocationSource source;
  base::test::TestFuture<device::LocationSystemPermissionStatus> status;
  source.RegisterPermissionUpdateCallback(base::BindLambdaForTesting(
      [&status](device::LocationSystemPermissionStatus value) {
        // This drops the extra (duplicated) status changes.
        if (!status.IsReady()) {
          status.GetRepeatingCallback().Run(value);
        } else {
          // This is a duplicated update, let's check that the status is not
          // changed (i.e. it is really duplication, not a real change).
          EXPECT_EQ(status.Get(), value);
        }
      }));
  // Initial value should be allowed and the callback should be called with this
  // value
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed, status.Take());
  EXPECT_EQ(GeolocationAccessLevel::kAllowed, GetUserPref());

  // Change user settings to "Blocked for all" and check that the sent value is
  // still `kAllowed`.
  SetUserPref(GeolocationAccessLevel::kDisallowed);
  EXPECT_EQ(GeolocationAccessLevel::kAllowed, GetUserPref());

  // Change user settings "Only allowed for system services" and check that the
  // sent value is still `kAllowed`.
  SetUserPref(GeolocationAccessLevel::kOnlyAllowedForSystem);
  EXPECT_EQ(GeolocationAccessLevel::kAllowed, GetUserPref());

  // Change user settings to "Allowed" and check that the sent value is still
  // `kAllowed`.
  SetUserPref(GeolocationAccessLevel::kAllowed);
  EXPECT_EQ(GeolocationAccessLevel::kAllowed, GetUserPref());
}

}  // namespace ash
