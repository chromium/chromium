// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_proxy_backend.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"
#include "components/password_manager/core/browser/password_store/password_store_change.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {
namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Optional;
using ::testing::Pointer;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::VariantWith;
using ::testing::WithArg;
using Type = PasswordStoreChange::Type;
using RemoveChangesReceived = PasswordStoreBackend::RemoteChangesReceived;

PasswordForm CreateTestForm() {
  PasswordForm form;
  form.username_value = u"Todd Tester";
  form.password_value = u"S3cr3t";
  form.url = GURL(u"https://example.com");
  form.match_type = PasswordForm::MatchType::kExact;
  return form;
}

std::vector<PasswordForm> CreateTestLogins() {
  std::vector<PasswordForm> forms = {
      CreateEntry("Todd Tester", "S3cr3t", GURL(u"https://example.com"),
                  PasswordForm::MatchType::kExact),
      CreateEntry("Marcus McSpartanGregor", "S0m3th1ngCr34t1v3",
                  GURL(u"https://m.example.com"),
                  PasswordForm::MatchType::kPSL)};
  return forms;
}

bool FilterNoUrl(const GURL& gurl) {
  return true;
}

MATCHER_P(PasswordChangesAre, expectations, "") {
  if (absl::holds_alternative<PasswordStoreBackendError>(arg)) {
    return false;
  }

  auto changes = absl::get<PasswordChanges>(arg);
  if (!changes.has_value()) {
    return false;
  }

  return changes.value() == expectations;
}

}  // namespace

class PasswordStoreProxyBackendBaseTest : public testing::Test {
 protected:
  PasswordStoreProxyBackendBaseTest() {
    prefs_.registry()->RegisterIntegerPref(
        prefs::kCurrentMigrationVersionToGoogleMobileServices, 0);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kUnenrolledFromGoogleMobileServicesDueToErrors, false);
    prefs_.registry()->RegisterIntegerPref(
        password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(
            password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff));
    prefs_.registry()->RegisterBooleanPref(
        prefs::kEmptyProfileStoreLoginDatabase, true);
  }

  void SetUp() override { proxy_backend_ = CreateProxyBackend(); }

  std::unique_ptr<PasswordStoreProxyBackend> CreateProxyBackend() {
    auto built_in_backend =
        std::make_unique<StrictMock<MockPasswordStoreBackend>>();
    auto android_backend =
        std::make_unique<StrictMock<MockPasswordStoreBackend>>();
    built_in_backend_ = built_in_backend.get();
    android_backend_ = android_backend.get();
    return std::make_unique<PasswordStoreProxyBackend>(
        std::move(built_in_backend), std::move(android_backend), &prefs_);
  }

  void TearDown() override {
    EXPECT_CALL(*android_backend_, Shutdown(_));
    EXPECT_CALL(*built_in_backend_, Shutdown(_));
    PasswordStoreBackend* backend = proxy_backend_.get();  // Will be destroyed.
    backend->Shutdown(base::DoNothing());
    proxy_backend_.reset();
  }

  void EnablePasswordSync() {
    sync_service_.GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, {syncer::UserSelectableType::kPasswords});
    sync_service_.FireStateChanged();
  }

  void DisablePasswordSync() {
    sync_service_.GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, /*types=*/{});
    sync_service_.FireStateChanged();
  }

  PasswordStoreBackend& proxy_backend() { return *proxy_backend_; }
  MockPasswordStoreBackend& built_in_backend() { return *built_in_backend_; }
  MockPasswordStoreBackend& android_backend() { return *android_backend_; }
  TestingPrefServiceSimple* prefs() { return &prefs_; }
  syncer::TestSyncService* sync_service() { return &sync_service_; }

  raw_ptr<StrictMock<MockPasswordStoreBackend>> built_in_backend_;
  raw_ptr<StrictMock<MockPasswordStoreBackend>> android_backend_;

 private:
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<PasswordStoreProxyBackend> proxy_backend_;
  syncer::TestSyncService sync_service_;
};

TEST_F(PasswordStoreProxyBackendBaseTest, CallCompletionCallbackAfterInit) {
  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;

  // Both backends need to be invoked for a successful completion call.
  EXPECT_CALL(built_in_backend(), InitBackend)
      .WillOnce(
          WithArg<3>(Invoke([](base::OnceCallback<void(bool)> reply) -> void {
            std::move(reply).Run(true);
          })));

  base::OnceCallback<void(bool)> captured_android_backend_reply;
  EXPECT_CALL(android_backend(), InitBackend)
      .WillOnce(
          WithArg<3>(Invoke([&](base::OnceCallback<void(bool)> reply) -> void {
            captured_android_backend_reply = std::move(reply);
          })));

  proxy_backend().InitBackend(nullptr, base::DoNothing(), base::DoNothing(),
                              completion_callback.Get());
  // The android backend requires the sync service to be initialized before
  // signaling that the backend initialization is complete.
  EXPECT_CALL(completion_callback, Run(true));
  EXPECT_CALL(android_backend(), OnSyncServiceInitialized(sync_service()))
      .WillOnce(Invoke([&]() -> void {
        std::move(captured_android_backend_reply).Run(true);
      }));
  proxy_backend().OnSyncServiceInitialized(sync_service());
}

TEST_F(PasswordStoreProxyBackendBaseTest,
       CallCompletionWithFailureForAnyError) {
  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;

  // If one backend fails to initialize, the result of the second is
  // irrelevant.
  EXPECT_CALL(built_in_backend(), InitBackend)
      .WillOnce(
          WithArg<3>(Invoke([](base::OnceCallback<void(bool)> reply) -> void {
            std::move(reply).Run(false);
          })));
  base::OnceCallback<void(bool)> captured_android_backend_reply;
  EXPECT_CALL(android_backend(), InitBackend)
      .Times(AtMost(1))
      .WillOnce(
          WithArg<3>(Invoke([&](base::OnceCallback<void(bool)> reply) -> void {
            captured_android_backend_reply = std::move(reply);
          })));

  proxy_backend().InitBackend(nullptr, base::DoNothing(), base::DoNothing(),
                              completion_callback.Get());
  // The android backend requires the sync service to be initialized before
  // signaling that the backend initialization is complete.
  EXPECT_CALL(completion_callback, Run(false));
  EXPECT_CALL(android_backend(), OnSyncServiceInitialized(sync_service()))
      .WillOnce(Invoke([&]() -> void {
        std::move(captured_android_backend_reply).Run(false);
      }));
  proxy_backend().OnSyncServiceInitialized(sync_service());
}

TEST_F(PasswordStoreProxyBackendBaseTest,
       ProfileNoLocalSupportCallRemoteChangesOnlyForMainBackend) {
  base::MockCallback<RemoveChangesReceived> original_callback;

  // Both backends receive a callback that they trigger for new remote changes.
  RemoveChangesReceived built_in_remote_changes_callback;
  EXPECT_CALL(built_in_backend(), InitBackend)
      .WillOnce(SaveArg<1>(&built_in_remote_changes_callback));
  RemoveChangesReceived android_remote_changes_callback;
  EXPECT_CALL(android_backend(), InitBackend)
      .WillOnce(SaveArg<1>(&android_remote_changes_callback));
  proxy_backend().InitBackend(nullptr, original_callback.Get(),
                              base::DoNothing(), base::DoNothing());
  EXPECT_CALL(android_backend(), OnSyncServiceInitialized(sync_service()));
  proxy_backend().OnSyncServiceInitialized(sync_service());

  // With sync enabled, only the android backend calls the original callback.
  EnablePasswordSync();
  EXPECT_CALL(original_callback, Run);
  android_remote_changes_callback.Run(std::nullopt);
  testing::Mock::VerifyAndClearExpectations(&original_callback);

  EXPECT_CALL(original_callback, Run).Times(0);
  built_in_remote_changes_callback.Run(std::nullopt);
  testing::Mock::VerifyAndClearExpectations(&original_callback);

  // As soon as sync is disabled, only the built-in backend calls the original
  // callback. The callbacks are stable. No new Init call is necessary.
  DisablePasswordSync();

  EXPECT_CALL(original_callback, Run).Times(0);
  android_remote_changes_callback.Run(std::nullopt);
  testing::Mock::VerifyAndClearExpectations(&original_callback);

  EXPECT_CALL(original_callback, Run);
  built_in_remote_changes_callback.Run(std::nullopt);
}

TEST_F(PasswordStoreProxyBackendBaseTest,
       ProfileNoLocalSupportCallSyncCallbackForTheBuiltInBackend) {
  base::MockCallback<base::RepeatingClosure> original_callback;

  // Both backends receive a callback that they trigger for new remote changes.
  base::RepeatingClosure built_in_sync_callback;
  EXPECT_CALL(built_in_backend(), InitBackend)
      .WillOnce(SaveArg<2>(&built_in_sync_callback));
  EXPECT_CALL(android_backend(), InitBackend);
  proxy_backend().InitBackend(nullptr, base::DoNothing(),
                              original_callback.Get(), base::DoNothing());
  EXPECT_CALL(android_backend(), OnSyncServiceInitialized(sync_service()));
  proxy_backend().OnSyncServiceInitialized(sync_service());

  // With sync enabled, only the built-in backend calls the original callback.
  EnablePasswordSync();

  EXPECT_CALL(original_callback, Run);
  built_in_sync_callback.Run();
  testing::Mock::VerifyAndClearExpectations(&original_callback);

  // With sync is disabled, the built-in backend remains the only to call the
  // original callback.
  DisablePasswordSync();

  EXPECT_CALL(original_callback, Run);
  built_in_sync_callback.Run();
}

TEST_F(PasswordStoreProxyBackendBaseTest, BuiltInBackendClearedOnSyncInit) {
  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);
  EnablePasswordSync();

  EXPECT_CALL(android_backend(), OnSyncServiceInitialized(sync_service()));
  EXPECT_CALL(built_in_backend(), RemoveLoginsCreatedBetweenAsync(
                                      _, base::Time(), base::Time::Max(), _));
  proxy_backend().OnSyncServiceInitialized(sync_service());
}

TEST_F(PasswordStoreProxyBackendBaseTest,
       BuiltInBackendNotClearedOnSyncInit_WhenUnenrolled) {
  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);
  prefs()->SetBoolean(prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
                      true);
  EnablePasswordSync();

  EXPECT_CALL(android_backend(), OnSyncServiceInitialized(sync_service()));
  EXPECT_CALL(built_in_backend(), RemoveLoginsCreatedBetweenAsync).Times(0);
  proxy_backend().OnSyncServiceInitialized(sync_service());
}

TEST_F(PasswordStoreProxyBackendBaseTest,
       BuiltInBackendNotClearedOnSyncInit_WhenInitialUPMMigrationNotFinished) {
  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 0);
  EnablePasswordSync();

  EXPECT_CALL(android_backend(), OnSyncServiceInitialized(sync_service()));
  EXPECT_CALL(built_in_backend(), RemoveLoginsCreatedBetweenAsync).Times(0);
  proxy_backend().OnSyncServiceInitialized(sync_service());
}

TEST_F(PasswordStoreProxyBackendBaseTest,
       BuiltInBackendNotClearedOnSyncInit_WhenSyncDisabled) {
  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 0);

  EXPECT_CALL(android_backend(), OnSyncServiceInitialized(sync_service()));
  EXPECT_CALL(built_in_backend(), RemoveLoginsCreatedBetweenAsync).Times(0);
  proxy_backend().OnSyncServiceInitialized(sync_service());
}

TEST_F(PasswordStoreProxyBackendBaseTest,
       BuiltInBackendClearedOnSyncInit_Metrics) {
  base::HistogramTester histogram_tester;
  const char kStatusMetric[] =
      "PasswordManager.PasswordStoreProxyBackend.PasswordRemovalStatus";
  const char kCountMetric[] =
      "PasswordManager.PasswordStoreProxyBackend.RemovedPasswordCount";

  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);
  EnablePasswordSync();

  PasswordStoreChangeList change_list;
  change_list.emplace_back(Type::REMOVE, CreateTestForm());
  change_list.emplace_back(Type::REMOVE, CreateTestForm());

  EXPECT_CALL(android_backend(), OnSyncServiceInitialized(sync_service()));
  EXPECT_CALL(built_in_backend(), RemoveLoginsCreatedBetweenAsync(
                                      _, base::Time(), base::Time::Max(), _))
      .WillOnce(base::test::RunOnceCallback<3>(change_list));
  proxy_backend().OnSyncServiceInitialized(sync_service());

  histogram_tester.ExpectTotalCount(kStatusMetric, 1);
  histogram_tester.ExpectBucketCount(kStatusMetric, true, 1);
  histogram_tester.ExpectTotalCount(kCountMetric, 1);
  histogram_tester.ExpectBucketCount(kCountMetric, change_list.size(), 1);
}

TEST_F(PasswordStoreProxyBackendBaseTest,
       InitialUPMMigrationPrefIsResetOnSyncInit) {
  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);
  DisablePasswordSync();

  EXPECT_CALL(android_backend(), OnSyncServiceInitialized(sync_service()));
  proxy_backend().OnSyncServiceInitialized(sync_service());
  EXPECT_EQ(0, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
}

// Holds the conditions affecting UPM eligibility and the backends
// which should be used for each.
struct UpmVariationParam {
  bool is_sync_enabled = false;
  bool is_unenrolled = false;
  bool is_login_db_empty = false;
  bool is_initial_migration_finished = false;
  bool android_is_main_backend = false;
};

class PasswordStoreProxyBackendTest
    : public PasswordStoreProxyBackendBaseTest,
      public testing::WithParamInterface<UpmVariationParam> {
 public:
  void SetUp() override {
    PasswordStoreProxyBackendBaseTest::SetUp();

    if (GetParam().is_sync_enabled) {
      EnablePasswordSync();
    } else {
      DisablePasswordSync();
    }
    prefs()->SetBoolean(prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
                        GetParam().is_unenrolled);
    prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices,
                        GetParam().is_initial_migration_finished ? 1 : 0);
    prefs()->SetBoolean(prefs::kEmptyProfileStoreLoginDatabase,
                        GetParam().is_login_db_empty);

    if (GetParam().is_sync_enabled &&
        GetParam().is_initial_migration_finished && !GetParam().is_unenrolled) {
      // The login DB should be cleared for healthy syncing users.
      EXPECT_CALL(built_in_backend(),
                  RemoveLoginsCreatedBetweenAsync(_, base::Time(),
                                                  base::Time::Max(), _));
    }

    EXPECT_CALL(android_backend(), InitBackend);
    EXPECT_CALL(built_in_backend(), InitBackend);
    proxy_backend().InitBackend(nullptr, base::DoNothing(), base::DoNothing(),
                                base::DoNothing());
    EXPECT_CALL(android_backend(), OnSyncServiceInitialized(sync_service()));
    proxy_backend().OnSyncServiceInitialized(sync_service());
  }

  MockPasswordStoreBackend& main_backend() {
    return GetParam().android_is_main_backend ? android_backend()
                                              : built_in_backend();
  }

  MockPasswordStoreBackend& shadow_backend() {
    return GetParam().android_is_main_backend ? built_in_backend()
                                              : android_backend();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(PasswordStoreProxyBackendTest, UseMainBackendToGetAllLoginsAsync) {
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<LoginsResult>(ElementsAreArray(CreateTestLogins()))));

  EXPECT_CALL(main_backend(), GetAllLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
        std::move(reply).Run(CreateTestLogins());
      })));
  EXPECT_CALL(shadow_backend(), GetAllLoginsAsync).Times(0);

  proxy_backend().GetAllLoginsAsync(mock_reply.Get());
}

TEST_P(PasswordStoreProxyBackendTest,
       UseMainBackendToGetAutofillableLoginsAsync) {
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<LoginsResult>(ElementsAreArray(CreateTestLogins()))));

  EXPECT_CALL(main_backend(), GetAutofillableLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
        std::move(reply).Run(CreateTestLogins());
      })));
  EXPECT_CALL(shadow_backend(), GetAutofillableLoginsAsync).Times(0);

  proxy_backend().GetAutofillableLoginsAsync(mock_reply.Get());
}

TEST_P(PasswordStoreProxyBackendTest, UseMainBackendToFillMatchingLoginsAsync) {
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<LoginsResult>(ElementsAreArray(CreateTestLogins()))));

  EXPECT_CALL(main_backend(), FillMatchingLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
        std::move(reply).Run(CreateTestLogins());
      })));
  EXPECT_CALL(shadow_backend(), FillMatchingLoginsAsync).Times(0);

  proxy_backend().FillMatchingLoginsAsync(mock_reply.Get(),
                                          /*include_psl=*/false,
                                          std::vector<PasswordFormDigest>());
}

TEST_P(PasswordStoreProxyBackendTest, UseMainBackendToAddLoginAsync) {
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form = CreateTestForm();
  PasswordStoreChangeList change_list;
  change_list.push_back(PasswordStoreChange(Type::ADD, form));
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(change_list))));

  EXPECT_CALL(main_backend(), AddLoginAsync(Eq(form), _))
      .WillOnce(WithArg<1>(
          Invoke([&change_list](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(change_list);
          })));
  EXPECT_CALL(shadow_backend(), AddLoginAsync).Times(0);

  proxy_backend().AddLoginAsync(form, mock_reply.Get());
}

TEST_P(PasswordStoreProxyBackendTest, UseMainBackendToUpdateLoginAsync) {
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form = CreateTestForm();
  PasswordStoreChangeList change_list;
  change_list.push_back(PasswordStoreChange(Type::UPDATE, form));
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(change_list))));

  EXPECT_CALL(main_backend(), UpdateLoginAsync(Eq(form), _))
      .WillOnce(WithArg<1>(
          Invoke([&change_list](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(change_list);
          })));
  EXPECT_CALL(shadow_backend(), UpdateLoginAsync).Times(0);

  proxy_backend().UpdateLoginAsync(form, mock_reply.Get());
}

TEST_P(PasswordStoreProxyBackendTest, UseBothBackendsToRemoveLoginAsyncIfUPM) {
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form = CreateTestForm();
  PasswordStoreChangeList change_list;
  change_list.push_back(PasswordStoreChange(Type::REMOVE, form));
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(change_list))));

  EXPECT_CALL(main_backend(), RemoveLoginAsync(_, Eq(form), _))
      .WillOnce(WithArg<2>(
          Invoke([&change_list](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(change_list);
          })));

  // The shadow backend should only be called to remove logins if the main
  // backend is the android backend, to ensure the login db passwords are
  // also removed.
  EXPECT_CALL(shadow_backend(), RemoveLoginAsync(_, Eq(form), _))
      .Times(GetParam().android_is_main_backend ? 1 : 0);
  proxy_backend().RemoveLoginAsync(FROM_HERE, form, mock_reply.Get());
}

TEST_P(PasswordStoreProxyBackendTest,
       UseBothBackendsToRemoveLoginsByURLAndTimeAsyncIfUPM) {
  base::Time kStart = base::Time::FromTimeT(111111);
  base::Time kEnd = base::Time::FromTimeT(22222222);
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form = CreateTestForm();
  PasswordStoreChangeList change_list;
  change_list.push_back(PasswordStoreChange(Type::REMOVE, form));
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(change_list))));

  EXPECT_CALL(main_backend(),
              RemoveLoginsByURLAndTimeAsync(_, _, Eq(kStart), Eq(kEnd), _, _))
      .WillOnce(WithArg<5>(
          Invoke([&change_list](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(change_list);
          })));

  // The shadow backend should only be called to remove logins if the main
  // backend is the android backend, to ensure the login db passwords are
  // also removed.
  EXPECT_CALL(shadow_backend(),
              RemoveLoginsByURLAndTimeAsync(_, _, Eq(kStart), Eq(kEnd), _, _))
      .Times(GetParam().android_is_main_backend ? 1 : 0);
  proxy_backend().RemoveLoginsByURLAndTimeAsync(
      FROM_HERE, base::BindRepeating(&FilterNoUrl), kStart, kEnd,
      base::NullCallback(), mock_reply.Get());
}

TEST_P(PasswordStoreProxyBackendTest,
       UseBothBackendsToRemoveLoginsCreatedBetweenAsyncIfUPM) {
  base::Time kStart = base::Time::FromTimeT(111111);
  base::Time kEnd = base::Time::FromTimeT(22222222);
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordStoreChangeList change_list;
  change_list.push_back(PasswordStoreChange(Type::REMOVE, CreateTestForm()));
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(change_list))));

  EXPECT_CALL(main_backend(),
              RemoveLoginsCreatedBetweenAsync(_, Eq(kStart), Eq(kEnd), _))
      .WillOnce(WithArg<3>(
          Invoke([&change_list](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(change_list);
          })));
  EXPECT_CALL(shadow_backend(),
              RemoveLoginsCreatedBetweenAsync(_, Eq(kStart), Eq(kEnd), _))
      .Times(GetParam().android_is_main_backend ? 1 : 0);
  proxy_backend().RemoveLoginsCreatedBetweenAsync(FROM_HERE, kStart, kEnd,
                                                  mock_reply.Get());
}

TEST_P(PasswordStoreProxyBackendTest,
       UseMainBackendToDisableAutoSignInForOriginsAsync) {
  base::MockCallback<base::OnceClosure> mock_reply;
  EXPECT_CALL(mock_reply, Run);
  EXPECT_CALL(main_backend(), DisableAutoSignInForOriginsAsync)
      .WillOnce(WithArg<1>(
          Invoke([](base::OnceClosure reply) { std::move(reply).Run(); })));
  EXPECT_CALL(shadow_backend(), DisableAutoSignInForOriginsAsync).Times(0);
  proxy_backend().DisableAutoSignInForOriginsAsync(
      base::BindRepeating(&FilterNoUrl), mock_reply.Get());
}

TEST_P(PasswordStoreProxyBackendTest,
       UseMainBackendToGetSmartBubbleStatsStore) {
  EXPECT_CALL(main_backend(), GetSmartBubbleStatsStore);
  EXPECT_CALL(shadow_backend(), GetSmartBubbleStatsStore).Times(0);
  proxy_backend().GetSmartBubbleStatsStore();
}

INSTANTIATE_TEST_SUITE_P(
    PasswordStoreProxyBackendBaseTest,
    PasswordStoreProxyBackendTest,
    // PasswordStoreProxyBackend is created only for `ProfilePasswordStore` when
    // `UseUpmLocalAndSeparateStoresState` is `kOff`. In this configuration
    // there are 5 possible variables which can influence when `android_backend`
    // is used. All 32 configurations are tested here.
    testing::Values(UpmVariationParam{.is_sync_enabled = false,
                                      .is_unenrolled = false,
                                      .is_login_db_empty = false,
                                      .is_initial_migration_finished = false,
                                      .android_is_main_backend = false},
                    UpmVariationParam{.is_sync_enabled = true,
                                      .is_unenrolled = false,
                                      .is_login_db_empty = false,
                                      .is_initial_migration_finished = false,
                                      .android_is_main_backend = false},
                    UpmVariationParam{.is_sync_enabled = false,
                                      .is_unenrolled = true,
                                      .is_login_db_empty = false,
                                      .is_initial_migration_finished = false,
                                      .android_is_main_backend = false},
                    UpmVariationParam{.is_sync_enabled = true,
                                      .is_unenrolled = true,
                                      .is_login_db_empty = false,
                                      .is_initial_migration_finished = false,
                                      .android_is_main_backend = false},
                    UpmVariationParam{.is_sync_enabled = false,
                                      .is_unenrolled = false,
                                      .is_login_db_empty = true,
                                      .is_initial_migration_finished = false,
                                      .android_is_main_backend = false},
                    UpmVariationParam{.is_sync_enabled = true,
                                      .is_unenrolled = false,
                                      .is_login_db_empty = true,
                                      .is_initial_migration_finished = false,
                                      .android_is_main_backend = true},
                    UpmVariationParam{.is_sync_enabled = false,
                                      .is_unenrolled = true,
                                      .is_login_db_empty = true,
                                      .is_initial_migration_finished = false,
                                      .android_is_main_backend = false},
                    UpmVariationParam{.is_sync_enabled = true,
                                      .is_unenrolled = true,
                                      .is_login_db_empty = true,
                                      .is_initial_migration_finished = false,
                                      .android_is_main_backend = true},
                    UpmVariationParam{.is_sync_enabled = false,
                                      .is_unenrolled = false,
                                      .is_login_db_empty = false,
                                      .is_initial_migration_finished = true,
                                      .android_is_main_backend = false},
                    UpmVariationParam{.is_sync_enabled = true,
                                      .is_unenrolled = false,
                                      .is_login_db_empty = false,
                                      .is_initial_migration_finished = true,
                                      .android_is_main_backend = true},
                    UpmVariationParam{.is_sync_enabled = false,
                                      .is_unenrolled = true,
                                      .is_login_db_empty = false,
                                      .is_initial_migration_finished = true,
                                      .android_is_main_backend = false},
                    UpmVariationParam{.is_sync_enabled = true,
                                      .is_unenrolled = true,
                                      .is_login_db_empty = false,
                                      .is_initial_migration_finished = true,
                                      .android_is_main_backend = false},
                    UpmVariationParam{.is_sync_enabled = false,
                                      .is_unenrolled = false,
                                      .is_login_db_empty = true,
                                      .is_initial_migration_finished = true,
                                      .android_is_main_backend = false},
                    UpmVariationParam{.is_sync_enabled = true,
                                      .is_unenrolled = false,
                                      .is_login_db_empty = true,
                                      .is_initial_migration_finished = true,
                                      .android_is_main_backend = true},
                    UpmVariationParam{.is_sync_enabled = false,
                                      .is_unenrolled = true,
                                      .is_login_db_empty = true,
                                      .is_initial_migration_finished = true,
                                      .android_is_main_backend = false},
                    UpmVariationParam{.is_sync_enabled = true,
                                      .is_unenrolled = true,
                                      .is_login_db_empty = true,
                                      .is_initial_migration_finished = true,
                                      .android_is_main_backend = true}),
    [](const ::testing::TestParamInfo<UpmVariationParam>& info) {
      std::string name;
      name += info.param.is_sync_enabled ? "Syncing" : "Local";
      name += info.param.is_unenrolled ? "Unenrolled" : "";
      name += info.param.is_initial_migration_finished ? "" : "NotMigrated";
      name += info.param.is_login_db_empty ? "EmptyDB" : "";
      return name;
    });

}  // namespace password_manager
