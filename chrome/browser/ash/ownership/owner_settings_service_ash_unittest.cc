// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_path_override.h"
#include "base/values.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/device_settings_provider.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace ash {

namespace {

const char kUserAllowlist[] = "*@allowlist-domain.com";
const char kOther[] = "other";

void OnPrefChanged(const std::string& /* setting */) {}

class PrefsChecker : public ownership::OwnerSettingsService::Observer {
 public:
  PrefsChecker(OwnerSettingsServiceAsh* service,
               DeviceSettingsProvider* provider)
      : service_(service), provider_(provider) {
    CHECK(service_);
    CHECK(provider_);
    service_->AddObserver(this);
  }

  ~PrefsChecker() override { service_->RemoveObserver(this); }

  // OwnerSettingsService::Observer implementation:
  void OnSignedPolicyStored(bool success) override {
    if (service_->HasPendingChanges())
      return;

    while (!set_requests_.empty()) {
      SetRequest request = std::move(set_requests_.front());
      set_requests_.pop();
      const base::Value* value = provider_->Get(request.first);
      ASSERT_EQ(request.second, *value);
    }
    loop_.Quit();
  }

  bool Set(const std::string& setting, const base::Value& value) {
    if (!service_->Set(setting, value))
      return false;
    set_requests_.push(SetRequest(setting, value.Clone()));
    return true;
  }

  void Wait() { loop_.Run(); }

 private:
  OwnerSettingsServiceAsh* service_;
  DeviceSettingsProvider* provider_;
  base::RunLoop loop_;

  using SetRequest = std::pair<std::string, base::Value>;
  base::queue<SetRequest> set_requests_;

  DISALLOW_COPY_AND_ASSIGN(PrefsChecker);
};

bool FindInListValue(const std::string& needle, const base::Value* haystack) {
  const base::ListValue* list;
  if (!haystack->GetAsList(&list))
    return false;
  return list->end() != list->Find(base::Value(needle));
}

}  // namespace

class OwnerSettingsServiceAshTest : public DeviceSettingsTestBase {
 public:
  OwnerSettingsServiceAshTest()
      : service_(nullptr),
        local_state_(TestingBrowserProcess::GetGlobal()),
        user_data_dir_override_(chrome::DIR_USER_DATA),
        management_settings_set_(false) {}

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();
    provider_.reset(new DeviceSettingsProvider(
        base::BindRepeating(&OnPrefChanged), device_settings_service_.get(),
        TestingBrowserProcess::GetGlobal()->local_state()));
    owner_key_util_->SetPrivateKey(device_policy_->GetSigningKey());
    InitOwner(
        AccountId::FromUserEmail(device_policy_->policy_data().username()),
        true);
    FlushDeviceSettings();

    service_ =
        OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_.get());
    ASSERT_TRUE(service_);
    ASSERT_TRUE(service_->IsOwner());

    device_policy_->policy_data().set_management_mode(
        em::PolicyData::LOCAL_OWNER);
    device_policy_->Build();
    session_manager_client_.set_device_policy(device_policy_->GetBlob());
    ReloadDeviceSettings();
  }

  void TearDown() override {
    provider_.reset();
    DeviceSettingsTestBase::TearDown();
  }

  void TestSingleSet(OwnerSettingsServiceAsh* service,
                     const std::string& setting,
                     const base::Value& in_value) {
    PrefsChecker checker(service, provider_.get());
    checker.Set(setting, in_value);
    FlushDeviceSettings();
    checker.Wait();
  }

  void OnManagementSettingsSet(bool success) {
    management_settings_set_ = success;
  }

  const em::ChromeDeviceSettingsProto& device_settings() const {
    const auto* settings = device_settings_service_->device_settings();
    CHECK_NE(nullptr, settings);
    return *settings;
  }

 protected:
  OwnerSettingsServiceAsh* service_;
  ScopedTestingLocalState local_state_;
  std::unique_ptr<DeviceSettingsProvider> provider_;
  base::ScopedPathOverride user_data_dir_override_;
  bool management_settings_set_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OwnerSettingsServiceAshTest);
};

TEST_F(OwnerSettingsServiceAshTest, SingleSetTest) {
  TestSingleSet(service_, kReleaseChannel, base::Value("dev-channel"));
  TestSingleSet(service_, kReleaseChannel, base::Value("beta-channel"));
  TestSingleSet(service_, kReleaseChannel, base::Value("stable-channel"));
}

TEST_F(OwnerSettingsServiceAshTest, MultipleSetTest) {
  base::Value allow_guest(false);
  base::Value release_channel("stable-channel");
  base::Value show_user_names(true);

  PrefsChecker checker(service_, provider_.get());

  checker.Set(kAccountsPrefAllowGuest, allow_guest);
  checker.Set(kReleaseChannel, release_channel);
  checker.Set(kAccountsPrefShowUserNamesOnSignIn, show_user_names);

  FlushDeviceSettings();
  checker.Wait();
}

TEST_F(OwnerSettingsServiceAshTest, FailedSetRequest) {
  session_manager_client_.ForceStorePolicyFailure(true);
  std::string current_channel;
  ASSERT_TRUE(provider_->Get(kReleaseChannel)->GetAsString(&current_channel));
  ASSERT_NE(current_channel, "stable-channel");

  // Check that DeviceSettingsProvider's cache is updated.
  PrefsChecker checker(service_, provider_.get());
  checker.Set(kReleaseChannel, base::Value("stable-channel"));
  FlushDeviceSettings();
  checker.Wait();

  // Check that DeviceSettingsService's policy isn't updated.
  ASSERT_EQ(current_channel,
            device_settings().release_channel().release_channel());
}

TEST_F(OwnerSettingsServiceAshTest, ForceAllowlist) {
  EXPECT_FALSE(FindInListValue(device_policy_->policy_data().username(),
                               provider_->Get(kAccountsPrefUsers)));
  // Force a settings write.
  TestSingleSet(service_, kReleaseChannel, base::Value("dev-channel"));
  EXPECT_TRUE(FindInListValue(device_policy_->policy_data().username(),
                              provider_->Get(kAccountsPrefUsers)));
}

TEST_F(OwnerSettingsServiceAshTest, AccountPrefUsersEmptyLists) {
  std::vector<base::Value> list;
  list.push_back(base::Value(kUserAllowlist));

  EXPECT_EQ(0,
            device_policy_->payload().user_allowlist().user_allowlist().size());
  EXPECT_EQ(0,
            device_policy_->payload().user_whitelist().user_whitelist().size());

  OwnerSettingsServiceAsh::UpdateDeviceSettings(
      kAccountsPrefUsers, base::ListValue(list), device_policy_->payload());

  EXPECT_EQ(1,
            device_policy_->payload().user_allowlist().user_allowlist().size());
  EXPECT_EQ(kUserAllowlist,
            device_policy_->payload().user_allowlist().user_allowlist(0));
  EXPECT_EQ(0,
            device_policy_->payload().user_whitelist().user_whitelist().size());
}

TEST_F(OwnerSettingsServiceAshTest, AccountPrefUsersAllowList) {
  std::vector<base::Value> list;
  list.push_back(base::Value(kUserAllowlist));

  device_policy_->payload().mutable_user_allowlist()->add_user_allowlist(
      kOther);

  EXPECT_EQ(1,
            device_policy_->payload().user_allowlist().user_allowlist().size());
  EXPECT_EQ(0,
            device_policy_->payload().user_whitelist().user_whitelist().size());

  OwnerSettingsServiceAsh::UpdateDeviceSettings(
      kAccountsPrefUsers, base::ListValue(list), device_policy_->payload());

  EXPECT_EQ(1,
            device_policy_->payload().user_allowlist().user_allowlist().size());
  EXPECT_EQ(kUserAllowlist,
            device_policy_->payload().user_allowlist().user_allowlist(0));
  EXPECT_EQ(0,
            device_policy_->payload().user_whitelist().user_whitelist().size());
}

TEST_F(OwnerSettingsServiceAshTest, AccountPrefUsersWhiteList) {
  std::vector<base::Value> list;
  list.push_back(base::Value(kUserAllowlist));

  device_policy_->payload().mutable_user_whitelist()->add_user_whitelist(
      kOther);

  EXPECT_EQ(0,
            device_policy_->payload().user_allowlist().user_allowlist().size());
  EXPECT_EQ(1,
            device_policy_->payload().user_whitelist().user_whitelist().size());

  OwnerSettingsServiceAsh::UpdateDeviceSettings(
      kAccountsPrefUsers, base::ListValue(list), device_policy_->payload());

  EXPECT_EQ(0,
            device_policy_->payload().user_allowlist().user_allowlist().size());
  EXPECT_EQ(1,
            device_policy_->payload().user_whitelist().user_whitelist().size());
  EXPECT_EQ(kUserAllowlist,
            device_policy_->payload().user_whitelist().user_whitelist(0));
}

TEST_F(OwnerSettingsServiceAshTest, AccountPrefUsersBothLists) {
  std::vector<base::Value> list;
  list.push_back(base::Value(kUserAllowlist));

  device_policy_->payload().mutable_user_allowlist()->add_user_allowlist(
      kOther);
  device_policy_->payload().mutable_user_whitelist()->add_user_whitelist(
      kOther);

  EXPECT_EQ(1,
            device_policy_->payload().user_allowlist().user_allowlist().size());
  EXPECT_EQ(1,
            device_policy_->payload().user_whitelist().user_whitelist().size());

  OwnerSettingsServiceAsh::UpdateDeviceSettings(
      kAccountsPrefUsers, base::ListValue(list), device_policy_->payload());

  EXPECT_EQ(1,
            device_policy_->payload().user_allowlist().user_allowlist().size());
  EXPECT_EQ(kUserAllowlist,
            device_policy_->payload().user_allowlist().user_allowlist(0));
  EXPECT_EQ(0,
            device_policy_->payload().user_whitelist().user_whitelist().size());
}

TEST_F(OwnerSettingsServiceAshTest, MigrateFeatureFlagsAbsent) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(device_settings().has_feature_flags());

  // Force a settings write. No changes to feature flags or switches.
  TestSingleSet(service_, kReleaseChannel, base::Value("dev-channel"));

  EXPECT_FALSE(device_settings().has_feature_flags());
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.DeviceSettings.FeatureFlagsMigration",
      FeatureFlagsMigrationStatus::kNoFeatureFlags, 1);
}

TEST_F(OwnerSettingsServiceAshTest, MigrateFeatureFlagsNoSwitches) {
  base::HistogramTester histogram_tester;
  device_policy_->payload().mutable_feature_flags();
  EXPECT_TRUE(device_policy_->payload().has_feature_flags());

  // Force a settings write. No changes to feature flags.
  TestSingleSet(service_, kReleaseChannel, base::Value("dev-channel"));

  EXPECT_EQ(0, device_settings().feature_flags().feature_flags_size());
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.DeviceSettings.FeatureFlagsMigration",
      FeatureFlagsMigrationStatus::kNoFeatureFlags, 1);
}

TEST_F(OwnerSettingsServiceAshTest, MigrateFeatureFlagsSuccess) {
  base::HistogramTester histogram_tester;
  device_policy_->payload().mutable_feature_flags()->add_switches("--foobar");
  device_policy_->Build();
  session_manager_client_.set_device_policy(device_policy_->GetBlob());
  ReloadDeviceSettings();

  ASSERT_EQ(1, device_settings().feature_flags().switches_size());
  EXPECT_EQ("--foobar", device_settings().feature_flags().switches(0));

  flags_ui::PrefServiceFlagsStorage flags_storage(profile_->GetPrefs());
  flags_storage.SetFlags({"feature-name"});

  // Force a settings write. The switches field should be dropped and the
  // feature_flags field be re-initialized from OwnerFlagsStorage.
  TestSingleSet(service_, kReleaseChannel, base::Value("dev-channel"));

  EXPECT_EQ(0, device_settings().feature_flags().switches_size());
  ASSERT_EQ(1, device_settings().feature_flags().feature_flags_size());
  EXPECT_EQ("feature-name", device_settings().feature_flags().feature_flags(0));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.DeviceSettings.FeatureFlagsMigration",
      FeatureFlagsMigrationStatus::kMigrationPerformed, 1);
}

TEST_F(OwnerSettingsServiceAshTest, MigrateFeatureFlagsAlreadyMigrated) {
  base::HistogramTester histogram_tester;
  device_policy_->payload().mutable_feature_flags()->add_switches("--foobar");
  device_policy_->payload().mutable_feature_flags()->add_feature_flags(
      "feature-name");
  device_policy_->Build();
  session_manager_client_.set_device_policy(device_policy_->GetBlob());
  ReloadDeviceSettings();

  ASSERT_EQ(1, device_settings().feature_flags().switches_size());
  EXPECT_EQ("--foobar", device_settings().feature_flags().switches(0));

  flags_ui::PrefServiceFlagsStorage flags_storage(profile_->GetPrefs());
  flags_storage.SetFlags({"feature-name-2"});

  // Force a settings write. No migration should take place because the
  // feature flags field is already populated.
  TestSingleSet(service_, kReleaseChannel, base::Value("dev-channel"));

  EXPECT_EQ(0, device_settings().feature_flags().switches_size());
  ASSERT_EQ(1, device_settings().feature_flags().feature_flags_size());
  EXPECT_EQ("feature-name", device_settings().feature_flags().feature_flags(0));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.DeviceSettings.FeatureFlagsMigration",
      FeatureFlagsMigrationStatus::kAlreadyMigrated, 1);
}

class OwnerSettingsServiceAshNoOwnerTest
    : public OwnerSettingsServiceAshTest {
 public:
  OwnerSettingsServiceAshNoOwnerTest() {}
  ~OwnerSettingsServiceAshNoOwnerTest() override {}

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();
    provider_.reset(new DeviceSettingsProvider(
        base::BindRepeating(&OnPrefChanged), device_settings_service_.get(),
        TestingBrowserProcess::GetGlobal()->local_state()));
    FlushDeviceSettings();
    service_ =
        OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_.get());
    ASSERT_TRUE(service_);
    ASSERT_FALSE(service_->IsOwner());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OwnerSettingsServiceAshNoOwnerTest);
};

TEST_F(OwnerSettingsServiceAshNoOwnerTest, SingleSetTest) {
  ASSERT_FALSE(service_->SetBoolean(kAccountsPrefAllowGuest, false));
}

TEST_F(OwnerSettingsServiceAshNoOwnerTest, TakeOwnershipForceAllowlist) {
  EXPECT_FALSE(FindInListValue(device_policy_->policy_data().username(),
                               provider_->Get(kAccountsPrefUsers)));
  owner_key_util_->SetPrivateKey(device_policy_->GetSigningKey());
  InitOwner(AccountId::FromUserEmail(device_policy_->policy_data().username()),
            true);
  ReloadDeviceSettings();
  ASSERT_TRUE(service_->IsOwner());

  EXPECT_TRUE(FindInListValue(device_policy_->policy_data().username(),
                              provider_->Get(kAccountsPrefUsers)));
}

}  // namespace ash
