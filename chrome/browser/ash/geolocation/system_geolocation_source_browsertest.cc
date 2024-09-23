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
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"

namespace {
class Observer
    : public device::GeolocationSystemPermissionManager::PermissionObserver {
 public:
  // device::GeolocationSystemPermissionManager::PermissionObserver:
  void OnSystemPermissionUpdated(
      device::LocationSystemPermissionStatus status) override {
    status_.GetRepeatingCallback().Run(std::move(status));
  }
  base::test::TestFuture<device::LocationSystemPermissionStatus> status_;
};
}  // namespace

namespace ash {

class SystemGeolocationSourceBrowserTests : public InProcessBrowserTest {
 protected:
  SystemGeolocationSourceBrowserTests() : InProcessBrowserTest() {}

  // InProcessBrowserTest:
  void SetUp() override { InProcessBrowserTest::SetUp(); }

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
    : public SystemGeolocationSourceBrowserTests {
 protected:
  SystemGeolocationSourceTestsGeolocationOn() {
    scoped_feature_list_.InitWithFeatures({features::kCrosPrivacyHub}, {});
  }
};

class SystemGeolocationSourceTestsGeolocationOff
    : public SystemGeolocationSourceBrowserTests {
 protected:
  SystemGeolocationSourceTestsGeolocationOff() {
    // Disables the geolocation part of the PrivacyHub
    scoped_feature_list_.InitWithFeatures({}, {ash::features::kCrosPrivacyHub});
  }
};

IN_PROC_BROWSER_TEST_F(SystemGeolocationSourceTestsGeolocationOn,
                       ObservationInBrowser) {
  device::GeolocationSystemPermissionManager* manager =
      device::GeolocationSystemPermissionManager::GetInstance();
  ASSERT_TRUE(manager);

  Observer observer;
  manager->AddObserver(&observer);

  // Initial value should be to allow.
  EXPECT_EQ(GetUserPref(), ash::GeolocationAccessLevel::kAllowed);
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            manager->GetSystemPermission());

  // Change the pref value
  SetUserPref(ash::GeolocationAccessLevel::kDisallowed);
  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied,
            manager->GetSystemPermission());
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied,
            observer.status_.Take());

  // Change the pref value
  SetUserPref(ash::GeolocationAccessLevel::kAllowed);
  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            manager->GetSystemPermission());
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            observer.status_.Take());

  // Change the pref value
  SetUserPref(ash::GeolocationAccessLevel::kOnlyAllowedForSystem);
  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied,
            manager->GetSystemPermission());
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied,
            observer.status_.Take());

  // Observer needs to be removed here because it is allocated on stack.
  manager->RemoveObserver(&observer);
}

IN_PROC_BROWSER_TEST_F(SystemGeolocationSourceTestsGeolocationOff,
                       ObservationInBrowser) {
  device::GeolocationSystemPermissionManager* manager =
      device::GeolocationSystemPermissionManager::GetInstance();
  ASSERT_TRUE(manager);

  Observer observer;
  manager->AddObserver(&observer);

  // Initial value should be to allow.
  EXPECT_EQ(GetUserPref(), ash::GeolocationAccessLevel::kAllowed);
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            manager->GetSystemPermission());

  // Change the pref value
  SetUserPref(ash::GeolocationAccessLevel::kDisallowed);
  // Check that the permission is not changed.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            manager->GetSystemPermission());
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            observer.status_.Take());

  // Change the pref value
  SetUserPref(ash::GeolocationAccessLevel::kAllowed);
  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            manager->GetSystemPermission());
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            observer.status_.Take());

  // Change the pref value
  SetUserPref(ash::GeolocationAccessLevel::kOnlyAllowedForSystem);
  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            manager->GetSystemPermission());
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            observer.status_.Take());

  // Observer needs to be removed here because it is allocated on stack.
  manager->RemoveObserver(&observer);
}

}  // namespace ash
