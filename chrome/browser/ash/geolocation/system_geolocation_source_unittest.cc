// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"

#include <utility>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/geolocation/system_geolocation_source.h"
#include "components/prefs/pref_service.h"

namespace ash {

class SystemGeolocationSourceTests : public AshTestBase {
 protected:
  SystemGeolocationSourceTests()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kCrosPrivacyHub);
  }

  // AshTestBase:
  void SetUp() override { AshTestBase::SetUp(); }

  void SetUserPref(bool allowed) {
    Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
        prefs::kUserGeolocationAllowed, allowed);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SystemGeolocationSourceTests, PermissionUpdate) {
  SystemGeolocationSource source;
  base::test::RepeatingTestFuture<device::LocationSystemPermissionStatus>
      status;

  source.RegisterPermissionUpdateCallback(status.GetCallback());

  // Initial value should be to allow.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed, status.Take());

  // Change user settings to deny and check that the callback is called.
  SetUserPref(false);
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied, status.Take());

  // Change user settings back to allowedy and check that the callback is
  // called.
  SetUserPref(true);
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed, status.Take());
}

}  // namespace ash
