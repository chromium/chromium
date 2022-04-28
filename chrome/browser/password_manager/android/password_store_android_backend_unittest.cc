// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend.h"

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/android/fake_password_manager_lifecycle_helper.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge.h"
#include "chrome/browser/password_manager/android/password_sync_controller_delegate_android.h"
#include "chrome/browser/password_manager/android/password_sync_controller_delegate_bridge_impl.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/sync/driver/test_sync_service.h"
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
using testing::WithArg;
using JobId = PasswordStoreAndroidBackendBridge::JobId;

const char kTestAccount[] = "test@gmail.com";
const std::u16string kTestUsername(u"Todd Tester");
const std::u16string kTestPassword(u"S3cr3t");
constexpr char kTestUrl[] = "https://example.com";
const base::Time kTestDateCreated = base::Time::FromTimeT(1500);

MATCHER_P(ExpectError, expectation, "") {
  return absl::holds_alternative<PasswordStoreBackendError>(arg) &&
         expectation == absl::get<PasswordStoreBackendError>(arg);
}

MATCHER_P(ExpectSyncingAccount, expectation, "") {
  return absl::holds_alternative<
             PasswordStoreAndroidBackendBridge::SyncingAccount>(arg) &&
         expectation ==
             absl::get<PasswordStoreAndroidBackendBridge::SyncingAccount>(arg)
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

class MockPasswordStoreAndroidBackendBridge
    : public PasswordStoreAndroidBackendBridge {
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

class MockSyncDelegate : public PasswordStoreBackend::SyncDelegate {
 public:
  MOCK_METHOD(bool, IsSyncingPasswordsEnabled, (), (override));
  MOCK_METHOD(absl::optional<std::string>, GetSyncingAccount, (), (override));
};

}  // namespace

class PasswordStoreAndroidBackendTest : public testing::Test {
 protected:
  PasswordStoreAndroidBackendTest() {
    backend_ = std::make_unique<PasswordStoreAndroidBackend>(
        base::PassKey<class PasswordStoreAndroidBackendTest>(),
        CreateMockBridge(), CreateFakeLifecycleHelper(),
        CreateMockSyncDelegate(), CreatePasswordSyncControllerDelegate());
  }

  ~PasswordStoreAndroidBackendTest() override {
    lifecycle_helper_->UnregisterObserver();
    lifecycle_helper_ = nullptr;
    testing::Mock::VerifyAndClearExpectations(bridge_);
  }

  PasswordStoreBackend& backend() { return *backend_; }
  PasswordStoreAndroidBackendBridge::Consumer& consumer() { return *backend_; }
  MockPasswordStoreAndroidBackendBridge* bridge() { return bridge_; }
  FakePasswordManagerLifecycleHelper* lifecycle_helper() {
    return lifecycle_helper_;
  }
  MockSyncDelegate* sync_delegate() { return sync_delegate_; }
  PasswordSyncControllerDelegateAndroid* sync_controller_delegate() {
    return sync_controller_delegate_;
  }
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  void EnableSyncForTestAccount() {
    EXPECT_CALL(*sync_delegate(), IsSyncingPasswordsEnabled)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*sync_delegate(), GetSyncingAccount)
        .WillRepeatedly(Return(kTestAccount));
  }

  void DisableSyncFeature() {
    EXPECT_CALL(*sync_delegate(), IsSyncingPasswordsEnabled)
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*sync_delegate(), GetSyncingAccount)
        .WillRepeatedly(Return(absl::nullopt));
  }

 private:
  std::unique_ptr<PasswordStoreAndroidBackendBridge> CreateMockBridge() {
    auto unique_bridge =
        std::make_unique<StrictMock<MockPasswordStoreAndroidBackendBridge>>();
    bridge_ = unique_bridge.get();
    EXPECT_CALL(*bridge_, SetConsumer);
    return unique_bridge;
  }

  std::unique_ptr<PasswordManagerLifecycleHelper> CreateFakeLifecycleHelper() {
    auto new_helper = std::make_unique<FakePasswordManagerLifecycleHelper>();
    lifecycle_helper_ = new_helper.get();
    return new_helper;
  }

  std::unique_ptr<PasswordStoreBackend::SyncDelegate> CreateMockSyncDelegate() {
    auto unique_delegate = std::make_unique<NiceMock<MockSyncDelegate>>();
    sync_delegate_ = unique_delegate.get();
    return unique_delegate;
  }

  std::unique_ptr<PasswordSyncControllerDelegateAndroid>
  CreatePasswordSyncControllerDelegate() {
    auto unique_delegate =
        std::make_unique<PasswordSyncControllerDelegateAndroid>(
            std::make_unique<PasswordSyncControllerDelegateBridgeImpl>(),
            sync_delegate_);
    sync_controller_delegate_ = unique_delegate.get();
    return unique_delegate;
  }

  std::unique_ptr<PasswordStoreAndroidBackend> backend_;
  raw_ptr<StrictMock<MockPasswordStoreAndroidBackendBridge>> bridge_;
  raw_ptr<FakePasswordManagerLifecycleHelper> lifecycle_helper_;
  raw_ptr<PasswordSyncControllerDelegateAndroid> sync_controller_delegate_;
  raw_ptr<MockSyncDelegate> sync_delegate_;
};

TEST_F(PasswordStoreAndroidBackendTest, CallsCompletionCallbackAfterInit) {
  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;
  EXPECT_CALL(completion_callback, Run(true));
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), completion_callback.Get());
}

TEST_F(PasswordStoreAndroidBackendTest, CallsBridgeForLogins) {
  EnableSyncForTestAccount();
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  const JobId kJobId{1337};
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge(), GetAllLogins(ExpectSyncingAccount(kTestAccount)))
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
  base::MockCallback<LoginsReply> mock_reply;

  const JobId kFirstJobId{1337};
  EXPECT_CALL(*bridge(), GetLoginsForSignonRealm).WillOnce(Return(kFirstJobId));
  constexpr auto kLatencyDelta = base::Milliseconds(123u);

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
  EXPECT_CALL(*bridge(), GetLoginsForSignonRealm)
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
  EXPECT_CALL(mock_reply,
              Run(UnorderedPasswordFormElementsAre(&expected_logins)));

  task_environment_.FastForwardBy(kLatencyDelta);
  consumer().OnCompleteWithLogins(kSecondJobId, {matching_signon_realm});
  RunUntilIdle();

  histogram_tester.ExpectTimeBucketCount(
      "PasswordManager.PasswordStoreAndroidBackend.FillMatchingLoginsAsync."
      "Latency",
      kLatencyDelta, 1);
}

TEST_F(PasswordStoreAndroidBackendTest, FillMatchingLoginsPSL) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  base::MockCallback<LoginsReply> mock_reply;

  const JobId kFirstJobId{1337};
  EXPECT_CALL(*bridge(), GetLoginsForSignonRealm).WillOnce(Return(kFirstJobId));
  constexpr auto kLatencyDelta = base::Milliseconds(123u);

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
  EXPECT_CALL(*bridge(), GetLoginsForSignonRealm)
      .WillOnce(Return(kSecondJobId));
  // Logins will be retrieved for forms from |forms| in a backwards order.
  consumer().OnCompleteWithLogins(kFirstJobId, {psl_matching_federated});
  RunUntilIdle();

  // Retrieving logins for the last form should trigger the final callback.
  LoginsResult expected_logins;
  expected_logins.push_back(std::make_unique<PasswordForm>(psl_matching));
  expected_logins.push_back(
      std::make_unique<PasswordForm>(psl_matching_federated));
  EXPECT_CALL(mock_reply,
              Run(UnorderedPasswordFormElementsAre(&expected_logins)));

  task_environment_.FastForwardBy(kLatencyDelta);
  consumer().OnCompleteWithLogins(kSecondJobId, {psl_matching, not_matching});
  RunUntilIdle();
  histogram_tester.ExpectTimeBucketCount(
      "PasswordManager.PasswordStoreAndroidBackend.FillMatchingLoginsAsync."
      "Latency",
      kLatencyDelta, 1);
}

TEST_F(PasswordStoreAndroidBackendTest, CallsBridgeForAutofillableLogins) {
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  const JobId kJobId{1337};
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge(), GetAutofillableLogins).WillOnce(Return(kJobId));
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
  const JobId kJobId{1337};
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge(), GetAllLogins).WillOnce(Return(kJobId));
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
  const JobId kJobId{13388};
  base::MockCallback<PasswordStoreChangeListReply> mock_reply;

  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  EXPECT_CALL(*bridge(), RemoveLogin(form, ExpectLocalAccount()))
      .WillOnce(Return(kJobId));
  backend().RemoveLoginAsync(form, mock_reply.Get());

  PasswordStoreChangeList expected_changes;
  expected_changes.emplace_back(
      PasswordStoreChange(PasswordStoreChange::REMOVE, form));
  EXPECT_CALL(mock_reply, Run(Optional(expected_changes)));
  consumer().OnLoginsChanged(kJobId, expected_changes);
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendTest,
       CallsBridgeForRemoveLoginsByURLAndTime) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  base::MockCallback<PasswordStoreChangeListReply> mock_deletion_reply;
  base::RepeatingCallback<bool(const GURL&)> url_filter = base::BindRepeating(
      [](const GURL& url) { return url == GURL(kTestUrl); });
  base::Time delete_begin = base::Time::FromTimeT(1000);
  base::Time delete_end = base::Time::FromTimeT(2000);
  constexpr auto kLatencyDelta = base::Milliseconds(123u);
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend."
      "RemoveLoginsByURLAndTimeAsync.Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend."
      "RemoveLoginsByURLAndTimeAsync.Success";

  // Check that calling RemoveLoginsByURLAndTime triggers logins retrieval
  // first.
  base::MockCallback<LoginsReply> mock_logins_reply;
  const JobId kGetLoginsJobId{13387};
  EXPECT_CALL(*bridge(), GetAllLogins).WillOnce(Return(kGetLoginsJobId));
  backend().RemoveLoginsByURLAndTimeAsync(url_filter, delete_begin, delete_end,
                                          base::OnceCallback<void(bool)>(),
                                          mock_deletion_reply.Get());

  // Imitate login retrieval and check that it triggers the removal of matching
  // forms.
  const JobId kRemoveLoginJobId{13388};
  EXPECT_CALL(*bridge(), RemoveLogin).WillOnce(Return(kRemoveLoginJobId));
  PasswordForm form_to_delete = CreateTestLogin(
      kTestUsername, kTestPassword, kTestUrl, base::Time::FromTimeT(1500));
  PasswordForm form_to_keep =
      CreateTestLogin(kTestUsername, kTestPassword, "https://differentsite.com",
                      base::Time::FromTimeT(1500));
  consumer().OnCompleteWithLogins(kGetLoginsJobId,
                                  {form_to_delete, form_to_keep});
  RunUntilIdle();
  task_environment_.FastForwardBy(kLatencyDelta);

  // Verify that the callback is called.
  PasswordStoreChangeList expected_changes;
  expected_changes.emplace_back(
      PasswordStoreChange(PasswordStoreChange::REMOVE, form_to_delete));
  EXPECT_CALL(mock_deletion_reply, Run(Optional(expected_changes)));
  consumer().OnLoginsChanged(kRemoveLoginJobId, expected_changes);
  RunUntilIdle();

  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, 1, 1);

  // Check that other values are not recorded.
  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
}

TEST_F(PasswordStoreAndroidBackendTest,
       CallsBridgeForRemoveLoginsCreatedBetween) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  base::MockCallback<PasswordStoreChangeListReply> mock_deletion_reply;
  base::Time delete_begin = base::Time::FromTimeT(1000);
  base::Time delete_end = base::Time::FromTimeT(2000);
  constexpr auto kLatencyDelta = base::Milliseconds(123u);
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend."
      "RemoveLoginsCreatedBetweenAsync.Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend."
      "RemoveLoginsCreatedBetweenAsync.Success";

  // Check that calling RemoveLoginsCreatedBetween triggers logins retrieval
  // first.
  base::MockCallback<LoginsReply> mock_logins_reply;
  const JobId kGetLoginsJobId{13387};
  EXPECT_CALL(*bridge(), GetAllLogins).WillOnce(Return(kGetLoginsJobId));
  backend().RemoveLoginsCreatedBetweenAsync(delete_begin, delete_end,
                                            mock_deletion_reply.Get());

  // Imitate login retrieval and check that it triggers the removal of matching
  // forms.
  const JobId kRemoveLoginJobId{13388};
  EXPECT_CALL(*bridge(), RemoveLogin).WillOnce(Return(kRemoveLoginJobId));
  PasswordForm form_to_delete = CreateTestLogin(
      kTestUsername, kTestPassword, kTestUrl, base::Time::FromTimeT(1500));
  PasswordForm form_to_keep = CreateTestLogin(
      kTestUsername, kTestPassword, kTestUrl, base::Time::FromTimeT(2500));
  consumer().OnCompleteWithLogins(kGetLoginsJobId,
                                  {form_to_delete, form_to_keep});
  RunUntilIdle();
  task_environment_.FastForwardBy(kLatencyDelta);

  // Verify that the callback is called.
  PasswordStoreChangeList expected_changes;
  expected_changes.emplace_back(
      PasswordStoreChange(PasswordStoreChange::REMOVE, form_to_delete));
  EXPECT_CALL(mock_deletion_reply, Run(Optional(expected_changes)));
  consumer().OnLoginsChanged(kRemoveLoginJobId, expected_changes);
  RunUntilIdle();

  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, 1, 1);

  // Check that other values are not recorded.
  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
}

TEST_F(PasswordStoreAndroidBackendTest, CallsBridgeForAddLogin) {
  EnableSyncForTestAccount();
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  const JobId kJobId{13388};
  base::MockCallback<PasswordStoreChangeListReply> mock_reply;

  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  EXPECT_CALL(*bridge(), AddLogin(form, ExpectSyncingAccount(kTestAccount)))
      .WillOnce(Return(kJobId));
  backend().AddLoginAsync(form, mock_reply.Get());

  PasswordStoreChangeList expected_changes;
  expected_changes.emplace_back(
      PasswordStoreChange(PasswordStoreChange::ADD, form));
  EXPECT_CALL(mock_reply, Run(Optional(expected_changes)));
  consumer().OnLoginsChanged(kJobId, expected_changes);
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendTest, CallsBridgeForUpdateLogin) {
  DisableSyncFeature();
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  const JobId kJobId{13388};
  base::MockCallback<PasswordStoreChangeListReply> mock_reply;

  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  EXPECT_CALL(*bridge(), UpdateLogin(form, ExpectLocalAccount()))
      .WillOnce(Return(kJobId));
  backend().UpdateLoginAsync(form, mock_reply.Get());

  PasswordStoreChangeList expected_changes;
  expected_changes.emplace_back(
      PasswordStoreChange(PasswordStoreChange::UPDATE, form));
  EXPECT_CALL(mock_reply, Run(Optional(expected_changes)));
  consumer().OnLoginsChanged(kJobId, expected_changes);
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendTest, OnExternalError) {
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  const JobId kJobId{1337};
  base::HistogramTester histogram_tester;
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge(), GetAllLogins).WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());
  EXPECT_CALL(mock_reply,
              Run(ExpectError(PasswordStoreBackendError::kUnspecified)));
  AndroidBackendError error{AndroidBackendErrorType::kExternalError};
  error.api_error_code = absl::optional<int>(11004);
  consumer().OnError(kJobId, std::move(error));
  RunUntilIdle();

  const char kErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.ErrorCode";
  const char kAPIErrorMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.APIError";

  histogram_tester.ExpectBucketCount(kErrorCodeMetric, 7, 1);
  histogram_tester.ExpectBucketCount(kAPIErrorMetric, 11004, 1);
}

TEST_F(PasswordStoreAndroidBackendTest, DisableAutoSignInForOrigins) {
  EnableSyncForTestAccount();
  base::HistogramTester histogram_tester;
  constexpr auto kLatencyDelta = base::Milliseconds(123u);

  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());

  // Check that calling DisableAutoSignInForOrigins triggers logins retrieval
  // first.
  const JobId kGetLoginsJobId{13387};
  EXPECT_CALL(*bridge(), GetAllLogins).WillOnce(Return(kGetLoginsJobId));

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
  EXPECT_CALL(*bridge(),
              UpdateLogin(FormWithDisabledAutoSignIn(form_to_update2),
                          ExpectSyncingAccount(kTestAccount)))
      .WillOnce(Return(kUpdateJobId1));

  consumer().OnCompleteWithLogins(
      kGetLoginsJobId,
      {form_to_update1, form_to_update2, form_with_autosignin_disabled,
       form_with_different_origin});
  RunUntilIdle();

  // Fast forward to check latency metric recording.
  task_environment_.FastForwardBy(kLatencyDelta);

  // Receiving callback after updating the first login should trigger
  // updating of the second login.
  PasswordStoreChangeList change1;
  change1.emplace_back(
      PasswordStoreChange(PasswordStoreChange::UPDATE,
                          FormWithDisabledAutoSignIn(form_to_update2)));
  const JobId kUpdateJobId2{13389};
  EXPECT_CALL(*bridge(),
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
      "PasswordManager.PasswordStoreAndroidBackend."
      "DisableAutoSignInForOriginsAsync."
      "Latency",
      kLatencyDelta, 1);

  histogram_tester.ExpectTotalCount(
      "PasswordManager.PasswordStoreAndroidBackend."
      "DisableAutoSignInForOriginsAsync."
      "Success",
      1);
}

TEST_F(PasswordStoreAndroidBackendTest, RemoveAllLocalLogins) {
  base::HistogramTester histogram_tester;
  constexpr auto kLatencyDelta = base::Milliseconds(123u);
  EnableSyncForTestAccount();
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());

  base::MockCallback<LoginsReply> mock_logins_reply;
  const JobId kGetLoginsJobId{13387};
  EXPECT_CALL(*bridge(), GetAllLogins(ExpectLocalAccount()))
      .WillOnce(Return(kGetLoginsJobId));
  backend().ClearAllLocalPasswords();

  // Imitate login retrieval and check that it triggers the removal of forms.
  const JobId kRemoveLoginJobId{13388};
  PasswordForm form_to_delete = CreateTestLogin(
      kTestUsername, kTestPassword, kTestUrl, base::Time::FromTimeT(1500));
  EXPECT_CALL(*bridge(), RemoveLogin(form_to_delete, ExpectLocalAccount()))
      .WillOnce(Return(kRemoveLoginJobId));

  task_environment_.FastForwardBy(kLatencyDelta);
  consumer().OnCompleteWithLogins(kGetLoginsJobId, {form_to_delete});
  RunUntilIdle();

  task_environment_.FastForwardBy(kLatencyDelta);
  consumer().OnLoginsChanged(kRemoveLoginJobId, PasswordStoreChangeList());
  RunUntilIdle();

  histogram_tester.ExpectTimeBucketCount(
      "PasswordManager.PasswordStoreAndroidBackend."
      "ClearAllLocalPasswords."
      "Latency",
      kLatencyDelta * 2, 1);

  histogram_tester.ExpectTotalCount(
      "PasswordManager.PasswordStoreAndroidBackend."
      "ClearAllLocalPasswords."
      "Success",
      1);
}

TEST_F(PasswordStoreAndroidBackendTest, RemoveAllLocalLoginsSuccessMetrics) {
  EnableSyncForTestAccount();
  base::HistogramTester histogram_tester;
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());

  base::MockCallback<LoginsReply> mock_logins_reply;
  const JobId kGetLoginsJobId{13387};
  EXPECT_CALL(*bridge(), GetAllLogins(ExpectLocalAccount()))
      .WillOnce(Return(kGetLoginsJobId));
  backend().ClearAllLocalPasswords();

  // Imitate login retrieval and check that it triggers the removal of forms.
  const JobId kRemoveLoginJobId1{13388}, kRemoveLoginJobId2{13389};
  PasswordForm form = CreateTestLogin(kTestUsername, kTestPassword, kTestUrl,
                                      base::Time::FromTimeT(1500));
  EXPECT_CALL(*bridge(), RemoveLogin).WillOnce(Return(kRemoveLoginJobId1));

  // Simulate GetLoginAsync returns two identical logins
  consumer().OnCompleteWithLogins(kGetLoginsJobId, {form, form});
  RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(bridge());

  PasswordStoreChangeList changes = {
      PasswordStoreChange(PasswordStoreChange::REMOVE, form)};
  // Receiving callback after removing the first login should trigger
  // removal of the second login.
  EXPECT_CALL(*bridge(), RemoveLogin).WillOnce(Return(kRemoveLoginJobId2));
  consumer().OnLoginsChanged(kRemoveLoginJobId1, changes);
  RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(bridge());

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
      /*stored_passwords_changed=*/store_notification_trigger.Get(),
      /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
      /*completion=*/base::DoNothing());

  // Verify that the store would be notified.
  EXPECT_CALL(store_notification_trigger, Run(Eq(absl::nullopt)));
  lifecycle_helper()->OnForegroundSessionStart();
}

TEST_F(PasswordStoreAndroidBackendTest,
       AttachesObserverOnSyncServiceInitialized) {
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());

  syncer::TestSyncService sync_service;
  backend().OnSyncServiceInitialized(&sync_service);

  EXPECT_TRUE(sync_service.HasObserver(sync_controller_delegate()));
}

TEST_F(PasswordStoreAndroidBackendTest, RecordClearedZombieTaskWithoutLatency) {
  constexpr JobId kJobId{1337};
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.AddLoginAsync.Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.AddLoginAsync.Success";
  const char kErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.ErrorCode";
  base::HistogramTester histogram_tester;
  backend().InitBackend(/*stored_passwords_changed=*/base::DoNothing(),
                        /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
                        /*completion=*/base::DoNothing());

  base::MockCallback<PasswordStoreChangeListReply> mock_reply;
  EXPECT_CALL(*bridge(), AddLogin).WillOnce(Return(kJobId));
  // Since tasks are cleaned up too early, the reply should never be called.
  EXPECT_CALL(mock_reply, Run).Times(0);

  backend().AddLoginAsync(
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated),
      mock_reply.Get());

  // If Chrome was only very briefly backgrounded, the task might still respond.
  lifecycle_helper()->OnForegroundSessionStart();
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_THAT(histogram_tester.GetAllSamples(kErrorCodeMetric),
              testing::IsEmpty());  // No timeout yet.

  // If Chrome did not receive a response after 30s, the task times out.
  task_environment_.AdvanceClock(base::Seconds(29));
  lifecycle_helper()->OnForegroundSessionStart();
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_THAT(histogram_tester.GetAllSamples(kErrorCodeMetric),
              ElementsAre(base::Bucket(8, 1)));  // Timeout now!.

  // Clear the task queue to verify that a late answer doesn't record again.
  consumer().OnLoginsChanged(kJobId, {});  // Can be delayed or never happen.
  task_environment_.FastForwardUntilNoTasksRemain();  // For would-be response.

  histogram_tester.ExpectTotalCount(kDurationMetric, 0);
  EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessMetric),
              ElementsAre(base::Bucket(false, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kErrorCodeMetric),
              ElementsAre(base::Bucket(8, 1)));  // Record only once.
}

class PasswordStoreAndroidBackendTestForMetrics
    : public PasswordStoreAndroidBackendTest,
      public testing::WithParamInterface<bool> {
 public:
  bool ShouldSucceed() const { return GetParam(); }
};

// Tests the PasswordManager.PasswordStore.GetAllLoginsAsync metric.
TEST_P(PasswordStoreAndroidBackendTestForMetrics, GetAllLoginsAsyncMetrics) {
  backend().InitBackend(
      PasswordStoreAndroidBackend::RemoteChangesReceived(),
      /*sync_enabled_or_disabled_cb=*/base::RepeatingClosure(),
      /*completion=*/base::DoNothing());
  constexpr auto kLatencyDelta = base::Milliseconds(123u);
  constexpr JobId kJobId{1337};
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.GetAllLoginsAsync.Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.GetAllLoginsAsync.Success";
  const char kErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.ErrorCode";
  const char kPerApiErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.GetAllLoginsAsync.ErrorCode";
  base::HistogramTester histogram_tester;
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge(), GetAllLogins).WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());
  EXPECT_CALL(mock_reply, Run(_)).Times(1);
  task_environment_.FastForwardBy(kLatencyDelta);
  if (ShouldSucceed())
    consumer().OnCompleteWithLogins(kJobId, {});
  else
    consumer().OnError(
        kJobId, AndroidBackendError(AndroidBackendErrorType::kUncategorized));
  RunUntilIdle();
  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, true, ShouldSucceed());
  histogram_tester.ExpectBucketCount(kSuccessMetric, false, !ShouldSucceed());
  if (!ShouldSucceed()) {
    histogram_tester.ExpectBucketCount(kErrorCodeMetric, 0, 1);
    histogram_tester.ExpectBucketCount(kPerApiErrorCodeMetric, 0, 1);
  }
}

// Tests the PasswordManager.PasswordStore.AddLoginAsync.* metric.
TEST_P(PasswordStoreAndroidBackendTestForMetrics, AddLoginAsyncMetrics) {
  backend().InitBackend(
      PasswordStoreAndroidBackend::RemoteChangesReceived(),
      /*sync_enabled_or_disabled_cb=*/base::RepeatingClosure(),
      /*completion=*/base::DoNothing());
  constexpr auto kLatencyDelta = base::Milliseconds(123u);
  constexpr JobId kJobId{1337};
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.AddLoginAsync.Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.AddLoginAsync.Success";
  const char kErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.ErrorCode";
  const char kPerApiErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.AddLoginAsync.ErrorCode";
  base::HistogramTester histogram_tester;

  base::MockCallback<PasswordStoreChangeListReply> mock_reply;
  EXPECT_CALL(*bridge(), AddLogin).WillOnce(Return(kJobId));
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  backend().AddLoginAsync(form, mock_reply.Get());
  EXPECT_CALL(mock_reply, Run);
  task_environment_.FastForwardBy(kLatencyDelta);

  if (ShouldSucceed()) {
    consumer().OnLoginsChanged(kJobId, {});
  } else {
    consumer().OnError(
        kJobId, AndroidBackendError(AndroidBackendErrorType::kUncategorized));
  }
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, true, ShouldSucceed());
  histogram_tester.ExpectBucketCount(kSuccessMetric, false, !ShouldSucceed());
  if (!ShouldSucceed()) {
    histogram_tester.ExpectBucketCount(kErrorCodeMetric, 0, 1);
    histogram_tester.ExpectBucketCount(kPerApiErrorCodeMetric, 0, 1);
  }
}

// Tests the PasswordManager.PasswordStore.UpdateLoginAsync metric.
TEST_P(PasswordStoreAndroidBackendTestForMetrics, UpdateLoginAsyncMetrics) {
  backend().InitBackend(
      PasswordStoreAndroidBackend::RemoteChangesReceived(),
      /*sync_enabled_or_disabled_cb=*/base::RepeatingClosure(),
      /*completion=*/base::DoNothing());
  constexpr auto kLatencyDelta = base::Milliseconds(123u);
  constexpr JobId kJobId{1337};
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.UpdateLoginAsync.Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.UpdateLoginAsync.Success";
  const char kErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.ErrorCode";
  const char kPerApiErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.UpdateLoginAsync.ErrorCode";
  base::HistogramTester histogram_tester;

  base::MockCallback<PasswordStoreChangeListReply> mock_reply;
  EXPECT_CALL(*bridge(), UpdateLogin).WillOnce(Return(kJobId));
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  backend().UpdateLoginAsync(form, mock_reply.Get());
  EXPECT_CALL(mock_reply, Run);
  task_environment_.FastForwardBy(kLatencyDelta);

  if (ShouldSucceed()) {
    consumer().OnLoginsChanged(kJobId, {});
  } else {
    consumer().OnError(
        kJobId, AndroidBackendError(AndroidBackendErrorType::kUncategorized));
  }
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, true, ShouldSucceed());
  histogram_tester.ExpectBucketCount(kSuccessMetric, false, !ShouldSucceed());
  if (!ShouldSucceed()) {
    histogram_tester.ExpectBucketCount(kErrorCodeMetric, 0, 1);
    histogram_tester.ExpectBucketCount(kPerApiErrorCodeMetric, 0, 1);
  }
}

// Tests the PasswordManager.PasswordStore.RemoveLoginAsync metric.
TEST_P(PasswordStoreAndroidBackendTestForMetrics, RemoveLoginAsyncMetrics) {
  backend().InitBackend(
      PasswordStoreAndroidBackend::RemoteChangesReceived(),
      /*sync_enabled_or_disabled_cb=*/base::RepeatingClosure(),
      /*completion=*/base::DoNothing());
  constexpr auto kLatencyDelta = base::Milliseconds(123u);
  constexpr JobId kJobId{1337};
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.RemoveLoginAsync.Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.RemoveLoginAsync.Success";
  const char kErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.ErrorCode";
  const char kPerApiErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.RemoveLoginAsync.ErrorCode";
  base::HistogramTester histogram_tester;

  base::MockCallback<PasswordStoreChangeListReply> mock_reply;
  EXPECT_CALL(*bridge(), RemoveLogin).WillOnce(Return(kJobId));
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  backend().RemoveLoginAsync(form, mock_reply.Get());
  EXPECT_CALL(mock_reply, Run);
  task_environment_.FastForwardBy(kLatencyDelta);

  if (ShouldSucceed()) {
    consumer().OnLoginsChanged(kJobId, {});
  } else {
    consumer().OnError(
        kJobId, AndroidBackendError(AndroidBackendErrorType::kUncategorized));
  }
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, true, ShouldSucceed());
  histogram_tester.ExpectBucketCount(kSuccessMetric, false, !ShouldSucceed());
  if (!ShouldSucceed()) {
    histogram_tester.ExpectBucketCount(kErrorCodeMetric, 0, 1);
    histogram_tester.ExpectBucketCount(kPerApiErrorCodeMetric, 0, 1);
  }
}

TEST_P(PasswordStoreAndroidBackendTestForMetrics,
       GetAutofillableLoginsAsyncMetrics) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      PasswordStoreAndroidBackend::RemoteChangesReceived(),
      /*sync_enabled_or_disabled_cb=*/base::RepeatingClosure(),
      /*completion=*/base::DoNothing());
  constexpr auto kLatencyDelta = base::Milliseconds(123u);
  constexpr JobId kJobId{1337};
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.GetAutofillableLoginsAsync."
      "Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.GetAutofillableLoginsAsync."
      "Success";
  const char kErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.ErrorCode";
  const char kPerApiErrorCodeMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.GetAutofillableLoginsAsync."
      "ErrorCode";
  base::MockCallback<LoginsOrErrorReply> mock_reply;

  EXPECT_CALL(*bridge(), GetAutofillableLogins).WillOnce(Return(kJobId));
  backend().GetAutofillableLoginsAsync(mock_reply.Get());

  EXPECT_CALL(mock_reply, Run(_)).Times(1);
  task_environment_.FastForwardBy(kLatencyDelta);
  if (ShouldSucceed())
    consumer().OnCompleteWithLogins(kJobId, {});
  else
    consumer().OnError(
        kJobId, AndroidBackendError(AndroidBackendErrorType::kUncategorized));
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, true, ShouldSucceed());
  histogram_tester.ExpectBucketCount(kSuccessMetric, false, !ShouldSucceed());
  if (!ShouldSucceed()) {
    histogram_tester.ExpectBucketCount(kErrorCodeMetric, 0, 1);
    histogram_tester.ExpectBucketCount(kPerApiErrorCodeMetric, 0, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         PasswordStoreAndroidBackendTestForMetrics,
                         testing::Bool());

}  // namespace password_manager
