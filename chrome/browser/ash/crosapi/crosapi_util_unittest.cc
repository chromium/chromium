// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/crosapi_util.h"

#include <stdint.h>

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
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/crosapi/mojom/browser_service.mojom.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using user_manager::User;

namespace crosapi {

namespace {
std::unique_ptr<policy::UserCloudPolicyManagerAsh> CreateUserCloudPolicyManager(
    Profile* profile,
    AccountId account_id,
    std::unique_ptr<policy::CloudPolicyStore> store) {
  auto fatal_error_callback = []() {
    LOG(ERROR) << "Fatal error: policy could not be loaded";
  };
  return std::make_unique<policy::UserCloudPolicyManagerAsh>(
      profile, std::move(store),
      std::make_unique<policy::MockCloudExternalDataManager>(),
      /*component_policy_cache_path=*/base::FilePath(),
      policy::UserCloudPolicyManagerAsh::PolicyEnforcement::kPolicyRequired,
      g_browser_process->local_state(),
      /*policy_refresh_timeout=*/base::Minutes(1),
      base::BindOnce(fatal_error_callback), account_id,
      base::SingleThreadTaskRunner::GetCurrentDefault());
}
}  // namespace

class CrosapiUtilTest : public testing::Test {
 public:
  CrosapiUtilTest() = default;
  ~CrosapiUtilTest() override = default;

  void SetUp() override {
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    user_manager::UserManagerImpl::RegisterProfilePrefs(
        pref_service_.registry());
    ash::system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal(), &local_state_);
    ASSERT_TRUE(profile_manager_->SetUp());
    testing_profile_ = profile_manager_->CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName);

    auto cloud_policy_store = std::make_unique<policy::MockCloudPolicyStore>();
    cloud_policy_store_ = cloud_policy_store.get();
    testing_profile_->SetUserCloudPolicyManagerAsh(CreateUserCloudPolicyManager(
        testing_profile_,
        AccountId::FromUserEmail(TestingProfile::kDefaultProfileUserName),
        std::move(cloud_policy_store)));
  }

  void TearDown() override {
    for (const auto& account_id : profile_created_accounts_) {
      fake_user_manager_->OnUserProfileWillBeDestroyed(account_id);
    }

    cloud_policy_store_ = nullptr;
    testing_profile_ = nullptr;
    profile_manager_.reset();
    ash::system::StatisticsProvider::SetTestProvider(nullptr);
  }

  void AddRegularUser(const std::string& email) {
    AccountId account_id = AccountId::FromUserEmail(email);
    const User* user = fake_user_manager_->AddUser(account_id);
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
    fake_user_manager_->OnUserProfileCreated(account_id, &pref_service_);
    profile_created_accounts_.push_back(account_id);
  }

  policy::MockCloudPolicyStore* GetCloudPolicyStore() {
    return cloud_policy_store_;
  }

  // The order of these members is relevant for both construction and
  // destruction timing.
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  content::BrowserTaskEnvironment task_environment_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  ash::system::FakeStatisticsProvider statistics_provider_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> testing_profile_;
  TestingPrefServiceSimple pref_service_;
  std::vector<AccountId> profile_created_accounts_;
  raw_ptr<policy::MockCloudPolicyStore> cloud_policy_store_ = nullptr;
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
  EXPECT_EQ(settings->device_extensions_system_log_enabled,
            crosapi::mojom::DeviceSettings::OptionalBool::kUnset);
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
  testing_profile_->ScopedCrosSettingsTestHelper()
      ->GetStubbedProvider()
      ->SetBoolean(ash::kDeviceExtensionsSystemLogEnabled, true);

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
  EXPECT_EQ(settings->device_extensions_system_log_enabled,
            crosapi::mojom::DeviceSettings::OptionalBool::kTrue);
}

TEST_F(CrosapiUtilTest, IsArcAvailable) {
  arc::SetArcAvailableCommandLineForTesting(
      base::CommandLine::ForCurrentProcess());
  IdleServiceAsh::DisableForTesting();
  AddRegularUser(TestingProfile::kDefaultProfileUserName);

  mojom::BrowserInitParamsPtr browser_init_params =
      browser_util::GetBrowserInitParams(
          browser_util::InitialBrowserAction(
              crosapi::mojom::InitialBrowserAction::kDoNotOpenWindow),
          /*is_keep_alive_enabled=*/false, std::nullopt);
  EXPECT_TRUE(browser_init_params->device_properties->is_arc_available);
  EXPECT_FALSE(browser_init_params->device_properties->is_tablet_form_factor);
}

TEST_F(CrosapiUtilTest, IsTabletFormFactor) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableTabletFormFactor);
  IdleServiceAsh::DisableForTesting();
  AddRegularUser(TestingProfile::kDefaultProfileUserName);

  mojom::BrowserInitParamsPtr browser_init_params =
      browser_util::GetBrowserInitParams(
          browser_util::InitialBrowserAction(
              crosapi::mojom::InitialBrowserAction::kDoNotOpenWindow),
          /*is_keep_alive_enabled=*/false, std::nullopt);
  EXPECT_FALSE(browser_init_params->device_properties->is_arc_available);
  EXPECT_TRUE(browser_init_params->device_properties->is_tablet_form_factor);
}

TEST_F(CrosapiUtilTest, SerialNumber) {
  IdleServiceAsh::DisableForTesting();
  AddRegularUser(TestingProfile::kDefaultProfileUserName);

  std::string expected_serial_number = "fake-serial-number";
  statistics_provider_.SetMachineStatistic("serial_number",
                                           expected_serial_number);

  mojom::BrowserInitParamsPtr browser_init_params =
      browser_util::GetBrowserInitParams(
          browser_util::InitialBrowserAction(
              crosapi::mojom::InitialBrowserAction::kDoNotOpenWindow),
          /*is_keep_alive_enabled=*/false, std::nullopt);

  auto serial_number = browser_init_params->device_properties->serial_number;
  ASSERT_TRUE(serial_number.has_value());
  EXPECT_EQ(serial_number.value(), expected_serial_number);
}

TEST_F(CrosapiUtilTest, BrowserInitParamsContainsUserPolicy) {
  IdleServiceAsh::DisableForTesting();
  AddRegularUser(TestingProfile::kDefaultProfileUserName);

  enterprise_management::CloudPolicySettings user_policies;
  user_policies.mutable_userprintersallowed()->set_value(false);
  auto user_policy_data = std::make_unique<enterprise_management::PolicyData>();
  user_policies.SerializeToString(user_policy_data->mutable_policy_value());
  GetCloudPolicyStore()->set_policy_data_for_testing(
      std::move(user_policy_data));
  std::string expected_policy_blob;
  GetCloudPolicyStore()->policy_fetch_response()->SerializeToString(
      &expected_policy_blob);
  std::vector<uint8_t> expected_policy_bytes = std::vector<uint8_t>(
      expected_policy_blob.begin(), expected_policy_blob.end());

  task_environment_.RunUntilIdle();

  std::string actual_user_policy_blob;
  mojom::BrowserInitParamsPtr browser_init_params =
      browser_util::GetBrowserInitParams(
          browser_util::InitialBrowserAction(
              crosapi::mojom::InitialBrowserAction::kDoNotOpenWindow),
          /*is_keep_alive_enabled=*/false, std::nullopt);

  EXPECT_EQ(expected_policy_bytes, browser_init_params->device_account_policy);
}

TEST_F(CrosapiUtilTest, DeviceExtensionsSystemLogEnabledFalse) {
  testing_profile_->ScopedCrosSettingsTestHelper()
      ->ReplaceDeviceSettingsProviderWithStub();
  testing_profile_->ScopedCrosSettingsTestHelper()->SetTrustedStatus(
      ash::CrosSettingsProvider::TRUSTED);
  base::RunLoop().RunUntilIdle();
  testing_profile_->ScopedCrosSettingsTestHelper()
      ->GetStubbedProvider()
      ->SetBoolean(ash::kDeviceExtensionsSystemLogEnabled, false);

  auto settings = browser_util::GetDeviceSettings();
  testing_profile_->ScopedCrosSettingsTestHelper()
      ->RestoreRealDeviceSettingsProvider();

  EXPECT_EQ(settings->device_extensions_system_log_enabled,
            crosapi::mojom::DeviceSettings::OptionalBool::kFalse);
}

}  // namespace crosapi
