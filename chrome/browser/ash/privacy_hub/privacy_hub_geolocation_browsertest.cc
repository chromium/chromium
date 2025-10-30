// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/shell.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "base/notreached.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/ash/settings/pages/privacy/privacy_hub_handler.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/geolocation/system_location_provider.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

namespace {

bool IsLocationEnabledForBrowser(GeolocationAccessLevel access_level) {
  switch (access_level) {
    case GeolocationAccessLevel::kAllowed:
      return true;
    case GeolocationAccessLevel::kOnlyAllowedForSystem:
    case GeolocationAccessLevel::kDisallowed:
      return false;
    default:
      NOTREACHED() << "Invalid access level";
  }
}

}  // namespace

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
  SystemLocationProvider* provider = SystemLocationProvider::GetInstance();
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
  ASSERT_EQ(privacy_hub_util::ContentBlocked(
                privacy_hub_util::ContentType::GEOLOCATION),
            !IsLocationEnabledForBrowser(primary_user_geolocation_choice));

  // Modify the underlying preference for the secondary user. Check that the
  // effective geolocation setting is unaffected; still following the primary
  // user's choice.
  SetGeolocationAccessLevelPref(secondary_user_geolocation_choice);
  ASSERT_EQ(provider->GetGeolocationAccessLevel(),
            primary_user_geolocation_choice);
  ASSERT_EQ(privacy_hub_util::ContentBlocked(
                privacy_hub_util::ContentType::GEOLOCATION),
            !IsLocationEnabledForBrowser(primary_user_geolocation_choice));
}

IN_PROC_BROWSER_TEST_P(PrivacyHubGeolocationBrowsertestMultiUserSession,
                       SecondaryUsersCanNotChangeGeolocationSetting) {
  SystemLocationProvider* provider = SystemLocationProvider::GetInstance();
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
  ASSERT_EQ(privacy_hub_util::ContentBlocked(
                privacy_hub_util::ContentType::GEOLOCATION),
            !IsLocationEnabledForBrowser(primary_user_geolocation_choice));

  // Modify the underlying preference for the secondary user. Check that the
  // effective geolocation setting is unaffected; still following the primary
  // user's choice.
  SetGeolocationAccessLevelPref(secondary_user_geolocation_choice);
  ASSERT_EQ(provider->GetGeolocationAccessLevel(),
            primary_user_geolocation_choice);
  ASSERT_EQ(privacy_hub_util::ContentBlocked(
                privacy_hub_util::ContentType::GEOLOCATION),
            !IsLocationEnabledForBrowser(primary_user_geolocation_choice));

  // Add another secondary user and conduct the same testing.
  ash::UserAddingScreen::Get()->Start();
  AddUser(regular_secondary_user_2_);

  // Check initial location access level follows the primary user choice.
  ASSERT_EQ(provider->GetGeolocationAccessLevel(),
            primary_user_geolocation_choice);
  ASSERT_EQ(privacy_hub_util::ContentBlocked(
                privacy_hub_util::ContentType::GEOLOCATION),
            !IsLocationEnabledForBrowser(primary_user_geolocation_choice));

  // Change the underlying preference for this user too. Check that the
  // effective geolocation setting is unaffected.
  SetGeolocationAccessLevelPref(secondary_user_geolocation_choice);
  ASSERT_EQ(provider->GetGeolocationAccessLevel(),
            primary_user_geolocation_choice);
  ASSERT_EQ(privacy_hub_util::ContentBlocked(
                privacy_hub_util::ContentType::GEOLOCATION),
            !IsLocationEnabledForBrowser(primary_user_geolocation_choice));
}

class MockPrivacyHubHandler : public settings::PrivacyHubHandler {
 public:
  MOCK_METHOD(void,
              SystemGeolocationAccessLevelChanged,
              (GeolocationAccessLevel),
              (override));
};

IN_PROC_BROWSER_TEST_P(PrivacyHubGeolocationBrowsertestMultiUserSession,
                       CheckCorrectSystemUICallbackIsCalled) {
  SystemLocationProvider* provider = SystemLocationProvider::GetInstance();
  CHECK(provider);

  // Log in primary user.
  LoginUser(regular_primary_user_);
  ASSERT_EQ(provider->GetGeolocationAccessLevel(),
            GeolocationAccessLevel::kAllowed);

  // Simulate opening the chrome://os-settings, by artificially setting the mock
  // frontend. Check that this will automatically notify the UI of the initial
  // geolocation value.
  ::testing::StrictMock<MockPrivacyHubHandler> primary_user_frontend;
  EXPECT_CALL(primary_user_frontend, SystemGeolocationAccessLevelChanged(
                                         GeolocationAccessLevel::kAllowed));
  Shell::Get()->privacy_hub_controller()->geolocation_controller()->SetFrontend(
      &primary_user_frontend);

  // Modify the primary user location preference and check that the UI is
  // notified.
  const GeolocationAccessLevel primary_user_geolocation_choice =
      std::get<0>(GetParam());
  EXPECT_CALL(primary_user_frontend, SystemGeolocationAccessLevelChanged(
                                         primary_user_geolocation_choice));
  SetGeolocationAccessLevelPref(primary_user_geolocation_choice);

  // Add secondary user and log in.
  ash::UserAddingScreen::Get()->Start();
  AddUser(regular_secondary_user_1_);

  // Simulate opening the chrome://os-settings, by artificially setting the mock
  // frontend. Check that this will automatically notify the UI of the initial
  // geolocation value, but the value should be of the primary user pref.
  ::testing::StrictMock<MockPrivacyHubHandler> secondary_user_frontend;
  EXPECT_CALL(secondary_user_frontend, SystemGeolocationAccessLevelChanged(
                                           primary_user_geolocation_choice));
  Shell::Get()->privacy_hub_controller()->geolocation_controller()->SetFrontend(
      &secondary_user_frontend);

  // Modify the underlying location preference for the secondary user and check
  // that it won't trigger the UI callback.
  const GeolocationAccessLevel secondary_user_geolocation_choice =
      std::get<1>(GetParam());
  EXPECT_CALL(secondary_user_frontend,
              SystemGeolocationAccessLevelChanged(testing::_))
      .Times(0);
  SetGeolocationAccessLevelPref(secondary_user_geolocation_choice);

  // Switch back to primary user and check its UI will re-fetch system
  // geolocation.
  EXPECT_CALL(primary_user_frontend, SystemGeolocationAccessLevelChanged(
                                         primary_user_geolocation_choice));
  user_manager::UserManager::Get()->SwitchActiveUser(regular_primary_user_);
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
  // Sign in with the first/primary user.
  LoginUser(primary_user_);

  ash::SystemWebAppManager::Get(
      Profile::FromBrowserContext(
          ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
              primary_user_)))
      ->InstallSystemAppsForTesting();
  {
    content::CreateAndLoadWebContentsObserver app_loaded_observer;
    // Directly call the underlying method to simulate the link click.
    privacy_hub_util::OpenSystemSettings(
        privacy_hub_util::ContentType::GEOLOCATION);
    auto* web_contents = app_loaded_observer.Wait();
    ASSERT_TRUE(web_contents);
    EXPECT_EQ(
        web_contents->GetURL(),
        chrome::GetOSSettingsUrl(
            chromeos::settings::mojom::kPrivacyHubGeolocationSubpagePath));
    EXPECT_EQ(primary_user_,
              *ash::AnnotatedAccountId::Get(web_contents->GetBrowserContext()));
  }

  // Add another/secondary user to the session and log in.
  ash::UserAddingScreen::Get()->Start();
  AddUser(secondary_user_);
  EXPECT_EQ(secondary_user_,
            user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
  ash::SystemWebAppManager::Get(
      Profile::FromBrowserContext(
          ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
              secondary_user_)))
      ->InstallSystemAppsForTesting();

  {
    content::CreateAndLoadWebContentsObserver app_loaded_observer;
    // Directly call the underlying method to simulate the link click.
    privacy_hub_util::OpenSystemSettings(
        privacy_hub_util::ContentType::GEOLOCATION);
    auto* web_contents = app_loaded_observer.Wait();
    ASSERT_TRUE(web_contents);
    EXPECT_EQ(
        web_contents->GetURL(),
        chrome::GetOSSettingsUrl(
            chromeos::settings::mojom::kPrivacyHubGeolocationSubpagePath));
    EXPECT_EQ(secondary_user_,
              *ash::AnnotatedAccountId::Get(web_contents->GetBrowserContext()));
  }
}

}  // namespace ash
