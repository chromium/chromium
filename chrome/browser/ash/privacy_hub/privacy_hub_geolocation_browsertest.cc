// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class PrivacyHubGeolocationBrowsertestBase : public LoginManagerTest {
 public:
  PrivacyHubGeolocationBrowsertestBase() {
    scoped_feature_list_.InitWithFeatures({ash::features::kCrosPrivacyHub}, {});
  }

  ~PrivacyHubGeolocationBrowsertestBase() override = default;

  // Set the `kUserGeolocationAccessLevel` pref for the active user.
  void SetGeolocationAccessLevelPref(GeolocationAccessLevel access_level) {
    g_browser_process->profile_manager()
        ->GetActiveUserProfile()
        ->GetPrefs()
        ->SetInteger(prefs::kUserGeolocationAccessLevel,
                     static_cast<int>(access_level));
  }

 protected:
  LoginManagerMixin login_manager_{&mixin_host_};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};
  base::test::ScopedFeatureList scoped_feature_list_;
};

class PrivacyHubGeolocationBrowsertestMultiUserSession
    : public PrivacyHubGeolocationBrowsertestBase,
      public testing::WithParamInterface<
          std::tuple<GeolocationAccessLevel, GeolocationAccessLevel>> {
 public:
  PrivacyHubGeolocationBrowsertestMultiUserSession() {
    login_manager_.AppendRegularUsers(3);
    regular_primary_user_ = login_manager_.users()[0].account_id;
    regular_secondary_user_1_ = login_manager_.users()[1].account_id;
    regular_secondary_user_2_ = login_manager_.users()[2].account_id;
  }
  ~PrivacyHubGeolocationBrowsertestMultiUserSession() = default;

 protected:
  AccountId regular_primary_user_;
  AccountId regular_secondary_user_1_;
  AccountId regular_secondary_user_2_;
};

IN_PROC_BROWSER_TEST_P(PrivacyHubGeolocationBrowsertestMultiUserSession,
                       SecondUserCanNotChangeGeolocationSetting) {
  SimpleGeolocationProvider* provider =
      SimpleGeolocationProvider::GetInstance();
  CHECK(provider);

  // Log in primary user.
  LoginUser(regular_primary_user_);
  ASSERT_EQ(provider->GetGeolocationAccessLevel(),
            GeolocationAccessLevel::kAllowed);

  const GeolocationAccessLevel primary_user_geolocation_choice =
      std::get<0>(GetParam());
  SetGeolocationAccessLevelPref(primary_user_geolocation_choice);
  ASSERT_EQ(provider->GetGeolocationAccessLevel(),
            primary_user_geolocation_choice);

  // Add secondary user and log in.
  ash::UserAddingScreen::Get()->Start();
  AddUser(regular_secondary_user_1_);
  const GeolocationAccessLevel secondary_user_geolocation_choice =
      std::get<1>(GetParam());

  // Check that primary user's choice for Geolocation setting is
  // overriding secondary user's choice.
  ASSERT_EQ(provider->GetGeolocationAccessLevel(),
            primary_user_geolocation_choice);

  // Modify the underlying preference for the secondary user. Check that the
  // effective geolocation setting is unaffected; still following the primary
  // user's choice.
  SetGeolocationAccessLevelPref(secondary_user_geolocation_choice);
  ASSERT_EQ(provider->GetGeolocationAccessLevel(),
            primary_user_geolocation_choice);
}

IN_PROC_BROWSER_TEST_P(PrivacyHubGeolocationBrowsertestMultiUserSession,
                       SecondaryUsersCanNotChangeGeolocationSetting) {
  SimpleGeolocationProvider* provider =
      SimpleGeolocationProvider::GetInstance();
  CHECK(provider);

  // Log in primary user.
  LoginUser(regular_primary_user_);
  ASSERT_EQ(provider->GetGeolocationAccessLevel(),
            GeolocationAccessLevel::kAllowed);

  const GeolocationAccessLevel primary_user_geolocation_choice =
      std::get<0>(GetParam());
  SetGeolocationAccessLevelPref(primary_user_geolocation_choice);
  ASSERT_EQ(provider->GetGeolocationAccessLevel(),
            primary_user_geolocation_choice);

  // Add secondary user and log in.
  ash::UserAddingScreen::Get()->Start();
  AddUser(regular_secondary_user_1_);
  const GeolocationAccessLevel secondary_user_geolocation_choice =
      std::get<1>(GetParam());
  // Check that primary user's choice for Geolocation setting is
  // overriding secondary user's choice.
  ASSERT_EQ(provider->GetGeolocationAccessLevel(),
            primary_user_geolocation_choice);

  // Modify the underlying preference for the secondary user. Check that the
  // effective geolocation setting is unaffected; still following the primary
  // user's choice.
  SetGeolocationAccessLevelPref(secondary_user_geolocation_choice);
  ASSERT_EQ(provider->GetGeolocationAccessLevel(),
            primary_user_geolocation_choice);

  // Add another secondary user and conduct the same testing.
  ash::UserAddingScreen::Get()->Start();
  AddUser(regular_secondary_user_2_);

  // Check initial location access level follows the primary user choice.
  ASSERT_EQ(provider->GetGeolocationAccessLevel(),
            primary_user_geolocation_choice);

  // Change the underlying preference for this user too. Check that the
  // effective geolocation setting is unaffected.
  SetGeolocationAccessLevelPref(secondary_user_geolocation_choice);
  ASSERT_EQ(provider->GetGeolocationAccessLevel(),
            primary_user_geolocation_choice);
}

// std::get<0>(GetParam()) - Location preference of the primary user.
// std::get<1>(GetParam()) - Location preference of the secondary user[s].
// Values of these pairs are meant to be different to test that secondary
// users' preference won't affect the effective geolocation state.
INSTANTIATE_TEST_SUITE_P(
    All,
    PrivacyHubGeolocationBrowsertestMultiUserSession,
    testing::Values(
        std::make_tuple(GeolocationAccessLevel::kDisallowed,
                        GeolocationAccessLevel::kAllowed),
        std::make_tuple(GeolocationAccessLevel::kAllowed,
                        GeolocationAccessLevel::kDisallowed),
        std::make_tuple(GeolocationAccessLevel::kOnlyAllowedForSystem,
                        GeolocationAccessLevel::kDisallowed)));

class MockSettingsWindowManager : public chrome::SettingsWindowManager {
 public:
  MOCK_METHOD(void,
              ShowChromePageForProfile,
              (Profile * profile,
               const GURL& gurl,
               int64_t display_id,
               apps::LaunchCallback callback),
              (override));
};

class PrivacyHubGeolocationBrowsertestCheckSystemSettingsLink
    : public PrivacyHubGeolocationBrowsertestBase {
 public:
  PrivacyHubGeolocationBrowsertestCheckSystemSettingsLink() {
    login_manager_.AppendRegularUsers(2);
    primary_user_ = login_manager_.users()[0].account_id;
    secondary_user_ = login_manager_.users()[1].account_id;
  }
  ~PrivacyHubGeolocationBrowsertestCheckSystemSettingsLink() override = default;

 protected:
  AccountId primary_user_;
  AccountId secondary_user_;
};

IN_PROC_BROWSER_TEST_F(PrivacyHubGeolocationBrowsertestCheckSystemSettingsLink,
                       AlwaysOpenActiveUserSettingsPage) {
  MockSettingsWindowManager mock_settings_window_manager;
  chrome::SettingsWindowManager::SetInstanceForTesting(
      &mock_settings_window_manager);

  // Sign in with the first/primary user.
  LoginUser(primary_user_);
  Profile* primary_profile = ProfileManager::GetActiveUserProfile();
  // When primary user clicks the redirection link from the Browser, the opened
  // OS settings page has to be tied to the primary user's profile.
  EXPECT_CALL(
      mock_settings_window_manager,
      ShowChromePageForProfile(
          primary_profile,
          chrome::GetOSSettingsUrl(
              chromeos::settings::mojom::kPrivacyHubGeolocationSubpagePath),
          testing::_, testing::_));
  // Directly call the underlying method to simulate the link click.
  privacy_hub_util::OpenSystemSettings(
      privacy_hub_util::ContentType::GEOLOCATION);

  // Add another/secondary user to the session and log in.
  ash::UserAddingScreen::Get()->Start();
  AddUser(secondary_user_);
  // Check that a different profile is being loaded.
  Profile* secondary_profile = ProfileManager::GetActiveUserProfile();
  ASSERT_NE(primary_profile, secondary_profile);
  // When secondary user clicks the redirection link from the Browser, the
  // opened OS settings page has to be tied to the secondary user's profile.
  EXPECT_CALL(
      mock_settings_window_manager,
      ShowChromePageForProfile(
          secondary_profile,
          chrome::GetOSSettingsUrl(
              chromeos::settings::mojom::kPrivacyHubGeolocationSubpagePath),
          display::kInvalidDisplayId, testing::_));
  // Directly call the underlying method to simulate the link click.
  privacy_hub_util::OpenSystemSettings(
      privacy_hub_util::ContentType::GEOLOCATION);
}

}  // namespace ash
