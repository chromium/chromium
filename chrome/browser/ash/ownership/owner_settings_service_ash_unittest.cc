// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/ownership/ownership_histograms.h"
#include "chrome/browser/ash/settings/device_settings_provider.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;
using base::Bucket;
using testing::ElementsAre;

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

  PrefsChecker(const PrefsChecker&) = delete;
  PrefsChecker& operator=(const PrefsChecker&) = delete;

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
  raw_ptr<OwnerSettingsServiceAsh, ExperimentalAsh> service_;
  raw_ptr<DeviceSettingsProvider, ExperimentalAsh> provider_;
  base::RunLoop loop_;

  using SetRequest = std::pair<std::string, base::Value>;
  base::queue<SetRequest> set_requests_;
};

bool FindInListValue(const std::string& needle, const base::Value* haystack) {
  if (!haystack->is_list())
    return false;
  return base::Contains(haystack->GetList(), base::Value(needle));
}

}  // namespace

class OwnerSettingsServiceAshTest : public DeviceSettingsTestBase {
 public:
  OwnerSettingsServiceAshTest()
      : local_state_(TestingBrowserProcess::GetGlobal()),
        user_data_dir_override_(chrome::DIR_USER_DATA) {}

  OwnerSettingsServiceAshTest(const OwnerSettingsServiceAshTest&) = delete;
  OwnerSettingsServiceAshTest& operator=(const OwnerSettingsServiceAshTest&) =
      delete;

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();
    provider_ = std::make_unique<DeviceSettingsProvider>(
        base::BindRepeating(&OnPrefChanged), device_settings_service_.get(),
        TestingBrowserProcess::GetGlobal()->local_state());
    owner_key_util_->ImportPrivateKeyAndSetPublicKey(
        device_policy_->GetSigningKey());
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
  raw_ptr<OwnerSettingsServiceAsh, ExperimentalAsh> service_ = nullptr;
  ScopedTestingLocalState local_state_;
  std::unique_ptr<DeviceSettingsProvider> provider_;
  base::ScopedPathOverride user_data_dir_override_;
  bool management_settings_set_ = false;
  base::HistogramTester histogram_tester_;
};

TEST_F(OwnerSettingsServiceAshTest, SingleSetTest) {
  TestSingleSet(service_, kReleaseChannel, base::Value("dev-channel"));
  TestSingleSet(service_, kReleaseChannel, base::Value("beta-channel"));
  TestSingleSet(service_, kReleaseChannel, base::Value("stable-channel"));

  EXPECT_LE(1, histogram_tester_.GetBucketCount(
                   kOwnerKeyHistogramName,
                   OwnerKeyUmaEvent::kStartSigningPolicySuccess));
  EXPECT_LE(
      1, histogram_tester_.GetBucketCount(
             kOwnerKeyHistogramName, OwnerKeyUmaEvent::kSignedPolicySuccess));
  EXPECT_LE(
      1, histogram_tester_.GetBucketCount(
             kOwnerKeyHistogramName, OwnerKeyUmaEvent::kStoredPolicySuccess));
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
  ASSERT_TRUE(provider_->Get(kReleaseChannel)->is_string());
  const std::string current_channel =
      provider_->Get(kReleaseChannel)->GetString();
  ASSERT_NE("stable-channel", current_channel);

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
  base::Value::List list;
  list.Append(kUserAllowlist);

  EXPECT_EQ(0,
            device_policy_->payload().user_allowlist().user_allowlist().size());
  EXPECT_EQ(0,
            device_policy_->payload().user_whitelist().user_whitelist().size());

  OwnerSettingsServiceAsh::UpdateDeviceSettings(kAccountsPrefUsers,
                                                base::Value(std::move(list)),
                                                device_policy_->payload());

  EXPECT_EQ(1,
            device_policy_->payload().user_allowlist().user_allowlist().size());
  EXPECT_EQ(kUserAllowlist,
            device_policy_->payload().user_allowlist().user_allowlist(0));
  EXPECT_EQ(0,
            device_policy_->payload().user_whitelist().user_whitelist().size());
}

TEST_F(OwnerSettingsServiceAshTest, AccountPrefUsersAllowList) {
  base::Value::List list;
  list.Append(kUserAllowlist);

  device_policy_->payload().mutable_user_allowlist()->add_user_allowlist(
      kOther);

  EXPECT_EQ(1,
            device_policy_->payload().user_allowlist().user_allowlist().size());
  EXPECT_EQ(0,
            device_policy_->payload().user_whitelist().user_whitelist().size());

  OwnerSettingsServiceAsh::UpdateDeviceSettings(kAccountsPrefUsers,
                                                base::Value(std::move(list)),
                                                device_policy_->payload());

  EXPECT_EQ(1,
            device_policy_->payload().user_allowlist().user_allowlist().size());
  EXPECT_EQ(kUserAllowlist,
            device_policy_->payload().user_allowlist().user_allowlist(0));
  EXPECT_EQ(0,
            device_policy_->payload().user_whitelist().user_whitelist().size());
}

TEST_F(OwnerSettingsServiceAshTest, AccountPrefUsersWhiteList) {
  base::Value::List list;
  list.Append(kUserAllowlist);

  device_policy_->payload().mutable_user_whitelist()->add_user_whitelist(
      kOther);

  EXPECT_EQ(0,
            device_policy_->payload().user_allowlist().user_allowlist().size());
  EXPECT_EQ(1,
            device_policy_->payload().user_whitelist().user_whitelist().size());

  OwnerSettingsServiceAsh::UpdateDeviceSettings(kAccountsPrefUsers,
                                                base::Value(std::move(list)),
                                                device_policy_->payload());

  EXPECT_EQ(0,
            device_policy_->payload().user_allowlist().user_allowlist().size());
  EXPECT_EQ(1,
            device_policy_->payload().user_whitelist().user_whitelist().size());
  EXPECT_EQ(kUserAllowlist,
            device_policy_->payload().user_whitelist().user_whitelist(0));
}

TEST_F(OwnerSettingsServiceAshTest, AccountPrefUsersBothLists) {
  base::Value::List list;
  list.Append(kUserAllowlist);

  device_policy_->payload().mutable_user_allowlist()->add_user_allowlist(
      kOther);
  device_policy_->payload().mutable_user_whitelist()->add_user_whitelist(
      kOther);

  EXPECT_EQ(1,
            device_policy_->payload().user_allowlist().user_allowlist().size());
  EXPECT_EQ(1,
            device_policy_->payload().user_whitelist().user_whitelist().size());

  OwnerSettingsServiceAsh::UpdateDeviceSettings(kAccountsPrefUsers,
                                                base::Value(std::move(list)),
                                                device_policy_->payload());

  EXPECT_EQ(1,
            device_policy_->payload().user_allowlist().user_allowlist().size());
  EXPECT_EQ(kUserAllowlist,
            device_policy_->payload().user_allowlist().user_allowlist(0));
  EXPECT_EQ(0,
            device_policy_->payload().user_whitelist().user_whitelist().size());
}

class OwnerSettingsServiceAshNoOwnerTest : public OwnerSettingsServiceAshTest {
 public:
  OwnerSettingsServiceAshNoOwnerTest() {}

  OwnerSettingsServiceAshNoOwnerTest(
      const OwnerSettingsServiceAshNoOwnerTest&) = delete;
  OwnerSettingsServiceAshNoOwnerTest& operator=(
      const OwnerSettingsServiceAshNoOwnerTest&) = delete;

  ~OwnerSettingsServiceAshNoOwnerTest() override {}

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();
    provider_ = std::make_unique<DeviceSettingsProvider>(
        base::BindRepeating(&OnPrefChanged), device_settings_service_.get(),
        TestingBrowserProcess::GetGlobal()->local_state());
    FlushDeviceSettings();
    service_ =
        OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_.get());
    ASSERT_TRUE(service_);
    ASSERT_FALSE(service_->IsOwner());
  }
};

// Test that a non-owner cannot set owner settings.
TEST_F(OwnerSettingsServiceAshNoOwnerTest, SingleSetTest) {
  ASSERT_FALSE(service_->SetBoolean(kAccountsPrefAllowGuest, false));
}

// Test that when ownership is taken, the owner is forcefully added to the list
// of allowed users (i.e. into the kAccountsPrefUsers allowlist policy).
TEST_F(OwnerSettingsServiceAshNoOwnerTest, TakeOwnershipForceAllowlist) {
  EXPECT_FALSE(FindInListValue(device_policy_->policy_data().username(),
                               provider_->Get(kAccountsPrefUsers)));
  owner_key_util_->ImportPrivateKeyAndSetPublicKey(
      device_policy_->GetSigningKey());
  InitOwner(AccountId::FromUserEmail(device_policy_->policy_data().username()),
            true);
  ReloadDeviceSettings();
  ASSERT_TRUE(service_->IsOwner());

  EXPECT_TRUE(FindInListValue(device_policy_->policy_data().username(),
                              provider_->Get(kAccountsPrefUsers)));
}

// Test that OwnerSettingsService can successfully finish the key loading flow
// when owner keys don't exist and `IsReady()`, `IsOwner()`, `IsOwnerAsync()`
// methods return correct results.
TEST_F(OwnerSettingsServiceAshNoOwnerTest, LoadKeysNoKeys) {
  EXPECT_FALSE(service_->IsReady());
  service_->OnTPMTokenReady();  // Trigger key load.

  base::test::TestFuture<bool> is_owner;
  service_->IsOwnerAsync(is_owner.GetCallback());
  EXPECT_FALSE(is_owner.Get());

  EXPECT_TRUE(service_->IsReady());
  EXPECT_EQ(service_->IsOwner(), is_owner.Get());
}

// Test that OwnerSettingsService can successfully finish the key loading flow
// when owner only the public owner key exists and `IsReady()`, `IsOwner()`,
// `IsOwnerAsync()` methods return correct results.
TEST_F(OwnerSettingsServiceAshNoOwnerTest, LoadKeysPublicKeyOnly) {
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());

  EXPECT_FALSE(service_->IsReady());
  service_->OnTPMTokenReady();  // Trigger key load.

  base::test::TestFuture<bool> is_owner;
  service_->IsOwnerAsync(is_owner.GetCallback());
  EXPECT_FALSE(is_owner.Get());

  EXPECT_TRUE(service_->IsReady());
  EXPECT_EQ(service_->IsOwner(), is_owner.Get());
}

// Test that OwnerSettingsService can successfully finish the key loading flow
// when both keys exist and `IsReady()`, `IsOwner()`, `IsOwnerAsync()` methods
// return correct results.
TEST_F(OwnerSettingsServiceAshNoOwnerTest, LoadKeysBothKeys) {
  owner_key_util_->ImportPrivateKeyAndSetPublicKey(
      device_policy_->GetSigningKey());

  EXPECT_FALSE(service_->IsReady());
  service_->OnTPMTokenReady();  // Trigger key load.

  base::test::TestFuture<bool> is_owner;
  service_->IsOwnerAsync(is_owner.GetCallback());
  EXPECT_TRUE(is_owner.Get());

  EXPECT_TRUE(service_->IsReady());
  EXPECT_EQ(service_->IsOwner(), is_owner.Get());
}

}  // namespace ash
