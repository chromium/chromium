// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend.h"

#include <memory>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/android/fake_password_manager_lifecycle_helper.h"
#include "chrome/browser/password_manager/android/mock_password_sync_controller_delegate_bridge.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_dispatcher_bridge.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_receiver_bridge.h"
#include "chrome/browser/password_manager/android/password_sync_controller_delegate_android.h"
#include "chrome/browser/password_manager/android/password_sync_controller_delegate_bridge_impl.h"
#include "components/password_manager/core/browser/android_backend_error.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_android_backend_api_error_codes.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::NiceMock;
using testing::Optional;
using testing::Return;
using testing::StrictMock;
using testing::VariantWith;
using testing::WithArg;
using JobId = PasswordStoreAndroidBackendDispatcherBridge::JobId;

constexpr char kTestAccount[] = "test@gmail.com";
const std::u16string kTestUsername(u"Todd Tester");
const std::u16string kTestPassword(u"S3cr3t");
constexpr char kTestUrl[] = "https://example.com";
constexpr base::Time kTestDateCreated = base::Time::FromTimeT(1500);
constexpr base::TimeDelta kTestLatencyDelta = base::Milliseconds(123u);
constexpr char kBackendErrorCodeMetric[] =
    "PasswordManager.PasswordStoreAndroidBackend.ErrorCode";
constexpr char kUnenrollmentHistogram[] =
    "PasswordManager.UnenrolledFromUPMDueToErrors";
constexpr char kUPMActiveHistogram[] =
    "PasswordManager.UnifiedPasswordManager.ActiveStatus2";
constexpr char kRetryHistogramBase[] =
    "PasswordManager.PasswordStoreAndroidBackend.Retry";
constexpr AndroidBackendErrorType kExternalErrorType =
    AndroidBackendErrorType::kExternalError;
constexpr int kInternalApiErrorCode =
    static_cast<int>(AndroidBackendAPIErrorCode::kInternalError);
constexpr AndroidBackendErrorType kCleanedUpWithoutResponseErrorType =
    AndroidBackendErrorType::kCleanedUpWithoutResponse;
constexpr JobId kJobId{1337};
const int kNetworkErrorCode =
    static_cast<int>(AndroidBackendAPIErrorCode::kNetworkError);

MATCHER_P2(ExpectError, error_type, recovery_type, "") {
  return absl::holds_alternative<PasswordStoreBackendError>(arg) &&
         error_type == absl::get<PasswordStoreBackendError>(arg).type &&
         recovery_type ==
             absl::get<PasswordStoreBackendError>(arg).recovery_type;
}

MATCHER_P(ExpectSyncingAccount, expectation, "") {
  return absl::holds_alternative<
             PasswordStoreAndroidBackendDispatcherBridge::SyncingAccount>(
             arg) &&
         expectation ==
             absl::get<
                 PasswordStoreAndroidBackendDispatcherBridge::SyncingAccount>(
                 arg)
                 .value();
}

MATCHER(ExpectLocalAccount, "") {
  return absl::holds_alternative<PasswordStoreOperationTarget>(arg) &&
         (PasswordStoreOperationTarget::kLocalStorage ==
          absl::get<PasswordStoreOperationTarget>(arg));
}

std::vector<std::unique_ptr<PasswordForm>> CreateTestLogins() {
  std::vector<std::unique_ptr<PasswordForm>> forms;
  forms.push_back(CreateEntry("Todd Tester", "S3cr3t",
                              GURL(u"https://example.com"),
                              /*psl=*/false,
                              /*affiliation=*/false));
  forms.push_back(CreateEntry("Marcus McSpartanGregor", "S0m3th1ngCr34t1v3",
                              GURL(u"https://m.example.com"), /*psl=*/true,
                              /*affiliation=*/false));
  return forms;
}

PasswordForm CreateTestLogin(const std::u16string& username_value,
                             const std::u16string& password_value,
                             const std::string& url,
                             base::Time date_created) {
  PasswordForm form;
  form.username_value = username_value;
  form.password_value = password_value;
  form.url = GURL(url);
  form.signon_realm = url;
  form.date_created = date_created;
  return form;
}

AndroidBackendError CreateNetworkError() {
  AndroidBackendError error{AndroidBackendErrorType::kExternalError};
  error.api_error_code = kNetworkErrorCode;
  return error;
}

std::vector<PasswordForm> UnwrapForms(
    std::vector<std::unique_ptr<PasswordForm>> password_ptrs) {
  std::vector<PasswordForm> forms;
  forms.reserve(password_ptrs.size());
  for (auto& password : password_ptrs) {
    forms.push_back(std::move(*password));
  }
  return forms;
}

PasswordForm FormWithDisabledAutoSignIn(const PasswordForm& form_to_update) {
  PasswordForm result = form_to_update;
  result.skip_zero_click = 1;
  return result;
}

std::string DurationMetricName(const std::string& method_name) {
  return "PasswordManager.PasswordStoreAndroidBackend." + method_name +
         ".Latency";
}

std::string SuccessMetricName(const std::string& method_name) {
  return "PasswordManager.PasswordStoreAndroidBackend." + method_name +
         ".Success";
}

std::string PerMethodErrorCodeMetricName(const std::string& method_name) {
  return "PasswordManager.PasswordStoreAndroidBackend." + method_name +
         ".ErrorCode";
}

std::string ApiErrorMetricName(const std::string& method_name) {
  return "PasswordManager.PasswordStoreAndroidBackend." + method_name +
         ".APIError";
}

class MockPasswordStoreAndroidBackendBridgeHelper
    : public PasswordStoreAndroidBackendBridgeHelper {
 public:
  MOCK_METHOD(void, SetConsumer, (base::WeakPtr<Consumer>), (override));
  MOCK_METHOD(JobId, GetAllLogins, (Account), (override));
  MOCK_METHOD(JobId, GetAutofillableLogins, (Account), (override));
  MOCK_METHOD(JobId,
              GetLoginsForSignonRealm,
              (const std::string&, Account),
              (override));
  MOCK_METHOD(JobId, AddLogin, (const PasswordForm&, Account), (override));
  MOCK_METHOD(JobId, UpdateLogin, (const PasswordForm&, Account), (override));
  MOCK_METHOD(JobId, RemoveLogin, (const PasswordForm&, Account), (override));
};

}  // namespace

class PasswordStoreAndroidBackendTest : public testing::Test {
 protected:
  PasswordStoreAndroidBackendTest() {
    prefs_.registry()->RegisterBooleanPref(
        prefs::kUnenrolledFromGoogleMobileServicesDueToErrors, false);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kSavePasswordsSuspendedByError, false);
    prefs_.registry()->RegisterIntegerPref(
        prefs::kUnenrolledFromGoogleMobileServicesAfterApiErrorCode, 0);
    prefs_.registry()->RegisterIntegerPref(
        prefs::kUnenrolledFromGoogleMobileServicesWithErrorListVersion, 0);
    prefs_.registry()->RegisterIntegerPref(
        prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);
    prefs_.registry()->RegisterDoublePref(prefs::kTimeOfLastMigrationAttempt,
                                          20.22);
    prefs_.registry()->RegisterBooleanPref(prefs::kSettingsMigratedToUPM, true);

    backend_ = std::make_unique<PasswordStoreAndroidBackend>(
        base::PassKey<class PasswordStoreAndroidBackendTest>(),
        CreateMockBridgeHelper(), CreateFakeLifecycleHelper(),
        CreatePasswordSyncControllerDelegate(), &prefs_);
  }

  ~PasswordStoreAndroidBackendTest() override {
    lifecycle_helper_->UnregisterObserver();
    lifecycle_helper_ = nullptr;
    testing::Mock::VerifyAndClearExpectations(bridge_helper_);
  }

  PasswordStoreBackend& backend() { return *backend_; }
  PasswordStoreAndroidBackendReceiverBridge::Consumer& consumer() {
    return *backend_;
  }
  MockPasswordStoreAndroidBackendBridgeHelper* bridge_helper() {
    return bridge_helper_;
  }
  FakePasswordManagerLifecycleHelper* lifecycle_helper() {
    return lifecycle_helper_;
  }
  syncer::TestSyncService* sync_service() { return &sync_service_; }
  PasswordSyncControllerDelegateAndroid* sync_controller_delegate() {
    return sync_controller_delegate_;
  }
  PrefService* prefs() { return &prefs_; }
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void EnableSyncForTestAccount() {
    sync_service_.GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, {syncer::UserSelectableType::kPasswords});
    AccountInfo account_info;
    account_info.email = kTestAccount;
    sync_service_.SetAccountInfo(account_info);
  }

  void DisableSyncFeature() {
    sync_service_.GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, /*types=*/{});
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  std::unique_ptr<PasswordStoreAndroidBackendBridgeHelper>
  CreateMockBridgeHelper() {
    auto unique_bridge_helper = std::make_unique<
        StrictMock<MockPasswordStoreAndroidBackendBridgeHelper>>();
    bridge_helper_ = unique_bridge_helper.get();
    EXPECT_CALL(*bridge_helper_, SetConsumer);
    return unique_bridge_helper;
  }

  std::unique_ptr<PasswordManagerLifecycleHelper> CreateFakeLifecycleHelper() {
    auto new_helper = std::make_unique<FakePasswordManagerLifecycleHelper>();
    lifecycle_helper_ = new_helper.get();
    return new_helper;
  }

  std::unique_ptr<PasswordSyncControllerDelegateAndroid>
  CreatePasswordSyncControllerDelegate() {
    auto unique_delegate = std::make_unique<
        PasswordSyncControllerDelegateAndroid>(
        std::make_unique<NiceMock<MockPasswordSyncControllerDelegateBridge>>(),
        base::DoNothing());
    sync_controller_delegate_ = unique_delegate.get();
    return unique_delegate;
  }

  std::unique_ptr<PasswordStoreAndroidBackend> backend_;
  raw_ptr<StrictMock<MockPasswordStoreAndroidBackendBridgeHelper>>
      bridge_helper_;
  raw_ptr<FakePasswordManagerLifecycleHelper> lifecycle_helper_;
  raw_ptr<PasswordSyncControllerDelegateAndroid> sync_controller_delegate_;
  syncer::TestSyncService sync_service_;
  TestingPrefServiceSimple prefs_;
};

TEST_F(PasswordStoreAndroidBackendTest, CallsCompletionCallbackAfterInit) {
  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;
  EXPECT_CALL(completion_callback, Run(true));
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), completion_callback.Get());
}

TEST_F(PasswordStoreAndroidBackendTest, CallsBridgeForLogins) {
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  EnableSyncForTestAccount();
  backend().OnSyncServiceInitialized(sync_service());

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(),
              GetAllLogins(ExpectSyncingAccount(kTestAccount)))
      .WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());

  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();
  EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));
  consumer().OnCompleteWithLogins(kJobId, UnwrapForms(CreateTestLogins()));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendTest, FillMatchingLoginsNoPSL) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  base::MockCallback<LoginsOrErrorReply> mock_reply;

  const JobId kFirstJobId{1337};
  EXPECT_CALL(*bridge_helper(), GetLoginsForSignonRealm)
      .WillOnce(Return(kFirstJobId));

  std::string TestURL1("https://firstexample.com");
  std::string TestURL2("https://secondexample.com");

  std::vector<PasswordFormDigest> forms;
  forms.push_back(PasswordFormDigest(PasswordForm::Scheme::kHtml, TestURL1,
                                     GURL(TestURL1)));
  forms.push_back(PasswordFormDigest(PasswordForm::Scheme::kHtml, TestURL2,
                                     GURL(TestURL2)));
  backend().FillMatchingLoginsAsync(mock_reply.Get(), /*include_psl=*/false,
                                    forms);

  // Imitate login retrieval.
  PasswordForm matching_signon_realm =
      CreateTestLogin(kTestUsername, kTestPassword, TestURL1, kTestDateCreated);
  PasswordForm matching_federated = CreateTestLogin(
      kTestUsername, kTestPassword,
      "federation://secondexample.com/google.com/", kTestDateCreated);
  PasswordForm not_matching =
      CreateTestLogin(kTestUsername, kTestPassword,
                      "https://mobile.secondexample.com/", kTestDateCreated);

  const JobId kSecondJobId{1338};
  EXPECT_CALL(*bridge_helper(), GetLoginsForSignonRealm)
      .WillOnce(Return(kSecondJobId));
  // Logins will be retrieved for forms from |forms| in a backwards order.
  consumer().OnCompleteWithLogins(kFirstJobId,
                                  {matching_federated, not_matching});
  RunUntilIdle();

  // Retrieving logins for the last form should trigger the final callback.
  LoginsResult expected_logins;
  expected_logins.push_back(std::make_unique<PasswordForm>(matching_federated));
  expected_logins.push_back(
      std::make_unique<PasswordForm>(matching_signon_realm));
  EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));

  task_environment_.FastForwardBy(kTestLatencyDelta);
  consumer().OnCompleteWithLogins(kSecondJobId, {matching_signon_realm});
  RunUntilIdle();

  histogram_tester.ExpectTimeBucketCount(
      DurationMetricName("FillMatchingLoginsAsync"), kTestLatencyDelta, 1);
}

TEST_F(PasswordStoreAndroidBackendTest, FillMatchingLoginsPSL) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  base::MockCallback<LoginsOrErrorReply> mock_reply;

  const JobId kFirstJobId{1337};
  EXPECT_CALL(*bridge_helper(), GetLoginsForSignonRealm)
      .WillOnce(Return(kFirstJobId));

  std::string TestURL1("https://firstexample.com");
  std::string TestURL2("https://secondexample.com");

  std::vector<PasswordFormDigest> forms;
  forms.push_back(PasswordFormDigest(PasswordForm::Scheme::kHtml, TestURL1,
                                     GURL(TestURL1)));
  forms.push_back(PasswordFormDigest(PasswordForm::Scheme::kHtml, TestURL2,
                                     GURL(TestURL2)));
  backend().FillMatchingLoginsAsync(mock_reply.Get(), /*include_psl=*/true,
                                    forms);

  // Imitate login retrieval.
  PasswordForm psl_matching =
      CreateTestLogin(kTestUsername, kTestPassword,
                      "https://mobile.firstexample.com/", kTestDateCreated);
  PasswordForm not_matching =
      CreateTestLogin(kTestUsername, kTestPassword,
                      "https://nomatchfirstexample.com/", kTestDateCreated);
  PasswordForm psl_matching_federated = CreateTestLogin(
      kTestUsername, kTestPassword,
      "federation://mobile.secondexample.com/google.com/", kTestDateCreated);

  const JobId kSecondJobId{1338};
  EXPECT_CALL(*bridge_helper(), GetLoginsForSignonRealm)
      .WillOnce(Return(kSecondJobId));
  // Logins will be retrieved for forms from |forms| in a backwards order.
  consumer().OnCompleteWithLogins(kFirstJobId, {psl_matching_federated});
  RunUntilIdle();

  // Retrieving logins for the last form should trigger the final callback.
  LoginsResult expected_logins;
  expected_logins.push_back(std::make_unique<PasswordForm>(psl_matching));
  expected_logins.push_back(
      std::make_unique<PasswordForm>(psl_matching_federated));
  EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));

  task_environment_.FastForwardBy(kTestLatencyDelta);
  consumer().OnCompleteWithLogins(kSecondJobId, {psl_matching, not_matching});
  RunUntilIdle();
  histogram_tester.ExpectTimeBucketCount(
      DurationMetricName("FillMatchingLoginsAsync"), kTestLatencyDelta, 1);
}

TEST_F(PasswordStoreAndroidBackendTest, FillMatchingLoginsGooglePSLMatch) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  base::MockCallback<LoginsOrErrorReply> mock_reply;

  const JobId kFirstJobId{1337};
  EXPECT_CALL(*bridge_helper(), GetLoginsForSignonRealm)
      .WillOnce(Return(kFirstJobId));

  std::string TestURL1("https://google.com");

  std::vector<PasswordFormDigest> forms;
  forms.push_back(PasswordFormDigest(PasswordForm::Scheme::kHtml, TestURL1,
                                     GURL(TestURL1)));
  backend().FillMatchingLoginsAsync(mock_reply.Get(), /*include_psl=*/true,
                                    forms);

  // Imitate login retrieval.
  PasswordForm exact_match = CreateTestLogin(
      kTestUsername, kTestPassword, "https://google.com/", kTestDateCreated);
  PasswordForm psl_match =
      CreateTestLogin(kTestUsername, kTestPassword,
                      "https://accounts.google.com/", kTestDateCreated);

  // Retrieving logins for the last form should trigger the final callback.
  LoginsResult expected_logins;
  expected_logins.push_back(std::make_unique<PasswordForm>(exact_match));
  EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));

  consumer().OnCompleteWithLogins(kFirstJobId, {exact_match, psl_match});
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendTest, CallsBridgeForAutofillableLogins) {
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAutofillableLogins).WillOnce(Return(kJobId));
  backend().GetAutofillableLoginsAsync(mock_reply.Get());

  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();
  EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));
  consumer().OnCompleteWithLogins(kJobId, UnwrapForms(CreateTestLogins()));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendTest, CallsBridgeForLoginsForAccount) {
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kJobId));
  absl::optional<std::string> account = "mytestemail@gmail.com";
  backend().GetAllLoginsForAccountAsync(account, mock_reply.Get());

  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();
  EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));
  consumer().OnCompleteWithLogins(kJobId, UnwrapForms(CreateTestLogins()));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendTest, CallsBridgeForRemoveLogin) {
  DisableSyncFeature();
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  const JobId kRemoveLoginJobId{13388};
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;

  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  EXPECT_CALL(*bridge_helper(), RemoveLogin(form, ExpectLocalAccount()))
      .WillOnce(Return(kRemoveLoginJobId));
  backend().RemoveLoginAsync(form, mock_reply.Get());

  PasswordStoreChangeList expected_changes;
  expected_changes.emplace_back(
      PasswordStoreChange(PasswordStoreChange::REMOVE, form));
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(expected_changes))));
  consumer().OnLoginsChanged(kRemoveLoginJobId, expected_changes);
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendTest,
       CallsBridgeForRemoveLoginsByURLAndTime) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  base::MockCallback<PasswordChangesOrErrorReply> mock_deletion_reply;
  base::RepeatingCallback<bool(const GURL&)> url_filter = base::BindRepeating(
      [](const GURL& url) { return url == GURL(kTestUrl); });
  base::Time delete_begin = base::Time::FromTimeT(1000);
  base::Time delete_end = base::Time::FromTimeT(2000);
  const std::string kDurationMetric =
      DurationMetricName("RemoveLoginsByURLAndTimeAsync");
  const std::string kSuccessMetric =
      SuccessMetricName("RemoveLoginsByURLAndTimeAsync");

  // Check that calling RemoveLoginsByURLAndTime triggers logins retrieval
  // first.
  const JobId kGetLoginsJobId{13387};
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kGetLoginsJobId));
  backend().RemoveLoginsByURLAndTimeAsync(url_filter, delete_begin, delete_end,
                                          base::OnceCallback<void(bool)>(),
                                          mock_deletion_reply.Get());

  // Imitate login retrieval and check that it triggers the removal of matching
  // forms.
  const JobId kRemoveLoginJobId{13388};
  EXPECT_CALL(*bridge_helper(), RemoveLogin)
      .WillOnce(Return(kRemoveLoginJobId));
  PasswordForm form_to_delete = CreateTestLogin(
      kTestUsername, kTestPassword, kTestUrl, base::Time::FromTimeT(1500));
  PasswordForm form_to_keep =
      CreateTestLogin(kTestUsername, kTestPassword, "https://differentsite.com",
                      base::Time::FromTimeT(1500));
  consumer().OnCompleteWithLogins(kGetLoginsJobId,
                                  {form_to_delete, form_to_keep});
  RunUntilIdle();
  task_environment_.FastForwardBy(kTestLatencyDelta);

  // Verify that the callback is called.
  PasswordStoreChangeList expected_changes;
  expected_changes.emplace_back(
      PasswordStoreChange(PasswordStoreChange::REMOVE, form_to_delete));
  EXPECT_CALL(mock_deletion_reply,
              Run(VariantWith<PasswordChanges>(Optional(expected_changes))));
  consumer().OnLoginsChanged(kRemoveLoginJobId, expected_changes);
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kTestLatencyDelta, 1);
  histogram_tester.ExpectUniqueSample(kSuccessMetric, 1, 1);
}

TEST_F(PasswordStoreAndroidBackendTest,
       CallsBridgeForRemoveLoginsCreatedBetween) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  base::MockCallback<PasswordChangesOrErrorReply> mock_deletion_reply;
  base::Time delete_begin = base::Time::FromTimeT(1000);
  base::Time delete_end = base::Time::FromTimeT(2000);
  const std::string kDurationMetric =
      DurationMetricName("RemoveLoginsCreatedBetweenAsync");
  const std::string kSuccessMetric =
      SuccessMetricName("RemoveLoginsCreatedBetweenAsync");

  // Check that calling RemoveLoginsCreatedBetween triggers logins retrieval
  // first.
  const JobId kGetLoginsJobId{13387};
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kGetLoginsJobId));
  backend().RemoveLoginsCreatedBetweenAsync(delete_begin, delete_end,
                                            mock_deletion_reply.Get());

  // Imitate login retrieval and check that it triggers the removal of matching
  // forms.
  const JobId kRemoveLoginJobId{13388};
  EXPECT_CALL(*bridge_helper(), RemoveLogin)
      .WillOnce(Return(kRemoveLoginJobId));
  PasswordForm form_to_delete = CreateTestLogin(
      kTestUsername, kTestPassword, kTestUrl, base::Time::FromTimeT(1500));
  PasswordForm form_to_keep = CreateTestLogin(
      kTestUsername, kTestPassword, kTestUrl, base::Time::FromTimeT(2500));
  consumer().OnCompleteWithLogins(kGetLoginsJobId,
                                  {form_to_delete, form_to_keep});
  RunUntilIdle();
  task_environment_.FastForwardBy(kTestLatencyDelta);

  // Verify that the callback is called.
  PasswordStoreChangeList expected_changes;
  expected_changes.emplace_back(
      PasswordStoreChange(PasswordStoreChange::REMOVE, form_to_delete));
  EXPECT_CALL(mock_deletion_reply,
              Run(VariantWith<PasswordChanges>(Optional(expected_changes))));
  consumer().OnLoginsChanged(kRemoveLoginJobId, expected_changes);
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kTestLatencyDelta, 1);
  histogram_tester.ExpectUniqueSample(kSuccessMetric, 1, 1);
}

TEST_F(PasswordStoreAndroidBackendTest, CallsBridgeForAddLogin) {
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  EnableSyncForTestAccount();
  backend().OnSyncServiceInitialized(sync_service());

  const JobId kAddLoginJobId{13388};
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  EXPECT_CALL(*bridge_helper(),
              AddLogin(form, ExpectSyncingAccount(kTestAccount)))
      .WillOnce(Return(kAddLoginJobId));
  backend().AddLoginAsync(form, mock_reply.Get());

  PasswordStoreChangeList expected_changes;
  expected_changes.emplace_back(
      PasswordStoreChange(PasswordStoreChange::ADD, form));
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(expected_changes))));
  consumer().OnLoginsChanged(kAddLoginJobId, expected_changes);
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendTest, CallsBridgeForUpdateLogin) {
  DisableSyncFeature();
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  const JobId kUpdateLoginJobId{13388};
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  EXPECT_CALL(*bridge_helper(), UpdateLogin(form, ExpectLocalAccount()))
      .WillOnce(Return(kUpdateLoginJobId));
  backend().UpdateLoginAsync(form, mock_reply.Get());

  PasswordStoreChangeList expected_changes;
  expected_changes.emplace_back(
      PasswordStoreChange(PasswordStoreChange::UPDATE, form));
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(expected_changes))));
  consumer().OnLoginsChanged(kUpdateLoginJobId, expected_changes);
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendTest,
       OnExternalErrorCausingExperimentUnenrollment) {
  // INTERNAL_ERROR is neither in ignored nor retriable error lists by default.
  base::HistogramTester histogram_tester;

  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());
  EXPECT_CALL(
      mock_reply,
      Run(ExpectError(PasswordStoreBackendErrorType::kUncategorized,
                      PasswordStoreBackendErrorRecoveryType::kUnrecoverable)));
  AndroidBackendError error{AndroidBackendErrorType::kExternalError};
  // Simulate receiving INTERNAL_ERROR code.
  int kInternalErrorCode =
      static_cast<int>(AndroidBackendAPIErrorCode::kInternalError);
  error.api_error_code = absl::optional<int>(kInternalErrorCode);
  consumer().OnError(kJobId, std::move(error));
  RunUntilIdle();

  EXPECT_TRUE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_EQ(prefs()->GetInteger(
                prefs::kUnenrolledFromGoogleMobileServicesAfterApiErrorCode),
            kInternalErrorCode);
  EXPECT_EQ(prefs()->GetInteger(
                prefs::kCurrentMigrationVersionToGoogleMobileServices),
            0);
  EXPECT_EQ(prefs()->GetDouble(prefs::kTimeOfLastMigrationAttempt), 0.0);
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kSettingsMigratedToUPM));

  const char kErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.ErrorCode";
  const char kAPIErrorMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.APIError";

  histogram_tester.ExpectBucketCount(kErrorCodeMetric, 7, 1);
  histogram_tester.ExpectBucketCount(kAPIErrorMetric, kInternalErrorCode, 1);
  histogram_tester.ExpectBucketCount(kUnenrollmentHistogram, true, 1);
}

TEST_F(PasswordStoreAndroidBackendTest,
       OnExternalIgnoredErrorNotCausingExperimentUnenrollment) {
  base::HistogramTester histogram_tester;

  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());
  EXPECT_CALL(
      mock_reply,
      Run(ExpectError(PasswordStoreBackendErrorType::kAuthErrorResolvable,
                      PasswordStoreBackendErrorRecoveryType::kRecoverable)));
  AndroidBackendError error{AndroidBackendErrorType::kExternalError};
  // Simulate receiving AUTH_ERROR_RESOLVABLE code.
  int kAuthErrorResolvableCode =
      static_cast<int>(AndroidBackendAPIErrorCode::kAuthErrorResolvable);
  error.api_error_code = absl::optional<int>(kAuthErrorResolvableCode);
  consumer().OnError(kJobId, std::move(error));
  RunUntilIdle();

  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_EQ(prefs()->GetInteger(
                prefs::kUnenrolledFromGoogleMobileServicesAfterApiErrorCode),
            0);
  EXPECT_NE(prefs()->GetInteger(
                prefs::kCurrentMigrationVersionToGoogleMobileServices),
            0);
  EXPECT_NE(prefs()->GetDouble(prefs::kTimeOfLastMigrationAttempt), 0.0);
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kSettingsMigratedToUPM));

  const char kErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.ErrorCode";
  const char kAPIErrorMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.APIError";

  histogram_tester.ExpectBucketCount(kErrorCodeMetric, 7, 1);
  histogram_tester.ExpectBucketCount(kAPIErrorMetric, kAuthErrorResolvableCode,
                                     1);
}

TEST_F(
    PasswordStoreAndroidBackendTest,
    OnUnretriableOperationWithExternalRetriableErrorOnCausesExperimentUnenrollment) {
  base::HistogramTester histogram_tester;

  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), AddLogin).WillOnce(Return(kJobId));
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);

  backend().AddLoginAsync(form, mock_reply.Get());

  AndroidBackendError error{AndroidBackendErrorType::kExternalError};
  // Simulate receiving NETWORK_ERROR code.
  error.api_error_code = absl::optional<int>(kNetworkErrorCode);

  // AddLogin operation is non-retriable, so the returned error should not be
  // indicated as retriable even if the error itself is retriable.
  EXPECT_CALL(
      mock_reply,
      Run(ExpectError(PasswordStoreBackendErrorType::kUncategorized,
                      PasswordStoreBackendErrorRecoveryType::kUnrecoverable)));

  consumer().OnError(kJobId, std::move(error));
  RunUntilIdle();

  // User should be unenrolled from the experiment as operation could not be
  // retried.
  EXPECT_TRUE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_EQ(prefs()->GetInteger(
                prefs::kUnenrolledFromGoogleMobileServicesAfterApiErrorCode),
            kNetworkErrorCode);
  EXPECT_EQ(prefs()->GetInteger(
                prefs::kCurrentMigrationVersionToGoogleMobileServices),
            0);
  EXPECT_EQ(prefs()->GetDouble(prefs::kTimeOfLastMigrationAttempt), 0.0);
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kSettingsMigratedToUPM));

  const char kErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.ErrorCode";
  const char kAPIErrorMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.APIError";

  histogram_tester.ExpectBucketCount(
      kErrorCodeMetric, AndroidBackendErrorType::kExternalError, 1);
  histogram_tester.ExpectBucketCount(kAPIErrorMetric, kNetworkErrorCode, 1);
  histogram_tester.ExpectBucketCount(kUnenrollmentHistogram, true, 1);

  // Per-operation retry histograms
  histogram_tester.ExpectTotalCount(
      base::StrCat({kRetryHistogramBase, ".GetAllLoginsAsync.APIError"}), 0);
  histogram_tester.ExpectTotalCount(
      base::StrCat({kRetryHistogramBase, ".GetAllLoginsAsync.Attempt"}), 0);

  // Aggregated retry histograms
  histogram_tester.ExpectTotalCount(
      base::StrCat({kRetryHistogramBase, ".APIError"}), 0);
  histogram_tester.ExpectTotalCount(
      base::StrCat({kRetryHistogramBase, ".Attempt"}), 0);
}

TEST_F(PasswordStoreAndroidBackendTest,
       OnNetworkErrorRetriableStopsRetryingAfterTimeout) {
  base::HistogramTester histogram_tester;

  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins)
      .Times(6)
      .WillRepeatedly(Return(kJobId));

  // Initiating the first call.
  backend().GetAllLoginsAsync(mock_reply.Get());

  for (int i = 0; i < 5; i++) {
    // Answering the previous call with an error.
    // Simulate receiving NETWORK_ERROR code.
    consumer().OnError(kJobId, CreateNetworkError());
    // Runs the delayed tasks which results in GetAllLogins being called on
    // the bridge.
    task_environment_.FastForwardUntilNoTasksRemain();
  }
  EXPECT_CALL(
      mock_reply,
      Run(ExpectError(PasswordStoreBackendErrorType::kUncategorized,
                      PasswordStoreBackendErrorRecoveryType::kRetriable)));
  consumer().OnError(kJobId, CreateNetworkError());

  RunUntilIdle();

  // User should not be unenrolled even if retries failed as only operations
  // performed at Chrome startup are retried.
  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_EQ(prefs()->GetInteger(
                prefs::kUnenrolledFromGoogleMobileServicesAfterApiErrorCode),
            0);
  EXPECT_NE(prefs()->GetInteger(
                prefs::kCurrentMigrationVersionToGoogleMobileServices),
            0);
  EXPECT_NE(prefs()->GetDouble(prefs::kTimeOfLastMigrationAttempt), 0.0);
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kSettingsMigratedToUPM));

  const char kErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.ErrorCode";
  const char kAPIErrorMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.APIError";

  histogram_tester.ExpectBucketCount(kErrorCodeMetric, 7, 1);
  histogram_tester.ExpectBucketCount(
      kAPIErrorMetric,
      static_cast<int>(AndroidBackendAPIErrorCode::kNetworkError), 1);

  // Per-operation retry histograms
  histogram_tester.ExpectBucketCount(
      base::StrCat({kRetryHistogramBase, ".GetAllLoginsAsync.APIError"}),
      static_cast<int>(AndroidBackendAPIErrorCode::kNetworkError), 5);

  for (int attempt = 1; attempt < 6; attempt++) {
    histogram_tester.ExpectBucketCount(
        base::StrCat({kRetryHistogramBase, ".GetAllLoginsAsync.Attempt"}),
        attempt, 1);
  }
  histogram_tester.ExpectTotalCount(
      base::StrCat({kRetryHistogramBase, ".GetAllLoginsAsync.Attempt"}), 5);

  // Aggregated retry histograms
  histogram_tester.ExpectBucketCount(
      base::StrCat({kRetryHistogramBase, ".APIError"}),
      static_cast<int>(AndroidBackendAPIErrorCode::kNetworkError), 5);

  for (int attempt = 1; attempt < 6; attempt++) {
    histogram_tester.ExpectBucketCount(
        base::StrCat({kRetryHistogramBase, ".Attempt"}), attempt, 1);
  }
  histogram_tester.ExpectTotalCount(
      base::StrCat({kRetryHistogramBase, ".Attempt"}), 5);
}

TEST_F(PasswordStoreAndroidBackendTest,
       OnNetworkErrorRetriableStopsRetryingAfterSuccess) {
  base::HistogramTester histogram_tester;

  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

  base::MockCallback<LoginsOrErrorReply> mock_reply;

  // GetAllLogins will be called once with an error and it will succeed after
  // repeating.
  const JobId kFailedJobId{1};
  const JobId kSucceedJobId{2};
  EXPECT_CALL(*bridge_helper(), GetAllLogins)
      .WillOnce(Return(kFailedJobId))
      .WillOnce(Return(kSucceedJobId));

  base::Time before_call_time = task_environment_.GetMockClock()->Now();

  // Initiating the first call.
  backend().GetAllLoginsAsync(mock_reply.Get());

  // Answering the call with an error.
  consumer().OnError(kFailedJobId, CreateNetworkError());
  task_environment_.FastForwardUntilNoTasksRemain();

  // Retry should be performed after timeout.
  base::Time after_retry_time = task_environment_.GetMockClock()->Now();
  EXPECT_GE(after_retry_time - before_call_time, base::Seconds(1));

  // Answering the call with logins.
  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();
  EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));
  consumer().OnCompleteWithLogins(kSucceedJobId,
                                  UnwrapForms(CreateTestLogins()));
  task_environment_.FastForwardUntilNoTasksRemain();

  // Per-operation retry histograms
  histogram_tester.ExpectBucketCount(
      base::StrCat({kRetryHistogramBase, ".GetAllLoginsAsync.APIError"}),
      static_cast<int>(AndroidBackendAPIErrorCode::kNetworkError), 1);

  histogram_tester.ExpectBucketCount(
      base::StrCat({kRetryHistogramBase, ".GetAllLoginsAsync.Attempt"}), 1, 1);
  histogram_tester.ExpectTotalCount(
      base::StrCat({kRetryHistogramBase, ".GetAllLoginsAsync.Attempt"}), 1);

  // Aggregated retry histograms
  histogram_tester.ExpectBucketCount(
      base::StrCat({kRetryHistogramBase, ".APIError"}),
      static_cast<int>(AndroidBackendAPIErrorCode::kNetworkError), 1);

  histogram_tester.ExpectBucketCount(
      base::StrCat({kRetryHistogramBase, ".Attempt"}), 1, 1);
  histogram_tester.ExpectTotalCount(
      base::StrCat({kRetryHistogramBase, ".Attempt"}), 1);
}

TEST_F(PasswordStoreAndroidBackendTest,
       OnExternalAuthErrorNotCausingExperimentUnenrollmentButSuspendsSaving) {
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
  ASSERT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kSavePasswordsSuspendedByError));

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());
  EXPECT_CALL(
      mock_reply,
      Run(ExpectError(PasswordStoreBackendErrorType::kAuthErrorUnresolvable,
                      PasswordStoreBackendErrorRecoveryType::kRecoverable)));
  AndroidBackendError error{AndroidBackendErrorType::kExternalError};
  // Simulate receiving AUTH_ERROR_UNRESOLVABLE code.
  int kUnresolvableAuthErrorCode =
      static_cast<int>(AndroidBackendAPIErrorCode::kAuthErrorUnresolvable);
  error.api_error_code = absl::optional<int>(kUnresolvableAuthErrorCode);
  consumer().OnError(kJobId, std::move(error));
  RunUntilIdle();

  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kSavePasswordsSuspendedByError));
}

TEST_F(PasswordStoreAndroidBackendTest,
       ResetTemporarySavingSuspensionAfterSuccessfulLogin) {
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
  prefs()->SetBoolean(prefs::kSavePasswordsSuspendedByError, true);

  // Simulate a successful logins call.
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());
  EXPECT_CALL(mock_reply, Run(_));
  task_environment_.FastForwardBy(kTestLatencyDelta);
  consumer().OnCompleteWithLogins(kJobId, {});
  RunUntilIdle();

  EXPECT_FALSE(prefs()->GetBoolean(prefs::kSavePasswordsSuspendedByError));
}

TEST_F(PasswordStoreAndroidBackendTest,
       OnExternalPassphraseRequiredCausingExperimentUnenrollment) {
  base::HistogramTester histogram_tester;

  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());
  EXPECT_CALL(
      mock_reply,
      Run(ExpectError(PasswordStoreBackendErrorType::kUncategorized,
                      PasswordStoreBackendErrorRecoveryType::kUnrecoverable)));
  AndroidBackendError error{AndroidBackendErrorType::kExternalError};
  // Simulate receiving PASSPHRASE_REQUIRED code.
  int kPassphraseRequiredErrorCode =
      static_cast<int>(AndroidBackendAPIErrorCode::kPassphraseRequired);
  error.api_error_code = absl::optional<int>(kPassphraseRequiredErrorCode);
  consumer().OnError(kJobId, std::move(error));
  RunUntilIdle();

  EXPECT_TRUE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_EQ(prefs()->GetInteger(
                prefs::kUnenrolledFromGoogleMobileServicesAfterApiErrorCode),
            kPassphraseRequiredErrorCode);
  EXPECT_EQ(prefs()->GetInteger(
                prefs::kCurrentMigrationVersionToGoogleMobileServices),
            0);
  EXPECT_EQ(prefs()->GetDouble(prefs::kTimeOfLastMigrationAttempt), 0.0);
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kSettingsMigratedToUPM));

  const char kErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.ErrorCode";
  const char kAPIErrorMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.APIError";

  histogram_tester.ExpectBucketCount(kErrorCodeMetric, 7, 1);
  histogram_tester.ExpectBucketCount(kAPIErrorMetric,
                                     kPassphraseRequiredErrorCode, 1);
  histogram_tester.ExpectBucketCount(kUnenrollmentHistogram, true, 1);
}

TEST_F(PasswordStoreAndroidBackendTest,
       OnExternalErrorInCombinationWithNoSyncError) {
  base::HistogramTester histogram_tester;

  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

  ASSERT_FALSE(sync_service()->GetAuthError().IsTransientError());
  ASSERT_FALSE(sync_service()->GetAuthError().IsPersistentError());

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());
  EXPECT_CALL(
      mock_reply,
      Run(ExpectError(PasswordStoreBackendErrorType::kUncategorized,
                      PasswordStoreBackendErrorRecoveryType::kUnrecoverable)));
  AndroidBackendError error{AndroidBackendErrorType::kExternalError};
  // Simulate receiving INTERNAL_ERROR code.
  int kInternalErrorCode =
      static_cast<int>(AndroidBackendAPIErrorCode::kInternalError);
  error.api_error_code = absl::optional<int>(kInternalErrorCode);
  consumer().OnError(kJobId, std::move(error));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendTest,
       OnExternalErrorInCombinationWithPersistentSyncError) {
  base::HistogramTester histogram_tester;

  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

  sync_service()->SetPersistentAuthError();

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());
  EXPECT_CALL(
      mock_reply,
      Run(ExpectError(PasswordStoreBackendErrorType::kUncategorized,
                      PasswordStoreBackendErrorRecoveryType::kUnrecoverable)));
  AndroidBackendError error{AndroidBackendErrorType::kExternalError};
  // Simulate receiving INTERNAL_ERROR code.
  int kInternalErrorCode =
      static_cast<int>(AndroidBackendAPIErrorCode::kInternalError);
  error.api_error_code = absl::optional<int>(kInternalErrorCode);
  consumer().OnError(kJobId, std::move(error));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendTest, DisableAutoSignInForOrigins) {
  base::HistogramTester histogram_tester;

  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  EnableSyncForTestAccount();
  backend().OnSyncServiceInitialized(sync_service());

  // Check that calling DisableAutoSignInForOrigins triggers logins retrieval
  // first.
  const JobId kGetLoginsJobId{13387};
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kGetLoginsJobId));

  base::RepeatingCallback<bool(const GURL&)> origin_filter =
      base::BindRepeating(
          [](const GURL& url) { return url == GURL(kTestUrl); });
  base::MockCallback<base::OnceClosure> mock_reply;
  backend().DisableAutoSignInForOriginsAsync(origin_filter, mock_reply.Get());

  // Imitate login retrieval and check that it triggers updating of matching
  // forms.
  PasswordForm form_to_update1 =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  PasswordForm form_to_update2 = CreateTestLogin(
      u"OtherUsername", u"OtherPassword", kTestUrl, kTestDateCreated);
  PasswordForm form_with_autosignin_disabled =
      FormWithDisabledAutoSignIn(form_to_update1);
  PasswordForm form_with_different_origin =
      CreateTestLogin(kTestUsername, kTestPassword,
                      "https://differentorigin.com", kTestDateCreated);

  const JobId kUpdateJobId1{13388};
  // Forms are updated in reverse order.
  EXPECT_CALL(*bridge_helper(),
              UpdateLogin(FormWithDisabledAutoSignIn(form_to_update2),
                          ExpectSyncingAccount(kTestAccount)))
      .WillOnce(Return(kUpdateJobId1));

  consumer().OnCompleteWithLogins(
      kGetLoginsJobId,
      {form_to_update1, form_to_update2, form_with_autosignin_disabled,
       form_with_different_origin});
  RunUntilIdle();

  // Fast forward to check latency metric recording.
  task_environment_.FastForwardBy(kTestLatencyDelta);

  // Receiving callback after updating the first login should trigger
  // updating of the second login.
  PasswordStoreChangeList change1;
  change1.emplace_back(
      PasswordStoreChange(PasswordStoreChange::UPDATE,
                          FormWithDisabledAutoSignIn(form_to_update2)));
  const JobId kUpdateJobId2{13389};
  EXPECT_CALL(*bridge_helper(),
              UpdateLogin(FormWithDisabledAutoSignIn(form_to_update1),
                          ExpectSyncingAccount(kTestAccount)))
      .WillOnce(Return(kUpdateJobId2));
  consumer().OnLoginsChanged(kUpdateJobId1, change1);
  RunUntilIdle();

  // Verify that the callback is called.
  EXPECT_CALL(mock_reply, Run());
  PasswordStoreChangeList change2;
  change2.emplace_back(
      PasswordStoreChange(PasswordStoreChange::UPDATE,
                          FormWithDisabledAutoSignIn(form_to_update1)));
  consumer().OnLoginsChanged(kUpdateJobId2, change2);
  RunUntilIdle();

  histogram_tester.ExpectTimeBucketCount(
      DurationMetricName("DisableAutoSignInForOriginsAsync"), kTestLatencyDelta,
      1);

  histogram_tester.ExpectUniqueSample(
      SuccessMetricName("DisableAutoSignInForOriginsAsync"), 1, 1);
}

TEST_F(PasswordStoreAndroidBackendTest, RemoveAllLocalLogins) {
  base::HistogramTester histogram_tester;

  backend().OnSyncServiceInitialized(sync_service());
  EnableSyncForTestAccount();
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());

  const JobId kGetLoginsJobId{13387};
  EXPECT_CALL(*bridge_helper(), GetAllLogins(ExpectLocalAccount()))
      .WillOnce(Return(kGetLoginsJobId));
  backend().ClearAllLocalPasswords();

  // Imitate login retrieval and check that it triggers the removal of forms.
  const JobId kRemoveLoginJobId{13388};
  PasswordForm form_to_delete = CreateTestLogin(
      kTestUsername, kTestPassword, kTestUrl, base::Time::FromTimeT(1500));
  EXPECT_CALL(*bridge_helper(),
              RemoveLogin(form_to_delete, ExpectLocalAccount()))
      .WillOnce(Return(kRemoveLoginJobId));

  task_environment_.FastForwardBy(kTestLatencyDelta);
  consumer().OnCompleteWithLogins(kGetLoginsJobId, {form_to_delete});
  RunUntilIdle();

  task_environment_.FastForwardBy(kTestLatencyDelta);
  consumer().OnLoginsChanged(kRemoveLoginJobId, absl::nullopt);
  RunUntilIdle();

  histogram_tester.ExpectTimeBucketCount(
      DurationMetricName("ClearAllLocalPasswords"), kTestLatencyDelta * 2, 1);

  histogram_tester.ExpectUniqueSample(
      SuccessMetricName("ClearAllLocalPasswords"), true, 1);
}

TEST_F(PasswordStoreAndroidBackendTest, RemoveAllLocalLoginsSuccessMetrics) {
  base::HistogramTester histogram_tester;

  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  EnableSyncForTestAccount();
  backend().OnSyncServiceInitialized(sync_service());

  const JobId kGetLoginsJobId{13387};
  EXPECT_CALL(*bridge_helper(), GetAllLogins(ExpectLocalAccount()))
      .WillOnce(Return(kGetLoginsJobId));
  backend().ClearAllLocalPasswords();

  // Imitate login retrieval and check that it triggers the removal of forms.
  const JobId kRemoveLoginJobId1{13388}, kRemoveLoginJobId2{13389};
  PasswordForm form = CreateTestLogin(kTestUsername, kTestPassword, kTestUrl,
                                      base::Time::FromTimeT(1500));
  EXPECT_CALL(*bridge_helper(), RemoveLogin)
      .WillOnce(Return(kRemoveLoginJobId1));

  // Simulate GetLoginAsync returns two identical logins
  consumer().OnCompleteWithLogins(kGetLoginsJobId, {form, form});
  RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(bridge_helper());

  PasswordStoreChangeList changes = {
      PasswordStoreChange(PasswordStoreChange::REMOVE, form)};
  // Receiving callback after removing the first login should trigger
  // removal of the second login.
  EXPECT_CALL(*bridge_helper(), RemoveLogin)
      .WillOnce(Return(kRemoveLoginJobId2));
  consumer().OnLoginsChanged(kRemoveLoginJobId1, changes);
  RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(bridge_helper());

  // Simulate that the second removal failed.
  consumer().OnError(
      kRemoveLoginJobId2,
      AndroidBackendError(AndroidBackendErrorType::kUncategorized));
  RunUntilIdle();

  const char kTotalCountMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.ClearAllLocalPasswords."
      "LoginsToRemove";
  const char kSuccessRateMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.ClearAllLocalPasswords."
      "SuccessRate";

  histogram_tester.ExpectTotalCount(kTotalCountMetric, 1);
  histogram_tester.ExpectUniqueSample(kTotalCountMetric, 2, 1);
  histogram_tester.ExpectTotalCount(kSuccessRateMetric, 1);
  histogram_tester.ExpectUniqueSample(kSuccessRateMetric, 50, 1);
}

TEST_F(PasswordStoreAndroidBackendTest, NotifyStoreOnForegroundSessionStart) {
  base::MockCallback<PasswordStoreBackend::RemoteChangesReceived>
      store_notification_trigger;
  backend().InitBackend(
      /*remote_form_changes_received=*/store_notification_trigger.Get(),
      /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
      /*completion=*/base::DoNothing());

  // The initial foregrounding should issue a delayed notification so it
  // doesn't overlap with other startup tasks.
  EXPECT_CALL(store_notification_trigger, Run(_)).Times(0);
  lifecycle_helper()->OnForegroundSessionStart();

  EXPECT_CALL(store_notification_trigger, Run(Eq(absl::nullopt)));
  task_environment_.FastForwardBy(base::Seconds(5));

  // Subsequent foregroundings should issue immediate notifications.
  EXPECT_CALL(store_notification_trigger, Run(Eq(absl::nullopt)));
  lifecycle_helper()->OnForegroundSessionStart();
}

TEST_F(PasswordStoreAndroidBackendTest,
       AttachesObserverOnSyncServiceInitialized) {
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

  EXPECT_TRUE(sync_service()->HasObserver(sync_controller_delegate()));
}

TEST_F(PasswordStoreAndroidBackendTest, RecordClearedZombieTaskWithoutLatency) {
  const char kStartedMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.AddLoginAsync";
  const std::string kDurationMetric = DurationMetricName("AddLoginAsync");
  const std::string kSuccessMetric = SuccessMetricName("AddLoginAsync");
  base::HistogramTester histogram_tester;
  backend().InitBackend(/*remote_form_changes_received=*/base::DoNothing(),
                        /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
                        /*completion=*/base::DoNothing());

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), AddLogin).WillOnce(Return(kJobId));
  // Since tasks are cleaned up too early, the reply should never be called.
  EXPECT_CALL(mock_reply, Run).Times(0);

  backend().AddLoginAsync(
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated),
      mock_reply.Get());

  // If Chrome was only very briefly backgrounded, the task might still respond.
  lifecycle_helper()->OnForegroundSessionStart();
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_THAT(histogram_tester.GetAllSamples(kBackendErrorCodeMetric),
              testing::IsEmpty());  // No timeout yet.

  // If Chrome did not receive a response after 30s, the task times out.
  task_environment_.AdvanceClock(base::Seconds(29));
  lifecycle_helper()->OnForegroundSessionStart();
  task_environment_.FastForwardBy(base::Seconds(1));  // Timeout now!.
  histogram_tester.ExpectUniqueSample(kBackendErrorCodeMetric,
                                      kCleanedUpWithoutResponseErrorType, 1);

  // Clear the task queue to verify that a late answer doesn't record again.
  // Can be delayed or never happen.
  consumer().OnLoginsChanged(kJobId, absl::nullopt);
  task_environment_.FastForwardUntilNoTasksRemain();  // For would-be response.

  histogram_tester.ExpectTotalCount(kDurationMetric, 0);
  histogram_tester.ExpectUniqueSample(kSuccessMetric, false, 1);
  histogram_tester.ExpectUniqueSample(kBackendErrorCodeMetric,
                                      kCleanedUpWithoutResponseErrorType,
                                      1);  // Was recorded only once.
  EXPECT_THAT(histogram_tester.GetAllSamples(kStartedMetric),
              ElementsAre(base::Bucket(/* Requested */ 0, 1),
                          base::Bucket(/* Timeout */ 1, 1)));
}

TEST_F(PasswordStoreAndroidBackendTest, RecordsRequestStartAndEndMetric) {
  const char kStartedMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.AddLoginAsync";
  base::HistogramTester histogram_tester;
  backend().InitBackend(/*remote_form_changes_received=*/base::DoNothing(),
                        /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
                        /*completion=*/base::DoNothing());

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), AddLogin).WillOnce(Return(kJobId));
  // Since tasks are never run, the reply should never be called.
  EXPECT_CALL(mock_reply, Run).Times(0);

  backend().AddLoginAsync(
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated),
      mock_reply.Get());

  // Don't wait for execution, check that request start is already logged.
  EXPECT_THAT(histogram_tester.GetAllSamples(kStartedMetric),
              ElementsAre(base::Bucket(/* Requested */ 0, 1)));

  task_environment_.FastForwardUntilNoTasksRemain();
  consumer().OnLoginsChanged(kJobId, absl::nullopt);

  // After execution, check that request is logged again.
  EXPECT_THAT(histogram_tester.GetAllSamples(kStartedMetric),
              ElementsAre(base::Bucket(/* Requested */ 0, 1),
                          base::Bucket(/* Completed */ 2, 1)));
}

TEST_F(PasswordStoreAndroidBackendTest,
       RecordActiveStatusOnSyncServiceInitialized) {
  base::HistogramTester histogram_tester;
  sync_service()->GetUserSettings()->SetSelectedTypes(
      false, {syncer::UserSelectableType::kPasswords});
  backend().OnSyncServiceInitialized(sync_service());
  histogram_tester.ExpectUniqueSample(
      kUPMActiveHistogram, UnifiedPasswordManagerActiveStatus::kActive, 1);
}

TEST_F(PasswordStoreAndroidBackendTest, RecordInactiveStatusSyncOff) {
  base::HistogramTester histogram_tester;
  sync_service()->GetUserSettings()->SetSelectedTypes(false, {});
  backend().OnSyncServiceInitialized(sync_service());
  histogram_tester.ExpectUniqueSample(
      kUPMActiveHistogram, UnifiedPasswordManagerActiveStatus::kInactiveSyncOff,
      1);
}

TEST_F(PasswordStoreAndroidBackendTest, RecordInactiveStatusUnenrolled) {
  base::HistogramTester histogram_tester;
  sync_service()->GetUserSettings()->SetSelectedTypes(
      false, {syncer::UserSelectableType::kPasswords});
  prefs()->SetBoolean(prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
                      true);
  prefs()->SetInteger(
      prefs::kUnenrolledFromGoogleMobileServicesAfterApiErrorCode,
      static_cast<int>(AndroidBackendAPIErrorCode::kInternalError));
  backend().OnSyncServiceInitialized(sync_service());
  histogram_tester.ExpectUniqueSample(
      kUPMActiveHistogram,
      UnifiedPasswordManagerActiveStatus::kInactiveUnenrolledDueToErrors, 1);
}

TEST_F(PasswordStoreAndroidBackendTest, FillMatchingLoginsWithSchemeMismatch) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  base::MockCallback<LoginsOrErrorReply> mock_reply;

  const JobId kFirstJobId{1337};
  EXPECT_CALL(*bridge_helper(), GetLoginsForSignonRealm)
      .WillOnce(Return(kFirstJobId));

  std::string TestURL1("https://a.test.com");

  std::vector<PasswordFormDigest> forms;
  forms.emplace_back(PasswordForm::Scheme::kHtml, TestURL1, GURL(TestURL1));
  backend().FillMatchingLoginsAsync(mock_reply.Get(), /*include_psl=*/true,
                                    forms);

  // Imitate login retrieval.
  PasswordForm exact_match = CreateTestLogin(
      kTestUsername, kTestPassword, "https://a.test.com/", kTestDateCreated);
  PasswordForm psl_match = CreateTestLogin(
      kTestUsername, kTestPassword, "https://b.test.com/", kTestDateCreated);
  psl_match.scheme = PasswordForm::Scheme::kDigest;

  // Retrieving logins for the last form should trigger the final callback.
  LoginsResult expected_logins;
  expected_logins.push_back(std::make_unique<PasswordForm>(exact_match));
  EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));

  consumer().OnCompleteWithLogins(kFirstJobId, {exact_match, psl_match});
  RunUntilIdle();
}

class PasswordStoreAndroidBackendTestForMetrics
    : public PasswordStoreAndroidBackendTest,
      public testing::WithParamInterface<bool> {
 public:
  bool ShouldSucceed() const { return GetParam(); }
};

// Tests the PasswordManager.PasswordStore.GetAllLoginsAsync metric.
TEST_P(PasswordStoreAndroidBackendTestForMetrics, GetAllLoginsAsyncMetrics) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      PasswordStoreAndroidBackend::RemoteChangesReceived(),
      /*sync_enabled_or_disabled_cb=*/base::RepeatingClosure(),
      /*completion=*/base::DoNothing());

  const char kGetAllLoginsMethodName[] = "GetAllLoginsAsync";
  const std::string kDurationMetric =
      DurationMetricName(kGetAllLoginsMethodName);
  const std::string kSuccessMetric = SuccessMetricName(kGetAllLoginsMethodName);
  const std::string kPerMethodErrorCodeMetric =
      PerMethodErrorCodeMetricName(kGetAllLoginsMethodName);
  const std::string kApiErrorMetric =
      ApiErrorMetricName(kGetAllLoginsMethodName);

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());
  EXPECT_CALL(mock_reply, Run(_)).Times(1);
  task_environment_.FastForwardBy(kTestLatencyDelta);

  if (ShouldSucceed()) {
    consumer().OnCompleteWithLogins(kJobId, {});
  } else {
    AndroidBackendError error{kExternalErrorType};
    // Simulate receiving INTERNAL_ERROR code.
    error.api_error_code = absl::optional<int>(kInternalApiErrorCode);
    consumer().OnError(kJobId, std::move(error));
  }
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kTestLatencyDelta, 1);
  histogram_tester.ExpectUniqueSample(kSuccessMetric, ShouldSucceed(), 1);
  if (!ShouldSucceed()) {
    histogram_tester.ExpectUniqueSample(kBackendErrorCodeMetric,
                                        kExternalErrorType, 1);
    histogram_tester.ExpectUniqueSample(kPerMethodErrorCodeMetric,
                                        kExternalErrorType, 1);
    histogram_tester.ExpectUniqueSample(kApiErrorMetric, kInternalApiErrorCode,
                                        1);
  }
}

// Tests the PasswordManager.PasswordStore.AddLoginAsync.* metric.
TEST_P(PasswordStoreAndroidBackendTestForMetrics, AddLoginAsyncMetrics) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      PasswordStoreAndroidBackend::RemoteChangesReceived(),
      /*sync_enabled_or_disabled_cb=*/base::RepeatingClosure(),
      /*completion=*/base::DoNothing());

  const char kAddLoginMethodName[] = "AddLoginAsync";
  const std::string kDurationMetric = DurationMetricName(kAddLoginMethodName);
  const std::string kSuccessMetric = SuccessMetricName(kAddLoginMethodName);
  const std::string kPerMethodErrorCodeMetric =
      PerMethodErrorCodeMetricName(kAddLoginMethodName);
  const std::string kApiErrorMetric = ApiErrorMetricName(kAddLoginMethodName);

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), AddLogin).WillOnce(Return(kJobId));
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  backend().AddLoginAsync(form, mock_reply.Get());
  EXPECT_CALL(mock_reply, Run);
  task_environment_.FastForwardBy(kTestLatencyDelta);

  if (ShouldSucceed()) {
    consumer().OnLoginsChanged(kJobId, absl::nullopt);
  } else {
    AndroidBackendError error{kExternalErrorType};
    // Simulate receiving INTERNAL_ERROR code.
    error.api_error_code = absl::optional<int>(kInternalApiErrorCode);
    consumer().OnError(kJobId, std::move(error));
  }
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kTestLatencyDelta, 1);
  histogram_tester.ExpectUniqueSample(kSuccessMetric, ShouldSucceed(), 1);
  if (!ShouldSucceed()) {
    histogram_tester.ExpectUniqueSample(kBackendErrorCodeMetric,
                                        kExternalErrorType, 1);
    histogram_tester.ExpectUniqueSample(kPerMethodErrorCodeMetric,
                                        kExternalErrorType, 1);
    histogram_tester.ExpectUniqueSample(kApiErrorMetric, kInternalApiErrorCode,
                                        1);
  }
}

// Tests the PasswordManager.PasswordStore.UpdateLoginAsync metric.
TEST_P(PasswordStoreAndroidBackendTestForMetrics, UpdateLoginAsyncMetrics) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      PasswordStoreAndroidBackend::RemoteChangesReceived(),
      /*sync_enabled_or_disabled_cb=*/base::RepeatingClosure(),
      /*completion=*/base::DoNothing());

  const char kUpdateLoginMethodName[] = "UpdateLoginAsync";
  const std::string kDurationMetric =
      DurationMetricName(kUpdateLoginMethodName);
  const std::string kSuccessMetric = SuccessMetricName(kUpdateLoginMethodName);
  const std::string kPerMethodErrorCodeMetric =
      PerMethodErrorCodeMetricName(kUpdateLoginMethodName);
  const std::string kApiErrorMetric =
      ApiErrorMetricName(kUpdateLoginMethodName);

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), UpdateLogin).WillOnce(Return(kJobId));
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  backend().UpdateLoginAsync(form, mock_reply.Get());
  EXPECT_CALL(mock_reply, Run);
  task_environment_.FastForwardBy(kTestLatencyDelta);

  if (ShouldSucceed()) {
    consumer().OnLoginsChanged(kJobId, absl::nullopt);
  } else {
    AndroidBackendError error{kExternalErrorType};
    // Simulate receiving INTERNAL_ERROR code.
    error.api_error_code = absl::optional<int>(kInternalApiErrorCode);
    consumer().OnError(kJobId, std::move(error));
  }
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kTestLatencyDelta, 1);
  histogram_tester.ExpectUniqueSample(kSuccessMetric, ShouldSucceed(), 1);
  if (!ShouldSucceed()) {
    histogram_tester.ExpectUniqueSample(kBackendErrorCodeMetric,
                                        kExternalErrorType, 1);
    histogram_tester.ExpectUniqueSample(kPerMethodErrorCodeMetric,
                                        kExternalErrorType, 1);
    histogram_tester.ExpectUniqueSample(kApiErrorMetric, kInternalApiErrorCode,
                                        1);
  }
}

// Tests the PasswordManager.PasswordStore.RemoveLoginAsync metric.
TEST_P(PasswordStoreAndroidBackendTestForMetrics, RemoveLoginAsyncMetrics) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      PasswordStoreAndroidBackend::RemoteChangesReceived(),
      /*sync_enabled_or_disabled_cb=*/base::RepeatingClosure(),
      /*completion=*/base::DoNothing());

  const char kRemoveLoginMethodName[] = "RemoveLoginAsync";
  const std::string kDurationMetric =
      DurationMetricName(kRemoveLoginMethodName);
  const std::string kSuccessMetric = SuccessMetricName(kRemoveLoginMethodName);
  const std::string kPerMethodErrorCodeMetric =
      PerMethodErrorCodeMetricName(kRemoveLoginMethodName);
  const std::string kApiErrorMetric =
      ApiErrorMetricName(kRemoveLoginMethodName);

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), RemoveLogin).WillOnce(Return(kJobId));
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  backend().RemoveLoginAsync(form, mock_reply.Get());
  EXPECT_CALL(mock_reply, Run);
  task_environment_.FastForwardBy(kTestLatencyDelta);

  if (ShouldSucceed()) {
    consumer().OnLoginsChanged(kJobId, absl::nullopt);
  } else {
    AndroidBackendError error{kExternalErrorType};
    // Simulate receiving INTERNAL_ERROR code.
    error.api_error_code = absl::optional<int>(kInternalApiErrorCode);
    consumer().OnError(kJobId, std::move(error));
  }
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kTestLatencyDelta, 1);
  histogram_tester.ExpectUniqueSample(kSuccessMetric, ShouldSucceed(), 1);
  if (!ShouldSucceed()) {
    histogram_tester.ExpectUniqueSample(kBackendErrorCodeMetric,
                                        kExternalErrorType, 1);
    histogram_tester.ExpectUniqueSample(kPerMethodErrorCodeMetric,
                                        kExternalErrorType, 1);
    histogram_tester.ExpectUniqueSample(kApiErrorMetric, kInternalApiErrorCode,
                                        1);
  }
}

TEST_P(PasswordStoreAndroidBackendTestForMetrics,
       GetAutofillableLoginsAsyncMetrics) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      PasswordStoreAndroidBackend::RemoteChangesReceived(),
      /*sync_enabled_or_disabled_cb=*/base::RepeatingClosure(),
      /*completion=*/base::DoNothing());

  const char kGetAutofillableLoginsMethodName[] = "GetAutofillableLoginsAsync";
  const std::string kDurationMetric =
      DurationMetricName(kGetAutofillableLoginsMethodName);
  const std::string kSuccessMetric =
      SuccessMetricName(kGetAutofillableLoginsMethodName);
  const std::string kPerMethodErrorCodeMetric =
      PerMethodErrorCodeMetricName(kGetAutofillableLoginsMethodName);
  const std::string kApiErrorMetric =
      ApiErrorMetricName(kGetAutofillableLoginsMethodName);

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAutofillableLogins).WillOnce(Return(kJobId));
  backend().GetAutofillableLoginsAsync(mock_reply.Get());
  EXPECT_CALL(mock_reply, Run(_)).Times(1);
  task_environment_.FastForwardBy(kTestLatencyDelta);

  if (ShouldSucceed()) {
    consumer().OnCompleteWithLogins(kJobId, {});
  } else {
    AndroidBackendError error{kExternalErrorType};
    // Simulate receiving INTERNAL_ERROR code.
    error.api_error_code = absl::optional<int>(kInternalApiErrorCode);
    consumer().OnError(kJobId, std::move(error));
  }
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kTestLatencyDelta, 1);
  histogram_tester.ExpectUniqueSample(kSuccessMetric, ShouldSucceed(), 1);
  if (!ShouldSucceed()) {
    histogram_tester.ExpectUniqueSample(kBackendErrorCodeMetric,
                                        kExternalErrorType, 1);
    histogram_tester.ExpectUniqueSample(kPerMethodErrorCodeMetric,
                                        kExternalErrorType, 1);
    histogram_tester.ExpectUniqueSample(kApiErrorMetric, kInternalApiErrorCode,
                                        1);
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         PasswordStoreAndroidBackendTestForMetrics,
                         testing::Bool());

}  // namespace password_manager
