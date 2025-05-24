// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/geolocation/system_geolocation_source.h"

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
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"
#include "testing/gmock/include/gmock/gmock.h"

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

class MockObserver
    : public device::GeolocationSystemPermissionManager::PermissionObserver {
 public:
  MOCK_METHOD(void,
              OnSystemPermissionUpdated,
              (device::LocationSystemPermissionStatus status),
              (override));
};

}  // namespace

namespace ash {

class SystemGeolocationSourceTestBase {
 protected:
  void SetActiveUserPref(GeolocationAccessLevel access_level) {
    Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
        prefs::kUserGeolocationAccessLevel, static_cast<int>(access_level));
  }

  void SetPrimaryUserPref(GeolocationAccessLevel access_level) {
    Shell::Get()->session_controller()->GetPrimaryUserPrefService()->SetInteger(
        prefs::kUserGeolocationAccessLevel, static_cast<int>(access_level));
  }

  GeolocationAccessLevel GetActiveUserPref() {
    return static_cast<GeolocationAccessLevel>(
        Shell::Get()->session_controller()->GetActivePrefService()->GetInteger(
            prefs::kUserGeolocationAccessLevel));
  }
};

class SystemGeolocationSourceTestsGeolocationOn
    : public SystemGeolocationSourceTestBase,
      public InProcessBrowserTest {
 protected:
  SystemGeolocationSourceTestsGeolocationOn() {
    scoped_feature_list_.InitWithFeatures({features::kCrosPrivacyHub}, {});
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

class SystemGeolocationSourceTestsGeolocationOff
    : public SystemGeolocationSourceTestBase,
      public InProcessBrowserTest {
 protected:
  SystemGeolocationSourceTestsGeolocationOff() {
    // Disables the geolocation part of the PrivacyHub
    scoped_feature_list_.InitWithFeatures({}, {ash::features::kCrosPrivacyHub});
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

class SystemGeolocationSourceTestMultiUser
    : public SystemGeolocationSourceTestBase,
      public LoginManagerTest {
 public:
  SystemGeolocationSourceTestMultiUser() {
    scoped_feature_list_.InitWithFeatures({features::kCrosPrivacyHub}, {});
    login_mixin_.AppendRegularUsers(2);
    primary_user_ = login_mixin_.users()[0].account_id;
    secondary_user_ = login_mixin_.users()[1].account_id;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  LoginManagerMixin login_mixin_{&mixin_host_};
  AccountId primary_user_;
  AccountId secondary_user_;
};

IN_PROC_BROWSER_TEST_F(SystemGeolocationSourceTestsGeolocationOn,
                       ObservationInBrowser) {
  device::GeolocationSystemPermissionManager* manager =
      device::GeolocationSystemPermissionManager::GetInstance();
  ASSERT_TRUE(manager);

  Observer observer;
  manager->AddObserver(&observer);

  // Initial value should be to allow.
  EXPECT_EQ(GetActiveUserPref(), ash::GeolocationAccessLevel::kAllowed);
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            manager->GetSystemPermission());

  // Change the pref value
  SetActiveUserPref(ash::GeolocationAccessLevel::kDisallowed);
  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied,
            manager->GetSystemPermission());
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied,
            observer.status_.Take());

  // Change the pref value
  SetActiveUserPref(ash::GeolocationAccessLevel::kAllowed);
  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            manager->GetSystemPermission());
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            observer.status_.Take());

  // Change the pref value
  SetActiveUserPref(ash::GeolocationAccessLevel::kOnlyAllowedForSystem);
  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied,
            manager->GetSystemPermission());
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied,
            observer.status_.Take());

  // Observer needs to be removed here because it is allocated on stack.
  manager->RemoveObserver(&observer);
}

IN_PROC_BROWSER_TEST_F(SystemGeolocationSourceTestMultiUser,
                       SecondaryUserObservation) {
  // Sign in with the primary user.
  LoginUser(primary_user_);
  device::GeolocationSystemPermissionManager* manager =
      device::GeolocationSystemPermissionManager::GetInstance();
  ASSERT_TRUE(manager);

  Observer observer;
  manager->AddObserver(&observer);

  // Initial value should be to allow.
  EXPECT_EQ(GetActiveUserPref(), ash::GeolocationAccessLevel::kAllowed);
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            manager->GetSystemPermission());

  // Set pref to `kDisallowed`.
  SetActiveUserPref(ash::GeolocationAccessLevel::kDisallowed);
  // Check that the change in primary user pref affected the system location
  // state.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied,
            manager->GetSystemPermission());
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied,
            observer.status_.Take());

  // Add second user to the session and sign in.
  ash::UserAddingScreen::Get()->Start();
  AddUser(secondary_user_);
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied,
            manager->GetSystemPermission());

  // This observer is used to test that the secondary user pref changes below
  // will not trigger the system location updates.
  MockObserver NoOpObserver;
  manager->AddObserver(&NoOpObserver);
  // `OnSystemPermissionUpdated` hook shall not not be executed.
  EXPECT_CALL(NoOpObserver, OnSystemPermissionUpdated(testing::_)).Times(0);

  // Update secondary user pref for location pref and check it has no effect.
  // Location setting should still follow the the primary user preference.
  SetActiveUserPref(GeolocationAccessLevel::kAllowed);
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied,
            manager->GetSystemPermission());
  // Ditto.
  SetActiveUserPref(GeolocationAccessLevel::kOnlyAllowedForSystem);
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied,
            manager->GetSystemPermission());
  // Ditto.
  SetActiveUserPref(GeolocationAccessLevel::kDisallowed);
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied,
            manager->GetSystemPermission());

  // Observers need to be removed here because it is allocated on stack.
  manager->RemoveObserver(&NoOpObserver);
  manager->RemoveObserver(&observer);
}

IN_PROC_BROWSER_TEST_F(SystemGeolocationSourceTestMultiUser,
                       SecondaryUserObserversPrimaryPrefUpdate) {
  // Sign in with the primary user.
  LoginUser(primary_user_);
  device::GeolocationSystemPermissionManager* manager =
      device::GeolocationSystemPermissionManager::GetInstance();
  ASSERT_TRUE(manager);

  Observer observer;
  manager->AddObserver(&observer);

  // Initial value should be to allow.
  EXPECT_EQ(GetActiveUserPref(), ash::GeolocationAccessLevel::kAllowed);
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            manager->GetSystemPermission());

  // Add second user to the session and sign in.
  ash::UserAddingScreen::Get()->Start();
  AddUser(secondary_user_);
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            manager->GetSystemPermission());

  // Update primary user's location preference while secondary user is active
  // and check it affects the system permission.
  SetPrimaryUserPref(ash::GeolocationAccessLevel::kDisallowed);
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied,
            manager->GetSystemPermission());
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied,
            observer.status_.Take());

  SetPrimaryUserPref(ash::GeolocationAccessLevel::kAllowed);
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            manager->GetSystemPermission());
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            observer.status_.Take());

  SetPrimaryUserPref(ash::GeolocationAccessLevel::kOnlyAllowedForSystem);
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
  EXPECT_EQ(GetActiveUserPref(), ash::GeolocationAccessLevel::kAllowed);
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            manager->GetSystemPermission());

  // Change the pref value
  SetActiveUserPref(ash::GeolocationAccessLevel::kDisallowed);
  // Check that the permission is not changed.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            manager->GetSystemPermission());
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            observer.status_.Take());

  // Change the pref value
  SetActiveUserPref(ash::GeolocationAccessLevel::kAllowed);
  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            manager->GetSystemPermission());
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            observer.status_.Take());

  // Change the pref value
  SetActiveUserPref(ash::GeolocationAccessLevel::kOnlyAllowedForSystem);
  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            manager->GetSystemPermission());
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            observer.status_.Take());

  // Observer needs to be removed here because it is allocated on stack.
  manager->RemoveObserver(&observer);
}

}  // namespace ash
