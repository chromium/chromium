// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_local_backend.h"

#include <jni.h>

#include <cstdint>

#include "base/location.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/password_manager/android/fake_password_manager_lifecycle_helper.h"
#include "chrome/browser/password_manager/android/mock_password_store_android_backend_bridge_helper.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_dispatcher_bridge.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_receiver_bridge.h"
#include "chrome/browser/password_manager/android/password_store_android_local_backend.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/password_manager/core/browser/affiliation/password_affiliation_source_adapter.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/android_backend_error.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using testing::_;
using testing::ElementsAreArray;
using testing::IsEmpty;
using testing::NiceMock;
using testing::Optional;
using testing::Return;
using testing::VariantWith;
using testing::WithArg;
using JobId = PasswordStoreAndroidBackendDispatcherBridge::JobId;

constexpr JobId kJobId{1337};
constexpr char kTestUrl[] = "https://example.com";
constexpr base::TimeDelta kTestLatencyDelta = base::Milliseconds(123u);

PasswordForm CreateEntry(
    const std::string& username,
    const std::string& password,
    const GURL& origin_url,
    PasswordForm::MatchType match_type = PasswordForm::MatchType::kExact) {
  PasswordForm form;
  form.username_value = base::ASCIIToUTF16(username);
  form.password_value = base::ASCIIToUTF16(password);
  form.url = origin_url;
  form.signon_realm = origin_url.GetWithEmptyPath().spec();
  form.match_type = match_type;
  return form;
}

std::vector<PasswordForm> CreateTestLogins() {
  std::vector<PasswordForm> forms;
  forms.push_back(CreateEntry("Todd Tester", "S3cr3t",
                              GURL(u"https://example.com"),
                              PasswordForm::MatchType::kExact));
  forms.push_back(CreateEntry("Marcus McSpartanGregor", "S0m3th1ngCr34t1v3",
                              GURL(u"https://m.example.com"),
                              PasswordForm::MatchType::kPSL));
  return forms;
}

}  // namespace

class PasswordStoreAndroidLocalBackendTest : public testing::Test {
 protected:
  PasswordStoreAndroidLocalBackendTest() {
    prefs_.registry()->RegisterBooleanPref(
        prefs::kUnenrolledFromGoogleMobileServicesDueToErrors, false);
    ResetBackend();
  }

  ~PasswordStoreAndroidLocalBackendTest() override {
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
  PrefService* prefs() { return &prefs_; }
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Prefer using the already created `backend()` when possible.
  void ResetBackend() {
    backend_ = std::make_unique<PasswordStoreAndroidLocalBackend>(
        CreateMockBridgeHelper(), CreateFakeLifecycleHelper(), &prefs_,
        password_affiliation_adapter_);
  }

 private:
  std::unique_ptr<PasswordStoreAndroidBackendBridgeHelper>
  CreateMockBridgeHelper() {
    auto unique_bridge_helper = std::make_unique<
        NiceMock<MockPasswordStoreAndroidBackendBridgeHelper>>();
    bridge_helper_ = unique_bridge_helper.get();
    EXPECT_CALL(*bridge_helper_, SetConsumer);
    return unique_bridge_helper;
  }

  std::unique_ptr<PasswordManagerLifecycleHelper> CreateFakeLifecycleHelper() {
    auto new_helper = std::make_unique<FakePasswordManagerLifecycleHelper>();
    lifecycle_helper_ = new_helper.get();
    return new_helper;
  }

  PasswordAffiliationSourceAdapter password_affiliation_adapter_;
  std::unique_ptr<PasswordStoreAndroidLocalBackend> backend_;
  raw_ptr<NiceMock<MockPasswordStoreAndroidBackendBridgeHelper>> bridge_helper_;
  raw_ptr<FakePasswordManagerLifecycleHelper> lifecycle_helper_;
  TestingPrefServiceSimple prefs_;
};

TEST_F(PasswordStoreAndroidLocalBackendTest, CallsBridgeForGetAllLogins) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidLocalBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins(IsEmpty()))
      .WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());

  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<LoginsResult>(ElementsAreArray(CreateTestLogins()))));
  consumer().OnCompleteWithLogins(kJobId, CreateTestLogins());
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidLocalBackendTest,
       CallsBridgeForGetAllLoginsWithAffiliationAndBranding) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidLocalBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLoginsWithBrandingInfo(IsEmpty()))
      .WillOnce(Return(kJobId));
  backend().GetAllLoginsWithAffiliationAndBrandingAsync(mock_reply.Get());

  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<LoginsResult>(ElementsAreArray(CreateTestLogins()))));
  consumer().OnCompleteWithLogins(kJobId, CreateTestLogins());
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidLocalBackendTest,
       CallsBridgeForGetAutofillableLogins) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidLocalBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAutofillableLogins(IsEmpty()))
      .WillOnce(Return(kJobId));
  backend().GetAutofillableLoginsAsync(mock_reply.Get());

  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<LoginsResult>(ElementsAreArray(CreateTestLogins()))));
  consumer().OnCompleteWithLogins(kJobId, CreateTestLogins());
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidLocalBackendTest,
       CallsBridgeForGroupedMatchingLogins) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidLocalBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  std::string TestURL1("https://example.com/");
  PasswordFormDigest form_digest(PasswordForm::Scheme::kHtml, TestURL1,
                                 GURL(TestURL1));

  EXPECT_CALL(*bridge_helper(), GetAffiliatedLoginsForSignonRealm(TestURL1, _))
      .WillOnce(Return(kJobId));
  backend().GetGroupedMatchingLoginsAsync(form_digest, mock_reply.Get());

  LoginsResult returned_logins;
  returned_logins.push_back(CreateEntry("Todd Tester", "S3cr3t",
                                        GURL(u"https://example.com/"),
                                        PasswordForm::MatchType::kAffiliated));
  returned_logins.push_back(CreateEntry(
      "Marcus McSpartanGregor", "S0m3th1ngCr34t1v3",
      GURL(u"https://m.example.com/"), PasswordForm::MatchType::kGrouped));
  returned_logins.push_back(CreateEntry(
      "Marcus McSpartanGregor", "S0m3th1ngCr34t1v3",
      GURL(u"https://example.org/"), PasswordForm::MatchType::kGrouped));

  std::vector<PasswordForm> expected_logins;
  // Exact match is defined as such even if it was marked as affiliated match
  // before.
  expected_logins.push_back(CreateEntry("Todd Tester", "S3cr3t",
                                        GURL(u"https://example.com/"),
                                        PasswordForm::MatchType::kExact));
  // Grouped match is also a PSL match.
  expected_logins.push_back(CreateEntry(
      "Marcus McSpartanGregor", "S0m3th1ngCr34t1v3",
      GURL(u"https://m.example.com/"),
      PasswordForm::MatchType::kGrouped | PasswordForm::MatchType::kPSL));
  // Grouped only match.
  expected_logins.push_back(CreateEntry(
      "Marcus McSpartanGregor", "S0m3th1ngCr34t1v3",
      GURL(u"https://example.org/"), PasswordForm::MatchType::kGrouped));

  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<LoginsResult>(ElementsAreArray(expected_logins))));
  consumer().OnCompleteWithLogins(kJobId, std::move(returned_logins));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidLocalBackendTest, CallsBridgeForAddLogin) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidLocalBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());

  const JobId kAddLoginJobId{13388};
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form = CreateEntry("tod", "qwerty", GURL(kTestUrl));
  EXPECT_CALL(*bridge_helper(), AddLogin(form, std::string()))
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

TEST_F(PasswordStoreAndroidLocalBackendTest, CallsBridgeForUpdateLogin) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidLocalBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  const JobId kUpdateLoginJobId{13388};
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form = CreateEntry("tod", "qwerty", GURL(kTestUrl));
  EXPECT_CALL(*bridge_helper(), UpdateLogin(form, std::string()))
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

TEST_F(PasswordStoreAndroidLocalBackendTest,
       CallsBridgeForRemoveLoginsByURLAndTime) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidLocalBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());

  base::MockCallback<PasswordChangesOrErrorReply> mock_deletion_reply;
  base::RepeatingCallback<bool(const GURL&)> url_filter = base::BindRepeating(
      [](const GURL& url) { return url == GURL(kTestUrl); });
  base::Time delete_begin = base::Time::FromTimeT(1000);
  base::Time delete_end = base::Time::FromTimeT(2000);

  // Check that calling RemoveLoginsByURLAndTime triggers logins retrieval
  // first.
  const JobId kGetLoginsJobId{13387};
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kGetLoginsJobId));
  backend().RemoveLoginsByURLAndTimeAsync(
      FROM_HERE, url_filter, delete_begin, delete_end,
      base::OnceCallback<void(bool)>(), mock_deletion_reply.Get());

  // Imitate login retrieval and check that it triggers the removal of matching
  // forms.
  const JobId kRemoveLoginJobId{13388};
  EXPECT_CALL(*bridge_helper(), RemoveLogin)
      .WillOnce(Return(kRemoveLoginJobId));
  PasswordForm form_to_delete = CreateEntry("tod", "qwerty", GURL(kTestUrl));
  form_to_delete.date_created = base::Time::FromTimeT(1500);
  PasswordForm form_to_keep =
      CreateEntry("username", "pass", GURL("https://differentsite.com"));
  form_to_keep.date_created = base::Time::FromTimeT(1500);

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
}

// Error from GMSCore doesn't cause unenrollment.
TEST_F(PasswordStoreAndroidLocalBackendTest,
       ExternalErrorDontCauseUnenrollment) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidLocalBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());
  int kInternalErrorCode =
      static_cast<int>(AndroidBackendAPIErrorCode::kInternalError);
  PasswordStoreBackendError expected_error = {
      PasswordStoreBackendErrorType::kUncategorized};
  expected_error.android_backend_api_error = kInternalErrorCode;
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordStoreBackendError>(expected_error)));
  AndroidBackendError error{AndroidBackendErrorType::kExternalError};
  // Simulate receiving INTERNAL_ERROR code from GMSCore.
  error.api_error_code = std::optional<int>(kInternalErrorCode);
  consumer().OnError(kJobId, std::move(error));
  RunUntilIdle();

  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
}

TEST_F(PasswordStoreAndroidLocalBackendTest, RecordPasswordStoreMetrics) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidLocalBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());

  backend().RecordAddLoginAsyncCalledFromTheStore();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordStore.LocalBackend.AddLoginCalledOnStore", true,
      1);

  backend().RecordUpdateLoginAsyncCalledFromTheStore();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordStore.LocalBackend.UpdateLoginCalledOnStore",
      true, 1);
}

TEST_F(PasswordStoreAndroidLocalBackendTest,
       ResetTemporarySavingSuspensionAfterSuccessfulLogin) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidLocalBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  EXPECT_TRUE(backend().IsAbleToSavePasswords());

  std::string TestURL1("https://example.com/");
  PasswordFormDigest form_digest(PasswordForm::Scheme::kHtml, TestURL1,
                                 GURL(TestURL1));
  EXPECT_CALL(*bridge_helper(), GetAffiliatedLoginsForSignonRealm)
      .WillOnce(Return(kJobId));
  backend().GetGroupedMatchingLoginsAsync(form_digest, base::DoNothing());
  AndroidBackendError error{AndroidBackendErrorType::kExternalError};
  error.api_error_code =
      static_cast<int>(AndroidBackendAPIErrorCode::kApiNotConnected);
  consumer().OnError(kJobId, std::move(error));

  EXPECT_FALSE(backend().IsAbleToSavePasswords());

  // Simulate a successful logins call.
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAffiliatedLoginsForSignonRealm)
      .WillOnce(Return(kJobId));
  backend().GetGroupedMatchingLoginsAsync(form_digest, mock_reply.Get());
  EXPECT_CALL(mock_reply, Run);
  task_environment_.FastForwardBy(kTestLatencyDelta);
  consumer().OnCompleteWithLogins(kJobId, {});
  RunUntilIdle();

  EXPECT_TRUE(backend().IsAbleToSavePasswords());
}

class PasswordStoreAndroidLocalBackendRetriesTest
    : public PasswordStoreAndroidLocalBackendTest,
      public testing::WithParamInterface<AndroidBackendAPIErrorCode> {};

TEST_P(PasswordStoreAndroidLocalBackendRetriesTest,
       GetAllLoginsIsRetriedUntilSuccess) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidLocalBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins(IsEmpty()))
      .Times(3)
      .WillRepeatedly(Return(kJobId));

  AndroidBackendError error{AndroidBackendErrorType::kExternalError,
                            static_cast<int>(GetParam())};

  // Initiating the first call.
  backend().GetAllLoginsAsync(mock_reply.Get());

  for (int i = 0; i < 2; i++) {
    consumer().OnError(kJobId, error);
    // Runs the delayed tasks which results in GetAllLogins being called on
    // the bridge.
    task_environment_.FastForwardUntilNoTasksRemain();
  }

  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<LoginsResult>(ElementsAreArray(CreateTestLogins()))));
  consumer().OnCompleteWithLogins(kJobId, CreateTestLogins());

  RunUntilIdle();

  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
}

TEST_P(PasswordStoreAndroidLocalBackendRetriesTest,
       GetAutofillableLoginsIsRetriedUntilSuccess) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidLocalBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAutofillableLogins(IsEmpty()))
      .Times(3)
      .WillRepeatedly(Return(kJobId));

  // Initiating the first call.
  backend().GetAutofillableLoginsAsync(mock_reply.Get());

  AndroidBackendError error{AndroidBackendErrorType::kExternalError,
                            static_cast<int>(GetParam())};

  for (int i = 0; i < 2; i++) {
    consumer().OnError(kJobId, error);
    // Runs the delayed tasks which results in GetAllLogins being called on
    // the bridge.
    task_environment_.FastForwardUntilNoTasksRemain();
  }

  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<LoginsResult>(ElementsAreArray(CreateTestLogins()))));
  consumer().OnCompleteWithLogins(kJobId, CreateTestLogins());

  RunUntilIdle();

  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
}

TEST_P(PasswordStoreAndroidLocalBackendRetriesTest,
       GetAllLoginsIsRetriedUntilTimeout) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidLocalBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins(IsEmpty()))
      .Times(6)
      .WillRepeatedly(Return(kJobId));

  AndroidBackendError error{AndroidBackendErrorType::kExternalError,
                            static_cast<int>(GetParam())};

  // Initiating the first call.
  backend().GetAllLoginsAsync(mock_reply.Get());

  for (int i = 0; i < 5; i++) {
    consumer().OnError(kJobId, error);
    // Runs the delayed tasks which results in GetAllLogins being called on
    // the bridge.
    task_environment_.FastForwardUntilNoTasksRemain();
  }

  PasswordStoreBackendError expected_error{
      PasswordStoreBackendErrorType::kUncategorized};
  expected_error.android_backend_api_error = static_cast<int>(GetParam());
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordStoreBackendError>(expected_error)));
  consumer().OnError(kJobId, error);

  RunUntilIdle();

  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
}

TEST_P(PasswordStoreAndroidLocalBackendRetriesTest,
       GetAutofillableLoginsIsRetriedUntilTimeout) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidLocalBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAutofillableLogins(IsEmpty()))
      .Times(6)
      .WillRepeatedly(Return(kJobId));

  // Initiating the first call.
  backend().GetAutofillableLoginsAsync(mock_reply.Get());

  AndroidBackendError error{AndroidBackendErrorType::kExternalError,
                            static_cast<int>(GetParam())};

  for (int i = 0; i < 5; i++) {
    consumer().OnError(kJobId, error);
    // Runs the delayed tasks which results in GetAllLogins being called on
    // the bridge.
    task_environment_.FastForwardUntilNoTasksRemain();
  }

  PasswordStoreBackendError expected_error{
      PasswordStoreBackendErrorType::kUncategorized};
  expected_error.android_backend_api_error = static_cast<int>(GetParam());
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordStoreBackendError>(expected_error)));
  consumer().OnError(kJobId, error);

  RunUntilIdle();

  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PasswordStoreAndroidLocalBackendRetriesTest,
    testing::ValuesIn(
        {AndroidBackendAPIErrorCode::kNetworkError,
         AndroidBackendAPIErrorCode::kApiNotConnected,
         AndroidBackendAPIErrorCode::kConnectionSuspendedDuringCall,
         AndroidBackendAPIErrorCode::kReconnectionTimedOut,
         AndroidBackendAPIErrorCode::kBackendGeneric}),
    [](const ::testing::TestParamInfo<AndroidBackendAPIErrorCode>& info) {
      return "APIErrorCode_" + base::ToString(static_cast<int>(info.param));
    });

}  // namespace password_manager
