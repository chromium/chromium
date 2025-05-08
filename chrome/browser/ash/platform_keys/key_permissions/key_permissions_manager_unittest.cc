// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager.h"

#include <map>
#include <vector>

#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager_impl.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_util.h"
#include "chrome/browser/ash/platform_keys/mock_platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/kcer/key_permissions.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::platform_keys {
namespace {

using base::test::RunOnceCallback;
using base::test::RunOnceCallbackRepeatedly;
using base::test::RunUntil;
using chromeos::platform_keys::KeyAttributeType;
using chromeos::platform_keys::Status;
using chromeos::platform_keys::TokenId;
using testing::_;
using testing::DoAll;
using ::testing::InSequence;
using testing::Invoke;
using ::testing::NiceMock;
using testing::SaveArg;

const TokenId kDefaultToken = TokenId::kUser;
const bool kDefaultArcAllowed = false;

class FakeArcKpmDelegate : public ArcKpmDelegate {
 public:
  explicit FakeArcKpmDelegate(bool arc_allowed) : arc_allowed_(arc_allowed) {}
  bool AreCorporateKeysAllowedForArcUsage() const override {
    return arc_allowed_;
  }

  void NotifyArcUsageAllowanceForCorporateKeysChanged(bool allowed) override {
    arc_allowed_ = allowed;
    ArcKpmDelegate::NotifyArcUsageAllowanceForCorporateKeysChanged(allowed);
  }

 private:
  bool arc_allowed_ = false;
};

class KeyPermissionsManagerTest : public testing::Test {
 public:
  void SetUp() override {
    KeyPermissionsManagerImpl::RegisterLocalStatePrefs(
        pref_service_.registry());
    pref_service_.registry()->RegisterDictionaryPref(prefs::kPlatformKeys);
    InitPlatformKeysService(kDefaultToken);

    previous_log_handler_ = logging::GetLogMessageHandler();
  }

  void TearDown() override {
    logging::SetLogMessageHandler(previous_log_handler_);
    if (permissions_manager_) {
      arc_kpm_delegate_ = nullptr;
      permissions_manager_->Shutdown();
      permissions_manager_.reset();
    }
  }

 protected:
  void InitPlatformKeysService(TokenId token_id) {
    // Each KeyPermissionsManagerImpl works with a single token and double
    // checks that it's available. By default make it a success.
    ON_CALL(platform_keys_service_, GetTokens)
        .WillByDefault(RunOnceCallbackRepeatedly<0>(
            std::vector<TokenId>{token_id}, Status::kSuccess));
    // `get_all_keys_result_` is expected to be configured by each test.
    ON_CALL(platform_keys_service_, GetAllKeys(token_id, _))
        .WillByDefault(RunOnceCallbackRepeatedly<1>(
            testing::ByRef(get_all_keys_result_), Status::kSuccess));
    ON_CALL(
        platform_keys_service_,
        GetAttributeForKey(token_id, _, KeyAttributeType::kKeyPermissions, _))
        .WillByDefault(Invoke(
            this, &KeyPermissionsManagerTest::OnGetAttributesForKeyCalled));
    ON_CALL(platform_keys_service_,
            SetAttributeForKey(token_id, _, KeyAttributeType::kKeyPermissions,
                               _, _))
        .WillByDefault(RunOnceCallbackRepeatedly<4>(Status::kSuccess));
  }

  void InitKeyPermissionsManager(bool arc_allowed = kDefaultArcAllowed) {
    auto arc_kpm_delegate = std::make_unique<FakeArcKpmDelegate>(arc_allowed);
    arc_kpm_delegate_ = arc_kpm_delegate.get();

    permissions_manager_ = std::make_unique<KeyPermissionsManagerImpl>(
        kDefaultToken, std::move(arc_kpm_delegate), &platform_keys_service_,
        &pref_service_);
    arc_kpm_delegate_->NotifyArcUsageAllowanceForCorporateKeysChanged(
        arc_allowed);
  }

  void UpdateStoredKeyPermissions(
      const std::vector<uint8_t>& public_key_spki_der,
      bool is_corporate,
      bool arc_allowed) {
    chaps::KeyPermissions new_permissions;
    new_permissions.mutable_key_usages()->set_corporate(is_corporate);
    new_permissions.mutable_key_usages()->set_arc(arc_allowed);
    UpdateStoredKeyPermissions(
        public_key_spki_der,
        ash::platform_keys::internal::KeyPermissionsProtoToBytes(
            new_permissions));
  }

  void UpdateStoredKeyPermissions(
      const std::vector<uint8_t>& public_key_spki_der,
      std::vector<uint8_t> serialized_permissions) {
    key_permissions_[public_key_spki_der] = serialized_permissions;
  }

  [[nodiscard]] bool VerifyKeyPermissions(
      std::vector<uint8_t> serialized_permissions,
      bool expected_is_corporate,
      bool expected_arc_allowed) {
    chaps::KeyPermissions key_permissions;
    if (serialized_permissions.empty() ||
        !ash::platform_keys::internal::KeyPermissionsProtoFromBytes(
            serialized_permissions, key_permissions) ||
        !key_permissions.has_key_usages()) {
      LOG(ERROR) << "Failed to parse permissions.";
      return false;
    }
    if (key_permissions.key_usages().corporate() != expected_is_corporate) {
      LOG(ERROR) << "Unexpected is_corporate. Want " << expected_is_corporate
                 << " got " << key_permissions.key_usages().corporate();
      return false;
    }
    if (key_permissions.key_usages().arc() != expected_arc_allowed) {
      LOG(ERROR) << "Unexpected arc_allowed. Want " << expected_arc_allowed
                 << " got " << key_permissions.key_usages().arc();
      return false;
    }
    return true;
  }

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

  // This is a helper method that is a better alternative for RunUntilIdle. By
  // using it tests can wait until KeyPermissionsManager is ready for requests.
  void MakeACallAndWaitForResult(
      std::unique_ptr<KeyPermissionsManagerImpl>& permissions_manager) {
    // Makes some request and wait until it completes.
    base::test::TestFuture<std::optional<bool>, Status> result;
    permissions_manager->IsKeyAllowedForUsage(result.GetCallback(),
                                              KeyUsage::kCorporate, key_0_);
    EXPECT_EQ(result.Get<Status>(), Status::kSuccess);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  NiceMock<MockPlatformKeysService> platform_keys_service_;
  std::unique_ptr<KeyPermissionsManagerImpl> permissions_manager_;
  raw_ptr<FakeArcKpmDelegate> arc_kpm_delegate_ = nullptr;
  // Some tests set a custom log handler, this one is store and recover the
  // original one.
  logging::LogMessageHandlerFunction previous_log_handler_;

  std::vector<uint8_t> key_0_ = {0};
  std::vector<uint8_t> key_1_ = {1};
  std::vector<std::vector<uint8_t>> get_all_keys_result_;
  // Used to emulate permissions stored in Chaps.
  std::map<std::vector<uint8_t> /*key*/,
           std::vector<uint8_t> /*serialized_permissions*/>
      key_permissions_;
};

// Runs the tests with all combinations of is_corporate and arc_allowed. Note
// that keys with is_corporate=false && arc_allowed=true permissions are not
// supposed to actually exist.
class KeyPermissionsManagerPerPermissionTest
    : public KeyPermissionsManagerTest,
      public ::testing::WithParamInterface<
          std::tuple</*is_corporate=*/bool, /*arc_allowed=*/bool>> {
 public:
  bool IsCorporateParam() { return std::get<0>(GetParam()); }
  bool ArcAllowedParam() { return std::get<1>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(AllPermissions,
                         KeyPermissionsManagerPerPermissionTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

// Test that a key can be marked as corporate for all combinations of initial
// key permissions.
TEST_P(KeyPermissionsManagerPerPermissionTest,
       AllowKeyForUsageSettingCorpPermissionsSucceeds) {
  UpdateStoredKeyPermissions(key_0_, IsCorporateParam(), ArcAllowedParam());
  InitKeyPermissionsManager(ArcAllowedParam());

  // Expect a call to set new permissions, record the provided value.
  std::vector<uint8_t> attribute_value;
  EXPECT_CALL(platform_keys_service_,
              SetAttributeForKey(kDefaultToken, _,
                                 KeyAttributeType::kKeyPermissions, _, _))
      .WillOnce(DoAll(SaveArg<3>(&attribute_value),
                      RunOnceCallback<4>(Status::kSuccess)));

  base::test::TestFuture<Status> result;
  permissions_manager_->AllowKeyForUsage(result.GetCallback(),
                                         KeyUsage::kCorporate, key_0_);

  EXPECT_EQ(result.Get(), Status::kSuccess);
  EXPECT_TRUE(VerifyKeyPermissions(attribute_value,
                                   /*expected_is_corporate=*/true,
                                   /*expected_arc_allowed=*/ArcAllowedParam()));
}

// Test that it's not allowed to manually mark a key as allowed for ARC++ for
// all initial key permissions. ARC++ permission is managed automatically.
TEST_P(KeyPermissionsManagerPerPermissionTest,
       AllowKeyForUsageManuallySettingArcPermissionsFails) {
  UpdateStoredKeyPermissions(key_0_, IsCorporateParam(), ArcAllowedParam());
  InitKeyPermissionsManager();

  base::test::TestFuture<Status> result;
  permissions_manager_->AllowKeyForUsage(result.GetCallback(), KeyUsage::kArc,
                                         key_0_);

  EXPECT_EQ(result.Get(), Status::kErrorInternal);
}

// Test that when saving new permissions fails, the error is correctly
// propagated back.
TEST_P(KeyPermissionsManagerPerPermissionTest,
       AllowKeyForUsageSettingCorpPermissionsPropagatesError) {
  Status result_status = Status::kErrorInputTooLong;
  EXPECT_CALL(platform_keys_service_, SetAttributeForKey)
      .WillOnce(RunOnceCallback<4>(result_status));
  UpdateStoredKeyPermissions(key_0_, IsCorporateParam(), ArcAllowedParam());
  InitKeyPermissionsManager();

  base::test::TestFuture<Status> result;
  permissions_manager_->AllowKeyForUsage(result.GetCallback(),
                                         KeyUsage::kCorporate, key_0_);

  EXPECT_EQ(result.Get<Status>(), result_status);
}

// Test that for keys on the system token IsKeyAllowedForUsage always returns
// true for corporate usage and the stored value for arc usage.
TEST_P(KeyPermissionsManagerPerPermissionTest,
       IsKeyAllowedForUsageCorporateAllowedForSystemToken) {
  UpdateStoredKeyPermissions(key_0_, IsCorporateParam(), ArcAllowedParam());
  InitPlatformKeysService(TokenId::kSystem);
  permissions_manager_ = std::make_unique<KeyPermissionsManagerImpl>(
      TokenId::kSystem, std::make_unique<FakeArcKpmDelegate>(ArcAllowedParam()),
      &platform_keys_service_, &pref_service_);

  base::test::TestFuture<std::optional<bool>, Status> corporate_result;
  permissions_manager_->IsKeyAllowedForUsage(corporate_result.GetCallback(),
                                             KeyUsage::kCorporate, key_0_);
  base::test::TestFuture<std::optional<bool>, Status> arc_result;
  permissions_manager_->IsKeyAllowedForUsage(arc_result.GetCallback(),
                                             KeyUsage::kArc, key_0_);

  EXPECT_EQ(corporate_result.Get<Status>(), Status::kSuccess);
  std::optional<bool> corporate_allowed = corporate_result.Get<0>();
  EXPECT_TRUE(corporate_allowed.has_value());
  EXPECT_TRUE(corporate_allowed.value());

  EXPECT_EQ(arc_result.Get<Status>(), Status::kSuccess);
  std::optional<bool> arc_allowed = arc_result.Get<0>();
  EXPECT_TRUE(arc_allowed.has_value());
  EXPECT_EQ(arc_allowed.value(), ArcAllowedParam());
}

// Test that IsKeyAllowedForUsage returns results according to the stored
// permissions for keys on the user token.
TEST_P(KeyPermissionsManagerPerPermissionTest, IsKeyAllowedForUsage) {
  UpdateStoredKeyPermissions(key_0_, IsCorporateParam(), ArcAllowedParam());
  InitKeyPermissionsManager();

  base::test::TestFuture<std::optional<bool>, Status> corporate_result;
  permissions_manager_->IsKeyAllowedForUsage(corporate_result.GetCallback(),
                                             KeyUsage::kCorporate, key_0_);
  base::test::TestFuture<std::optional<bool>, Status> arc_result;
  permissions_manager_->IsKeyAllowedForUsage(arc_result.GetCallback(),
                                             KeyUsage::kArc, key_0_);

  EXPECT_EQ(corporate_result.Get<Status>(), Status::kSuccess);
  std::optional<bool> corporate_allowed = corporate_result.Get<0>();
  EXPECT_TRUE(corporate_allowed.has_value());
  EXPECT_EQ(corporate_allowed.value(), IsCorporateParam());

  EXPECT_EQ(arc_result.Get<Status>(), Status::kSuccess);
  std::optional<bool> arc_allowed = arc_result.Get<0>();
  EXPECT_TRUE(arc_allowed.has_value());
  EXPECT_EQ(arc_allowed.value(), ArcAllowedParam());
}

// Test that when IsKeyAllowedForUsage fails to read permissions, it propagates
// the correct error.
TEST_F(KeyPermissionsManagerTest,
       IsKeyAllowedForUsageFailsToReadPermissionsPropagatesError) {
  Status result_status = Status::kErrorInputTooLong;
  EXPECT_CALL(platform_keys_service_, GetAttributeForKey)
      .WillOnce(RunOnceCallback<3>(std::vector<uint8_t>(), result_status));
  InitKeyPermissionsManager();

  base::test::TestFuture<std::optional<bool>, Status> result;
  permissions_manager_->IsKeyAllowedForUsage(result.GetCallback(),
                                             KeyUsage::kCorporate, key_0_);

  EXPECT_EQ(result.Get<Status>(), result_status);
}

// Test that when IsKeyAllowedForUsage reads empty permissions, it returns "not
// allowed".
TEST_F(KeyPermissionsManagerTest, IsKeyAllowedForUsageReadsEmptyPermissions) {
  EXPECT_CALL(platform_keys_service_, GetAttributeForKey)
      .WillRepeatedly(RunOnceCallbackRepeatedly<3>(
          std::optional<std::vector<uint8_t>>(), Status::kSuccess));
  InitKeyPermissionsManager();

  base::test::TestFuture<std::optional<bool>, Status> corporate_result;
  permissions_manager_->IsKeyAllowedForUsage(corporate_result.GetCallback(),
                                             KeyUsage::kCorporate, key_0_);
  base::test::TestFuture<std::optional<bool>, Status> arc_result;
  permissions_manager_->IsKeyAllowedForUsage(arc_result.GetCallback(),
                                             KeyUsage::kArc, key_0_);

  EXPECT_EQ(corporate_result.Get<Status>(), Status::kSuccess);
  std::optional<bool> corporate_allowed = corporate_result.Get<0>();
  EXPECT_TRUE(corporate_allowed.has_value());
  EXPECT_FALSE(corporate_allowed.value());

  EXPECT_EQ(arc_result.Get<Status>(), Status::kSuccess);
  std::optional<bool> arc_allowed = arc_result.Get<0>();
  EXPECT_TRUE(arc_allowed.has_value());
  EXPECT_FALSE(arc_allowed.value());
}

// Test that when IsKeyAllowedForUsage reads permissions that it cannot parse it
// returns an error.
TEST_F(KeyPermissionsManagerTest, IsKeyAllowedForUsageFailsToParsePermissions) {
  Status result_status = Status::kSuccess;
  EXPECT_CALL(platform_keys_service_, GetAttributeForKey)
      .WillRepeatedly(RunOnceCallbackRepeatedly<3>(
          std::vector<uint8_t>{1, 2, 3}, result_status));
  InitKeyPermissionsManager();

  base::test::TestFuture<std::optional<bool>, Status> corporate_result;
  permissions_manager_->IsKeyAllowedForUsage(corporate_result.GetCallback(),
                                             KeyUsage::kCorporate, key_0_);
  base::test::TestFuture<std::optional<bool>, Status> arc_result;
  permissions_manager_->IsKeyAllowedForUsage(arc_result.GetCallback(),
                                             KeyUsage::kArc, key_0_);

  EXPECT_EQ(corporate_result.Get<Status>(), Status::kErrorInternal);
  EXPECT_EQ(arc_result.Get<Status>(), Status::kErrorInternal);
}

// Test that KeyPermissionsManager completes the one-time migration if it was
// not done yet.
TEST_F(KeyPermissionsManagerTest, OneTimeMigrationSucceeds) {
  EXPECT_FALSE(
      pref_service_.GetBoolean(prefs::kKeyPermissionsOneTimeMigrationDone));

  get_all_keys_result_ = {key_0_};
  internal::MarkUserKeyCorporateInPref(key_0_, &pref_service_);

  InitKeyPermissionsManager();
  MakeACallAndWaitForResult(permissions_manager_);

  EXPECT_TRUE(
      pref_service_.GetBoolean(prefs::kKeyPermissionsOneTimeMigrationDone));
}

// Verify that it becomes available even if KeyPermissionsManager fails to read
// some permissions for the one-time migration.
TEST_F(KeyPermissionsManagerTest, OneTimeMigrationFailsAndManagerBecomesReady) {
  EXPECT_FALSE(
      pref_service_.GetBoolean(prefs::kKeyPermissionsOneTimeMigrationDone));

  get_all_keys_result_ = {key_0_};
  internal::MarkUserKeyCorporateInPref(key_0_, &pref_service_);

  EXPECT_CALL(platform_keys_service_, GetAttributeForKey)
      // The first call is for the migration.
      .WillOnce(
          RunOnceCallback<3>(std::vector<uint8_t>(), Status::kErrorInternal))
      // The second call is for IsKeyAllowedForUsage.
      .WillOnce(RunOnceCallback<3>(std::vector<uint8_t>(), Status::kSuccess));

  InitKeyPermissionsManager();
  MakeACallAndWaitForResult(permissions_manager_);
  // The EXPECT_CALL is verified here.
}

// Test that when the one-time migration fails, the correct histogram is
// recorded.
TEST_F(KeyPermissionsManagerTest, OneTimeMigrationFailed) {
  base::HistogramTester histogram_tester;

  get_all_keys_result_ = {key_0_};
  ON_CALL(platform_keys_service_, SetAttributeForKey)
      .WillByDefault(RunOnceCallbackRepeatedly<4>(Status::kErrorInternal));

  permissions_manager_ = std::make_unique<KeyPermissionsManagerImpl>(
      kDefaultToken, std::make_unique<FakeArcKpmDelegate>(kDefaultArcAllowed),
      &platform_keys_service_, &pref_service_);

  const char kHistogram[] = "ChromeOS.KeyPermissionsManager.Migration";

  EXPECT_TRUE(RunUntil(
      [&]() { return histogram_tester.GetBucketCount(kHistogram, 2) > 0; }));

  EXPECT_THAT(histogram_tester.GetAllSamples(kHistogram),
              BucketsInclude(base::Bucket(/*MigrationStatus::kFailed*/ 2, 1)));
}

// Test that KeyPermissionsManager successfully completes initialization when
// the one-time migration was already done before.
TEST_F(KeyPermissionsManagerTest, MigrationIsDoneFromTheBeginning) {
  pref_service_.SetBoolean(prefs::kKeyPermissionsOneTimeMigrationDone, true);

  InitKeyPermissionsManager();

  base::test::TestFuture<std::optional<bool>, Status> is_allowed_result;
  permissions_manager_->IsKeyAllowedForUsage(is_allowed_result.GetCallback(),
                                             KeyUsage::kCorporate, key_0_);
  EXPECT_EQ(is_allowed_result.Get<Status>(), Status::kSuccess);
  EXPECT_EQ(is_allowed_result.Get<0>(), std::optional<bool>(false));

  base::test::TestFuture<Status> allow_result;
  permissions_manager_->AllowKeyForUsage(allow_result.GetCallback(),
                                         KeyUsage::kCorporate, key_0_);
  EXPECT_EQ(allow_result.Get(), Status::kSuccess);
}

class KeyPermissionsManagerArcAllowedChangesTest
    : public KeyPermissionsManagerTest,
      public ::testing::WithParamInterface<
          std::tuple</*initial_global_arc_allowed=*/bool,
                     /*new_global_arc_allowed=*/bool,
                     /*key_is_corporate=*/bool,
                     /*expected_key_arc_allowed*/ bool>> {
 public:
  bool InitialGlobalArcAllowedParam() { return std::get<0>(GetParam()); }
  bool NewGlobalArcAllowedParam() { return std::get<1>(GetParam()); }
  bool KeyIsCorporateParam() { return std::get<2>(GetParam()); }
  bool ExpectedKeyArcAllowedParam() { return std::get<3>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(
    ArcAllowedParam,
    KeyPermissionsManagerArcAllowedChangesTest,
    testing::Values(
        // Test that corporate keys become allowed for ARC++ when it becomes
        // globally allowed.
        std::make_tuple(/*initial_global_arc_allowed=*/false,
                        /*new_global_arc_allowed=*/true,
                        /*key_is_corporate=*/true,
                        /*expected_key_arc_allowed=*/true),
        // Test that corporate keys become not allowed for ARC++ when it becomes
        // globally not allowed.
        std::make_tuple(/*initial_global_arc_allowed=*/true,
                        /*new_global_arc_allowed=*/false,
                        /*key_is_corporate=*/true,
                        /*expected_key_arc_allowed=*/false),
        // Test that non-corporate keys do NOT become allowed for ARC++ when it
        // becomes globally allowed.
        std::make_tuple(/*initial_global_arc_allowed=*/false,
                        /*new_global_arc_allowed=*/true,
                        /*key_is_corporate=*/false,
                        /*expected_key_arc_allowed=*/false)));

// Test that corporate keys become allowed for ARC++ when global ARC++
// permissions change.
TEST_P(KeyPermissionsManagerArcAllowedChangesTest,
       ArcAllowanceChangedCorporateKeysAreUpdated) {
  // Use a static because only captureless lambdas can be converted to a
  // function pointer for SetLogMessageHandler().
  static const char kExpectedErrorLog[] =
      "Updating arc key permissions in chaps succeeded, 1 key(s) updated";
  static base::NoDestructor<std::string> log_string;
  logging::SetLogMessageHandler([](int severity, const char* file, int line,
                                   size_t start,
                                   const std::string& str) -> bool {
    if (base::Contains(str, kExpectedErrorLog)) {
      *log_string = str;
    }
    return false;
  });

  get_all_keys_result_ = {key_0_};
  UpdateStoredKeyPermissions(key_0_, KeyIsCorporateParam(),
                             InitialGlobalArcAllowedParam());

  InitKeyPermissionsManager(InitialGlobalArcAllowedParam());

  base::test::TestFuture<std::optional<bool>, Status> is_allowed_before;
  permissions_manager_->IsKeyAllowedForUsage(is_allowed_before.GetCallback(),
                                             KeyUsage::kArc, key_0_);
  EXPECT_EQ(is_allowed_before.Get<Status>(), Status::kSuccess);
  EXPECT_EQ(is_allowed_before.Get<0>(),
            std::optional<bool>(InitialGlobalArcAllowedParam()));

  std::vector<uint8_t> attribute_value;
  EXPECT_CALL(platform_keys_service_,
              SetAttributeForKey(kDefaultToken, _,
                                 KeyAttributeType::kKeyPermissions, _, _))
      .WillOnce(DoAll(SaveArg<3>(&attribute_value),
                      RunOnceCallback<4>(Status::kSuccess)));

  arc_kpm_delegate_->NotifyArcUsageAllowanceForCorporateKeysChanged(
      NewGlobalArcAllowedParam());

  EXPECT_TRUE(VerifyKeyPermissions(
      attribute_value,
      /*expected_is_corporate=*/KeyIsCorporateParam(),
      /*expected_arc_allowed=*/ExpectedKeyArcAllowedParam()));

  // Emulate that chaps stored the new permissions.
  UpdateStoredKeyPermissions(key_0_, attribute_value);

  base::test::TestFuture<std::optional<bool>, Status> is_allowed_after;
  permissions_manager_->IsKeyAllowedForUsage(is_allowed_after.GetCallback(),
                                             KeyUsage::kArc, key_0_);
  EXPECT_EQ(is_allowed_after.Get<Status>(), Status::kSuccess);
  EXPECT_EQ(is_allowed_after.Get<0>(),
            std::optional<bool>(ExpectedKeyArcAllowedParam()));
  EXPECT_FALSE(log_string->empty());
}

// Test that KeyPermissionsManager continues updating keys even when for one of
// them it receives a kErrorKeyNotFound error.
TEST_F(KeyPermissionsManagerTest, NotFoundKeysAreSkipped) {
  InitKeyPermissionsManager(/*arc_allowed=*/false);

  get_all_keys_result_ = {key_0_, key_1_};
  bool is_key_corporate = true;
  UpdateStoredKeyPermissions(key_0_, is_key_corporate, /*arc_allowed=*/false);
  UpdateStoredKeyPermissions(key_1_, is_key_corporate, /*arc_allowed=*/false);

  MakeACallAndWaitForResult(permissions_manager_);

  InSequence s;
  EXPECT_CALL(platform_keys_service_,
              SetAttributeForKey(kDefaultToken, key_0_,
                                 KeyAttributeType::kKeyPermissions, _, _))
      .WillOnce(RunOnceCallback<4>(Status::kErrorKeyNotFound));
  EXPECT_CALL(platform_keys_service_,
              SetAttributeForKey(kDefaultToken, key_1_,
                                 KeyAttributeType::kKeyPermissions, _, _))
      .WillOnce(RunOnceCallback<4>(Status::kSuccess));

  arc_kpm_delegate_->NotifyArcUsageAllowanceForCorporateKeysChanged(
      /*allowed=*/true);

  MakeACallAndWaitForResult(permissions_manager_);

  // EXPECT_CALLs are checked here. Specifically check that the second call
  // still happened even though the first one failed with the kErrorKeyNotFound
  // error.
}

// Test that KeyPermissionsManager stops updating the keys when for one of them
// it receives an error different from kErrorKeyNotFound.
TEST_F(KeyPermissionsManagerTest, FailsUpdatingKeysAndStops) {
  // Use a static because only captureless lambdas can be converted to a
  // function pointer for SetLogMessageHandler().
  static const char kExpectedErrorLog[] =
      "Updating arc key permissions in chaps failed, 0 key(s) updated";
  static base::NoDestructor<std::string> log_string;
  logging::SetLogMessageHandler([](int severity, const char* file, int line,
                                   size_t start,
                                   const std::string& str) -> bool {
    if (base::Contains(str, kExpectedErrorLog)) {
      *log_string = str;
    }
    return false;
  });

  InitKeyPermissionsManager(/*arc_allowed=*/false);

  get_all_keys_result_ = {key_0_, key_1_};
  bool is_key_corporate = true;
  UpdateStoredKeyPermissions(key_0_, is_key_corporate, /*arc_allowed=*/false);
  UpdateStoredKeyPermissions(key_1_, is_key_corporate, /*arc_allowed=*/false);

  MakeACallAndWaitForResult(permissions_manager_);

  InSequence s;
  EXPECT_CALL(platform_keys_service_,
              SetAttributeForKey(kDefaultToken, key_0_,
                                 KeyAttributeType::kKeyPermissions, _, _))
      .Times(1)
      .WillOnce(RunOnceCallback<4>(Status::kErrorInternal));
  EXPECT_CALL(platform_keys_service_,
              SetAttributeForKey(kDefaultToken, key_1_,
                                 KeyAttributeType::kKeyPermissions, _, _))
      .Times(0);

  arc_kpm_delegate_->NotifyArcUsageAllowanceForCorporateKeysChanged(
      /*allowed=*/true);

  MakeACallAndWaitForResult(permissions_manager_);

  EXPECT_FALSE(log_string->empty());

  // EXPECT_CALLs are checked here. Specifically check that an update for the
  // second key doesn't happen if the first one fails.
}

// Test that if KeyPermissionsManager receives an error while trying to get the
// tokens during initialization, it gracefully fails and logs an error.
TEST_F(KeyPermissionsManagerTest, FailsToFetchTokensNeverBecomesAvailable) {
  // Use a static because only captureless lambdas can be converted to a
  // function pointer for SetLogMessageHandler().
  static const char kExpectedErrorLog[] =
      "Error while waiting for token to be ready";
  static base::NoDestructor<std::string> log_string;
  logging::SetLogMessageHandler([](int severity, const char* file, int line,
                                   size_t start,
                                   const std::string& str) -> bool {
    if (base::Contains(str, kExpectedErrorLog)) {
      *log_string = str;
    }
    return false;
  });

  ON_CALL(platform_keys_service_, GetTokens)
      .WillByDefault(RunOnceCallbackRepeatedly<0>(std::vector<TokenId>{},
                                                  Status::kErrorInternal));

  InitKeyPermissionsManager();

  base::test::TestFuture<std::optional<bool>, Status> result;
  permissions_manager_->IsKeyAllowedForUsage(result.GetCallback(),
                                             KeyUsage::kCorporate, key_0_);
  EXPECT_TRUE(RunUntil([&]() { return !log_string->empty(); }));

  EXPECT_FALSE(result.IsReady());
}

// Test that if KeyPermissionsManager tries to initialize for a token that is
// not currently available, it gracefully fails and logs an error.
TEST_F(KeyPermissionsManagerTest, TokenIsNotAllowedNeverBecomesAvailable) {
  // Use a static because only captureless lambdas can be converted to a
  // function pointer for SetLogMessageHandler().
  static const char kExpectedErrorLog[] =
      "KeyPermissionsManager doesn't have access to token";
  static base::NoDestructor<std::string> log_string;
  logging::SetLogMessageHandler([](int severity, const char* file, int line,
                                   size_t start,
                                   const std::string& str) -> bool {
    if (base::Contains(str, kExpectedErrorLog)) {
      *log_string = str;
    }
    return false;
  });

  ON_CALL(platform_keys_service_, GetTokens)
      .WillByDefault(RunOnceCallbackRepeatedly<0>(
          std::vector<TokenId>{TokenId::kUser}, Status::kSuccess));

  permissions_manager_ = std::make_unique<KeyPermissionsManagerImpl>(
      TokenId::kSystem,
      std::make_unique<FakeArcKpmDelegate>(kDefaultArcAllowed),
      &platform_keys_service_, &pref_service_);

  base::test::TestFuture<std::optional<bool>, Status> result;
  permissions_manager_->IsKeyAllowedForUsage(result.GetCallback(),
                                             KeyUsage::kCorporate, key_0_);

  EXPECT_TRUE(RunUntil([&]() { return !log_string->empty(); }));

  EXPECT_FALSE(result.IsReady());
}

// Test that calls to KeyPermissionsManager before it is ready are queued and
// executed when it is ready.
TEST_F(KeyPermissionsManagerTest, EarlyCallsAreQueued) {
  GetAllKeysCallback get_keys_callback_1;
  GetAllKeysCallback get_keys_callback_2;
  EXPECT_CALL(platform_keys_service_, GetAllKeys)
      // This call happens during the one-time migration while
      // KeyPermissionsManager is initializing.
      .WillOnce(MoveArg<1>(&get_keys_callback_1))
      // This call happens after the one-time migration and is not relevant for
      // the test.
      .WillOnce(MoveArg<1>(&get_keys_callback_2));

  InitKeyPermissionsManager();

  base::test::TestFuture<std::optional<bool>, Status> is_allowed_result;
  permissions_manager_->IsKeyAllowedForUsage(is_allowed_result.GetCallback(),
                                             KeyUsage::kCorporate, key_0_);
  base::test::TestFuture<Status> allow_result;
  permissions_manager_->AllowKeyForUsage(allow_result.GetCallback(),
                                         KeyUsage::kCorporate, key_0_);

  // Wait until the one-time migration is started.
  EXPECT_TRUE(RunUntil([&]() { return bool(get_keys_callback_1); }));

  // Check that the calls were not processed yet.
  EXPECT_FALSE(is_allowed_result.IsReady());
  EXPECT_FALSE(allow_result.IsReady());

  // Allow the one-time migration to finish.
  std::move(get_keys_callback_1)
      .Run(std::vector<std::vector<uint8_t>>(), Status::kSuccess);

  // Now the calls should be able to finish.
  EXPECT_EQ(is_allowed_result.Get<Status>(), Status::kSuccess);
  EXPECT_EQ(allow_result.Get<Status>(), Status::kSuccess);
}

}  // namespace
}  // namespace ash::platform_keys
