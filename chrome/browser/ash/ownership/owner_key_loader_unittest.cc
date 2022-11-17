// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/ownership/owner_key_loader.h"

#include <memory>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/ownership/ownership_histograms.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/net/fake_nss_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/rsa_private_key.h"
#include "testing/gtest/include/gtest/gtest.h"

using PublicKeyRefPtr = scoped_refptr<ownership::PublicKey>;
using PrivateKeyRefPtr = scoped_refptr<ownership::PrivateKey>;
using base::Bucket;
using testing::ElementsAre;

namespace ash {

std::vector<uint8_t> ExtractBytes(
    const std::unique_ptr<crypto::RSAPrivateKey>& key) {
  std::vector<uint8_t> bytes;
  key->ExportPublicKey(&bytes);
  return bytes;
}

class OwnerKeyLoaderTest : public testing::Test {
 public:
  // testing::Test:
  void SetUp() override {
    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    user_manager_ = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    user_manager_->set_local_state(&local_state_);
    user_manager_->RegisterPrefs(local_state_.registry());

    owner_key_util_ = base::MakeRefCounted<ownership::MockOwnerKeyUtil>();

    device_settings_service_.SetSessionManager(&session_manager_client_,
                                               owner_key_util_);

    profile_ = TestingProfile::Builder().Build();

    FakeNssService::InitializeForBrowserContext(profile_.get(),
                                                /*enable_system_slot=*/false);

    key_loader_ = std::make_unique<OwnerKeyLoader>(
        profile_.get(), &device_settings_service_, owner_key_util_,
        /*is_enterprise_enrolled=*/false, result_observer_.GetCallback());
  }

 protected:
  std::unique_ptr<crypto::RSAPrivateKey> ConfigureExistingPolicies(
      const std::string& owner_username) {
    // The actual content of the policies doesn't matter, OwnerKeyLoader only
    // looks at the username that created them (i.e. at the user that was
    // recognised as the owner when they were created).
    policy::DevicePolicyBuilder policy_builder;
    policy_builder.policy_data().set_username(owner_username);
    policy_builder.Build();
    session_manager_client_.set_device_policy(policy_builder.GetBlob());
    return policy_builder.GetSigningKey();
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;

  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  user_manager::FakeUserManager* user_manager_ = nullptr;

  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_;
  FakeSessionManagerClient session_manager_client_;
  std::unique_ptr<TestingProfile> profile_;
  ash::DeviceSettingsService device_settings_service_;
  std::unique_ptr<OwnerKeyLoader> key_loader_;
  base::test::TestFuture<PublicKeyRefPtr, PrivateKeyRefPtr> result_observer_;
  base::HistogramTester histogram_tester_;
};

// Test that the first user generates a new owner key.
TEST_F(OwnerKeyLoaderTest, FirstUserGeneratesOwnerKey) {
  // In real code DeviceSettingsService must call this for the first user.
  device_settings_service_.MarkWillEstablishConsumerOwnership();
  // Do not prepare any keys, so key_loader_ has to generate a new one.

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  EXPECT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  ASSERT_TRUE(result_observer_.Get<PrivateKeyRefPtr>());
  EXPECT_TRUE(result_observer_.Get<PrivateKeyRefPtr>()->key());

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
      ElementsAre(
          Bucket(OwnerKeyUmaEvent::kEstablishingConsumerOwnershipSuccess, 1),
          Bucket(OwnerKeyUmaEvent::kOwnerKeyGeneratedSuccess, 1)));
}

// Test that the first user generates owner key after a crash. If during the
// first attempt Chrome crashes, on the next launch DeviceSettingsService won't
// indicate again that the consumer owners needs to be established.
// In such a case OwnerKeyLoader should read the identity of the owner from
// local state.
TEST_F(OwnerKeyLoaderTest, FirstUserGeneratesOwnerKeyAfterCrash) {
  // Populate local state data that OwnerKeyLoader should read.
  user_manager_->RecordOwner(
      AccountId::FromUserEmail(profile_->GetProfileUserName()));

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  EXPECT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  ASSERT_TRUE(result_observer_.Get<PrivateKeyRefPtr>());
  EXPECT_TRUE(result_observer_.Get<PrivateKeyRefPtr>()->key());

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
      ElementsAre(
          Bucket(
              OwnerKeyUmaEvent::kRegeneratingOwnerKeyBasedOnLocalStateSuccess,
              1),
          Bucket(OwnerKeyUmaEvent::kOwnerKeyGeneratedSuccess, 1)));
}

// Test that the second user doesn't try to generate a new owner key.
TEST_F(OwnerKeyLoaderTest, SecondUserDoesNotTakeOwnership) {
  // In real code the first user would have created some device policies and
  // saved the public owner key on disk. Emulate that.
  auto signing_key = ConfigureExistingPolicies("owner@example.com");
  owner_key_util_->SetPublicKeyFromPrivateKey(*signing_key);
  device_settings_service_.LoadImmediately();  // Reload policies.

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  ASSERT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  EXPECT_EQ(result_observer_.Get<PublicKeyRefPtr>()->data(),
            ExtractBytes(signing_key));
  EXPECT_FALSE(result_observer_.Get<PrivateKeyRefPtr>());

  EXPECT_THAT(histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
              ElementsAre(Bucket(
                  OwnerKeyUmaEvent::kUserNotAnOwnerBasedOnPolicySuccess, 1)));
}

// Test that an owner user gets recognized as the owner when it's mentioned in
// the existing device policies and owns the key.
TEST_F(OwnerKeyLoaderTest, OwnerUserLoadsExistingKey) {
  // Configure existing device policies and the owner key.
  auto signing_key = ConfigureExistingPolicies(profile_->GetProfileUserName());
  owner_key_util_->ImportPrivateKeyAndSetPublicKey(signing_key->Copy());
  device_settings_service_.LoadImmediately();  // Reload policies.

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  ASSERT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  EXPECT_EQ(result_observer_.Get<PublicKeyRefPtr>()->data(),
            ExtractBytes(signing_key));
  ASSERT_TRUE(result_observer_.Get<PrivateKeyRefPtr>());
  EXPECT_TRUE(result_observer_.Get<PrivateKeyRefPtr>()->key());

  EXPECT_THAT(histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
              ElementsAre(Bucket(OwnerKeyUmaEvent::kOwnerHasKeysSuccess, 1)));
}

// Test that even without existing device policies the owner key gets loaded
// (that will help Chrome to recognize the current user as the owner).
TEST_F(OwnerKeyLoaderTest, OwnerUserLoadsExistingKeyWithoutPolicies) {
  policy::DevicePolicyBuilder policy_builder;
  auto signing_key = policy_builder.GetSigningKey();

  owner_key_util_->ImportPrivateKeyAndSetPublicKey(signing_key->Copy());

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  ASSERT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  EXPECT_EQ(result_observer_.Get<PublicKeyRefPtr>()->data(),
            ExtractBytes(signing_key));
  ASSERT_TRUE(result_observer_.Get<PrivateKeyRefPtr>());
  EXPECT_TRUE(result_observer_.Get<PrivateKeyRefPtr>()->key());

  EXPECT_THAT(histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
              ElementsAre(Bucket(OwnerKeyUmaEvent::kOwnerHasKeysSuccess, 1)));
}

// Test that the second user is not falsely recognized as the owner even if
// policies fail to load and does not have the owner key.
TEST_F(OwnerKeyLoaderTest, SecondaryUserWithoutPolicies) {
  policy::DevicePolicyBuilder policy_builder;
  auto signing_key = policy_builder.GetSigningKey();

  owner_key_util_->SetPublicKeyFromPrivateKey(*signing_key);

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  ASSERT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  EXPECT_EQ(result_observer_.Get<PublicKeyRefPtr>()->data(),
            ExtractBytes(signing_key));
  EXPECT_FALSE(result_observer_.Get<PrivateKeyRefPtr>());

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
      ElementsAre(Bucket(OwnerKeyUmaEvent::kUnsureUserNotAnOwnerSuccess, 1)));
}

// Test that an owner user still gets recognized as the owner when it's
// mentioned in the existing device policies, but the owner key was lost.
// The key must be re-generated in such a case.
TEST_F(OwnerKeyLoaderTest, OwnerUserRegeneratesMissingKeyBasedOnPolicies) {
  auto signing_key = ConfigureExistingPolicies(profile_->GetProfileUserName());
  // Configure that the public key is on disk, but the private key doesn't
  // exist.
  owner_key_util_->SetPublicKeyFromPrivateKey(*signing_key);
  device_settings_service_.LoadImmediately();  // Reload policies.

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  ASSERT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  EXPECT_NE(result_observer_.Get<PublicKeyRefPtr>()->data(),
            ExtractBytes(signing_key));
  ASSERT_TRUE(result_observer_.Get<PrivateKeyRefPtr>());
  EXPECT_TRUE(result_observer_.Get<PrivateKeyRefPtr>()->key());

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
      ElementsAre(
          Bucket(OwnerKeyUmaEvent::kRegeneratingOwnerKeyBasedOnPolicySuccess,
                 1),
          Bucket(OwnerKeyUmaEvent::kOwnerKeyGeneratedSuccess, 1)));
}

// Test that an owner user still gets recognized as the owner when it's
// mentioned in the local state, but the device policies and the owner key were
// lost. The key must be re-generated in such a case.
TEST_F(OwnerKeyLoaderTest, OwnerUserRegeneratesMissingKeyBasedOnLocalState) {
  // Populate local state.
  user_manager_->RecordOwner(
      AccountId::FromUserEmail(profile_->GetProfileUserName()));

  policy::DevicePolicyBuilder policy_builder;
  auto signing_key = policy_builder.GetSigningKey();

  owner_key_util_->SetPublicKeyFromPrivateKey(*signing_key);

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  ASSERT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  EXPECT_NE(result_observer_.Get<PublicKeyRefPtr>()->data(),
            ExtractBytes(signing_key));
  ASSERT_TRUE(result_observer_.Get<PrivateKeyRefPtr>());
  EXPECT_TRUE(result_observer_.Get<PrivateKeyRefPtr>()->key());

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
      ElementsAre(
          // "Fail" means that the existence of the public key is unexpected.
          Bucket(OwnerKeyUmaEvent::kRegeneratingOwnerKeyBasedOnLocalStateFail,
                 1),
          Bucket(OwnerKeyUmaEvent::kOwnerKeyGeneratedSuccess, 1)));
}

// Test that OwnerKeyLoader makes several attempts to generate the owner key
// pair.
TEST_F(OwnerKeyLoaderTest, KeyGenerationRetriedSuccessfully) {
  // Make key_loader_ generate the key for the first user.
  device_settings_service_.MarkWillEstablishConsumerOwnership();
  owner_key_util_->SimulateGenerateKeyFailure(/*fail_times=*/5);

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  ASSERT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  ASSERT_TRUE(result_observer_.Get<PrivateKeyRefPtr>());
  EXPECT_TRUE(result_observer_.Get<PrivateKeyRefPtr>()->key());

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
      ElementsAre(
          Bucket(OwnerKeyUmaEvent::kEstablishingConsumerOwnershipSuccess, 1),
          // "Fail" means that there were generation errors before it succeeded.
          Bucket(OwnerKeyUmaEvent::kOwnerKeyGeneratedFail, 1)));
}

// Test that OwnerKeyLoader gives up to generate the owner key pair after a
// certain amount of attempts.
TEST_F(OwnerKeyLoaderTest, KeyGenerationRetriedUnsuccessfully) {
  // Make key_loader_ generate the key for the first user.
  device_settings_service_.MarkWillEstablishConsumerOwnership();
  owner_key_util_->SimulateGenerateKeyFailure(/*fail_times=*/10);

  key_loader_->Run();

  EXPECT_FALSE(result_observer_.Get<PublicKeyRefPtr>());
  EXPECT_FALSE(result_observer_.Get<PrivateKeyRefPtr>());

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
      ElementsAre(
          Bucket(OwnerKeyUmaEvent::kEstablishingConsumerOwnershipSuccess, 1),
          Bucket(OwnerKeyUmaEvent::kFailedToGenerateOwnerKeyFail, 1)));
}

// Test that enterprise devices don't attempt to load private key. The signing
// key of the device policy is owned by the backend server in the
// enterprise-enrolled case.
TEST_F(OwnerKeyLoaderTest, EnterpriseDevicesDontNeedPrivateKey) {
  // Create favorable conditions for the code to load private key. Claim in the
  // device policies that the user is the owner (shouldn't happen on a real
  // device) and prepare a private key in case OwnerKeyLoader tries to load it.
  auto signing_key = ConfigureExistingPolicies(profile_->GetProfileUserName());
  owner_key_util_->ImportPrivateKeyAndSetPublicKey(signing_key->Copy());

  // Re-create the loader with is_enterprise_enrolled=true.
  key_loader_ = std::make_unique<OwnerKeyLoader>(
      profile_.get(), &device_settings_service_, owner_key_util_,
      /*is_enterprise_enrolled=*/true, result_observer_.GetCallback());

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  ASSERT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  // Check that the private key wasn't loaded.
  EXPECT_FALSE(result_observer_.Get<PrivateKeyRefPtr>());

  EXPECT_THAT(histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
              ElementsAre(Bucket(OwnerKeyUmaEvent::kManagedDeviceSuccess, 1)));
}

}  // namespace ash
