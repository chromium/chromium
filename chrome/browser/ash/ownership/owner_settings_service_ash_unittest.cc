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
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/ownership/owner_key_loader.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/ownership/ownership_histograms.h"
#include "chrome/browser/ash/settings/device_settings_provider.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/net/fake_nss_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/ownership/owner_settings_service.h"
#include "crypto/nss_key_util.h"
#include "crypto/signature_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;
using base::Bucket;
using testing::ElementsAre;

namespace ash {

namespace {

const char kUserAllowlist[] = "*@allowlist-domain.com";
const char kOther[] = "other";

const char kListStr1[] = "abcdef1234";
const char kListStr2[] = "xyz,.[];'";

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
  raw_ptr<OwnerSettingsServiceAsh> service_;
  raw_ptr<DeviceSettingsProvider> provider_;
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

    // By default disable the migration, so the imported key doesn't get
    // replaced.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{kStoreOwnerKeyInPrivateSlot},
        /*disabled_features=*/{kMigrateOwnerKeyToPrivateSlot});

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

  void AddObserverForSetting(const std::string& setting) const {
    service_->AddObserver(static_cast<DeviceSettingsProvider*>(
        CrosSettings::Get()->GetProvider(setting)));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<OwnerSettingsServiceAsh, DanglingUntriaged> service_ = nullptr;
  ScopedTestingLocalState local_state_;
  std::unique_ptr<DeviceSettingsProvider> provider_;
  base::ScopedPathOverride user_data_dir_override_;
  bool management_settings_set_ = false;
  base::HistogramTester histogram_tester_;
};

TEST_F(OwnerSettingsServiceAshTest, SingleSetTestSHA1) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      ownership::kOwnerSettingsWithSha256);

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

TEST_F(OwnerSettingsServiceAshTest, SingleSetTestSHA256) {
  base::test::ScopedFeatureList scoped_feature_list(
      ownership::kOwnerSettingsWithSha256);

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

TEST_F(OwnerSettingsServiceAshTest, DeviceExtendedAutoUpdateEnabledSetValue) {
  device_policy_->payload().clear_deviceextendedautoupdateenabled();
  ASSERT_FALSE(device_policy_->payload().has_deviceextendedautoupdateenabled());

  OwnerSettingsServiceAsh::UpdateDeviceSettings(
      kDeviceExtendedAutoUpdateEnabled, base::Value(true),
      device_policy_->payload());

  EXPECT_TRUE(
      device_policy_->payload().deviceextendedautoupdateenabled().value());
}

TEST_F(OwnerSettingsServiceAshTest,
       DeviceExtendedAutoUpdateEnabledSetValueWithPreviouslySet) {
  device_policy_->payload()
      .mutable_deviceextendedautoupdateenabled()
      ->set_value(false);
  ASSERT_FALSE(
      device_policy_->payload().deviceextendedautoupdateenabled().value());

  OwnerSettingsServiceAsh::UpdateDeviceSettings(
      kDeviceExtendedAutoUpdateEnabled, base::Value(true),
      device_policy_->payload());

  EXPECT_TRUE(
      device_policy_->payload().deviceextendedautoupdateenabled().value());
}

// Test that OwnerSettingsServiceAsh can successfully sign a policy with SHA1
// and that the signature is correct.
TEST_F(OwnerSettingsServiceAshTest, SignPolicySuccessSHA1) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      ownership::kOwnerSettingsWithSha256);

  auto policy = std::make_unique<enterprise_management::PolicyData>();
  policy->set_username("username0");

  base::test::TestFuture<
      scoped_refptr<ownership::PublicKey>,
      std::unique_ptr<enterprise_management::PolicyFetchResponse>>
      result_waiter;
  EXPECT_TRUE(service_->AssembleAndSignPolicyAsync(
      base::SequencedTaskRunner::GetCurrentDefault().get(), std::move(policy),
      result_waiter.GetCallback()));

  scoped_refptr<ownership::PublicKey> pub_key = result_waiter.Get<0>();
  const std::unique_ptr<enterprise_management::PolicyFetchResponse>&
      signed_policy = result_waiter.Get<1>();
  EXPECT_TRUE(signed_policy);

  crypto::SignatureVerifier signature_verifier;
  ASSERT_TRUE(signature_verifier.VerifyInit(
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1,
      base::as_bytes(base::make_span(signed_policy->policy_data_signature())),
      pub_key->data()));
  signature_verifier.VerifyUpdate(
      base::as_bytes(base::make_span(signed_policy->policy_data())));
  EXPECT_TRUE(signature_verifier.VerifyFinal());
}

// Test that OwnerSettingsServiceAsh can successfully sign a policy with SHA256
// and that the signature is correct.
TEST_F(OwnerSettingsServiceAshTest, SignPolicySuccessSHA256) {
  base::test::ScopedFeatureList scoped_feature_list(
      ownership::kOwnerSettingsWithSha256);

  auto policy = std::make_unique<enterprise_management::PolicyData>();
  policy->set_username("username0");

  base::test::TestFuture<
      scoped_refptr<ownership::PublicKey>,
      std::unique_ptr<enterprise_management::PolicyFetchResponse>>
      result_waiter;
  EXPECT_TRUE(service_->AssembleAndSignPolicyAsync(
      base::SequencedTaskRunner::GetCurrentDefault().get(), std::move(policy),
      result_waiter.GetCallback()));

  auto pub_key = result_waiter.Get<scoped_refptr<ownership::PublicKey>>();
  const auto& signed_policy =
      result_waiter
          .Get<std::unique_ptr<enterprise_management::PolicyFetchResponse>>();
  ASSERT_TRUE(signed_policy);

  crypto::SignatureVerifier signature_verifier;
  ASSERT_TRUE(signature_verifier.VerifyInit(
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256,
      base::as_bytes(base::make_span(signed_policy->policy_data_signature())),
      pub_key->data()));
  signature_verifier.VerifyUpdate(
      base::as_bytes(base::make_span(signed_policy->policy_data())));
  EXPECT_TRUE(signature_verifier.VerifyFinal());
}

// Test that OwnerSettingsServiceAsh correctly fails when it cannot sign
// policies.
TEST_F(OwnerSettingsServiceAshTest, SignPolicyFailure) {
  // Generate a new key and set it. 256 bits is not enough to perform SHA-1, so
  // it will cause a failure.
  crypto::ScopedSECKEYPublicKey public_key_nss;
  crypto::ScopedSECKEYPrivateKey private_key_nss;
  crypto::GenerateRSAKeyPairNSS(PK11_GetInternalSlot(), 256,
                                /*permanent=*/false, &public_key_nss,
                                &private_key_nss);
  scoped_refptr<ownership::PrivateKey> private_key =
      base::MakeRefCounted<ownership::PrivateKey>(std::move(private_key_nss));
  service_->SetPrivateKeyForTesting(private_key);

  auto policy = std::make_unique<enterprise_management::PolicyData>();
  policy->set_username("username0");

  base::test::TestFuture<
      scoped_refptr<ownership::PublicKey>,
      std::unique_ptr<enterprise_management::PolicyFetchResponse>>
      result_waiter;
  EXPECT_TRUE(service_->AssembleAndSignPolicyAsync(
      base::SequencedTaskRunner::GetCurrentDefault().get(), std::move(policy),
      result_waiter.GetCallback()));

  const std::unique_ptr<enterprise_management::PolicyFetchResponse>&
      signed_policy = result_waiter.Get<1>();
  EXPECT_FALSE(signed_policy);
}

// Testing list operations.

TEST_F(OwnerSettingsServiceAshTest, RemoveNonExistentElement) {
  AddObserverForSetting(kFeatureFlags);
  EXPECT_EQ(provider_->Get(kFeatureFlags), nullptr);
  EXPECT_TRUE(service_->RemoveFromList(kFeatureFlags, base::Value(kListStr1)));
  FlushDeviceSettings();
  EXPECT_EQ(provider_->Get(kFeatureFlags), nullptr);
}

// Append 1 item to an empty list.
TEST_F(OwnerSettingsServiceAshTest, AppendList) {
  AddObserverForSetting(kFeatureFlags);
  EXPECT_EQ(provider_->Get(kFeatureFlags), nullptr);
  EXPECT_TRUE(service_->AppendToList(kFeatureFlags, base::Value(kListStr1)));
  FlushDeviceSettings();
  auto expected_list = base::Value::List().Append(kListStr1);
  EXPECT_EQ(provider_->Get(kFeatureFlags)->Clone(), expected_list);
}

// Append two item to a list.
TEST_F(OwnerSettingsServiceAshTest, TwoAppendToList) {
  AddObserverForSetting(kFeatureFlags);
  EXPECT_TRUE(service_->AppendToList(kFeatureFlags, base::Value(kListStr1)));
  EXPECT_TRUE(service_->AppendToList(kFeatureFlags, base::Value(kListStr2)));
  FlushDeviceSettings();
  auto expected_list = base::Value::List().Append(kListStr1).Append(kListStr2);
  EXPECT_EQ(provider_->Get(kFeatureFlags)->Clone(), expected_list);
}

// Append the same item two times.
TEST_F(OwnerSettingsServiceAshTest, AppendSameItemTwiceToList) {
  AddObserverForSetting(kFeatureFlags);
  EXPECT_EQ(provider_->Get(kFeatureFlags), nullptr);
  EXPECT_TRUE(service_->AppendToList(kFeatureFlags, base::Value(kListStr2)));
  EXPECT_TRUE(service_->AppendToList(kFeatureFlags, base::Value(kListStr2)));
  FlushDeviceSettings();
  auto expected_list = base::Value::List().Append(kListStr2).Append(kListStr2);
  EXPECT_EQ(provider_->Get(kFeatureFlags)->Clone(), expected_list);
}

// Remove and append 1 item to an empty list.
TEST_F(OwnerSettingsServiceAshTest, RemoveAndAppendList) {
  AddObserverForSetting(kFeatureFlags);
  EXPECT_EQ(provider_->Get(kFeatureFlags), nullptr);
  EXPECT_TRUE(service_->RemoveFromList(kFeatureFlags, base::Value(kListStr1)));
  EXPECT_TRUE(service_->AppendToList(kFeatureFlags, base::Value(kListStr1)));
  FlushDeviceSettings();
  auto expected_list = base::Value::List().Append(kListStr1);
  EXPECT_EQ(provider_->Get(kFeatureFlags)->Clone(), expected_list);
}

// Append and remove the same item.
TEST_F(OwnerSettingsServiceAshTest, AppendAndRemove1) {
  AddObserverForSetting(kFeatureFlags);
  EXPECT_EQ(provider_->Get(kFeatureFlags), nullptr);
  EXPECT_TRUE(service_->AppendToList(kFeatureFlags, base::Value(kListStr1)));
  EXPECT_TRUE(service_->RemoveFromList(kFeatureFlags, base::Value(kListStr1)));
  FlushDeviceSettings();
  EXPECT_EQ(provider_->Get(kFeatureFlags), nullptr);
}

// Append and remove different items.
TEST_F(OwnerSettingsServiceAshTest, AppendAndRemove2) {
  AddObserverForSetting(kFeatureFlags);
  EXPECT_EQ(provider_->Get(kFeatureFlags), nullptr);
  EXPECT_TRUE(service_->AppendToList(kFeatureFlags, base::Value(kListStr1)));
  EXPECT_TRUE(service_->RemoveFromList(kFeatureFlags, base::Value(kListStr2)));
  FlushDeviceSettings();
  auto expected_list = base::Value::List().Append(kListStr1);
  EXPECT_EQ(provider_->Get(kFeatureFlags)->Clone(), expected_list);
}

// Append two item to and remove the first from the list.
TEST_F(OwnerSettingsServiceAshTest, TwoAppendAndRemoveList) {
  AddObserverForSetting(kFeatureFlags);
  EXPECT_EQ(provider_->Get(kFeatureFlags), nullptr);
  EXPECT_TRUE(service_->AppendToList(kFeatureFlags, base::Value(kListStr1)));
  EXPECT_TRUE(service_->AppendToList(kFeatureFlags, base::Value(kListStr2)));
  EXPECT_TRUE(service_->RemoveFromList(kFeatureFlags, base::Value(kListStr1)));
  FlushDeviceSettings();
  auto expected_list = base::Value::List().Append(kListStr2);
  EXPECT_EQ(provider_->Get(kFeatureFlags)->Clone(), expected_list);
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

    // By default disable the migration, so the imported key doesn't get
    // replaced.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{kStoreOwnerKeyInPrivateSlot},
        /*disabled_features=*/{kMigrateOwnerKeyToPrivateSlot});

    provider_ = std::make_unique<DeviceSettingsProvider>(
        base::BindRepeating(&OnPrefChanged), device_settings_service_.get(),
        TestingBrowserProcess::GetGlobal()->local_state());
    FlushDeviceSettings();
    service_ =
        OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_.get());
    ASSERT_TRUE(service_);
    ASSERT_FALSE(service_->IsOwner());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
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

// Test that the old owner key gets cleaned up after the new one is installed by
// session manager.
TEST_F(OwnerSettingsServiceAshNoOwnerTest, CleanUpOldOwnerKey) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kStoreOwnerKeyInPrivateSlot,
                            kMigrateOwnerKeyToPrivateSlot},
      /*disabled_features=*/{});

  FakeNssService* nss_service = FakeNssService::InitializeForBrowserContext(
      profile_.get(), /*enable_system_slot=*/false);
  owner_key_util_->ImportPrivateKeyInSlotAndSetPublicKey(
      device_policy_->GetSigningKey(), nss_service->GetPublicSlot());

  EXPECT_FALSE(service_->IsReady());
  service_->OnTPMTokenReady();  // Trigger key load.

  base::test::TestFuture<bool> is_owner;
  service_->IsOwnerAsync(is_owner.GetCallback());
  EXPECT_TRUE(is_owner.Get());

  // Check that the old key is not deleted too early.
  task_environment_.RunUntilIdle();
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
      BucketsInclude(Bucket(OwnerKeyUmaEvent::kOldOwnerKeyCleanUpStarted, 0)));

  service_->OwnerKeySet(/*success=*/true);

  task_environment_.RunUntilIdle();

  EXPECT_THAT(histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
              BucketsInclude(
                  Bucket(OwnerKeyUmaEvent::kMigrationToPrivateSlotStarted, 1),
                  Bucket(OwnerKeyUmaEvent::kOwnerKeySetSuccess, 1),
                  Bucket(OwnerKeyUmaEvent::kOldOwnerKeyCleanUpStarted, 1)));
}

}  // namespace ash
