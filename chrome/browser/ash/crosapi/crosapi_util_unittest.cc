// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/crosapi_util.h"

#include <memory>
#include <string>

#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using user_manager::User;

namespace crosapi {

class CrosapiUtilTest : public testing::Test {
 public:
  CrosapiUtilTest() = default;
  ~CrosapiUtilTest() override = default;

  void SetUp() override {
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    browser_util::RegisterLocalStatePrefs(pref_service_.registry());
    ash::system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    testing_profile_ = profile_manager_->CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName);
  }

  void TearDown() override {
    ash::system::StatisticsProvider::SetTestProvider(nullptr);
  }

  void AddRegularUser(const std::string& email) {
    AccountId account_id = AccountId::FromUserEmail(email);
    const User* user = fake_user_manager_->AddUser(account_id);
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);

    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, testing_profile_.get());
  }

  // The order of these members is relevant for both construction and
  // destruction timing.
  content::BrowserTaskEnvironment task_environment_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  ash::system::FakeStatisticsProvider statistics_provider_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> testing_profile_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(CrosapiUtilTest, GetInterfaceVersions) {
  base::flat_map<base::Token, uint32_t> versions =
      browser_util::GetInterfaceVersions();

  // Check that a known interface with version > 0 is present and has non-zero
  // version.
  EXPECT_GT(versions[mojom::KeystoreService::Uuid_], 0u);

  // Check that the empty token is not present.
  base::Token token;
  auto it = versions.find(token);
  EXPECT_EQ(it, versions.end());
}

TEST_F(CrosapiUtilTest, IsSigninProfileOrBelongsToAffiliatedUserSigninProfile) {
  TestingProfile::Builder builder;
  builder.SetPath(base::FilePath(ash::kSigninBrowserContextBaseName));
  std::unique_ptr<Profile> signin_profile = builder.Build();

  EXPECT_TRUE(browser_util::IsSigninProfileOrBelongsToAffiliatedUser(
      signin_profile.get()));
}

TEST_F(CrosapiUtilTest, IsSigninProfileOrBelongsToAffiliatedUserOffTheRecord) {
  Profile* otr_profile = testing_profile_->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);

  EXPECT_FALSE(
      browser_util::IsSigninProfileOrBelongsToAffiliatedUser(otr_profile));
}

TEST_F(CrosapiUtilTest,
       IsSigninProfileOrBelongsToAffiliatedUserAffiliatedUser) {
  AccountId account_id =
      AccountId::FromUserEmail(TestingProfile::kDefaultProfileUserName);
  const User* user = fake_user_manager_->AddUserWithAffiliation(
      account_id, /*is_affiliated=*/true);
  fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                   /*browser_restart=*/false,
                                   /*is_child=*/false);

  EXPECT_TRUE(
      browser_util::IsSigninProfileOrBelongsToAffiliatedUser(testing_profile_));
}

TEST_F(CrosapiUtilTest,
       IsSigninProfileOrBelongsToAffiliatedUserNotAffiliatedUser) {
  AddRegularUser(TestingProfile::kDefaultProfileUserName);

  EXPECT_FALSE(
      browser_util::IsSigninProfileOrBelongsToAffiliatedUser(testing_profile_));
}

TEST_F(CrosapiUtilTest,
       IsSigninProfileOrBelongsToAffiliatedUserLockScreenProfile) {
  TestingProfile::Builder builder;
  builder.SetPath(base::FilePath(ash::kLockScreenBrowserContextBaseName));
  std::unique_ptr<Profile> lock_screen_profile = builder.Build();

  EXPECT_FALSE(browser_util::IsSigninProfileOrBelongsToAffiliatedUser(
      lock_screen_profile.get()));
}

TEST_F(CrosapiUtilTest, EmptyDeviceSettings) {
  auto settings = browser_util::GetDeviceSettings();
  EXPECT_EQ(settings->attestation_for_content_protection_enabled,
            crosapi::mojom::DeviceSettings::OptionalBool::kUnset);
  EXPECT_EQ(settings->device_system_wide_tracing_enabled,
            crosapi::mojom::DeviceSettings::OptionalBool::kUnset);
  EXPECT_EQ(settings->device_restricted_managed_guest_session_enabled,
            crosapi::mojom::DeviceSettings::OptionalBool::kUnset);
  EXPECT_EQ(settings->report_device_network_status,
            crosapi::mojom::DeviceSettings::OptionalBool::kUnset);
  EXPECT_TRUE(settings->report_upload_frequency.is_null());
  EXPECT_TRUE(
      settings->report_device_network_telemetry_collection_rate_ms.is_null());
}

TEST_F(CrosapiUtilTest, DeviceSettingsWithData) {
  testing_profile_->ScopedCrosSettingsTestHelper()
      ->ReplaceDeviceSettingsProviderWithStub();
  testing_profile_->ScopedCrosSettingsTestHelper()->SetTrustedStatus(
      ash::CrosSettingsProvider::TRUSTED);
  base::RunLoop().RunUntilIdle();
  testing_profile_->ScopedCrosSettingsTestHelper()
      ->GetStubbedProvider()
      ->SetBoolean(ash::kAttestationForContentProtectionEnabled, true);
  testing_profile_->ScopedCrosSettingsTestHelper()
      ->GetStubbedProvider()
      ->SetBoolean(ash::kAccountsPrefEphemeralUsersEnabled, false);
  testing_profile_->ScopedCrosSettingsTestHelper()
      ->GetStubbedProvider()
      ->SetBoolean(ash::kDeviceRestrictedManagedGuestSessionEnabled, true);
  testing_profile_->ScopedCrosSettingsTestHelper()
      ->GetStubbedProvider()
      ->SetBoolean(ash::kReportDeviceNetworkStatus, true);

  const int64_t kReportUploadFrequencyMs = base::Hours(1).InMilliseconds();
  testing_profile_->ScopedCrosSettingsTestHelper()
      ->GetStubbedProvider()
      ->SetInteger(ash::kReportUploadFrequency, kReportUploadFrequencyMs);

  const int64_t kReportDeviceNetworkTelemetryCollectionRateMs =
      base::Minutes(15).InMilliseconds();
  testing_profile_->ScopedCrosSettingsTestHelper()
      ->GetStubbedProvider()
      ->SetInteger(ash::kReportDeviceNetworkTelemetryCollectionRateMs,
                   kReportDeviceNetworkTelemetryCollectionRateMs);

  base::Value::List allowlist;
  base::Value::Dict ids;
  ids.Set(ash::kUsbDetachableAllowlistKeyVid, 2);
  ids.Set(ash::kUsbDetachableAllowlistKeyPid, 3);
  allowlist.Append(std::move(ids));

  testing_profile_->ScopedCrosSettingsTestHelper()->GetStubbedProvider()->Set(
      ash::kUsbDetachableAllowlist, base::Value(std::move(allowlist)));

  auto settings = browser_util::GetDeviceSettings();
  testing_profile_->ScopedCrosSettingsTestHelper()
      ->RestoreRealDeviceSettingsProvider();

  EXPECT_EQ(settings->attestation_for_content_protection_enabled,
            crosapi::mojom::DeviceSettings::OptionalBool::kTrue);
  EXPECT_EQ(settings->device_restricted_managed_guest_session_enabled,
            crosapi::mojom::DeviceSettings::OptionalBool::kTrue);
  ASSERT_EQ(settings->usb_detachable_allow_list->usb_device_ids.size(), 1u);
  EXPECT_EQ(
      settings->usb_detachable_allow_list->usb_device_ids[0]->has_vendor_id,
      true);
  EXPECT_EQ(settings->usb_detachable_allow_list->usb_device_ids[0]->vendor_id,
            2);
  EXPECT_EQ(
      settings->usb_detachable_allow_list->usb_device_ids[0]->has_product_id,
      true);
  EXPECT_EQ(settings->usb_detachable_allow_list->usb_device_ids[0]->product_id,
            3);
  EXPECT_EQ(settings->report_device_network_status,
            crosapi::mojom::DeviceSettings::OptionalBool::kTrue);
  EXPECT_EQ(settings->report_upload_frequency->value, kReportUploadFrequencyMs);
  EXPECT_EQ(settings->report_device_network_telemetry_collection_rate_ms->value,
            kReportDeviceNetworkTelemetryCollectionRateMs);
}

TEST_F(CrosapiUtilTest, IsArcAvailable) {
  arc::SetArcAvailableCommandLineForTesting(
      base::CommandLine::ForCurrentProcess());
  IdleServiceAsh::DisableForTesting();
  AddRegularUser("user@google.com");

  EnvironmentProvider environment_provider;
  mojom::BrowserInitParamsPtr browser_init_params =
      browser_util::GetBrowserInitParams(
          &environment_provider,
          browser_util::InitialBrowserAction(
              crosapi::mojom::InitialBrowserAction::kDoNotOpenWindow),
          /*is_keep_alive_enabled=*/false, absl::nullopt);
  EXPECT_TRUE(browser_init_params->device_properties->is_arc_available);
  EXPECT_FALSE(browser_init_params->device_properties->is_tablet_form_factor);
}

TEST_F(CrosapiUtilTest, IsTabletFormFactor) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableTabletFormFactor);
  IdleServiceAsh::DisableForTesting();
  AddRegularUser("user@google.com");

  EnvironmentProvider environment_provider;
  mojom::BrowserInitParamsPtr browser_init_params =
      browser_util::GetBrowserInitParams(
          &environment_provider,
          browser_util::InitialBrowserAction(
              crosapi::mojom::InitialBrowserAction::kDoNotOpenWindow),
          /*is_keep_alive_enabled=*/false, absl::nullopt);
  EXPECT_FALSE(browser_init_params->device_properties->is_arc_available);
  EXPECT_TRUE(browser_init_params->device_properties->is_tablet_form_factor);
}

TEST_F(CrosapiUtilTest, SerialNumber) {
  IdleServiceAsh::DisableForTesting();
  AddRegularUser("user@google.com");

  std::string expected_serial_number = "fake-serial-number";
  statistics_provider_.SetMachineStatistic("serial_number",
                                           expected_serial_number);

  EnvironmentProvider environment_provider;
  mojom::BrowserInitParamsPtr browser_init_params =
      browser_util::GetBrowserInitParams(
          &environment_provider,
          browser_util::InitialBrowserAction(
              crosapi::mojom::InitialBrowserAction::kDoNotOpenWindow),
          /*is_keep_alive_enabled=*/false, absl::nullopt);

  auto serial_number = browser_init_params->device_properties->serial_number;
  ASSERT_TRUE(serial_number.has_value());
  EXPECT_EQ(serial_number.value(), expected_serial_number);
}

}  // namespace crosapi
