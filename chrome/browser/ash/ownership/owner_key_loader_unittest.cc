// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/ownership/owner_key_loader.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/ownership/ownership_histograms.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/net/fake_nss_service.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/ownership/owner_key_util_impl.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/rsa_private_key.h"
#include "testing/gtest/include/gtest/gtest.h"

using PublicKeyRefPtr = scoped_refptr<ownership::PublicKey>;
using PrivateKeyRefPtr = scoped_refptr<ownership::PrivateKey>;
using base::Bucket;

namespace ash {

constexpr char kUserEmail[] = "user@example.com";

std::vector<uint8_t> ExtractSpkiDer(
    const std::unique_ptr<crypto::RSAPrivateKey>& key) {
  std::vector<uint8_t> bytes;
  key->ExportPublicKey(&bytes);
  return bytes;
}

std::vector<uint8_t> ExtractSpkiDer(const crypto::ScopedSECKEYPrivateKey& key) {
  crypto::ScopedSECKEYPublicKey public_key(
      SECKEY_ConvertToPublicKey(key.get()));

  SECItem* public_key_bytes = PK11_DEREncodePublicKey(public_key.get());
  std::vector<uint8_t> bytes(public_key_bytes->data,
                             public_key_bytes->data + public_key_bytes->len);
  SECITEM_FreeItem(public_key_bytes, PR_TRUE);

  return bytes;
}

class OwnerKeyLoaderTestBase : public testing::Test {
 public:
  explicit OwnerKeyLoaderTestBase(user_manager::UserType user_type)
      : user_type_(user_type) {}

  // testing::Test:
  void SetUp() override {
    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    user_manager_ = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    owner_key_util_ = base::MakeRefCounted<ownership::MockOwnerKeyUtil>();

    device_settings_service_.SetSessionManager(&session_manager_client_,
                                               owner_key_util_);

    profile_ = TestingProfile::Builder().SetProfileName(kUserEmail).Build();

    user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        AccountId::FromUserEmail(kUserEmail), /*is_affiliated=*/false,
        user_type_, profile_.get());

    nss_service_ = FakeNssService::InitializeForBrowserContext(
        profile_.get(),
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

  // Checks whether the private key for `public_key_spki` is in the `slot`.
  bool IsKeyInSlot(const std::vector<uint8_t> public_key_spki,
                   PK11SlotInfo* slot) {
    scoped_refptr<ownership::OwnerKeyUtil> key_util =
        base::MakeRefCounted<ownership::OwnerKeyUtilImpl>(
            /*public_key_file=*/base::FilePath());
    return bool(key_util->FindPrivateKeyInSlot(public_key_spki, slot));
  }

  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState scoped_local_state_{
      TestingBrowserProcess::GetGlobal()};

  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  raw_ptr<ash::FakeChromeUserManager> user_manager_ = nullptr;

  const user_manager::UserType user_type_;
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_;
  FakeSessionManagerClient session_manager_client_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<FakeNssService> nss_service_ = nullptr;
  ash::DeviceSettingsService device_settings_service_;
  std::unique_ptr<OwnerKeyLoader> key_loader_;
  base::test::TestFuture<PublicKeyRefPtr, PrivateKeyRefPtr> result_observer_;
  base::HistogramTester histogram_tester_;
};

class RegularOwnerKeyLoaderTest : public OwnerKeyLoaderTestBase {
 public:
  RegularOwnerKeyLoaderTest()
      : OwnerKeyLoaderTestBase(user_manager::UserType::kRegular) {}
};

// Test that the first user generates a new owner key in the public slot (when
// the user is a regular user and the related experiment is disabled).
TEST_F(RegularOwnerKeyLoaderTest, FirstUserGeneratesOwnerKeyInPublicSlot) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kStoreOwnerKeyInPrivateSlot,
                             kMigrateOwnerKeyToPrivateSlot});

  // In real code DeviceSettingsService must call this for the first user.
  device_settings_service_.MarkWillEstablishConsumerOwnership();
  // Do not prepare any keys, so key_loader_ has to generate a new one.

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  EXPECT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  ASSERT_TRUE(result_observer_.Get<PrivateKeyRefPtr>());
  EXPECT_TRUE(result_observer_.Get<PrivateKeyRefPtr>()->key());
  EXPECT_TRUE(IsKeyInSlot(result_observer_.Get<PublicKeyRefPtr>()->data(),
                          nss_service_->GetPublicSlot()));

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
      BucketsInclude(
          Bucket(OwnerKeyUmaEvent::kEstablishingConsumerOwnershipSuccess, 1),
          Bucket(OwnerKeyUmaEvent::kOwnerKeyGeneratedSuccess, 1),
          Bucket(OwnerKeyUmaEvent::kPublicSlotKeyGenerationSuccess, 1)));
}

// Test that the first user generates a new owner key in the private slot (when
// the user is a regular user and the related experiment is enabled).
TEST_F(RegularOwnerKeyLoaderTest, FirstUserGeneratesOwnerKeyInPrivateSlot) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kStoreOwnerKeyInPrivateSlot},
      /*disabled_features=*/{kMigrateOwnerKeyToPrivateSlot});

  // In real code DeviceSettingsService must call this for the first user.
  device_settings_service_.MarkWillEstablishConsumerOwnership();
  // Do not prepare any keys, so key_loader_ has to generate a new one.

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  EXPECT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  ASSERT_TRUE(result_observer_.Get<PrivateKeyRefPtr>());
  EXPECT_TRUE(result_observer_.Get<PrivateKeyRefPtr>()->key());
  EXPECT_TRUE(IsKeyInSlot(result_observer_.Get<PublicKeyRefPtr>()->data(),
                          nss_service_->GetPrivateSlot()));

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
      BucketsInclude(
          Bucket(OwnerKeyUmaEvent::kEstablishingConsumerOwnershipSuccess, 1),
          Bucket(OwnerKeyUmaEvent::kOwnerKeyGeneratedSuccess, 1),
          Bucket(OwnerKeyUmaEvent::kPrivateSlotKeyGenerationSuccess, 1)));
}

// Test that the first user generates owner key after a crash. If during the
// first attempt Chrome crashes, on the next launch DeviceSettingsService won't
// indicate again that the consumer owners needs to be established.
// In such a case OwnerKeyLoader should read the identity of the owner from
// local state.
TEST_F(RegularOwnerKeyLoaderTest, FirstUserGeneratesOwnerKeyAfterCrash) {
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
      BucketsInclude(
          Bucket(
              OwnerKeyUmaEvent::kRegeneratingOwnerKeyBasedOnLocalStateSuccess,
              1),
          Bucket(OwnerKeyUmaEvent::kOwnerKeyGeneratedSuccess, 1)));
}

// Test that the second user doesn't try to generate a new owner key.
TEST_F(RegularOwnerKeyLoaderTest, SecondUserDoesNotTakeOwnership) {
  // In real code the first user would have created some device policies and
  // saved the public owner key on disk. Emulate that.
  auto signing_key = ConfigureExistingPolicies("owner@example.com");
  owner_key_util_->SetPublicKeyFromPrivateKey(*signing_key);
  device_settings_service_.LoadImmediately();  // Reload policies.

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  ASSERT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  EXPECT_EQ(result_observer_.Get<PublicKeyRefPtr>()->data(),
            ExtractSpkiDer(signing_key));
  EXPECT_FALSE(result_observer_.Get<PrivateKeyRefPtr>());

  EXPECT_THAT(histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
              BucketsInclude(Bucket(
                  OwnerKeyUmaEvent::kUserNotAnOwnerBasedOnPolicySuccess, 1)));
}

// Test that an owner user gets recognized as the owner when it's mentioned in
// the existing device policies and owns the key in the public slot.
TEST_F(RegularOwnerKeyLoaderTest, OwnerUserLoadsExistingKeyFromPublicSlot) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kStoreOwnerKeyInPrivateSlot},
      /*disabled_features=*/{kMigrateOwnerKeyToPrivateSlot});

  // Configure existing device policies and the owner key.
  auto signing_key = ConfigureExistingPolicies(profile_->GetProfileUserName());
  owner_key_util_->ImportPrivateKeyInSlotAndSetPublicKey(
      signing_key->Copy(), nss_service_->GetPublicSlot());
  device_settings_service_.LoadImmediately();  // Reload policies.

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  ASSERT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  EXPECT_EQ(result_observer_.Get<PublicKeyRefPtr>()->data(),
            ExtractSpkiDer(signing_key));
  ASSERT_TRUE(result_observer_.Get<PrivateKeyRefPtr>());
  EXPECT_TRUE(result_observer_.Get<PrivateKeyRefPtr>()->key());

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
      BucketsInclude(Bucket(OwnerKeyUmaEvent::kOwnerHasKeysSuccess, 1),
                     Bucket(OwnerKeyUmaEvent::kOwnerKeyInPublicSlotTrue, 1)));
}

// Test that an owner user gets recognized as the owner when it's mentioned in
// the existing device policies and owns the key in the private slot.
TEST_F(RegularOwnerKeyLoaderTest, OwnerUserLoadsExistingKeyFromPrivateSlot) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kStoreOwnerKeyInPrivateSlot},
      /*disabled_features=*/{kMigrateOwnerKeyToPrivateSlot});

  // Configure existing device policies and the owner key.
  auto signing_key = ConfigureExistingPolicies(profile_->GetProfileUserName());
  owner_key_util_->ImportPrivateKeyInSlotAndSetPublicKey(
      signing_key->Copy(), nss_service_->GetPrivateSlot());
  device_settings_service_.LoadImmediately();  // Reload policies.

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  ASSERT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  EXPECT_EQ(result_observer_.Get<PublicKeyRefPtr>()->data(),
            ExtractSpkiDer(signing_key));
  ASSERT_TRUE(result_observer_.Get<PrivateKeyRefPtr>());
  EXPECT_TRUE(result_observer_.Get<PrivateKeyRefPtr>()->key());

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
      BucketsInclude(Bucket(OwnerKeyUmaEvent::kOwnerHasKeysSuccess, 1),
                     Bucket(OwnerKeyUmaEvent::kOwnerKeyInPublicSlotFalse, 1)));
}

// Test that even without existing device policies the owner key gets loaded
// from the public slot (that will help Chrome to recognize the current user as
// the owner).
TEST_F(RegularOwnerKeyLoaderTest,
       OwnerUserLoadsExistingKeyFromPublicSlotWithoutPolicies) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kStoreOwnerKeyInPrivateSlot},
      /*disabled_features=*/{kMigrateOwnerKeyToPrivateSlot});

  policy::DevicePolicyBuilder policy_builder;
  auto signing_key = policy_builder.GetSigningKey();

  owner_key_util_->ImportPrivateKeyInSlotAndSetPublicKey(
      signing_key->Copy(), nss_service_->GetPublicSlot());

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  ASSERT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  EXPECT_EQ(result_observer_.Get<PublicKeyRefPtr>()->data(),
            ExtractSpkiDer(signing_key));
  ASSERT_TRUE(result_observer_.Get<PrivateKeyRefPtr>());
  EXPECT_TRUE(result_observer_.Get<PrivateKeyRefPtr>()->key());

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
      BucketsInclude(Bucket(OwnerKeyUmaEvent::kOwnerHasKeysSuccess, 1)));
}

// Test that even without existing device policies the owner key gets loaded
// from the private slot (that will help Chrome to recognize the current user as
// the owner).
TEST_F(RegularOwnerKeyLoaderTest,
       OwnerUserLoadsExistingKeyFromPrivateSlotWithoutPolicies) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kStoreOwnerKeyInPrivateSlot},
      /*disabled_features=*/{kMigrateOwnerKeyToPrivateSlot});

  policy::DevicePolicyBuilder policy_builder;
  auto signing_key = policy_builder.GetSigningKey();

  owner_key_util_->ImportPrivateKeyInSlotAndSetPublicKey(
      signing_key->Copy(), nss_service_->GetPrivateSlot());

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  ASSERT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  EXPECT_EQ(result_observer_.Get<PublicKeyRefPtr>()->data(),
            ExtractSpkiDer(signing_key));
  ASSERT_TRUE(result_observer_.Get<PrivateKeyRefPtr>());
  EXPECT_TRUE(result_observer_.Get<PrivateKeyRefPtr>()->key());

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
      BucketsInclude(Bucket(OwnerKeyUmaEvent::kOwnerHasKeysSuccess, 1)));
}

// Test that the second user is not falsely recognized as the owner even if
// policies fail to load and does not have the owner key.
TEST_F(RegularOwnerKeyLoaderTest, SecondaryUserWithoutPolicies) {
  policy::DevicePolicyBuilder policy_builder;
  auto signing_key = policy_builder.GetSigningKey();

  owner_key_util_->SetPublicKeyFromPrivateKey(*signing_key);

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  ASSERT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  EXPECT_EQ(result_observer_.Get<PublicKeyRefPtr>()->data(),
            ExtractSpkiDer(signing_key));
  EXPECT_FALSE(result_observer_.Get<PrivateKeyRefPtr>());

  EXPECT_THAT(histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
              BucketsInclude(
                  Bucket(OwnerKeyUmaEvent::kUnsureUserNotAnOwnerSuccess, 1)));
}

// Test that an owner user still gets recognized as the owner when it's
// mentioned in the existing device policies, but the owner key was lost.
// The key must be re-generated in such a case.
TEST_F(RegularOwnerKeyLoaderTest,
       OwnerUserRegeneratesMissingKeyBasedOnPolicies) {
  auto signing_key = ConfigureExistingPolicies(profile_->GetProfileUserName());
  // Configure that the public key is on disk, but the private key doesn't
  // exist.
  owner_key_util_->SetPublicKeyFromPrivateKey(*signing_key);
  device_settings_service_.LoadImmediately();  // Reload policies.

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  ASSERT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  EXPECT_NE(result_observer_.Get<PublicKeyRefPtr>()->data(),
            ExtractSpkiDer(signing_key));
  ASSERT_TRUE(result_observer_.Get<PrivateKeyRefPtr>());
  EXPECT_TRUE(result_observer_.Get<PrivateKeyRefPtr>()->key());

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
      BucketsInclude(
          Bucket(OwnerKeyUmaEvent::kRegeneratingOwnerKeyBasedOnPolicySuccess,
                 1),
          Bucket(OwnerKeyUmaEvent::kOwnerKeyGeneratedSuccess, 1)));
}

// Test that an owner user still gets recognized as the owner when it's
// mentioned in the local state, but the device policies and the owner key were
// lost. The key must be re-generated in such a case.
TEST_F(RegularOwnerKeyLoaderTest,
       OwnerUserRegeneratesMissingKeyBasedOnLocalState) {
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
            ExtractSpkiDer(signing_key));
  ASSERT_TRUE(result_observer_.Get<PrivateKeyRefPtr>());
  EXPECT_TRUE(result_observer_.Get<PrivateKeyRefPtr>()->key());

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
      BucketsInclude(
          // "Fail" means that the existence of the public key is unexpected.
          Bucket(OwnerKeyUmaEvent::kRegeneratingOwnerKeyBasedOnLocalStateFail,
                 1),
          Bucket(OwnerKeyUmaEvent::kOwnerKeyGeneratedSuccess, 1)));
}

// Test that OwnerKeyLoader makes several attempts to generate the owner key
// pair.
TEST_F(RegularOwnerKeyLoaderTest, KeyGenerationRetriedSuccessfully) {
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
      BucketsInclude(
          Bucket(OwnerKeyUmaEvent::kEstablishingConsumerOwnershipSuccess, 1),
          // "Fail" means that there were generation errors before it succeeded.
          Bucket(OwnerKeyUmaEvent::kOwnerKeyGeneratedFail, 1)));
}

// Test that OwnerKeyLoader gives up to generate the owner key pair after a
// certain amount of attempts.
TEST_F(RegularOwnerKeyLoaderTest, KeyGenerationRetriedUnsuccessfully) {
  // Make key_loader_ generate the key for the first user.
  device_settings_service_.MarkWillEstablishConsumerOwnership();
  owner_key_util_->SimulateGenerateKeyFailure(/*fail_times=*/10);

  key_loader_->Run();

  EXPECT_FALSE(result_observer_.Get<PublicKeyRefPtr>());
  EXPECT_FALSE(result_observer_.Get<PrivateKeyRefPtr>());

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
      BucketsInclude(
          Bucket(OwnerKeyUmaEvent::kEstablishingConsumerOwnershipSuccess, 1),
          Bucket(OwnerKeyUmaEvent::kFailedToGenerateOwnerKeyFail, 1)));
}

// Test that enterprise devices don't attempt to load private key. The signing
// key of the device policy is owned by the backend server in the
// enterprise-enrolled case.
TEST_F(RegularOwnerKeyLoaderTest, EnterpriseDevicesDontNeedPrivateKey) {
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

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
      BucketsInclude(Bucket(OwnerKeyUmaEvent::kManagedDeviceSuccess, 1)));
}

// Test that the owner key from the public slot is migrated into the private
// slot when the feature flags is enabled.
TEST_F(RegularOwnerKeyLoaderTest, MigrateFromPublicToPrivateSlot) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kStoreOwnerKeyInPrivateSlot,
                            kMigrateOwnerKeyToPrivateSlot},
      /*disabled_features=*/{});

  // Configure existing device policies and the owner key.
  auto signing_key = ConfigureExistingPolicies(profile_->GetProfileUserName());
  owner_key_util_->ImportPrivateKeyInSlotAndSetPublicKey(
      signing_key->Copy(), nss_service_->GetPublicSlot());
  device_settings_service_.LoadImmediately();  // Reload policies.

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  ASSERT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  EXPECT_NE(result_observer_.Get<PublicKeyRefPtr>()->data(),
            ExtractSpkiDer(signing_key));
  EXPECT_EQ(ExtractSpkiDer(key_loader_->ExtractOldOwnerKey()),
            ExtractSpkiDer(signing_key));
  ASSERT_TRUE(result_observer_.Get<PrivateKeyRefPtr>());
  EXPECT_TRUE(result_observer_.Get<PrivateKeyRefPtr>()->key());
  EXPECT_TRUE(IsKeyInSlot(result_observer_.Get<PublicKeyRefPtr>()->data(),
                          nss_service_->GetPrivateSlot()));

  EXPECT_THAT(histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
              BucketsInclude(
                  Bucket(OwnerKeyUmaEvent::kOwnerHasKeysSuccess, 1),
                  Bucket(OwnerKeyUmaEvent::kOwnerKeyInPublicSlotTrue, 1),
                  Bucket(OwnerKeyUmaEvent::kOwnerKeyGeneratedSuccess, 1),
                  Bucket(OwnerKeyUmaEvent::kPrivateSlotKeyGenerationSuccess, 1),
                  Bucket(OwnerKeyUmaEvent::kMigrationToPrivateSlotStarted, 1)));
}

// Test that the owner key from the public slot is not migrated when the feature
// flag is disabled.
TEST_F(RegularOwnerKeyLoaderTest, NotMigratedFromPublicToPrivateSlot) {
  base::test::ScopedFeatureList feature_list;
  // With this config Chrome should generate new keys in the private slot, but
  // not migrate existing keys from the public slot.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kStoreOwnerKeyInPrivateSlot},
      /*disabled_features=*/{kMigrateOwnerKeyToPrivateSlot});

  // Configure existing device policies and the owner key.
  auto signing_key = ConfigureExistingPolicies(profile_->GetProfileUserName());
  owner_key_util_->ImportPrivateKeyInSlotAndSetPublicKey(
      signing_key->Copy(), nss_service_->GetPublicSlot());
  device_settings_service_.LoadImmediately();  // Reload policies.

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  ASSERT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  EXPECT_EQ(result_observer_.Get<PublicKeyRefPtr>()->data(),
            ExtractSpkiDer(signing_key));
  EXPECT_EQ(key_loader_->ExtractOldOwnerKey(), nullptr);
  ASSERT_TRUE(result_observer_.Get<PrivateKeyRefPtr>());
  EXPECT_TRUE(result_observer_.Get<PrivateKeyRefPtr>()->key());
  EXPECT_FALSE(IsKeyInSlot(result_observer_.Get<PublicKeyRefPtr>()->data(),
                           nss_service_->GetPrivateSlot()));

  EXPECT_THAT(histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
              BucketsInclude(
                  Bucket(OwnerKeyUmaEvent::kOwnerHasKeysSuccess, 1),
                  Bucket(OwnerKeyUmaEvent::kOwnerKeyInPublicSlotTrue, 1),
                  Bucket(OwnerKeyUmaEvent::kMigrationToPrivateSlotStarted, 0)));
}

// Test that the owner key from the private slot is not migrated back into the
// public slot when the feature flags are enabled.
TEST_F(RegularOwnerKeyLoaderTest, NotMigratedFromPrivateToPublicSlot) {
  base::test::ScopedFeatureList feature_list;
  // With this config Chrome should generate new keys in the private slot, but
  // not migrate existing keys from the public slot.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kStoreOwnerKeyInPrivateSlot,
                            kMigrateOwnerKeyToPrivateSlot},
      /*disabled_features=*/{});

  // Configure existing device policies and the owner key.
  auto signing_key = ConfigureExistingPolicies(profile_->GetProfileUserName());
  owner_key_util_->ImportPrivateKeyInSlotAndSetPublicKey(
      signing_key->Copy(), nss_service_->GetPrivateSlot());
  device_settings_service_.LoadImmediately();  // Reload policies.

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  ASSERT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  EXPECT_EQ(result_observer_.Get<PublicKeyRefPtr>()->data(),
            ExtractSpkiDer(signing_key));
  EXPECT_EQ(key_loader_->ExtractOldOwnerKey(), nullptr);
  ASSERT_TRUE(result_observer_.Get<PrivateKeyRefPtr>());
  EXPECT_TRUE(result_observer_.Get<PrivateKeyRefPtr>()->key());
  EXPECT_FALSE(IsKeyInSlot(result_observer_.Get<PublicKeyRefPtr>()->data(),
                           nss_service_->GetPublicSlot()));

  EXPECT_THAT(histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
              BucketsInclude(
                  Bucket(OwnerKeyUmaEvent::kOwnerHasKeysSuccess, 1),
                  Bucket(OwnerKeyUmaEvent::kOwnerKeyInPublicSlotFalse, 1),
                  Bucket(OwnerKeyUmaEvent::kMigrationToPrivateSlotStarted, 0)));
}

// Test that the owner key from the private slot is migrated back into the
// public slot when the feature flags are disabled.
TEST_F(RegularOwnerKeyLoaderTest, MigrateFromPrivateToPublicSlot) {
  base::test::ScopedFeatureList feature_list;
  // With this config Chrome should generate new keys in the private slot, but
  // not migrate existing keys from the public slot.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kStoreOwnerKeyInPrivateSlot,
                             kMigrateOwnerKeyToPrivateSlot});

  // Configure existing device policies and the owner key.
  auto signing_key = ConfigureExistingPolicies(profile_->GetProfileUserName());
  owner_key_util_->ImportPrivateKeyInSlotAndSetPublicKey(
      signing_key->Copy(), nss_service_->GetPrivateSlot());
  device_settings_service_.LoadImmediately();  // Reload policies.

  key_loader_->Run();

  ASSERT_TRUE(result_observer_.Get<PublicKeyRefPtr>());
  ASSERT_TRUE(!result_observer_.Get<PublicKeyRefPtr>()->is_empty());
  EXPECT_NE(result_observer_.Get<PublicKeyRefPtr>()->data(),
            ExtractSpkiDer(signing_key));
  EXPECT_EQ(ExtractSpkiDer(key_loader_->ExtractOldOwnerKey()),
            ExtractSpkiDer(signing_key));
  ASSERT_TRUE(result_observer_.Get<PrivateKeyRefPtr>());
  EXPECT_TRUE(result_observer_.Get<PrivateKeyRefPtr>()->key());

  EXPECT_THAT(histogram_tester_.GetAllSamples(kOwnerKeyHistogramName),
              BucketsInclude(
                  Bucket(OwnerKeyUmaEvent::kOwnerHasKeysSuccess, 1),
                  Bucket(OwnerKeyUmaEvent::kOwnerKeyInPublicSlotFalse, 1),
                  Bucket(OwnerKeyUmaEvent::kMigrationToPublicSlotStarted, 1)));
}

// Test that OwnerKeyLoader silently exits if it was started after the shutdown
// had started.
TEST_F(RegularOwnerKeyLoaderTest, ExitOnShutdown) {
  // In real code DeviceSettingsService must call this for the first user.
  device_settings_service_.MarkWillEstablishConsumerOwnership();
  // Do not prepare any keys, so key_loader_ has to generate a new one.
  TestingBrowserProcess::GetGlobal()->SetShuttingDown(true);

  key_loader_->Run();

  EXPECT_FALSE(result_observer_.Get<PublicKeyRefPtr>());
  EXPECT_FALSE(result_observer_.Get<PrivateKeyRefPtr>());
  EXPECT_EQ(histogram_tester_.GetTotalSum(kOwnerKeyHistogramName), 0);
}

class ChildOwnerKeyLoaderTest : public OwnerKeyLoaderTestBase {
 public:
  ChildOwnerKeyLoaderTest()
      : OwnerKeyLoaderTestBase(user_manager::UserType::kChild) {}
};

// Test that the first user generates a new owner key (when the user is a
// child user).
TEST_F(ChildOwnerKeyLoaderTest, FirstUserGeneratesOwnerKey) {
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
      BucketsInclude(
          Bucket(OwnerKeyUmaEvent::kEstablishingConsumerOwnershipSuccess, 1),
          Bucket(OwnerKeyUmaEvent::kOwnerKeyGeneratedSuccess, 1)));
}

}  // namespace ash
