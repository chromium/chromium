// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_account_backend.h"

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/android/fake_password_manager_lifecycle_helper.h"
#include "chrome/browser/password_manager/android/mock_password_store_android_backend_bridge_helper.h"
#include "chrome/browser/password_manager/android/mock_password_sync_controller_delegate_bridge.h"
#include "chrome/browser/password_manager/android/password_manager_android_util.h"
#include "chrome/browser/password_manager/android/password_manager_eviction_util.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_api_error_codes.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_dispatcher_bridge.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_receiver_bridge.h"
#include "chrome/browser/password_manager/android/password_sync_controller_delegate_android.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliated_match_helper.h"
#include "components/password_manager/core/browser/affiliation/password_affiliation_source_adapter.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/android_backend_error.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using affiliations::FakeAffiliationService;
using affiliations::MockAffiliationService;
using testing::_;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Eq;
using testing::IsEmpty;
using testing::NiceMock;
using testing::Optional;
using testing::Return;
using testing::StrictMock;
using testing::UnorderedElementsAre;
using testing::VariantWith;
using testing::WithArg;
using JobId = PasswordStoreAndroidBackendDispatcherBridge::JobId;

constexpr char kTestAccount[] = "test@gmail.com";
const std::u16string kTestUsername(u"Todd Tester");
const std::u16string kTestPassword(u"S3cr3t");
constexpr char kTestUrl[] = "https://example.com";
constexpr const char kTestAndroidName[] = "Example Android App 1";
constexpr const char kTestAndroidIconURL[] = "https://example.com/icon_1.png";
constexpr base::Time kTestDateCreated = base::Time::FromTimeT(1500);
constexpr base::TimeDelta kTestLatencyDelta = base::Milliseconds(123u);
constexpr const char kTestAndroidRealm[] =
    "android://hash@com.example.android/";
constexpr char kBackendErrorCodeMetric[] =
    "PasswordManager.PasswordStoreAndroidBackend.ErrorCode";
constexpr char kBackendApiErrorMetric[] =
    "PasswordManager.PasswordStoreAndroidBackend.APIError";
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

PasswordForm CreateEntry(const std::string& username,
                         const std::string& password,
                         const GURL& origin_url,
                         PasswordForm::MatchType match_type) {
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

}  // namespace

class PasswordStoreAndroidAccountBackendTest : public testing::Test {
 protected:
  PasswordStoreAndroidAccountBackendTest() {
    mock_affiliation_service_ =
        std::make_unique<testing::NiceMock<MockAffiliationService>>();

    prefs_.registry()->RegisterBooleanPref(
        prefs::kUnenrolledFromGoogleMobileServicesDueToErrors, false);
    prefs_.registry()->RegisterIntegerPref(
        prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);
    prefs_.registry()->RegisterDoublePref(prefs::kTimeOfLastMigrationAttempt,
                                          20.22);
    prefs_.registry()->RegisterIntegerPref(
        prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(prefs::UseUpmLocalAndSeparateStoresState::kOff));
    prefs_.registry()->RegisterBooleanPref(
        prefs::kEmptyProfileStoreLoginDatabase, false);

    backend_ = std::make_unique<PasswordStoreAndroidAccountBackend>(
        base::PassKey<class PasswordStoreAndroidAccountBackendTest>(),
        CreateMockBridgeHelper(), CreateFakeLifecycleHelper(),
        CreatePasswordSyncControllerDelegate(), &prefs_,
        &password_affiliation_adapter_);
  }

  ~PasswordStoreAndroidAccountBackendTest() override {
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
  MockAffiliationService* mock_affiliation_service() {
    return mock_affiliation_service_.get();
  }
  PasswordAffiliationSourceAdapter* affiliation_source_adapter() {
    return &password_affiliation_adapter_;
  }
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void EnableSyncForTestAccount() {
    sync_service_.GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, {syncer::UserSelectableType::kPasswords});
    AccountInfo account_info;
    account_info.email = kTestAccount;
    sync_service_.SetSignedIn(signin::ConsentLevel::kSync, account_info);
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

  std::unique_ptr<PasswordSyncControllerDelegateAndroid>
  CreatePasswordSyncControllerDelegate() {
    auto unique_delegate = std::make_unique<
        PasswordSyncControllerDelegateAndroid>(
        std::make_unique<NiceMock<MockPasswordSyncControllerDelegateBridge>>());
    sync_controller_delegate_ = unique_delegate.get();
    return unique_delegate;
  }

  std::unique_ptr<MockAffiliationService> mock_affiliation_service_;
  PasswordAffiliationSourceAdapter password_affiliation_adapter_;
  std::unique_ptr<PasswordStoreAndroidAccountBackend> backend_;
  raw_ptr<NiceMock<MockPasswordStoreAndroidBackendBridgeHelper>> bridge_helper_;
  raw_ptr<FakePasswordManagerLifecycleHelper> lifecycle_helper_;
  raw_ptr<PasswordSyncControllerDelegateAndroid> sync_controller_delegate_;
  syncer::TestSyncService sync_service_;
  TestingPrefServiceSimple prefs_;
};

TEST_F(PasswordStoreAndroidAccountBackendTest,
       IsAbleToSavePasswordsDependsOnSyncInit) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  EXPECT_FALSE(backend().IsAbleToSavePasswords());
  backend().OnSyncServiceInitialized(sync_service());
  EXPECT_TRUE(backend().IsAbleToSavePasswords());
}

TEST_F(PasswordStoreAndroidAccountBackendTest, CallsBridgeForLogins) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  EnableSyncForTestAccount();
  backend().OnSyncServiceInitialized(sync_service());

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins(kTestAccount))
      .WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());

  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<LoginsResult>(ElementsAreArray(CreateTestLogins()))));
  consumer().OnCompleteWithLogins(kJobId, CreateTestLogins());
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidAccountBackendTest, FillMatchingLoginsNoPSL) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
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
  EXPECT_CALL(mock_reply, Run(VariantWith<LoginsResult>(ElementsAre(
                              matching_federated, matching_signon_realm))));

  task_environment_.FastForwardBy(kTestLatencyDelta);
  consumer().OnCompleteWithLogins(kSecondJobId, {matching_signon_realm});
  RunUntilIdle();

  histogram_tester.ExpectTimeBucketCount(
      DurationMetricName("FillMatchingLoginsAsync"), kTestLatencyDelta, 1);
}

TEST_F(PasswordStoreAndroidAccountBackendTest, FillMatchingLoginsPSL) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
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
  EXPECT_CALL(mock_reply, Run(VariantWith<LoginsResult>(UnorderedElementsAre(
                              psl_matching, psl_matching_federated))));

  task_environment_.FastForwardBy(kTestLatencyDelta);
  consumer().OnCompleteWithLogins(kSecondJobId, {psl_matching, not_matching});
  RunUntilIdle();
  histogram_tester.ExpectTimeBucketCount(
      DurationMetricName("FillMatchingLoginsAsync"), kTestLatencyDelta, 1);
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       FillMatchingLoginsGooglePSLMatch) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
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
  EXPECT_CALL(mock_reply,
              Run(VariantWith<LoginsResult>(ElementsAre(exact_match))));

  consumer().OnCompleteWithLogins(kFirstJobId, {exact_match, psl_match});
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       CallsBridgeForAutofillableLogins) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAutofillableLogins).WillOnce(Return(kJobId));
  backend().GetAutofillableLoginsAsync(mock_reply.Get());

  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<LoginsResult>(ElementsAreArray(CreateTestLogins()))));
  consumer().OnCompleteWithLogins(kJobId, CreateTestLogins());
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidAccountBackendTest, CallsBridgeForLoginsForAccount) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kJobId));
  std::string account = "mytestemail@gmail.com";
  backend().GetAllLoginsForAccountAsync(account, mock_reply.Get());

  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<LoginsResult>(ElementsAreArray(CreateTestLogins()))));
  consumer().OnCompleteWithLogins(kJobId, CreateTestLogins());
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidAccountBackendTest, CallsBridgeForRemoveLogin) {
  EnableSyncForTestAccount();
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
  const JobId kRemoveLoginJobId{13388};
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;

  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  EXPECT_CALL(*bridge_helper(), RemoveLogin(form, kTestAccount))
      .WillOnce(Return(kRemoveLoginJobId));
  backend().RemoveLoginAsync(FROM_HERE, form, mock_reply.Get());

  PasswordStoreChangeList expected_changes;
  expected_changes.emplace_back(
      PasswordStoreChange(PasswordStoreChange::REMOVE, form));
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(expected_changes))));
  consumer().OnLoginsChanged(kRemoveLoginJobId, expected_changes);
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       CallsBridgeForRemoveLoginsByURLAndTime) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
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
  backend().RemoveLoginsByURLAndTimeAsync(
      FROM_HERE, url_filter, delete_begin, delete_end,
      base::OnceCallback<void(bool)>(), mock_deletion_reply.Get());

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

TEST_F(PasswordStoreAndroidAccountBackendTest,
       CallsBridgeForRemoveLoginsCreatedBetween) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
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
  backend().RemoveLoginsCreatedBetweenAsync(FROM_HERE, delete_begin, delete_end,
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

TEST_F(PasswordStoreAndroidAccountBackendTest, CallsBridgeForAddLogin) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  EnableSyncForTestAccount();
  backend().OnSyncServiceInitialized(sync_service());

  const JobId kAddLoginJobId{13388};
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  EXPECT_CALL(*bridge_helper(), AddLogin(form, kTestAccount))
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

TEST_F(PasswordStoreAndroidAccountBackendTest,
       SanitizedFormBeforeCallingBridgeAddLogin) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  EnableSyncForTestAccount();
  backend().OnSyncServiceInitialized(sync_service());

  const JobId kAddLoginJobId{13388};
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  form.blocked_by_user = true;

  PasswordForm expected_form = form;
  expected_form.username_value.clear();
  expected_form.password_value.clear();

  EXPECT_CALL(*bridge_helper(), AddLogin(expected_form, kTestAccount))
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

TEST_F(PasswordStoreAndroidAccountBackendTest, CallsBridgeForUpdateLogin) {
  EnableSyncForTestAccount();
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
  const JobId kUpdateLoginJobId{13388};
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  EXPECT_CALL(*bridge_helper(), UpdateLogin(form, kTestAccount))
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

TEST_F(PasswordStoreAndroidAccountBackendTest,
       SanitizedFormBeforeCallingBridgeUpdateLogin) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  EnableSyncForTestAccount();
  backend().OnSyncServiceInitialized(sync_service());

  const JobId kUpdateLoginJobId{13388};
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  form.blocked_by_user = true;

  PasswordForm expected_form = form;
  expected_form.username_value.clear();
  expected_form.password_value.clear();

  EXPECT_CALL(*bridge_helper(), UpdateLogin(expected_form, kTestAccount))
      .WillOnce(Return(kUpdateLoginJobId));
  backend().UpdateLoginAsync(form, mock_reply.Get());

  PasswordStoreChangeList expected_changes;
  expected_changes.emplace_back(
      PasswordStoreChange(PasswordStoreChange::ADD, form));
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(expected_changes))));
  consumer().OnLoginsChanged(kUpdateLoginJobId, expected_changes);
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       OnExternalIgnoredErrorNotCausingExperimentUnenrollment) {
  base::HistogramTester histogram_tester;

  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());
  int kAuthErrorResolvableCode =
      static_cast<int>(AndroidBackendAPIErrorCode::kAuthErrorResolvable);
  PasswordStoreBackendError expected_error{
      PasswordStoreBackendErrorType::kAuthErrorResolvable};
  expected_error.android_backend_api_error = kAuthErrorResolvableCode;
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordStoreBackendError>(expected_error)));
  AndroidBackendError error{AndroidBackendErrorType::kExternalError};
  // Simulate receiving AUTH_ERROR_RESOLVABLE code.

  error.api_error_code = std::optional<int>(kAuthErrorResolvableCode);
  consumer().OnError(kJobId, std::move(error));
  RunUntilIdle();

  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_NE(prefs()->GetInteger(
                prefs::kCurrentMigrationVersionToGoogleMobileServices),
            0);
  EXPECT_NE(prefs()->GetDouble(prefs::kTimeOfLastMigrationAttempt), 0.0);

  histogram_tester.ExpectBucketCount(kBackendErrorCodeMetric, 7, 1);
  histogram_tester.ExpectBucketCount(kBackendApiErrorMetric,
                                     kAuthErrorResolvableCode, 1);
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       OnNetworkErrorRetriableStopsRetryingAfterTimeout) {
  base::HistogramTester histogram_tester;

  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
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
  PasswordStoreBackendError expected_error{
      PasswordStoreBackendErrorType::kUncategorized};
  expected_error.android_backend_api_error = kNetworkErrorCode;
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordStoreBackendError>(expected_error)));
  consumer().OnError(kJobId, CreateNetworkError());

  RunUntilIdle();

  // User should not be unenrolled even if retries failed as only operations
  // performed at Chrome startup are retried.
  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_NE(prefs()->GetInteger(
                prefs::kCurrentMigrationVersionToGoogleMobileServices),
            0);
  EXPECT_NE(prefs()->GetDouble(prefs::kTimeOfLastMigrationAttempt), 0.0);

  histogram_tester.ExpectBucketCount(kBackendErrorCodeMetric, 7, 1);
  histogram_tester.ExpectBucketCount(
      kBackendApiErrorMetric,
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

TEST_F(PasswordStoreAndroidAccountBackendTest,
       OnNetworkErrorRetriableStopsRetryingAfterSuccess) {
  base::HistogramTester histogram_tester;

  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
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
  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<LoginsResult>(ElementsAreArray(CreateTestLogins()))));
  consumer().OnCompleteWithLogins(kSucceedJobId, CreateTestLogins());
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

TEST_F(PasswordStoreAndroidAccountBackendTest,
       PostedDelayedRetryCancelledOnSyncStateChange) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
  EnableSyncForTestAccount();

  base::MockCallback<LoginsOrErrorReply> mock_reply;

  // GetAllLogins will be called once with a retriable error.
  const JobId kFailedJobId{1};
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kFailedJobId));

  base::Time before_call_time = task_environment_.GetMockClock()->Now();

  // Initiating the first call.
  backend().GetAllLoginsAsync(mock_reply.Get());

  // Answering the call with an error.
  consumer().OnError(kFailedJobId, CreateNetworkError());
  RunUntilIdle();

  DisableSyncFeature();
  sync_service()->FireStateChanged();
  PasswordStoreBackendError expected_error{
      PasswordStoreBackendErrorType::kUncategorized};
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordStoreBackendError>(expected_error)));
  RunUntilIdle();

  // Since the retry was cancelled, nothing should happen after the retry
  // timeout
  EXPECT_CALL(*bridge_helper(), GetAllLogins).Times(0);
  task_environment_.FastForwardUntilNoTasksRemain();
  base::Time after_retry_time = task_environment_.GetMockClock()->Now();
  EXPECT_GE(after_retry_time - before_call_time, base::Seconds(1));

  // Per-operation retry histograms
  histogram_tester.ExpectBucketCount(
      base::StrCat({kRetryHistogramBase, ".GetAllLoginsAsync.APIError"}),
      static_cast<int>(AndroidBackendAPIErrorCode::kNetworkError), 1);

  // "Attempt" is recorder when the method call attempt ends.
  histogram_tester.ExpectUniqueSample(
      base::StrCat({kRetryHistogramBase, ".GetAllLoginsAsync.Attempt"}), 1, 1);
  // "CancelledAtAttempt", records the attempt that was ongoing when the
  // sync status changes.
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {kRetryHistogramBase, ".GetAllLoginsAsync.CancelledAtAttempt"}),
      2, 1);

  // Aggregated retry histograms
  histogram_tester.ExpectBucketCount(
      base::StrCat({kRetryHistogramBase, ".APIError"}),
      static_cast<int>(AndroidBackendAPIErrorCode::kNetworkError), 1);

  histogram_tester.ExpectUniqueSample(
      base::StrCat({kRetryHistogramBase, ".Attempt"}), 1, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({kRetryHistogramBase, ".CancelledAtAttempt"}), 2, 1);
}

// Tests that switching sync state has no impact on retry tasks that have
// already been executed.
TEST_F(PasswordStoreAndroidAccountBackendTest,
       OnSyncStateChangeHasNoEffectOnFinishedRetries) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
  EnableSyncForTestAccount();

  base::MockCallback<LoginsOrErrorReply> mock_reply;

  // GetAllLogins will be called once with a retriable error.
  const JobId kFailedJobId{1};
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kFailedJobId));

  base::Time before_call_time = task_environment_.GetMockClock()->Now();

  // Initiating the first call.
  backend().GetAllLoginsAsync(mock_reply.Get());

  // Answering the call with an error.
  consumer().OnError(kFailedJobId, CreateNetworkError());
  RunUntilIdle();

  // Since the retry was cancelled, nothing should happen after the retry
  // timeout
  EXPECT_CALL(*bridge_helper(), GetAllLogins);
  // Execute the posted delayed retry.
  task_environment_.FastForwardUntilNoTasksRemain();

  base::Time after_retry_time = task_environment_.GetMockClock()->Now();

  EXPECT_GE(after_retry_time - before_call_time, base::Seconds(1));

  // Change the sync state. Since the retry was already executed, the
  // state change shouldn't invoke the reply callback.
  EXPECT_CALL(mock_reply, Run).Times(0);
  DisableSyncFeature();
  sync_service()->FireStateChanged();

  // "Attempt" is recorded when the method call attempt ends.
  histogram_tester.ExpectUniqueSample(
      base::StrCat({kRetryHistogramBase, ".GetAllLoginsAsync.Attempt"}), 1, 1);
  // Expect that no attempts were cancelled.
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {kRetryHistogramBase, ".GetAllLoginsAsync.CancelledAtAttempt"}),
      0);

  histogram_tester.ExpectTotalCount(
      base::StrCat({kRetryHistogramBase, ".CancelledAtAttempt"}), 0);
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       PostedDelayedRetryCancelledOnSyncShutdown) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
  EnableSyncForTestAccount();

  base::MockCallback<LoginsOrErrorReply> mock_reply;

  // GetAllLogins will be called once with a retriable error.
  const JobId kFailedJobId{1};
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kFailedJobId));

  base::Time before_call_time = task_environment_.GetMockClock()->Now();

  // Initiating the first call.
  backend().GetAllLoginsAsync(mock_reply.Get());

  // Answering the call with an error.
  consumer().OnError(kFailedJobId, CreateNetworkError());
  RunUntilIdle();

  sync_service()->Shutdown();
  PasswordStoreBackendError expected_error{
      PasswordStoreBackendErrorType::kUncategorized};
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordStoreBackendError>(expected_error)));
  RunUntilIdle();

  // Since the retry was cancelled, nothing should happen after the retry
  // timeout
  EXPECT_CALL(*bridge_helper(), GetAllLogins).Times(0);
  task_environment_.FastForwardUntilNoTasksRemain();
  base::Time after_retry_time = task_environment_.GetMockClock()->Now();
  EXPECT_GE(after_retry_time - before_call_time, base::Seconds(1));

  // Per-operation retry histograms
  histogram_tester.ExpectBucketCount(
      base::StrCat({kRetryHistogramBase, ".GetAllLoginsAsync.APIError"}),
      static_cast<int>(AndroidBackendAPIErrorCode::kNetworkError), 1);

  // "Attempt" is recorder when the method call attempt ends.
  histogram_tester.ExpectUniqueSample(
      base::StrCat({kRetryHistogramBase, ".GetAllLoginsAsync.Attempt"}), 1, 1);
  // "CancelledAtAttempt", records the attempt that was ongoing when the
  // sync status changes.
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {kRetryHistogramBase, ".GetAllLoginsAsync.CancelledAtAttempt"}),
      2, 1);

  // Aggregated retry histograms
  histogram_tester.ExpectBucketCount(
      base::StrCat({kRetryHistogramBase, ".APIError"}),
      static_cast<int>(AndroidBackendAPIErrorCode::kNetworkError), 1);

  histogram_tester.ExpectUniqueSample(
      base::StrCat({kRetryHistogramBase, ".Attempt"}), 1, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({kRetryHistogramBase, ".CancelledAtAttempt"}), 2, 1);
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       OnExternalAuthErrorNotCausingExperimentUnenrollmentButSuspendsSaving) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
  ASSERT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_TRUE(backend().IsAbleToSavePasswords());

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());
  int kUnresolvableAuthErrorCode =
      static_cast<int>(AndroidBackendAPIErrorCode::kAuthErrorUnresolvable);
  PasswordStoreBackendError expected_error{
      PasswordStoreBackendErrorType::kAuthErrorUnresolvable};
  expected_error.android_backend_api_error = kUnresolvableAuthErrorCode;
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordStoreBackendError>(expected_error)));
  AndroidBackendError error{AndroidBackendErrorType::kExternalError};
  // Simulate receiving AUTH_ERROR_UNRESOLVABLE code.
  error.api_error_code = std::optional<int>(kUnresolvableAuthErrorCode);
  consumer().OnError(kJobId, std::move(error));
  RunUntilIdle();

  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_FALSE(backend().IsAbleToSavePasswords());
  histogram_tester.ExpectBucketCount(
      "PasswordManager.PasswordSavingDisabledDueToGMSCoreError", 0, 1);
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       ResetTemporarySavingSuspensionAfterSuccessfulLogin) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

  EXPECT_TRUE(backend().IsAbleToSavePasswords());

  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(base::DoNothing());
  AndroidBackendError error{AndroidBackendErrorType::kExternalError};
  error.api_error_code =
      static_cast<int>(AndroidBackendAPIErrorCode::kAuthErrorUnresolvable);
  consumer().OnError(kJobId, std::move(error));

  EXPECT_FALSE(backend().IsAbleToSavePasswords());

  // Simulate a successful logins call.
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());
  EXPECT_CALL(mock_reply, Run);
  task_environment_.FastForwardBy(kTestLatencyDelta);
  consumer().OnCompleteWithLogins(kJobId, {});
  RunUntilIdle();

  EXPECT_TRUE(backend().IsAbleToSavePasswords());
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       PassphraseRequiredErrorCausesNoUnenrollmentIfFixSupported) {
  base::HistogramTester histogram_tester;

  base::MockCallback<base::RepeatingClosure> send_passphrase_cb;
  EXPECT_CALL(send_passphrase_cb, Run());
  sync_service()->SetPassphrasePlatformClientCallback(send_passphrase_cb.Get());
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());
  int kPassphraseRequiredErrorCode =
      static_cast<int>(AndroidBackendAPIErrorCode::kPassphraseRequired);
  PasswordStoreBackendError expected_error{
      PasswordStoreBackendErrorType::kUncategorized};
  expected_error.android_backend_api_error = kPassphraseRequiredErrorCode;
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordStoreBackendError>(expected_error)));
  // Simulate receiving PASSPHRASE_REQUIRED code.
  consumer().OnError(kJobId, {.type = AndroidBackendErrorType::kExternalError,
                              .api_error_code = kPassphraseRequiredErrorCode});
  RunUntilIdle();

  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_NE(prefs()->GetInteger(
                prefs::kCurrentMigrationVersionToGoogleMobileServices),
            0);
  EXPECT_NE(prefs()->GetDouble(prefs::kTimeOfLastMigrationAttempt), 0.0);
  EXPECT_FALSE(backend().IsAbleToSavePasswords());
  histogram_tester.ExpectBucketCount(kBackendErrorCodeMetric, 7, 1);
  histogram_tester.ExpectBucketCount(kBackendApiErrorMetric,
                                     kPassphraseRequiredErrorCode, 1);
}

TEST_F(PasswordStoreAndroidAccountBackendTest, DisableAutoSignInForOrigins) {
  base::HistogramTester histogram_tester;

  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
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
  EXPECT_CALL(
      *bridge_helper(),
      UpdateLogin(FormWithDisabledAutoSignIn(form_to_update2), kTestAccount))
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
  EXPECT_CALL(
      *bridge_helper(),
      UpdateLogin(FormWithDisabledAutoSignIn(form_to_update1), kTestAccount))
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

TEST_F(PasswordStoreAndroidAccountBackendTest,
       NotifyStoreOnForegroundSessionStart) {
  base::MockCallback<PasswordStoreBackend::RemoteChangesReceived>
      store_notification_trigger;
  backend().InitBackend(
      nullptr,
      /*remote_form_changes_received=*/store_notification_trigger.Get(),
      /*sync_enabled_or_disabled_cb=*/base::NullCallback(),
      /*completion=*/base::DoNothing());

  // The initial foregrounding should issue a delayed notification so it
  // doesn't overlap with other startup tasks.
  EXPECT_CALL(store_notification_trigger, Run(_)).Times(0);
  lifecycle_helper()->OnForegroundSessionStart();

  EXPECT_CALL(store_notification_trigger, Run(Eq(std::nullopt)));
  task_environment_.FastForwardBy(base::Seconds(5));

  // Subsequent foregroundings should issue immediate notifications.
  EXPECT_CALL(store_notification_trigger, Run(Eq(std::nullopt)));
  lifecycle_helper()->OnForegroundSessionStart();
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       AttachesObserverOnSyncServiceInitialized) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

  EXPECT_TRUE(sync_service()->HasObserver(sync_controller_delegate()));
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       CancelPendingJobsOnSyncStateChange) {
  const std::string kSuccessMetric = SuccessMetricName("GetAllLoginsAsync");
  const std::string kDurationMetric = DurationMetricName("GetAllLoginsAsync");
  base::HistogramTester histogram_tester;
  backend().InitBackend(/*affiliated_match_helper=*/nullptr,
                        /*remote_form_changes_received=*/base::DoNothing(),
                        /*sync_enabled_or_disabled_cb=*/base::NullCallback(),
                        /*completion=*/base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
  EnableSyncForTestAccount();

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kJobId));

  // This call will queue the job.
  backend().GetAllLoginsAsync(mock_reply.Get());

  DisableSyncFeature();
  sync_service()->FireStateChanged();
  PasswordStoreBackendError expected_error{
      PasswordStoreBackendErrorType::kUncategorized};
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordStoreBackendError>(expected_error)));
  RunUntilIdle();
  histogram_tester.ExpectUniqueSample(kSuccessMetric, false, 1);
  histogram_tester.ExpectUniqueSample(
      kBackendErrorCodeMetric,
      AndroidBackendErrorType::kCancelledPwdSyncStateChanged, 1);
  histogram_tester.ExpectTotalCount(kDurationMetric, 0);
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       CancelPendingJobsOnSyncShutdown) {
  const std::string kSuccessMetric = SuccessMetricName("GetAllLoginsAsync");
  const std::string kDurationMetric = DurationMetricName("GetAllLoginsAsync");
  base::HistogramTester histogram_tester;
  backend().InitBackend(/*affiliated_match_helper=*/nullptr,
                        /*remote_form_changes_received=*/base::DoNothing(),
                        /*sync_enabled_or_disabled_cb=*/base::NullCallback(),
                        /*completion=*/base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
  EnableSyncForTestAccount();

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kJobId));

  // This call will queue the job.
  backend().GetAllLoginsAsync(mock_reply.Get());

  sync_service()->Shutdown();
  PasswordStoreBackendError expected_error{
      PasswordStoreBackendErrorType::kUncategorized};
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordStoreBackendError>(expected_error)));
  RunUntilIdle();
  histogram_tester.ExpectUniqueSample(kSuccessMetric, false, 1);
  histogram_tester.ExpectUniqueSample(
      kBackendErrorCodeMetric,
      AndroidBackendErrorType::kCancelledPwdSyncStateChanged, 1);
  histogram_tester.ExpectTotalCount(kDurationMetric, 0);
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       RecordClearedZombieTaskWithoutLatency) {
  const char kStartedMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.AddLoginAsync";
  const std::string kDurationMetric = DurationMetricName("AddLoginAsync");
  const std::string kSuccessMetric = SuccessMetricName("AddLoginAsync");
  base::HistogramTester histogram_tester;
  backend().InitBackend(/*affiliated_match_helper=*/nullptr,
                        /*remote_form_changes_received=*/base::DoNothing(),
                        /*sync_enabled_or_disabled_cb=*/base::NullCallback(),
                        /*completion=*/base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

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
  consumer().OnLoginsChanged(kJobId, std::nullopt);
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

TEST_F(PasswordStoreAndroidAccountBackendTest,
       RecordsRequestStartAndEndMetric) {
  const char kStartedMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.AddLoginAsync";
  base::HistogramTester histogram_tester;
  backend().InitBackend(/*affiliated_match_helper=*/nullptr,
                        /*remote_form_changes_received=*/base::DoNothing(),
                        /*sync_enabled_or_disabled_cb=*/base::NullCallback(),
                        /*completion=*/base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

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
  consumer().OnLoginsChanged(kJobId, std::nullopt);

  // After execution, check that request is logged again.
  EXPECT_THAT(histogram_tester.GetAllSamples(kStartedMetric),
              ElementsAre(base::Bucket(/* Requested */ 0, 1),
                          base::Bucket(/* Completed */ 2, 1)));
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       RecordActiveStatusOnSyncServiceInitialized) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  base::HistogramTester histogram_tester;
  sync_service()->GetUserSettings()->SetSelectedTypes(
      false, {syncer::UserSelectableType::kPasswords});
  backend().OnSyncServiceInitialized(sync_service());
  histogram_tester.ExpectUniqueSample(
      kUPMActiveHistogram, UnifiedPasswordManagerActiveStatus::kActive, 1);
}

TEST_F(PasswordStoreAndroidAccountBackendTest, RecordInactiveStatusSyncOff) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  sync_service()->GetUserSettings()->SetSelectedTypes(false, {});
  backend().OnSyncServiceInitialized(sync_service());
  histogram_tester.ExpectUniqueSample(
      kUPMActiveHistogram, UnifiedPasswordManagerActiveStatus::kInactiveSyncOff,
      1);
}

TEST_F(PasswordStoreAndroidAccountBackendTest, RecordInactiveStatusUnenrolled) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  base::HistogramTester histogram_tester;
  sync_service()->GetUserSettings()->SetSelectedTypes(
      false, {syncer::UserSelectableType::kPasswords});
  prefs()->SetBoolean(prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
                      true);
  backend().OnSyncServiceInitialized(sync_service());
  histogram_tester.ExpectUniqueSample(
      kUPMActiveHistogram,
      UnifiedPasswordManagerActiveStatus::kInactiveUnenrolledDueToErrors, 1);
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       FillMatchingLoginsWithSchemeMismatch) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
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
  EXPECT_CALL(mock_reply,
              Run(VariantWith<LoginsResult>(ElementsAre(exact_match))));

  consumer().OnCompleteWithLogins(kFirstJobId, {exact_match, psl_match});
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidAccountBackendTest, GetGroupedMatchingLoginsAsync) {
  FakeAffiliationService fake_affiliation_service;
  MockAffiliatedMatchHelper mock_affiliated_match_helper(
      &fake_affiliation_service);
  backend().InitBackend(
      &mock_affiliated_match_helper,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
  base::MockCallback<LoginsOrErrorReply> mock_reply;

  const JobId kFirstJobId{1337};
  const JobId kSecondJobId{2903};
  EXPECT_CALL(*bridge_helper(), GetLoginsForSignonRealm("test.com", _))
      .WillOnce(Return(kFirstJobId));
  EXPECT_CALL(*bridge_helper(), GetLoginsForSignonRealm(kTestAndroidRealm, _))
      .WillOnce(Return(kSecondJobId));

  std::string TestURL1("https://a.test.com/");
  PasswordFormDigest form_digest(PasswordForm::Scheme::kHtml, TestURL1,
                                 GURL(TestURL1));

  EXPECT_CALL(*bridge_helper(), CanUseGetAffiliatedPasswordsAPI)
      .WillOnce(Return(false));

  std::vector<std::string> affiliated_android_realms;
  affiliated_android_realms.push_back(kTestAndroidRealm);
  mock_affiliated_match_helper.ExpectCallToGetAffiliatedAndGrouped(
      form_digest, affiliated_android_realms);
  mock_affiliated_match_helper
      .ExpectCallToInjectAffiliationAndBrandingInformation({});
  backend().GetGroupedMatchingLoginsAsync(form_digest, mock_reply.Get());

  // Imitate login retrieval.
  PasswordForm exact_match = CreateTestLogin(
      kTestUsername, kTestPassword, "https://a.test.com/", kTestDateCreated);
  PasswordForm psl_match = CreateTestLogin(
      kTestUsername, kTestPassword, "https://b.test.com/", kTestDateCreated);
  PasswordForm android_match = CreateTestLogin(
      kTestUsername, kTestPassword, kTestAndroidRealm, kTestDateCreated);

  // Retrieving logins for the last form should trigger the final callback.
  LoginsResult expected_logins;
  expected_logins.push_back(exact_match);
  expected_logins.back().match_type = PasswordForm::MatchType::kExact;
  expected_logins.push_back(psl_match);
  expected_logins.back().match_type = PasswordForm::MatchType::kPSL;
  expected_logins.push_back(android_match);
  expected_logins.back().match_type = PasswordForm::MatchType::kAffiliated;

  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<LoginsResult>(ElementsAreArray(expected_logins))));

  consumer().OnCompleteWithLogins(kFirstJobId, {exact_match, psl_match});
  consumer().OnCompleteWithLogins(kSecondJobId, {android_match});
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       CallsBridgeForGroupedMatchingLogins) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  std::string TestURL1("https://example.com/");
  PasswordFormDigest form_digest(PasswordForm::Scheme::kHtml, TestURL1,
                                 GURL(TestURL1));

  EXPECT_CALL(*bridge_helper(), CanUseGetAffiliatedPasswordsAPI)
      .WillOnce(Return(true));
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

  base::HistogramTester histogram_tester;
  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<LoginsResult>(ElementsAreArray(expected_logins))));
  consumer().OnCompleteWithLogins(kJobId, std::move(returned_logins));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       GetAllLoginsWithAffiliationAndBrandingInformation) {
  FakeAffiliationService fake_affiliation_service;
  MockAffiliatedMatchHelper mock_affiliated_match_helper(
      &fake_affiliation_service);
  backend().InitBackend(
      &mock_affiliated_match_helper,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
  std::vector<MockAffiliatedMatchHelper::AffiliationAndBrandingInformation>
      affiliation_info_for_results = {
          {kTestUrl, kTestAndroidName, GURL(kTestAndroidIconURL)},
          {/* Pretend affiliation or branding info is unavailable. */}};

  mock_affiliated_match_helper
      .ExpectCallToInjectAffiliationAndBrandingInformation(
          affiliation_info_for_results);

  PasswordForm android_form = CreateTestLogin(
      kTestUsername, kTestPassword, kTestAndroidRealm, kTestDateCreated);
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);

  std::vector<PasswordForm> expected_results;
  expected_results.push_back(android_form);
  // Expect branding info for android credential.
  expected_results.back().affiliated_web_realm = kTestUrl;
  expected_results.back().app_display_name = kTestAndroidName;
  expected_results.back().app_icon_url = GURL(kTestAndroidIconURL);
  expected_results.push_back(form);

  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillOnce(Return(kJobId));

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  EXPECT_CALL(*bridge_helper(), CanUseGetAllLoginsWithBrandingInfoAPI)
      .WillOnce(Return(false));
  backend().GetAllLoginsWithAffiliationAndBrandingAsync(mock_reply.Get());
  RunUntilIdle();

  std::vector<PasswordForm> returned_forms;
  returned_forms.push_back(android_form);
  returned_forms.push_back(form);
  consumer().OnCompleteWithLogins(kJobId, std::move(returned_forms));

  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<LoginsResult>(ElementsAreArray(expected_results))));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       CallsBridgeForGetAllLoginsWithAffiliationAndBrandingInformation) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  std::string TestURL1("https://example.com/");
  PasswordFormDigest form_digest(PasswordForm::Scheme::kHtml, TestURL1,
                                 GURL(TestURL1));

  EXPECT_CALL(*bridge_helper(), CanUseGetAllLoginsWithBrandingInfoAPI)
      .WillOnce(Return(true));
  EXPECT_CALL(*bridge_helper(), GetAllLoginsWithBrandingInfo(_))
      .WillOnce(Return(kJobId));
  backend().GetAllLoginsWithAffiliationAndBrandingAsync(mock_reply.Get());

  PasswordForm android_form = CreateTestLogin(
      kTestUsername, kTestPassword, kTestAndroidRealm, kTestDateCreated);
  android_form.app_display_name = kTestAndroidName;
  android_form.app_icon_url = GURL(kTestAndroidIconURL);
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);

  consumer().OnCompleteWithLogins(kJobId, {android_form, form});
  EXPECT_CALL(mock_reply,
              Run(VariantWith<LoginsResult>(ElementsAre(android_form, form))));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       DisablesAffiliationsPrefetching) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::RepeatingClosure(), base::DoNothing());
  EnableSyncForTestAccount();

  EXPECT_CALL(*bridge_helper(), CanUseGetAllLoginsWithBrandingInfoAPI)
      .WillOnce(Return(true));
  backend().OnSyncServiceInitialized(sync_service());

  // Test that the affiliation source got disabled and the data layer is never
  // queried.
  base::MockCallback<affiliations::AffiliationSource::ResultCallback> callback;
  EXPECT_CALL(callback, Run(IsEmpty()));
  EXPECT_CALL(*bridge_helper(), GetAllLogins).Times(0);
  affiliation_source_adapter()->GetFacets(callback.Get());
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       GetAllLoginsReturnsEmptyResultWhenSyncOff) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::RepeatingClosure(), base::DoNothing());
  DisableSyncFeature();
  backend().OnSyncServiceInitialized(sync_service());

  base::MockCallback<LoginsOrErrorReply> mock_reply;

  EXPECT_CALL(*bridge_helper(), GetAllLogins).Times(0);
  backend().GetAllLoginsAsync(mock_reply.Get());
  EXPECT_CALL(mock_reply, Run(VariantWith<LoginsResult>(IsEmpty())));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       GetAllLoginsWithBrandingReturnsEmptyResultWhenSyncOff) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::RepeatingClosure(), base::DoNothing());
  DisableSyncFeature();
  backend().OnSyncServiceInitialized(sync_service());

  base::MockCallback<LoginsOrErrorReply> mock_reply;

  EXPECT_CALL(*bridge_helper(), GetAllLoginsWithBrandingInfo).Times(0);
  backend().GetAllLoginsWithAffiliationAndBrandingAsync(mock_reply.Get());

  EXPECT_CALL(mock_reply, Run(VariantWith<LoginsResult>(IsEmpty())));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       GetAutofillableLoginsReturnsEmptyResultWhenSyncOff) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::RepeatingClosure(), base::DoNothing());
  DisableSyncFeature();
  backend().OnSyncServiceInitialized(sync_service());

  base::MockCallback<LoginsOrErrorReply> mock_reply;

  EXPECT_CALL(*bridge_helper(), GetAutofillableLogins).Times(0);
  backend().GetAutofillableLoginsAsync(mock_reply.Get());

  EXPECT_CALL(mock_reply, Run(VariantWith<LoginsResult>(IsEmpty())));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       FillMatchingReturnsEmptyResultWhenSyncOff) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::RepeatingClosure(), base::DoNothing());
  DisableSyncFeature();
  backend().OnSyncServiceInitialized(sync_service());

  base::MockCallback<LoginsOrErrorReply> mock_reply;

  EXPECT_CALL(*bridge_helper(), GetLoginsForSignonRealm).Times(0);
  std::vector<PasswordFormDigest> forms = {PasswordFormDigest(
      PasswordForm::Scheme::kHtml, kTestUrl, GURL(kTestUrl))};
  backend().FillMatchingLoginsAsync(mock_reply.Get(), /*include_psl=*/false,
                                    forms);

  EXPECT_CALL(mock_reply, Run(VariantWith<LoginsResult>(IsEmpty())));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       GetGroupedLoginsReturnsEmptyResultWhenSyncOff) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::RepeatingClosure(), base::DoNothing());
  DisableSyncFeature();
  backend().OnSyncServiceInitialized(sync_service());

  base::MockCallback<LoginsOrErrorReply> mock_reply;

  EXPECT_CALL(*bridge_helper(), GetAffiliatedLoginsForSignonRealm).Times(0);
  backend().GetGroupedMatchingLoginsAsync(
      PasswordFormDigest(PasswordForm::Scheme::kHtml, kTestUrl, GURL(kTestUrl)),
      mock_reply.Get());

  EXPECT_CALL(mock_reply, Run(VariantWith<LoginsResult>(IsEmpty())));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       RemoveLoginReturnsEmptyResultWhenSyncOff) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::RepeatingClosure(), base::DoNothing());
  DisableSyncFeature();
  backend().OnSyncServiceInitialized(sync_service());

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  EXPECT_CALL(*bridge_helper(), RemoveLogin).Times(0);
  backend().RemoveLoginAsync(FROM_HERE, form, mock_reply.Get());

  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(IsEmpty()))));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       RemoveLoginsByURLAndTimeReturnsEmptyResultWhenSyncOff) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::RepeatingClosure(), base::DoNothing());
  DisableSyncFeature();
  backend().OnSyncServiceInitialized(sync_service());

  base::RepeatingCallback<bool(const GURL&)> url_filter =
      base::BindRepeating([](const GURL& url) { return true; });
  base::Time delete_begin = base::Time::FromTimeT(1000);
  base::Time delete_end = base::Time::FromTimeT(2000);

  EXPECT_CALL(*bridge_helper(), RemoveLogin).Times(0);
  EXPECT_CALL(*bridge_helper(), GetAllLogins).Times(0);

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  backend().RemoveLoginsByURLAndTimeAsync(FROM_HERE, url_filter, delete_begin,
                                          delete_end, base::DoNothing(),
                                          mock_reply.Get());

  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(IsEmpty()))));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       RemoveLoginsCreatedBetweenReturnsEmptyResultWhenSyncOff) {
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::RepeatingClosure(), base::DoNothing());
  DisableSyncFeature();
  backend().OnSyncServiceInitialized(sync_service());

  base::Time delete_begin = base::Time::FromTimeT(1000);
  base::Time delete_end = base::Time::FromTimeT(2000);

  EXPECT_CALL(*bridge_helper(), RemoveLogin).Times(0);
  EXPECT_CALL(*bridge_helper(), GetAllLogins).Times(0);

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  backend().RemoveLoginsCreatedBetweenAsync(FROM_HERE, delete_begin, delete_end,
                                            mock_reply.Get());

  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(IsEmpty()))));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidAccountBackendTest, RecordPasswordStoreMetrics) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());

  backend().RecordAddLoginAsyncCalledFromTheStore();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordStore.AccountBackend.AddLoginCalledOnStore",
      true, 1);

  backend().RecordUpdateLoginAsyncCalledFromTheStore();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordStore.AccountBackend.UpdateLoginCalledOnStore",
      true, 1);
}

// Checks that unenrollment is disabled post M4.
TEST_F(PasswordStoreAndroidAccountBackendTest, NoEvictIfM4FlagEnabled) {
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  backend().InitBackend(
      /*affiliated_match_helper=*/nullptr,
      PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      base::NullCallback(), base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillRepeatedly(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());
  RunUntilIdle();

  AndroidBackendError error(AndroidBackendErrorType::kExternalError);
  error.api_error_code =
      static_cast<int>(AndroidBackendAPIErrorCode::kAccessDenied);

  consumer().OnError(kJobId, error);
  RunUntilIdle();

  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_FALSE(backend().IsAbleToSavePasswords());
}

TEST_F(PasswordStoreAndroidAccountBackendTest,
       CallOnSyncEnabledDisabledCallbackOnSyncChanges) {
  EnableSyncForTestAccount();

  base::MockRepeatingClosure mock_callback;
  backend().InitBackend(/*affiliated_match_helper=*/nullptr,
                        /*remote_form_changes_received=*/base::DoNothing(),
                        /*sync_enabled_or_disabled_cb=*/mock_callback.Get(),
                        /*completion=*/base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

  EXPECT_CALL(mock_callback, Run);

  DisableSyncFeature();
  RunUntilIdle();
}

// Test suite to verify there is no unenrollment for most of the errors except
// Passphrase. Each backend operation is checked by a separate test.
class PasswordStoreAndroidAccountBackendWithoutUnenrollmentTest
    : public PasswordStoreAndroidAccountBackendTest,
      public testing::WithParamInterface<
          std::pair<AndroidBackendAPIErrorCode,
                    PasswordStoreBackendErrorType>> {
 protected:
  PasswordStoreAndroidAccountBackendWithoutUnenrollmentTest() {
    backend().InitBackend(
        /*affiliated_match_helper=*/nullptr,
        PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
        base::NullCallback(), base::DoNothing());
    backend().OnSyncServiceInitialized(sync_service());
    prefs()->SetInteger(
        prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(prefs::UseUpmLocalAndSeparateStoresState::kOn));
  }

  AndroidBackendAPIErrorCode GetAPIErrorCode() { return GetParam().first; }

  PasswordStoreBackendErrorType GetBackendErrorType() {
    return GetParam().second;
  }

  AndroidBackendError GetError() {
    AndroidBackendError error(AndroidBackendErrorType::kExternalError);
    error.api_error_code = static_cast<int>(GetAPIErrorCode());
    return error;
  }

  bool IsRetriableError() {
    const base::flat_set<AndroidBackendAPIErrorCode> kRetriableErrors = {
        AndroidBackendAPIErrorCode::kNetworkError,
        AndroidBackendAPIErrorCode::kApiNotConnected,
        AndroidBackendAPIErrorCode::kConnectionSuspendedDuringCall,
        AndroidBackendAPIErrorCode::kReconnectionTimedOut,
        AndroidBackendAPIErrorCode::kBackendGeneric};
    return kRetriableErrors.contains(GetAPIErrorCode());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(PasswordStoreAndroidAccountBackendWithoutUnenrollmentTest,
       NoEvictionOnGetAllLogins) {
  base::MockCallback<LoginsOrErrorReply> mock_reply;

  EXPECT_CALL(*bridge_helper(), GetAllLogins).WillRepeatedly(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());
  RunUntilIdle();

  PasswordStoreBackendError error(GetBackendErrorType());
  error.android_backend_api_error = GetError().api_error_code;

  EXPECT_CALL(mock_reply, Run(VariantWith<PasswordStoreBackendError>(error)));
  if (IsRetriableError()) {
    // Simulate failure on all replies.
    for (int i = 0; i < 6; i++) {
      // Answering the previous call with an error.
      // Simulate receiving NETWORK_ERROR code.
      consumer().OnError(kJobId, GetError());
      // Runs the delayed tasks which results in GetAllLogins being called on
      // the bridge.
      task_environment_.FastForwardUntilNoTasksRemain();
    }
  } else {
    consumer().OnError(kJobId, GetError());
    RunUntilIdle();
  }

  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  if (IsRetriableError()) {
    EXPECT_TRUE(backend().IsAbleToSavePasswords());
  } else {
    EXPECT_FALSE(backend().IsAbleToSavePasswords());
  }
}

TEST_P(PasswordStoreAndroidAccountBackendWithoutUnenrollmentTest,
       NoEvictionOnGetAutofillableLogins) {
  base::MockCallback<LoginsOrErrorReply> mock_reply;

  EXPECT_CALL(*bridge_helper(), GetAutofillableLogins)
      .WillRepeatedly(Return(kJobId));
  backend().GetAutofillableLoginsAsync(mock_reply.Get());
  RunUntilIdle();

  PasswordStoreBackendError error(GetBackendErrorType());
  error.android_backend_api_error = GetError().api_error_code;

  EXPECT_CALL(mock_reply, Run(VariantWith<PasswordStoreBackendError>(error)));
  if (IsRetriableError()) {
    // Simulate failure on all replies.
    for (int i = 0; i < 6; i++) {
      // Answering the previous call with an error.
      // Simulate receiving NETWORK_ERROR code.
      consumer().OnError(kJobId, GetError());
      // Runs the delayed tasks which results in GetAllLogins being called on
      // the bridge.
      task_environment_.FastForwardUntilNoTasksRemain();
    }
  } else {
    consumer().OnError(kJobId, GetError());
    RunUntilIdle();
  }

  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  if (IsRetriableError()) {
    EXPECT_TRUE(backend().IsAbleToSavePasswords());
  } else {
    EXPECT_FALSE(backend().IsAbleToSavePasswords());
  }
}

TEST_P(PasswordStoreAndroidAccountBackendWithoutUnenrollmentTest,
       NoEvictionOnGetAllLoginsWithAffiliationAndBrandingAsync) {
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  ON_CALL(*bridge_helper(), CanUseGetAllLoginsWithBrandingInfoAPI)
      .WillByDefault(Return(true));

  EXPECT_CALL(*bridge_helper(), GetAllLoginsWithBrandingInfo)
      .WillRepeatedly(Return(kJobId));
  backend().GetAllLoginsWithAffiliationAndBrandingAsync(mock_reply.Get());
  RunUntilIdle();

  PasswordStoreBackendError error(GetBackendErrorType());
  error.android_backend_api_error = GetError().api_error_code;

  EXPECT_CALL(mock_reply, Run(VariantWith<PasswordStoreBackendError>(error)));
  consumer().OnError(kJobId, GetError());
  RunUntilIdle();

  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_FALSE(backend().IsAbleToSavePasswords());
}

TEST_P(PasswordStoreAndroidAccountBackendWithoutUnenrollmentTest,
       NoEvictionOnFillMatchingLoginsAsync) {
  base::MockCallback<LoginsOrErrorReply> mock_reply;

  EXPECT_CALL(*bridge_helper(), GetLoginsForSignonRealm)
      .WillRepeatedly(Return(kJobId));
  std::string TestURL1("https://example.com/");
  std::vector<PasswordFormDigest> forms;

  forms.push_back(PasswordFormDigest(PasswordForm::Scheme::kHtml, TestURL1,
                                     GURL(TestURL1)));
  backend().FillMatchingLoginsAsync(mock_reply.Get(), /*include_psl=*/false,
                                    forms);
  RunUntilIdle();

  PasswordStoreBackendError error(GetBackendErrorType());
  error.android_backend_api_error = GetError().api_error_code;

  EXPECT_CALL(mock_reply, Run(VariantWith<PasswordStoreBackendError>(error)));
  consumer().OnError(kJobId, GetError());
  RunUntilIdle();

  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_FALSE(backend().IsAbleToSavePasswords());
}

TEST_P(PasswordStoreAndroidAccountBackendWithoutUnenrollmentTest,
       NoEvictionOnGetGroupedMatchingLoginsAsync) {
  EXPECT_CALL(*bridge_helper(), CanUseGetAffiliatedPasswordsAPI)
      .WillOnce(Return(true));
  base::MockCallback<LoginsOrErrorReply> mock_reply;

  EXPECT_CALL(*bridge_helper(), GetAffiliatedLoginsForSignonRealm)
      .WillRepeatedly(Return(kJobId));
  std::string TestURL1("https://example.com/");

  backend().GetGroupedMatchingLoginsAsync(
      PasswordFormDigest(PasswordForm::Scheme::kHtml, TestURL1, GURL(TestURL1)),
      mock_reply.Get());
  RunUntilIdle();

  PasswordStoreBackendError error(GetBackendErrorType());
  error.android_backend_api_error = GetError().api_error_code;

  EXPECT_CALL(mock_reply, Run(VariantWith<PasswordStoreBackendError>(error)));
  consumer().OnError(kJobId, GetError());
  RunUntilIdle();

  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_FALSE(backend().IsAbleToSavePasswords());
}

TEST_P(PasswordStoreAndroidAccountBackendWithoutUnenrollmentTest,
       NoEvictionOnAddLogin) {
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  EXPECT_CALL(*bridge_helper(), AddLogin(form, _)).WillOnce(Return(kJobId));
  backend().AddLoginAsync(form, mock_reply.Get());

  PasswordStoreBackendError error(GetBackendErrorType());
  error.android_backend_api_error = GetError().api_error_code;

  EXPECT_CALL(mock_reply, Run(VariantWith<PasswordStoreBackendError>(error)));
  consumer().OnError(kJobId, GetError());
  RunUntilIdle();

  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_FALSE(backend().IsAbleToSavePasswords());
}

TEST_P(PasswordStoreAndroidAccountBackendWithoutUnenrollmentTest,
       NoEvictionOnUpdateLogin) {
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  EXPECT_CALL(*bridge_helper(), UpdateLogin(form, _)).WillOnce(Return(kJobId));
  backend().UpdateLoginAsync(form, mock_reply.Get());

  PasswordStoreBackendError error(GetBackendErrorType());
  error.android_backend_api_error = GetError().api_error_code;

  EXPECT_CALL(mock_reply, Run(VariantWith<PasswordStoreBackendError>(error)));
  consumer().OnError(kJobId, GetError());
  RunUntilIdle();

  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_FALSE(backend().IsAbleToSavePasswords());
}

TEST_P(PasswordStoreAndroidAccountBackendWithoutUnenrollmentTest,
       NoEvictionOnRemoveLogin) {
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form =
      CreateTestLogin(kTestUsername, kTestPassword, kTestUrl, kTestDateCreated);
  EXPECT_CALL(*bridge_helper(), RemoveLogin(form, _)).WillOnce(Return(kJobId));
  backend().RemoveLoginAsync(FROM_HERE, form, mock_reply.Get());

  PasswordStoreBackendError error(GetBackendErrorType());
  error.android_backend_api_error = GetError().api_error_code;
  EXPECT_CALL(mock_reply, Run(VariantWith<PasswordStoreBackendError>(error)));
  consumer().OnError(kJobId, GetError());
  RunUntilIdle();

  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_FALSE(backend().IsAbleToSavePasswords());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PasswordStoreAndroidAccountBackendWithoutUnenrollmentTest,
    testing::ValuesIn(
        {std::make_pair(AndroidBackendAPIErrorCode::kBackendGeneric,
                        PasswordStoreBackendErrorType::kUncategorized),
         std::make_pair(AndroidBackendAPIErrorCode::kNetworkError,
                        PasswordStoreBackendErrorType::kUncategorized),
         std::make_pair(AndroidBackendAPIErrorCode::kInternalError,
                        PasswordStoreBackendErrorType::kUncategorized),
         std::make_pair(AndroidBackendAPIErrorCode::kDeveloperError,
                        PasswordStoreBackendErrorType::kUncategorized),
         std::make_pair(
             AndroidBackendAPIErrorCode::kConnectionSuspendedDuringCall,
             PasswordStoreBackendErrorType::kUncategorized),
         std::make_pair(AndroidBackendAPIErrorCode::kReconnectionTimedOut,
                        PasswordStoreBackendErrorType::kUncategorized),
         std::make_pair(AndroidBackendAPIErrorCode::kAccessDenied,
                        PasswordStoreBackendErrorType::kUncategorized),
         std::make_pair(AndroidBackendAPIErrorCode::kBadRequest,
                        PasswordStoreBackendErrorType::kUncategorized),
         std::make_pair(AndroidBackendAPIErrorCode::kBackendResourceExhausted,
                        PasswordStoreBackendErrorType::kUncategorized),
         std::make_pair(AndroidBackendAPIErrorCode::kInvalidData,
                        PasswordStoreBackendErrorType::kUncategorized),
         std::make_pair(AndroidBackendAPIErrorCode::kUnmappedErrorCode,
                        PasswordStoreBackendErrorType::kUncategorized),
         std::make_pair(AndroidBackendAPIErrorCode::kUnexpectedError,
                        PasswordStoreBackendErrorType::kUncategorized),
         std::make_pair(AndroidBackendAPIErrorCode::kApiNotConnected,
                        PasswordStoreBackendErrorType::kUncategorized),
         std::make_pair(AndroidBackendAPIErrorCode::kPassphraseRequired,
                        PasswordStoreBackendErrorType::kUncategorized),
         std::make_pair(AndroidBackendAPIErrorCode::kAuthErrorResolvable,
                        PasswordStoreBackendErrorType::kAuthErrorResolvable),
         std::make_pair(AndroidBackendAPIErrorCode::kAuthErrorUnresolvable,
                        PasswordStoreBackendErrorType::kAuthErrorUnresolvable),
         std::make_pair(AndroidBackendAPIErrorCode::kKeyRetrievalRequired,
                        PasswordStoreBackendErrorType::kKeyRetrievalRequired)}),
    [](const ::testing::TestParamInfo<
        std::pair<AndroidBackendAPIErrorCode, PasswordStoreBackendErrorType>>&
           info) {
      return "APIErrorCode_" +
             base::ToString(static_cast<int>(info.param.first));
    });

class PasswordStoreAndroidAccountBackendTestForMetrics
    : public PasswordStoreAndroidAccountBackendTest,
      public testing::WithParamInterface<bool> {
 public:
  bool ShouldSucceed() const { return GetParam(); }
};

// Tests the PasswordManager.PasswordStore.GetAllLoginsAsync metric.
TEST_P(PasswordStoreAndroidAccountBackendTestForMetrics,
       GetAllLoginsAsyncMetrics) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      nullptr, PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      /*sync_enabled_or_disabled_cb=*/base::NullCallback(),
      /*completion=*/base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

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
    error.api_error_code = std::optional<int>(kInternalApiErrorCode);
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
TEST_P(PasswordStoreAndroidAccountBackendTestForMetrics, AddLoginAsyncMetrics) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      nullptr, PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      /*sync_enabled_or_disabled_cb=*/base::NullCallback(),
      /*completion=*/base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

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
    consumer().OnLoginsChanged(kJobId, std::nullopt);
  } else {
    AndroidBackendError error{kExternalErrorType};
    // Simulate receiving INTERNAL_ERROR code.
    error.api_error_code = std::optional<int>(kInternalApiErrorCode);
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
TEST_P(PasswordStoreAndroidAccountBackendTestForMetrics,
       UpdateLoginAsyncMetrics) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      nullptr, PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      /*sync_enabled_or_disabled_cb=*/base::NullCallback(),
      /*completion=*/base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

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
    consumer().OnLoginsChanged(kJobId, std::nullopt);
  } else {
    AndroidBackendError error{kExternalErrorType};
    // Simulate receiving INTERNAL_ERROR code.
    error.api_error_code = std::optional<int>(kInternalApiErrorCode);
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
TEST_P(PasswordStoreAndroidAccountBackendTestForMetrics,
       RemoveLoginAsyncMetrics) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      nullptr, PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      /*sync_enabled_or_disabled_cb=*/base::NullCallback(),
      /*completion=*/base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

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
  backend().RemoveLoginAsync(FROM_HERE, form, mock_reply.Get());
  EXPECT_CALL(mock_reply, Run);
  task_environment_.FastForwardBy(kTestLatencyDelta);

  if (ShouldSucceed()) {
    consumer().OnLoginsChanged(kJobId, std::nullopt);
  } else {
    AndroidBackendError error{kExternalErrorType};
    // Simulate receiving INTERNAL_ERROR code.
    error.api_error_code = std::optional<int>(kInternalApiErrorCode);
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

TEST_P(PasswordStoreAndroidAccountBackendTestForMetrics,
       GetAutofillableLoginsAsyncMetrics) {
  base::HistogramTester histogram_tester;
  backend().InitBackend(
      nullptr, PasswordStoreAndroidAccountBackend::RemoteChangesReceived(),
      /*sync_enabled_or_disabled_cb=*/base::NullCallback(),
      /*completion=*/base::DoNothing());
  backend().OnSyncServiceInitialized(sync_service());

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
    error.api_error_code = std::optional<int>(kInternalApiErrorCode);
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
                         PasswordStoreAndroidAccountBackendTestForMetrics,
                         testing::Bool());

}  // namespace password_manager
