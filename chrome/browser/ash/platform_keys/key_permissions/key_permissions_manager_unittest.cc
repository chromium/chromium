// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/key_permissions.pb.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager_impl.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_util.h"
#include "chrome/browser/ash/platform_keys/mock_platform_keys_service.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::platform_keys {
namespace {

using ::base::Bucket;
using MigrationStatus =
    KeyPermissionsManagerImpl::KeyPermissionsInChapsUpdater::MigrationStatus;
using base::test::RunOnceCallback;
using base::test::RunOnceCallbackRepeatedly;
using chromeos::platform_keys::KeyAttributeType;
using chromeos::platform_keys::Status;
using chromeos::platform_keys::TokenId;
using testing::_;
using testing::AnyNumber;
using ::testing::NiceMock;

std::vector<uint8_t> MakePermissions(bool corporate_usage_allowed,
                                     bool arc_usage_allowed) {
  chaps::KeyPermissions key_permissions;
  key_permissions.mutable_key_usages()->set_corporate(corporate_usage_allowed);
  key_permissions.mutable_key_usages()->set_arc(arc_usage_allowed);

  std::vector<uint8_t> result;
  result.resize(key_permissions.ByteSizeLong());
  EXPECT_TRUE(key_permissions.SerializeToArray(result.data(), result.size()));
  return result;
}

class FakeArcKpmDelegate : public ArcKpmDelegate {
 public:
  bool AreCorporateKeysAllowedForArcUsage() const override { return false; }
};

class KeyPermissionsManagerTest : public testing::Test {
 public:
  void SetUp() override {
    KeyPermissionsManagerImpl::RegisterLocalStatePrefs(
        pref_service_.registry());
    pref_service_.registry()->RegisterDictionaryPref(prefs::kPlatformKeys);

    // Each KeyPermissionsManagerImpl works with a single token and double
    // checks that it's available. By default make it a success.
    ON_CALL(platform_keys_service_, GetTokens)
        .WillByDefault(RunOnceCallbackRepeatedly<0>(
            std::vector<TokenId>{token_id_}, Status::kSuccess));
    // `all_keys_` is expected to be configured by each test.
    ON_CALL(platform_keys_service_, GetAllKeys(token_id_, _))
        .WillByDefault(RunOnceCallbackRepeatedly<1>(testing::ByRef(all_keys_),
                                                    Status::kSuccess));
    ON_CALL(
        platform_keys_service_,
        GetAttributeForKey(token_id_, _, KeyAttributeType::kKeyPermissions, _))
        .WillByDefault(Invoke(
            this, &KeyPermissionsManagerTest::OnGetAttributesForKeyCalled));
    ON_CALL(platform_keys_service_,
            SetAttributeForKey(token_id_, _, KeyAttributeType::kKeyPermissions,
                               _, _))
        .WillByDefault(RunOnceCallbackRepeatedly<4>(Status::kSuccess));
  }

 protected:
  void OnGetAttributesForKeyCalled(chromeos::platform_keys::TokenId token_id,
                                   std::vector<uint8_t> public_key_spki_der,
                                   KeyAttributeType attribute_type,
                                   GetAttributeForKeyCallback callback) {
    auto iter = key_permissions_.find(public_key_spki_der);
    if (iter == key_permissions_.end()) {
      return std::move(callback).Run(std::vector<uint8_t>(), Status::kSuccess);
    }
    return std::move(callback).Run(iter->second, Status::kSuccess);
  }

  TestingPrefServiceSimple pref_service_;
  NiceMock<MockPlatformKeysService> platform_keys_service_;
  std::unique_ptr<KeyPermissionsManagerImpl> permissions_manager_;
  base::HistogramTester histogram_tester_;
  TokenId token_id_ = TokenId::kUser;

  std::vector<uint8_t> key_0 = {0};
  std::vector<std::vector<uint8_t>> all_keys_;
  // Used to emulate permissions stored in Chaps.
  std::map<std::vector<uint8_t> /*key*/,
           std::vector<uint8_t> /*serialized_permissions*/>
      key_permissions_;
};

// Test that on a new device with no keys or preferences a migration is
// attempted, it is considered not necessary and a flag that it is done being
// stored.
TEST_F(KeyPermissionsManagerTest, NewDevicesDoNotNeedToMigrate) {
  EXPECT_FALSE(
      pref_service_.GetBoolean(prefs::kKeyPermissionsOneTimeMigrationDone));

  // A new device wouldn't have keys.
  all_keys_ = {};

  permissions_manager_ = std::make_unique<KeyPermissionsManagerImpl>(
      token_id_, std::make_unique<FakeArcKpmDelegate>(),
      &platform_keys_service_, &pref_service_);

  EXPECT_THAT(histogram_tester_.GetAllSamples(kMigrationStatusHistogramName),
              BucketsInclude(Bucket(MigrationStatus::kStarted, 1),
                             Bucket(MigrationStatus::kSucceeded, 1),
                             Bucket(MigrationStatus::kNecessary, 0)));
  EXPECT_TRUE(
      pref_service_.GetBoolean(prefs::kKeyPermissionsOneTimeMigrationDone));
}

// Test that when the kKeyPermissionsOneTimeMigrationDone preference indicates
// that the migration was already done, it is not happening again.
TEST_F(KeyPermissionsManagerTest, MigratedDevicesDoNotMigrate) {
  all_keys_ = {key_0};
  pref_service_.SetBoolean(prefs::kKeyPermissionsOneTimeMigrationDone, true);

  permissions_manager_ = std::make_unique<KeyPermissionsManagerImpl>(
      token_id_, std::make_unique<FakeArcKpmDelegate>(),
      &platform_keys_service_, &pref_service_);

  EXPECT_THAT(histogram_tester_.GetAllSamples(kMigrationStatusHistogramName),
              BucketsInclude(Bucket(MigrationStatus::kStarted, 0),
                             Bucket(MigrationStatus::kSucceeded, 0),
                             Bucket(MigrationStatus::kFailed, 0),
                             Bucket(MigrationStatus::kNecessary, 0)));
}

// Test that the migration is done and considered necessary when Chaps doesn't
// contain the correct permissions, but the preference storage does.
TEST_F(KeyPermissionsManagerTest, KeyPermissionsMigrated) {
  EXPECT_FALSE(
      pref_service_.GetBoolean(prefs::kKeyPermissionsOneTimeMigrationDone));

  all_keys_ = {key_0};
  internal::MarkUserKeyCorporateInPref(key_0, &pref_service_);

  permissions_manager_ = std::make_unique<KeyPermissionsManagerImpl>(
      token_id_, std::make_unique<FakeArcKpmDelegate>(),
      &platform_keys_service_, &pref_service_);

  EXPECT_THAT(histogram_tester_.GetAllSamples(kMigrationStatusHistogramName),
              BucketsInclude(Bucket(MigrationStatus::kStarted, 1),
                             Bucket(MigrationStatus::kSucceeded, 1),
                             Bucket(MigrationStatus::kFailed, 0),
                             Bucket(MigrationStatus::kNecessary, 1)));
  EXPECT_TRUE(
      pref_service_.GetBoolean(prefs::kKeyPermissionsOneTimeMigrationDone));
}

// Test that the migration is not considered necessary when Chaps already
// contains the correct permissions.
TEST_F(KeyPermissionsManagerTest, KeyPermissionsNotMigrated) {
  EXPECT_FALSE(
      pref_service_.GetBoolean(prefs::kKeyPermissionsOneTimeMigrationDone));

  all_keys_ = {key_0};
  key_permissions_[key_0] = MakePermissions(/*corporate_usage_allowed=*/true,
                                            /*arc_usage_allowed=*/false);

  permissions_manager_ = std::make_unique<KeyPermissionsManagerImpl>(
      token_id_, std::make_unique<FakeArcKpmDelegate>(),
      &platform_keys_service_, &pref_service_);

  EXPECT_THAT(histogram_tester_.GetAllSamples(kMigrationStatusHistogramName),
              BucketsInclude(Bucket(MigrationStatus::kStarted, 1),
                             Bucket(MigrationStatus::kSucceeded, 1),
                             Bucket(MigrationStatus::kFailed, 0),
                             Bucket(MigrationStatus::kNecessary, 0)));
  EXPECT_TRUE(
      pref_service_.GetBoolean(prefs::kKeyPermissionsOneTimeMigrationDone));
}

}  // namespace
}  // namespace ash::platform_keys
