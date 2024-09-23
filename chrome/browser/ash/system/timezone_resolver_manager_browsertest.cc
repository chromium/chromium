// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system/timezone_resolver_manager.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace em = enterprise_management;

namespace {

static constexpr std::array<
    em::SystemTimezoneProto::AutomaticTimezoneDetectionType,
    5>
    kTimeZoneAutomaticDetectionCloudPolicies = {
        em::SystemTimezoneProto::AutomaticTimezoneDetectionType::
            SystemTimezoneProto_AutomaticTimezoneDetectionType_USERS_DECIDE,
        em::SystemTimezoneProto::AutomaticTimezoneDetectionType::
            SystemTimezoneProto_AutomaticTimezoneDetectionType_DISABLED,
        em::SystemTimezoneProto::AutomaticTimezoneDetectionType::
            SystemTimezoneProto_AutomaticTimezoneDetectionType_IP_ONLY,
        em::SystemTimezoneProto::AutomaticTimezoneDetectionType::
            SystemTimezoneProto_AutomaticTimezoneDetectionType_SEND_WIFI_ACCESS_POINTS,
        em::SystemTimezoneProto::AutomaticTimezoneDetectionType::
            SystemTimezoneProto_AutomaticTimezoneDetectionType_SEND_ALL_LOCATION_INFO};

constexpr std::array<system::TimeZoneResolverManager::TimeZoneResolveMethod, 4>
    kUserTimeZoneAutomaticDetectionUserChoices = {
        system::TimeZoneResolverManager::TimeZoneResolveMethod::DISABLED,
        system::TimeZoneResolverManager::TimeZoneResolveMethod::IP_ONLY,
        system::TimeZoneResolverManager::TimeZoneResolveMethod::
            SEND_WIFI_ACCESS_POINTS,
        system::TimeZoneResolverManager::TimeZoneResolveMethod::
            SEND_ALL_LOCATION_INFO,
};

}  // namespace

class TimeZoneResolverManagerTestBase : public LoginManagerTest {
 public:
  TimeZoneResolverManagerTestBase() {
    login_manager_.AppendManagedUsers(1);
    login_manager_.AppendRegularUsers(2);

    managed_user_id_ = login_manager_.users()[0].account_id;
    regular_primary_user_id_ = login_manager_.users()[1].account_id;
    regular_secondary_user_id_ = login_manager_.users()[2].account_id;
  }

  ~TimeZoneResolverManagerTestBase() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // This is needed so that tests don't need to mock our policy
    // infrastructure. When settings this, profile initialization will not fail,
    // because of policies.
    command_line->AppendSwitch(switches::kAllowFailedPolicyFetchForTest);
    LoginManagerTest::SetUpCommandLine(command_line);
  }

  void SetUserTimeZoneResolveMethod(
      PrefService* pref_service,
      system::TimeZoneResolverManager::TimeZoneResolveMethod method) {
    // The way the automatic time zone is implemented today is that we have 2
    // user preferences. kResolveTimezoneByGeolocationMigratedToMethod - marked
    // to true, when user first interacts with the automatic time zone setting.
    // kResolveTimezoneByGeolocationMethod - storing the user choice, whether
    // they want to use coarse (IP-based) or precise (Wi-Fi and Cellular-based)
    // location.
    if (!pref_service->GetBoolean(
            ::prefs::kResolveTimezoneByGeolocationMigratedToMethod)) {
      pref_service->SetBoolean(
          ::prefs::kResolveTimezoneByGeolocationMigratedToMethod, true);
    }

    pref_service->SetInteger(::prefs::kResolveTimezoneByGeolocationMethod,
                             static_cast<int>(method));
  }

  bool IsStaticTimezoneSelected(PrefService* pref_service) {
    return !pref_service->GetBoolean(
               ::prefs::kResolveTimezoneByGeolocationMigratedToMethod) &&
           static_cast<system::TimeZoneResolverManager::TimeZoneResolveMethod>(
               pref_service->GetInteger(
                   ::prefs::kResolveTimezoneByGeolocationMethod)) ==
               system::TimeZoneResolverManager::TimeZoneResolveMethod::DISABLED;
  }

  void UpdateUserGeolocationPermission(PrefService* pref_service,
                                       GeolocationAccessLevel access_level) {
    pref_service->SetInteger(ash::prefs::kUserGeolocationAccessLevel,
                             static_cast<int>(access_level));
  }

  void UpdateDeviceGeolocationPermission(bool enabled) {
    PrefService* local_state = g_browser_process->local_state();

    GeolocationAccessLevel access_level;
    if (enabled) {
      access_level = GeolocationAccessLevel::kAllowed;
    } else {
      access_level = GeolocationAccessLevel::kDisallowed;
    }

    local_state->SetInteger(ash::prefs::kDeviceGeolocationAllowed,
                            static_cast<int>(access_level));
  }

 protected:
  AccountId managed_user_id_;
  AccountId regular_primary_user_id_;
  AccountId regular_secondary_user_id_;

  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  LoginManagerMixin login_manager_{&mixin_host_};

  ScopedTestingCrosSettings cros_settings_;
  policy::DevicePolicyBuilder device_policy_builder_;
};

class TimeZoneResolverManagerEnrolledDeviceTest
    : public TimeZoneResolverManagerTestBase {
 public:
  // Set the cloud policy for automatic time zone detection and wait until it's
  // propagated to the local state.
  void SetDeviceTimeZoneAutomaticDetectionPolicy(
      em::SystemTimezoneProto::AutomaticTimezoneDetectionType detection_type) {
    // Override the cloud policy for automatic time zone detection to the given
    // value.
    device_policy_builder_.SetDefaultSigningKey();
    device_policy_builder_.payload()
        .mutable_system_timezone()
        ->set_timezone_detection_type(detection_type);
    device_policy_builder_.Build();
    policy_test_server_mixin_.UpdateDevicePolicy(
        device_policy_builder_.payload());

    // Simulating a policy fetch.
    ash::FakeSessionManagerClient::Get()->set_device_policy(
        device_policy_builder_.GetBlob());
    ash::FakeSessionManagerClient::Get()->OnPropertyChangeComplete(true);

    // Wait for the policy value to get propagated.
    policy::LocalStateValueWaiter(
        ::prefs::kSystemTimezoneAutomaticDetectionPolicy,
        base::Value(detection_type))
        .Wait();
  }

  // Static policy overrides automatic detection policy
  void SetDeviceTimeZoneStaticPolicy(const std::string& value) {
    cros_settings_.device_settings()->SetString(kSystemTimezonePolicy, value);
  }

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

class TimeZoneResolverManagerUnenrolledDeviceTest
    : public TimeZoneResolverManagerTestBase {
 protected:
  TimeZoneResolverManagerUnenrolledDeviceTest() {
    // Enabling Privacy Hub with location switch
    scoped_feature_list_.InitWithFeatures({ash::features::kCrosPrivacyHub}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};
};

IN_PROC_BROWSER_TEST_F(TimeZoneResolverManagerUnenrolledDeviceTest,
                       RegularUser) {
  ash::system::TimeZoneResolverManager* tz_resolver_manager =
      g_browser_process->platform_part()->GetTimezoneResolverManager();

  // Log in a user.
  LoginUser(regular_primary_user_id_);
  base::RunLoop().RunUntilIdle();
  PrefService* pref_service =
      g_browser_process->profile_manager()->GetActiveUserProfile()->GetPrefs();

  // Check that all user options for automatic timezone is working correctly:
  SetUserTimeZoneResolveMethod(
      pref_service,
      system::TimeZoneResolverManager::TimeZoneResolveMethod::DISABLED);
  EXPECT_FALSE(tz_resolver_manager->ShouldSendWiFiGeolocationData());
  EXPECT_FALSE(tz_resolver_manager->ShouldSendCellularGeolocationData());

  SetUserTimeZoneResolveMethod(
      pref_service,
      system::TimeZoneResolverManager::TimeZoneResolveMethod::IP_ONLY);
  EXPECT_FALSE(tz_resolver_manager->ShouldSendWiFiGeolocationData());
  EXPECT_FALSE(tz_resolver_manager->ShouldSendCellularGeolocationData());

  SetUserTimeZoneResolveMethod(
      pref_service, system::TimeZoneResolverManager::TimeZoneResolveMethod::
                        SEND_WIFI_ACCESS_POINTS);
  EXPECT_TRUE(tz_resolver_manager->ShouldSendWiFiGeolocationData());
  EXPECT_FALSE(tz_resolver_manager->ShouldSendCellularGeolocationData());

  SetUserTimeZoneResolveMethod(
      pref_service, system::TimeZoneResolverManager::TimeZoneResolveMethod::
                        SEND_ALL_LOCATION_INFO);
  EXPECT_TRUE(tz_resolver_manager->ShouldSendWiFiGeolocationData());
  EXPECT_TRUE(tz_resolver_manager->ShouldSendCellularGeolocationData());
}

IN_PROC_BROWSER_TEST_F(TimeZoneResolverManagerUnenrolledDeviceTest,
                       RegularMultiUser) {
  ash::system::TimeZoneResolverManager* tz_resolver_manager =
      g_browser_process->platform_part()->GetTimezoneResolverManager();

  // Log in primary user.
  LoginUser(regular_primary_user_id_);
  base::RunLoop().RunUntilIdle();
  PrefService* primary_pref_service =
      g_browser_process->profile_manager()->GetActiveUserProfile()->GetPrefs();

  // Add secondary user and log in.
  ash::UserAddingScreen::Get()->Start();
  AddUser(regular_secondary_user_id_);
  base::RunLoop().RunUntilIdle();

  // Check that primary user's choice for time zone detection method is
  // overriding secondary user's choice. Checking all configurations:
  PrefService* secondary_pref_service =
      g_browser_process->profile_manager()->GetActiveUserProfile()->GetPrefs();
  SetUserTimeZoneResolveMethod(
      primary_pref_service,
      system::TimeZoneResolverManager::TimeZoneResolveMethod::DISABLED);
  for (auto tz_choice : kUserTimeZoneAutomaticDetectionUserChoices) {
    SetUserTimeZoneResolveMethod(secondary_pref_service, tz_choice);
    EXPECT_FALSE(tz_resolver_manager->ShouldSendWiFiGeolocationData());
    EXPECT_FALSE(tz_resolver_manager->ShouldSendCellularGeolocationData());
  }

  SetUserTimeZoneResolveMethod(
      primary_pref_service,
      system::TimeZoneResolverManager::TimeZoneResolveMethod::IP_ONLY);
  for (auto tz_choice : kUserTimeZoneAutomaticDetectionUserChoices) {
    SetUserTimeZoneResolveMethod(secondary_pref_service, tz_choice);
    EXPECT_FALSE(tz_resolver_manager->ShouldSendWiFiGeolocationData());
    EXPECT_FALSE(tz_resolver_manager->ShouldSendCellularGeolocationData());
  }

  SetUserTimeZoneResolveMethod(
      primary_pref_service, system::TimeZoneResolverManager::
                                TimeZoneResolveMethod::SEND_WIFI_ACCESS_POINTS);
  for (auto tz_choice : kUserTimeZoneAutomaticDetectionUserChoices) {
    SetUserTimeZoneResolveMethod(secondary_pref_service, tz_choice);
    EXPECT_TRUE(tz_resolver_manager->ShouldSendWiFiGeolocationData());
    EXPECT_FALSE(tz_resolver_manager->ShouldSendCellularGeolocationData());
  }

  SetUserTimeZoneResolveMethod(
      primary_pref_service, system::TimeZoneResolverManager::
                                TimeZoneResolveMethod::SEND_ALL_LOCATION_INFO);
  for (auto tz_choice : kUserTimeZoneAutomaticDetectionUserChoices) {
    SetUserTimeZoneResolveMethod(secondary_pref_service, tz_choice);
    EXPECT_TRUE(tz_resolver_manager->ShouldSendWiFiGeolocationData());
    EXPECT_TRUE(tz_resolver_manager->ShouldSendCellularGeolocationData());
  }
}

IN_PROC_BROWSER_TEST_F(TimeZoneResolverManagerEnrolledDeviceTest, ManagedUser) {
  ash::system::TimeZoneResolverManager* tz_resolver_manager =
      g_browser_process->platform_part()->GetTimezoneResolverManager();

  // Check that on login screen TimeZoneResolverManager is not sending any data
  // to Google servers.
  SetDeviceTimeZoneAutomaticDetectionPolicy(
      em::SystemTimezoneProto::AutomaticTimezoneDetectionType::
          SystemTimezoneProto_AutomaticTimezoneDetectionType_USERS_DECIDE);
  EXPECT_FALSE(tz_resolver_manager->ShouldSendWiFiGeolocationData());
  EXPECT_FALSE(tz_resolver_manager->ShouldSendCellularGeolocationData());

  // Log in a managed user.
  LoginUser(managed_user_id_);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tz_resolver_manager->ShouldSendWiFiGeolocationData());
  EXPECT_FALSE(tz_resolver_manager->ShouldSendCellularGeolocationData());

  // Disable automatic time zone detection by policy.
  SetDeviceTimeZoneAutomaticDetectionPolicy(
      em::SystemTimezoneProto::AutomaticTimezoneDetectionType::
          SystemTimezoneProto_AutomaticTimezoneDetectionType_DISABLED);
  EXPECT_FALSE(tz_resolver_manager->ShouldSendWiFiGeolocationData());
  EXPECT_FALSE(tz_resolver_manager->ShouldSendCellularGeolocationData());

  // Set automatic time zone to IP_ONLY.
  SetDeviceTimeZoneAutomaticDetectionPolicy(
      em::SystemTimezoneProto::AutomaticTimezoneDetectionType::
          SystemTimezoneProto_AutomaticTimezoneDetectionType_IP_ONLY);
  EXPECT_FALSE(tz_resolver_manager->ShouldSendWiFiGeolocationData());
  EXPECT_FALSE(tz_resolver_manager->ShouldSendCellularGeolocationData());

  // Set automatic time zone to WIFI.
  SetDeviceTimeZoneAutomaticDetectionPolicy(
      em::SystemTimezoneProto::AutomaticTimezoneDetectionType::
          SystemTimezoneProto_AutomaticTimezoneDetectionType_SEND_WIFI_ACCESS_POINTS);
  EXPECT_TRUE(tz_resolver_manager->ShouldSendWiFiGeolocationData());
  EXPECT_FALSE(tz_resolver_manager->ShouldSendCellularGeolocationData());

  // Set automatic time zone to ALL_INFO (i.e. Wi-Fi and cellular).
  SetDeviceTimeZoneAutomaticDetectionPolicy(
      em::SystemTimezoneProto::AutomaticTimezoneDetectionType::
          SystemTimezoneProto_AutomaticTimezoneDetectionType_SEND_ALL_LOCATION_INFO);
  EXPECT_TRUE(tz_resolver_manager->ShouldSendWiFiGeolocationData());
  EXPECT_TRUE(tz_resolver_manager->ShouldSendCellularGeolocationData());
}

IN_PROC_BROWSER_TEST_F(TimeZoneResolverManagerEnrolledDeviceTest,
                       ManagedUserStaticTimeZonePolicy) {
  // Set static time zone by policy and log in a managed user.
  // Static time zone policy overrides automatic time zone policies,
  // so no matter what other policies are provided, automatic time zone
  // resolution will be prohobited.
  SetDeviceTimeZoneStaticPolicy("Europe/Berlin");
  LoginUser(managed_user_id_);
  base::RunLoop().RunUntilIdle();

  // Iterate over all possible automatic time zone policies and check that
  // TimeZoneResolverManager is not sending out any sensor data.
  ash::system::TimeZoneResolverManager* tz_resolver_manager =
      g_browser_process->platform_part()->GetTimezoneResolverManager();
  for (auto tz_policy : kTimeZoneAutomaticDetectionCloudPolicies) {
    SetDeviceTimeZoneAutomaticDetectionPolicy(tz_policy);

    EXPECT_FALSE(tz_resolver_manager->ShouldSendWiFiGeolocationData());
    EXPECT_FALSE(tz_resolver_manager->ShouldSendCellularGeolocationData());
  }

  // Iterate over all possible user options in time zone settings and check that
  // TimeZoneResolverManager is not sending any sensor data.
  SetDeviceTimeZoneAutomaticDetectionPolicy(
      em::SystemTimezoneProto::AutomaticTimezoneDetectionType::
          SystemTimezoneProto_AutomaticTimezoneDetectionType_USERS_DECIDE);
  PrefService* pref_service =
      g_browser_process->profile_manager()->GetActiveUserProfile()->GetPrefs();
  for (auto tz_user_choice : kUserTimeZoneAutomaticDetectionUserChoices) {
    SetUserTimeZoneResolveMethod(pref_service, tz_user_choice);

    EXPECT_FALSE(tz_resolver_manager->ShouldSendWiFiGeolocationData());
    EXPECT_FALSE(tz_resolver_manager->ShouldSendCellularGeolocationData());
  }
}

IN_PROC_BROWSER_TEST_F(TimeZoneResolverManagerEnrolledDeviceTest,
                       CheckIsTimezonePrefManaged) {
  // Log in a user.
  LoginUser(regular_primary_user_id_);
  base::RunLoop().RunUntilIdle();

  // Without any policy applied, the timezone pref should be not managed by
  // default.
  EXPECT_FALSE(system::IsTimezonePrefsManaged(kSystemTimezone));
  EXPECT_FALSE(system::IsTimezonePrefsManaged(::prefs::kUserTimezone));
  EXPECT_FALSE(system::IsTimezonePrefsManaged(
      ::prefs::kResolveTimezoneByGeolocationMethod));

  // Apply device timezone automatic detection policy. kSystemTimezone is only
  // managed if kSystemTimezonePolicy is applied. Since kSystemTimezonePolicy is
  // not applied yet, kSystemTimezone should not be managed.
  for (auto tz_policy : kTimeZoneAutomaticDetectionCloudPolicies) {
    SetDeviceTimeZoneAutomaticDetectionPolicy(tz_policy);
    EXPECT_FALSE(system::IsTimezonePrefsManaged(kSystemTimezone));
    if (tz_policy ==
        enterprise_management::
            SystemTimezoneProto_AutomaticTimezoneDetectionType_USERS_DECIDE) {
      EXPECT_FALSE(system::IsTimezonePrefsManaged(::prefs::kUserTimezone));
      EXPECT_FALSE(system::IsTimezonePrefsManaged(
          ::prefs::kResolveTimezoneByGeolocationMethod));
    } else if (
        tz_policy ==
        enterprise_management::
            SystemTimezoneProto_AutomaticTimezoneDetectionType_DISABLED) {
      EXPECT_FALSE(system::IsTimezonePrefsManaged(::prefs::kUserTimezone));
      EXPECT_TRUE(system::IsTimezonePrefsManaged(
          ::prefs::kResolveTimezoneByGeolocationMethod));
    } else {
      EXPECT_TRUE(system::IsTimezonePrefsManaged(::prefs::kUserTimezone));
      EXPECT_TRUE(system::IsTimezonePrefsManaged(
          ::prefs::kResolveTimezoneByGeolocationMethod));
    }
  }

  // Apply user decided device timezone automatic detection policy, the timezone
  // pref should be not managed.
  SetDeviceTimeZoneAutomaticDetectionPolicy(
      enterprise_management::
          SystemTimezoneProto_AutomaticTimezoneDetectionType_USERS_DECIDE);
  EXPECT_FALSE(system::IsTimezonePrefsManaged(kSystemTimezone));
  EXPECT_FALSE(system::IsTimezonePrefsManaged(::prefs::kUserTimezone));
  EXPECT_FALSE(system::IsTimezonePrefsManaged(
      ::prefs::kResolveTimezoneByGeolocationMethod));

  // Apply device timezone static policy, all timezone prefs should be managed.
  SetDeviceTimeZoneStaticPolicy("Europe/Berlin");
  EXPECT_TRUE(system::IsTimezonePrefsManaged(kSystemTimezone));
  EXPECT_TRUE(system::IsTimezonePrefsManaged(::prefs::kUserTimezone));
  EXPECT_TRUE(system::IsTimezonePrefsManaged(
      ::prefs::kResolveTimezoneByGeolocationMethod));
}

IN_PROC_BROWSER_TEST_F(TimeZoneResolverManagerEnrolledDeviceTest,
                       CheckSystemGeolocationPermissionOnLogInScreen) {
  ash::system::TimeZoneResolverManager* tz_resolver_manager =
      g_browser_process->platform_part()->GetTimezoneResolverManager();
  ASSERT_NE(tz_resolver_manager, nullptr);

  // By default the resolver is up and running.
  EXPECT_TRUE(tz_resolver_manager->TimeZoneResolverShouldBeRunning());

  // Setting the detection policy to USERS_DECIDE should keep the default state.
  SetDeviceTimeZoneAutomaticDetectionPolicy(
      em::SystemTimezoneProto::AutomaticTimezoneDetectionType::
          SystemTimezoneProto_AutomaticTimezoneDetectionType_USERS_DECIDE);
  EXPECT_TRUE(tz_resolver_manager->TimeZoneResolverShouldBeRunning());

  // Disable timezone detection:
  SetDeviceTimeZoneAutomaticDetectionPolicy(
      em::SystemTimezoneProto::AutomaticTimezoneDetectionType::
          SystemTimezoneProto_AutomaticTimezoneDetectionType_DISABLED);
  ASSERT_TRUE(
      system::TimeZoneResolverManager::IsTimeZoneResolutionPolicyControlled());
  EXPECT_FALSE(tz_resolver_manager->TimeZoneResolverShouldBeRunning());

  // Re-enable timezone detection:
  SetDeviceTimeZoneAutomaticDetectionPolicy(
      em::SystemTimezoneProto::AutomaticTimezoneDetectionType::
          SystemTimezoneProto_AutomaticTimezoneDetectionType_IP_ONLY);
  EXPECT_TRUE(tz_resolver_manager->TimeZoneResolverShouldBeRunning());
}

IN_PROC_BROWSER_TEST_F(TimeZoneResolverManagerUnenrolledDeviceTest,
                       CheckSystemGeolocationPermission) {
  ASSERT_FALSE(
      system::TimeZoneResolverManager::IsTimeZoneResolutionPolicyControlled());

  ash::system::TimeZoneResolverManager* tz_resolver_manager =
      g_browser_process->platform_part()->GetTimezoneResolverManager();

  TimeZoneResolver* tz_resolver = tz_resolver_manager->GetResolver();
  ASSERT_NE(tz_resolver_manager, nullptr);
  ASSERT_NE(tz_resolver, nullptr);

  // Login-screen geolocation permission is On by default.
  EXPECT_TRUE(system::TimeZoneResolverManager::
                  IfServiceShouldBeRunningForSigninScreen());

  // Log in a user.
  LoginUser(regular_primary_user_id_);

  PrefService* pref_service =
      g_browser_process->profile_manager()->GetActiveUserProfile()->GetPrefs();

  // Check the default configuration: Should be set to IP-resolution.
  EXPECT_TRUE(
      tz_resolver_manager->TimeZoneResolverAllowedByTimeZoneConfigData());
  EXPECT_EQ(
      system::TimeZoneResolverManager::GetEffectiveUserTimeZoneResolveMethod(
          pref_service, true),
      system::TimeZoneResolverManager::TimeZoneResolveMethod::IP_ONLY);

  // Check the permission is granted and timezone resolver is actually running.
  EXPECT_EQ(
      SimpleGeolocationProvider::GetInstance()->GetGeolocationAccessLevel(),
      GeolocationAccessLevel::kAllowed);
  EXPECT_TRUE(tz_resolver_manager->TimeZoneResolverShouldBeRunning());
  EXPECT_TRUE(tz_resolver->IsRunning());

  // Disable geolocation permission.
  UpdateUserGeolocationPermission(pref_service,
                                  GeolocationAccessLevel::kDisallowed);
  // Change of `kUserGeolocationAccessLevel` will trigger the
  // `OnGeolocationPermissionChanged()`, stopping the scheduler.
  EXPECT_FALSE(tz_resolver_manager->TimeZoneResolverShouldBeRunning());
  EXPECT_FALSE(tz_resolver->IsRunning());
  // Check the timezone configuration didn't change on permission update.
  EXPECT_TRUE(
      tz_resolver_manager->TimeZoneResolverAllowedByTimeZoneConfigData());

  // Re-enable geolocation permission and check that the resolver is working
  // again.
  UpdateUserGeolocationPermission(pref_service,
                                  GeolocationAccessLevel::kAllowed);
  EXPECT_TRUE(tz_resolver_manager->TimeZoneResolverShouldBeRunning());
  EXPECT_TRUE(tz_resolver->IsRunning());
  // Check timezone configuration still hasn't changed.
  EXPECT_TRUE(
      tz_resolver_manager->TimeZoneResolverAllowedByTimeZoneConfigData());

  // Disable and re-enable geolocation permission with `kOnlyAllowedForSystem`.
  // Check that the resolver is working.
  UpdateUserGeolocationPermission(pref_service,
                                  GeolocationAccessLevel::kDisallowed);
  UpdateUserGeolocationPermission(
      pref_service, GeolocationAccessLevel::kOnlyAllowedForSystem);
  // Check that resolver is actually running.
  EXPECT_TRUE(tz_resolver_manager->TimeZoneResolverShouldBeRunning());
  EXPECT_TRUE(tz_resolver->IsRunning());
  // Check timezone configuration is still unchanged.
  EXPECT_TRUE(
      tz_resolver_manager->TimeZoneResolverAllowedByTimeZoneConfigData());
}

}  // namespace ash
