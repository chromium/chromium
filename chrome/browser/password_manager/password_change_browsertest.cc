// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <utility>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_change_service.h"
#include "chrome/browser/password_manager/password_change/change_password_form_finder.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/password_manager/password_change/password_change_submission_verifier.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/password_manager/password_change_delegate_impl.h"
#include "chrome/browser/password_manager/password_change_service_factory.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/passwords_navigation_observer.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/passwords/password_change_ui_controller.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/browser/ui/views/passwords/password_change/password_change_toast.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/test_event.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/button_test_api.h"

namespace {

using ::affiliations::MockAffiliationService;
using ::base::test::RunOnceCallback;
using ::base::test::RunOnceCallbackRepeatedly;
using ::optimization_guide::TestModelQualityLogsUploaderService;
using ::testing::_;
using ::testing::An;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::WithArg;
using FinalModelStatus = ::optimization_guide::proto::FinalModelStatus;
using OptimizationGuideModelExecutionError = ::optimization_guide::
    OptimizationGuideModelExecutionError::ModelExecutionError;
using PasswordChangeErrorCase = ::optimization_guide::proto::
    PasswordChangeSubmissionData_PasswordChangeErrorCase;
using PasswordChangeOutcome = ::optimization_guide::proto::
    PasswordChangeSubmissionData_PasswordChangeOutcome;
using OpenFormResponseData = ::optimization_guide::proto::OpenFormResponseData;
using QualityStatus = ::optimization_guide::proto::
    PasswordChangeQuality_StepQuality_SubmissionStatus;
using SubmissionOutcome = PasswordChangeSubmissionVerifier::SubmissionOutcome;

constexpr char kPasswordChangeSubmissionOutcomeHistogram[] =
    "PasswordManager.PasswordChangeSubmissionOutcome";
constexpr char kMainHost[] = "example.com";
constexpr char kDifferentHost[] = "foo.com";
constexpr char kChangePasswordURL[] = "https://example.com/password/";

class MockPasswordChangeDelegateObserver
    : public PasswordChangeDelegate::Observer {
 public:
  MOCK_METHOD(void,
              OnStateChanged,
              (PasswordChangeDelegate::State),
              (override));
  MOCK_METHOD(void,
              OnPasswordChangeStopped,
              (PasswordChangeDelegate*),
              (override));
};

std::unique_ptr<KeyedService> CreateTestAffiliationService(
    content::BrowserContext* context) {
  return std::make_unique<NiceMock<MockAffiliationService>>();
}

std::unique_ptr<KeyedService> CreateOptimizationService(
    content::BrowserContext* context) {
  return std::make_unique<NiceMock<MockOptimizationGuideKeyedService>>();
}

// Verifies that |test_ukm_recorder| recorder has a single entry called |entry|
// and returns it.
const ukm::mojom::UkmEntry* GetMetricEntry(
    const ukm::TestUkmRecorder& test_ukm_recorder,
    std::string_view entry) {
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>
      ukm_entries = test_ukm_recorder.GetEntriesByName(entry);
  EXPECT_THAT(ukm_entries, SizeIs(1));
  return ukm_entries[0];
}

}  // namespace

class PasswordChangeBrowserTest : public PasswordManagerBrowserTestBase {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    PasswordManagerBrowserTestBase::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  AffiliationServiceFactory::GetInstance()->SetTestingFactory(
                      context,
                      base::BindRepeating(&CreateTestAffiliationService));
                  OptimizationGuideKeyedServiceFactory::GetInstance()
                      ->SetTestingFactory(
                          context,
                          base::BindRepeating(&CreateOptimizationService));
                }));
  }

  void SetUpOnMainThread() override {
    PasswordManagerBrowserTestBase::SetUpOnMainThread();
    // Redirect all requests to localhost.
    host_resolver()->AddRule("*", "127.0.0.1");
    PasswordsNavigationObserver observer(WebContents());
    GURL url = embedded_test_server()->GetURL(kMainHost,
                                              "/password/simple_password.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    ASSERT_TRUE(observer.Wait());
  }

  void VerifyUniqueQualityLog(QualityStatus open_form_status,
                              QualityStatus submit_form_status,
                              QualityStatus verify_submission_status,
                              FinalModelStatus final_status) {
    const auto& logs = logs_uploader().uploaded_logs();
    ASSERT_EQ(1, std::ranges::count_if(logs, [](const auto& log) {
                return log->password_change_submission().has_quality();
              }));
    const auto it = std::find_if(logs.begin(), logs.end(), [](const auto& log) {
      return log->password_change_submission().has_quality();
    });
    // Verify the single log values.
    optimization_guide::proto::PasswordChangeQuality quality =
        it->get()->password_change_submission().quality();
    EXPECT_EQ(quality.final_model_status(), final_status);
    EXPECT_EQ(quality.verify_submission().status(), verify_submission_status);
    EXPECT_EQ(quality.open_form().status(), open_form_status);
    EXPECT_EQ(quality.submit_form().status(), submit_form_status);
  }

  void SetPrivacyNoticeAcceptedPref() {
    ON_CALL(*mock_optimization_guide_keyed_service(),
            ShouldFeatureBeCurrentlyEnabledForUser(
                optimization_guide::UserVisibleFeatureKey::
                    kPasswordChangeSubmission))
        .WillByDefault(Return(true));
  }

  TestModelQualityLogsUploaderService& logs_uploader() {
    return *static_cast<TestModelQualityLogsUploaderService*>(
        mock_optimization_guide_keyed_service()
            ->GetModelQualityLogsUploaderService());
  }

  MockAffiliationService* affiliation_service() {
    return static_cast<MockAffiliationService*>(
        AffiliationServiceFactory::GetForProfile(browser()->profile()));
  }

  MockOptimizationGuideKeyedService* mock_optimization_guide_keyed_service() {
    return static_cast<MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile()));
  }

  ChromePasswordChangeService* password_change_service() {
    return PasswordChangeServiceFactory::GetForProfile(browser()->profile());
  }

  void SetModelQualityLogsUploader() {
    MockOptimizationGuideKeyedService* optimization_service =
        mock_optimization_guide_keyed_service();
    auto logs_uploader = std::make_unique<TestModelQualityLogsUploaderService>(
        g_browser_process->local_state());
    auto logs_uploader_weak_ptr = logs_uploader->GetWeakPtr();
    optimization_service->SetModelQualityLogsUploaderServiceForTesting(
        std::move(logs_uploader));
  }

  void MockPasswordChangeOutcome(
      std::optional<PasswordChangeOutcome> outcome,
      std::optional<PasswordChangeErrorCase> error_case = std::nullopt) {
    optimization_guide::proto::PasswordChangeResponse response;
    response.mutable_outcome_data()->set_submission_outcome(outcome.value());
    if (error_case.has_value()) {
      response.mutable_outcome_data()->add_error_case(error_case.value());
    }

    MockOptimizationGuideKeyedService* optimization_service =
        mock_optimization_guide_keyed_service();
    auto logs_uploader = std::make_unique<TestModelQualityLogsUploaderService>(
        g_browser_process->local_state());
    auto logs_uploader_weak_ptr = logs_uploader->GetWeakPtr();
    optimization_service->SetModelQualityLogsUploaderServiceForTesting(
        std::move(logs_uploader));
    EXPECT_CALL(*optimization_service,
                ExecuteModel(optimization_guide::ModelBasedCapabilityKey::
                                 kPasswordChangeSubmission,
                             _, _, _))
        .WillOnce(DoAll(
            WithArg<1>([&](const google::protobuf::MessageLite& request) {
              auto& password_change_request = static_cast<
                  const optimization_guide::proto::PasswordChangeRequest&>(
                  request);
              ASSERT_TRUE(password_change_request.page_context()
                              .has_annotated_page_content());
            }),
            WithArg<3>(Invoke([response,
                               logs_uploader_weak_ptr](auto callback) {
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(
                      std::move(callback),
                      optimization_guide::OptimizationGuideModelExecutionResult(
                          optimization_guide::AnyWrapProto(response),
                          /*execution_info=*/nullptr),
                      std::make_unique<
                          optimization_guide::ModelQualityLogEntry>(
                          logs_uploader_weak_ptr)));
            }))));
  }

 private:
  base::CallbackListSubscription create_services_subscription_;
  base::WeakPtrFactory<PasswordChangeBrowserTest> weak_ptr_factory_{this};
};

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       PasswordChangeDoesNotStartUntilPrivacyNoticeAccepted) {
  base::HistogramTester histogram_tester;
  TabStripModel* tab_strip = browser()->tab_strip_model();
  // Assert that there is a single tab.
  ASSERT_EQ(tab_strip->count(), 1);
  ASSERT_FALSE(
      password_change_service()->GetPasswordChangeDelegate(WebContents()));

  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(Return(GURL(kChangePasswordURL)));

  password_change_service()->OfferPasswordChangeUi(main_url, u"test",
                                                   u"password", WebContents());
  auto* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());

  // Verify password change didn't start yet.
  EXPECT_FALSE(static_cast<PasswordChangeDelegateImpl*>(delegate)->executor());
  EXPECT_EQ(delegate->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForAgreement);

  // Privacy notice accepted.
  delegate->OnPrivacyNoticeAccepted();

  // Verify a new web_contents is created.
  auto* web_contents =
      static_cast<PasswordChangeDelegateImpl*>(delegate)->executor();
  EXPECT_TRUE(web_contents);
  // Verify a new web_contents is opened with a change pwd url.
  EXPECT_EQ(web_contents->GetURL(), GURL(kChangePasswordURL));
  EXPECT_EQ(delegate->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForChangePasswordForm);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.PasswordChange.LeakDetectionDialog.TimeSpent."
      "WithPrivacyNotice",
      1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       ChangePasswordFormIsFilledAutomatically) {
  SetPrivacyNoticeAcceptedPref();

  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields_no_submit.html")));

  password_change_service()->OfferPasswordChangeUi(main_url, u"test",
                                                   u"pa$$word", WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();

  content::WebContents* web_contents =
      static_cast<PasswordChangeDelegateImpl*>(delegate)->executor();
  // Start observing web_contents where password change happens.
  SetWebContents(web_contents);
  PasswordsNavigationObserver observer(web_contents);
  EXPECT_TRUE(observer.Wait());

  // Wait and verify the old password is filled correctly.
  WaitForElementValue("password", "pa$$word");

  // Verify there is a new password generated and it's filled into both fields.
  std::string new_password =
      GetElementValue(/*iframe_id=*/"null", "new_password_1");
  EXPECT_FALSE(new_password.empty());
  CheckElementValue("new_password_2", new_password);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, PasswordChangeStateUpdated) {
  base::HistogramTester histogram_tester;
  MockPasswordChangeDelegateObserver observer;

  SetPrivacyNoticeAcceptedPref();
  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields.html")));

  password_change_service()->OfferPasswordChangeUi(main_url, u"test",
                                                   u"pa$$word", WebContents());

  // Verify the delegate is created.
  base::WeakPtr<PasswordChangeDelegate> delegate =
      password_change_service()
          ->GetPasswordChangeDelegate(WebContents())
          ->AsWeakPtr();
  ASSERT_TRUE(delegate);

  // Verify delegate is waiting for change password form when password change
  // starts.
  delegate->AddObserver(&observer);
  delegate->StartPasswordChangeFlow();
  EXPECT_EQ(delegate->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForChangePasswordForm);

  // Verify observer is invoked when the state changes.
  EXPECT_CALL(observer,
              OnStateChanged(PasswordChangeDelegate::State::kChangingPassword));

  base::WeakPtr<content::WebContents> web_contents =
      static_cast<PasswordChangeDelegateImpl*>(delegate.get())
          ->executor()
          ->GetWeakPtr();
  // Start observing web_contents where password change happens.
  SetWebContents(web_contents.get());
  PasswordsNavigationObserver navigation_observer(web_contents.get());
  EXPECT_TRUE(navigation_observer.Wait());

  // Wait and verify the old password is filled correctly.
  WaitForElementValue("password", "pa$$word");
  EXPECT_EQ(delegate->GetCurrentState(),
            PasswordChangeDelegate::State::kChangingPassword);

  // Observe original web_contnets again to avoid dangling ptr.
  SetWebContents(browser()->tab_strip_model()->GetWebContentsAt(0));
  delegate->RemoveObserver(&observer);
  delegate->Stop();
  EXPECT_TRUE(base::test::RunUntil([&delegate]() {
    // Delegate's destructor is called async, so this is needed before checking
    // the metrics report.
    return delegate == nullptr;
  }));
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegate::State::kChangingPassword, 1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, GeneratedPasswordIsPreSaved) {
  SetPrivacyNoticeAcceptedPref();
  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields_no_submit.html")));

  password_change_service()->OfferPasswordChangeUi(main_url, u"test",
                                                   u"pa$$word", WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();

  // Start observing web_contents where password change happens.
  auto* delegate_impl = static_cast<PasswordChangeDelegateImpl*>(delegate);
  SetWebContents(delegate_impl->executor());
  PasswordsNavigationObserver observer(WebContents());
  EXPECT_TRUE(observer.Wait());
  WaitForElementValue("password", "pa$$word");

  // Verify generated password is pre-saved.
  WaitForPasswordStore();
  std::string generated_password =
      base::UTF16ToUTF8(delegate_impl->generated_password());
  EXPECT_EQ(generated_password,
            GetElementValue(/*iframe_id=*/"null", "new_password_1"));
  CheckThatCredentialsStored(
      /*username=*/"test", "pa$$word", generated_password);
}

// Verify that after password change is stopped, password change delegate is not
// returned.
IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, StopPasswordChange) {
  SetPrivacyNoticeAcceptedPref();

  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(Return(embedded_test_server()->GetURL("/password/done.html")));

  password_change_service()->OfferPasswordChangeUi(main_url, u"test",
                                                   u"pa$$word", WebContents());
  ASSERT_TRUE(
      password_change_service()->GetPasswordChangeDelegate(WebContents()));

  password_change_service()->GetPasswordChangeDelegate(WebContents())->Stop();
  EXPECT_FALSE(
      password_change_service()->GetPasswordChangeDelegate(WebContents()));
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, NewPasswordIsSaved) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetPrivacyNoticeAcceptedPref();
  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields.html")));

  password_change_service()->OfferPasswordChangeUi(main_url, u"test",
                                                   u"pa$$word", WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  MockPasswordChangeOutcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME);

  EXPECT_TRUE(base::test::RunUntil([delegate]() {
    return delegate->GetCurrentState() ==
           PasswordChangeDelegate::State::kPasswordSuccessfullyChanged;
  }));
  CheckThatCredentialsStored(
      /*username=*/"test",
      base::UTF16ToUTF8(static_cast<PasswordChangeDelegateImpl*>(delegate)
                            ->generated_password()),
      "pa$$word", password_manager::PasswordForm::Type::kChangeSubmission);

  base::WeakPtr<PasswordChangeDelegate> delegate_weak_ptr =
      delegate->AsWeakPtr();
  delegate_weak_ptr->Stop();
  EXPECT_TRUE(base::test::RunUntil([&delegate_weak_ptr]() {
    // Delegate's destructor is called async, so this is needed before checking
    // the metrics report.
    return delegate_weak_ptr == nullptr;
  }));
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegate::State::kPasswordSuccessfullyChanged, 1);
  histogram_tester.ExpectUniqueSample(kPasswordChangeSubmissionOutcomeHistogram,
                                      SubmissionOutcome::kSuccess, 1);
  histogram_tester.ExpectTotalCount("PasswordManager.PasswordChangeTimeOverall",
                                    1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ChangePasswordFormDetected", true, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChange.UserHasPasswordSavedOnAPCLaunch", false,
      1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.ChangePasswordFormDetectionTime", 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.ChangingPasswordToast.TimeSpent", 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.PasswordChange.LeakDetectionDialog.TimeSpent."
      "WithoutPrivacyNotice",
      1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetMetricEntry(
          test_ukm_recorder,
          ukm::builders::PasswordManager_PasswordChangeSubmissionOutcome::
              kEntryName),
      ukm::builders::PasswordManager_PasswordChangeSubmissionOutcome::
          kPasswordChangeSubmissionOutcomeName,
      static_cast<int>(SubmissionOutcome::kSuccess));
  VerifyUniqueQualityLog(
      /*open_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_STEP_SKIPPED,
      /* submit_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_STEP_SKIPPED,
      /*verify_submission_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      /*final_status=*/
      FinalModelStatus::FINAL_MODEL_STATUS_SUCCESS);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, OldPasswordIsUpdated) {
  SetPrivacyNoticeAcceptedPref();
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  const GURL url = WebContents()->GetLastCommittedURL();
  password_manager::PasswordForm form;
  form.signon_realm = url.GetWithEmptyPath().spec();
  form.url = url;
  form.username_value = u"test";
  form.password_value = u"pa$$word";
  password_store->AddLogin(form);
  WaitForPasswordStore();

  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(url))
      .WillOnce(Return(embedded_test_server()->GetURL(
          kMainHost, "/password/update_form_empty_fields.html")));

  password_change_service()->OfferPasswordChangeUi(url, u"test", u"pa$$word",
                                                   WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  MockPasswordChangeOutcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME);

  EXPECT_TRUE(base::test::RunUntil([delegate]() {
    return delegate->GetCurrentState() ==
           PasswordChangeDelegate::State::kPasswordSuccessfullyChanged;
  }));

  // Verify saved password is updated.
  WaitForPasswordStore();
  CheckThatCredentialsStored(
      base::UTF16ToUTF8(form.username_value),
      base::UTF16ToUTF8(static_cast<PasswordChangeDelegateImpl*>(delegate)
                            ->generated_password()),
      base::UTF16ToUTF8(form.password_value),
      password_manager::PasswordForm::Type::kChangeSubmission);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       PasswordChangeSubmissionFailedEmptyResponse) {
  base::HistogramTester histograms;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetPrivacyNoticeAcceptedPref();
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  const GURL origin = embedded_test_server()->GetURL(kMainHost, "/");
  password_manager::PasswordForm form;
  form.signon_realm = origin.spec();
  form.url = origin;
  form.username_value = u"test";
  form.password_value = u"pa$$word";
  password_store->AddLogin(form);
  WaitForPasswordStore();

  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(origin))
      .WillOnce(Return(embedded_test_server()->GetURL(
          kMainHost, "/password/update_form_empty_fields.html")));

  password_change_service()->OfferPasswordChangeUi(origin, u"test", u"pa$$word",
                                                   WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  EXPECT_CALL(
      *mock_optimization_guide_keyed_service(),
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::
                       kPasswordChangeSubmission,
                   _, _,
                   An<optimization_guide::
                          OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          optimization_guide::OptimizationGuideModelExecutionResult(
              base::unexpected(
                  optimization_guide::OptimizationGuideModelExecutionError::
                      FromModelExecutionError(
                          OptimizationGuideModelExecutionError::
                              kGenericFailure)),
              /*execution_info=*/nullptr),
          /*log_entry=*/nullptr));

  EXPECT_TRUE(base::test::RunUntil([delegate]() {
    return delegate->GetCurrentState() ==
           PasswordChangeDelegate::State::kPasswordChangeFailed;
  }));

  WaitForPasswordStore();
  histograms.ExpectUniqueSample(kPasswordChangeSubmissionOutcomeHistogram,
                                SubmissionOutcome::kNoResponse, 1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetMetricEntry(
          test_ukm_recorder,
          ukm::builders::PasswordManager_PasswordChangeSubmissionOutcome::
              kEntryName),
      ukm::builders::PasswordManager_PasswordChangeSubmissionOutcome::
          kPasswordChangeSubmissionOutcomeName,
      static_cast<int>(SubmissionOutcome::kNoResponse));
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       PasswordChangeSubmissionFailed) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetPrivacyNoticeAcceptedPref();
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  const GURL origin = embedded_test_server()->GetURL(kMainHost, "/");
  password_manager::PasswordForm form;
  form.signon_realm = origin.spec();
  form.url = origin;
  form.username_value = u"test";
  form.password_value = u"pa$$word";
  password_store->AddLogin(form);
  WaitForPasswordStore();

  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(origin))
      .WillOnce(Return(embedded_test_server()->GetURL(
          kMainHost, "/password/update_form_empty_fields.html")));

  password_change_service()->OfferPasswordChangeUi(origin, u"test", u"pa$$word",
                                                   WebContents());
  password_change_service()
      ->GetPasswordChangeDelegate(WebContents())
      ->StartPasswordChangeFlow();

  MockPasswordChangeOutcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_UNSUCCESSFUL_OUTCOME,
      PasswordChangeErrorCase::
          PasswordChangeSubmissionData_PasswordChangeErrorCase_PAGE_ERROR);

  base::WeakPtr<PasswordChangeDelegate> delegate =
      password_change_service()
          ->GetPasswordChangeDelegate(WebContents())
          ->AsWeakPtr();

  EXPECT_TRUE(base::test::RunUntil([delegate]() {
    return delegate->GetCurrentState() ==
           PasswordChangeDelegate::State::kPasswordChangeFailed;
  }));

  WaitForPasswordStore();
  CheckThatCredentialsStored(
      /*username=*/"test", "pa$$word",
      base::UTF16ToUTF8(
          static_cast<PasswordChangeDelegateImpl*>(
              password_change_service()->GetPasswordChangeDelegate(
                  WebContents()))
              ->generated_password()));

  delegate->Stop();
  EXPECT_TRUE(base::test::RunUntil([&delegate]() {
    // Delegate's destructor is called async, so this is needed before checking
    // the metrics report.
    return delegate == nullptr;
  }));
  histogram_tester.ExpectUniqueSample(
      PasswordChangeDelegateImpl::kFinalPasswordChangeStatusHistogram,
      PasswordChangeDelegate::State::kPasswordChangeFailed, 1);
  histogram_tester.ExpectUniqueSample(
      kPasswordChangeSubmissionOutcomeHistogram,
      PasswordChangeSubmissionVerifier::SubmissionOutcome::kPageError, 1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetMetricEntry(
          test_ukm_recorder,
          ukm::builders::PasswordManager_PasswordChangeSubmissionOutcome::
              kEntryName),
      ukm::builders::PasswordManager_PasswordChangeSubmissionOutcome::
          kPasswordChangeSubmissionOutcomeName,
      static_cast<int>(SubmissionOutcome::kPageError));
  VerifyUniqueQualityLog(
      /*open_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_STEP_SKIPPED,
      /* submit_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_STEP_SKIPPED,
      /*verify_submission_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FAILURE_STATUS,
      /*final_status=*/
      FinalModelStatus::FINAL_MODEL_STATUS_FAILURE);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, OpenTabWithPasswordChange) {
  SetPrivacyNoticeAcceptedPref();

  const GURL main_url = WebContents()->GetLastCommittedURL();
  const GURL change_password_url =
      embedded_test_server()->GetURL("/password/update_form_empty_fields.html");

  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(Return(change_password_url));
  password_change_service()->OfferPasswordChangeUi(main_url, u"test",
                                                   u"pa$$word", WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();

  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(tab_strip->count(), 1);

  EXPECT_EQ(tab_strip->active_index(), 0);
  delegate->OpenPasswordChangeTab();

  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_EQ(tab_strip->active_index(), 1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       LeakCheckDialogWithPrivacyNoticeDisplayed) {
  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields.html")));

  password_change_service()->OfferPasswordChangeUi(main_url, u"test",
                                                   u"password", WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  EXPECT_EQ(delegate->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForAgreement);
  EXPECT_TRUE(static_cast<PasswordChangeDelegateImpl*>(delegate)
                  ->ui_controller()
                  ->dialog_widget()
                  ->IsVisible());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, FailureDialogDisplayed) {
  SetPrivacyNoticeAcceptedPref();
  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields.html")));

  password_change_service()->OfferPasswordChangeUi(main_url, u"test",
                                                   u"pa$$word", WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  MockPasswordChangeOutcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_UNSUCCESSFUL_OUTCOME);

  ASSERT_TRUE(base::test::RunUntil([delegate]() {
    return delegate->GetCurrentState() ==
           PasswordChangeDelegate::State::kPasswordChangeFailed;
  }));

  EXPECT_TRUE(static_cast<PasswordChangeDelegateImpl*>(delegate)
                  ->ui_controller()
                  ->dialog_widget()
                  ->IsVisible());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       LeakCheckDialogWithoutPrivacyNoticeDisplayed) {
  SetPrivacyNoticeAcceptedPref();
  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields.html")));

  password_change_service()->OfferPasswordChangeUi(main_url, u"test",
                                                   u"pa$$word", WebContents());

  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  EXPECT_EQ(delegate->GetCurrentState(),
            PasswordChangeDelegate::State::kOfferingPasswordChange);
  EXPECT_TRUE(static_cast<PasswordChangeDelegateImpl*>(delegate)
                  ->ui_controller()
                  ->dialog_widget()
                  ->IsVisible());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, OTPDetectionHaltsTheFlow) {
  SetPrivacyNoticeAcceptedPref();
  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(Return(embedded_test_server()->GetURL("/password/done.html")));

  password_change_service()->OfferPasswordChangeUi(main_url, u"test",
                                                   u"pa$$word", WebContents());
  SetModelQualityLogsUploader();
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  ASSERT_TRUE(delegate);
  EXPECT_EQ(delegate->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForChangePasswordForm);

  auto* delegate_impl = static_cast<PasswordChangeDelegateImpl*>(delegate);
  delegate->OnOtpFieldDetected(delegate_impl->executor());

  EXPECT_EQ(delegate->GetCurrentState(),
            PasswordChangeDelegate::State::kOtpDetected);
  EXPECT_TRUE(delegate_impl->ui_controller()->dialog_widget()->IsVisible());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  delegate_impl->ui_controller()->CallOnDialogCanceledForTesting();

  // The quality log is uploaded in the destructor.
  base::WeakPtr<PasswordChangeDelegate> delegate_weak_ptr =
      delegate->AsWeakPtr();
  EXPECT_TRUE(base::test::RunUntil(
      [&delegate_weak_ptr]() { return !delegate_weak_ptr; }));

  VerifyUniqueQualityLog(
      /*open_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_OTP_DETECTED,
      /* submit_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*verify_submission_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*final_status=*/
      FinalModelStatus::FINAL_MODEL_STATUS_UNSPECIFIED);
}

// Verify that clicking cancel on the toast, stops the flow
IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, CancelFromToast) {
  SetPrivacyNoticeAcceptedPref();

  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(
          embedded_test_server()->GetURL("/password/done.html")));

  password_change_service()->OfferPasswordChangeUi(main_url, u"test",
                                                   u"pa$$word", WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  EXPECT_TRUE(delegate);
  delegate->StartPasswordChangeFlow();
  PasswordChangeUIController* ui_controller =
      static_cast<PasswordChangeDelegateImpl*>(delegate)->ui_controller();
  EXPECT_TRUE(ui_controller->toast_view());
  // Verify action button is present and visible.
  EXPECT_TRUE(ui_controller->toast_view()->action_button());
  EXPECT_TRUE(ui_controller->toast_view()->action_button()->GetVisible());

  SetModelQualityLogsUploader();

  // Click action button, this should cancel the flow.
  views::test::ButtonTestApi clicker(
      ui_controller->toast_view()->action_button());
  clicker.NotifyClick(ui::test::TestEvent());

  EXPECT_EQ(PasswordChangeDelegate::State::kCanceled,
            delegate->GetCurrentState());

  // Verify toast is displayed.
  EXPECT_TRUE(ui_controller->toast_view());
  // Verify the toast has no action button, meaning it's just a
  // confirmation.
  EXPECT_FALSE(ui_controller->toast_view()->action_button()->GetVisible());

  // The quality log is uploaded in the destructor.
  base::WeakPtr<PasswordChangeDelegate> delegate_weak_ptr =
      delegate->AsWeakPtr();
  EXPECT_TRUE(base::test::RunUntil(
      [&delegate_weak_ptr]() { return !delegate_weak_ptr; }));

  VerifyUniqueQualityLog(
      /*open_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FLOW_INTERRUPTED,
      /* submit_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*verify_submission_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*final_status=*/
      FinalModelStatus::FINAL_MODEL_STATUS_UNSPECIFIED);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       ViewDetailsFromToastAfterPageNavigation) {
  SetPrivacyNoticeAcceptedPref();
  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(embedded_test_server()->GetURL(
          kMainHost, "/password/update_form_empty_fields.html")));
  EXPECT_CALL(*affiliation_service(), GetPSLExtensions)
      .WillRepeatedly(RunOnceCallbackRepeatedly<0>(std::vector<std::string>()));
  EXPECT_CALL(*affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce(RunOnceCallback<1>(affiliations::AffiliatedFacets(), true));
  password_change_service()->OfferPasswordChangeUi(main_url, u"test",
                                                   u"pa$$word", WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();

  MockPasswordChangeOutcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME);

  EXPECT_TRUE(base::test::RunUntil([delegate]() {
    return delegate->GetCurrentState() ==
           PasswordChangeDelegate::State::kPasswordSuccessfullyChanged;
  }));

  EXPECT_TRUE(delegate);

  // Navigate to some other website before pressing the button.
  GURL url = embedded_test_server()->GetURL(
      kDifferentHost, "/password/update_form_empty_fields.html");
  ASSERT_TRUE(content::NavigateToURL(WebContents(), url));
  ASSERT_TRUE(content::WaitForLoadStop(WebContents()));

  auto* toast = static_cast<PasswordChangeDelegateImpl*>(delegate)
                    ->ui_controller()
                    ->toast_view();
  EXPECT_TRUE(toast);
  // Verify action button is present and visible.
  EXPECT_TRUE(toast->action_button());
  EXPECT_TRUE(toast->action_button()->GetVisible());

  // Click action button, this should open Password Management.
  views::test::ButtonTestApi clicker(toast->action_button());
  delegate = nullptr;
  toast = nullptr;

  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(0, tab_strip->active_index());

  clicker.NotifyClick(ui::test::TestEvent());

  EXPECT_EQ(2, tab_strip->count());
  EXPECT_EQ(1, tab_strip->active_index());

  // Verify Password Management UI is opened.
  EXPECT_EQ(url::Origin::Create(GURL("chrome://password-manager/")),
            url::Origin::Create(tab_strip->GetActiveWebContents()->GetURL()));
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, ViewPasswordBubbleFromToast) {
  SetPrivacyNoticeAcceptedPref();
  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(embedded_test_server()->GetURL(
          kMainHost, "/password/update_form_empty_fields.html")));
  EXPECT_CALL(*affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));
  EXPECT_CALL(*affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce(RunOnceCallback<1>(affiliations::AffiliatedFacets(), true));
  password_change_service()->OfferPasswordChangeUi(main_url, u"test",
                                                   u"pa$$word", WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();

  MockPasswordChangeOutcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME);

  EXPECT_TRUE(base::test::RunUntil([delegate]() {
    return delegate->GetCurrentState() ==
           PasswordChangeDelegate::State::kPasswordSuccessfullyChanged;
  }));
  EXPECT_TRUE(delegate);

  BubbleObserver prompt_observer(WebContents());

  PasswordChangeToast* toast =
      static_cast<PasswordChangeDelegateImpl*>(delegate)
          ->ui_controller()
          ->toast_view();
  EXPECT_TRUE(toast);
  // Verify action button is present and visible.
  EXPECT_TRUE(toast->action_button());
  EXPECT_TRUE(toast->action_button()->GetVisible());

  // Click action button, this should open the password bubble.
  views::test::ButtonTestApi clicker(toast->action_button());
  delegate = nullptr;
  toast = nullptr;

  clicker.NotifyClick(ui::test::TestEvent());
  EXPECT_TRUE(prompt_observer.IsBubbleDisplayedAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       ToastHiddenWhenDialogDisplayed) {
  SetPrivacyNoticeAcceptedPref();
  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields.html")));

  password_change_service()->OfferPasswordChangeUi(main_url, u"test",
                                                   u"pa$$word", WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  MockPasswordChangeOutcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_UNSUCCESSFUL_OUTCOME);

  ASSERT_TRUE(base::test::RunUntil([delegate]() {
    return delegate->GetCurrentState() ==
           PasswordChangeDelegate::State::kPasswordChangeFailed;
  }));

  PasswordChangeUIController* ui_controller =
      static_cast<PasswordChangeDelegateImpl*>(delegate)->ui_controller();
  EXPECT_TRUE(ui_controller->dialog_widget()->IsVisible());
  EXPECT_FALSE(ui_controller->toast_view());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       LogsUserHasPasswordSavedOnAPCLaunch) {
  password_manager::PasswordFormManager::
      set_wait_for_server_predictions_for_filling(false);
  const GURL url =
      embedded_test_server()->GetURL(kMainHost, "/password/password_form.html");
  password_manager::PasswordForm form;
  form.signon_realm = url.GetWithEmptyPath().spec();
  form.url = url;
  form.username_value = u"test";
  form.password_value = u"pa$$word";
  ProfilePasswordStoreFactory::GetForProfile(browser()->profile(),
                                             ServiceAccessType::IMPLICIT_ACCESS)
      ->AddLogin(form);
  WaitForPasswordStore();

  SetPrivacyNoticeAcceptedPref();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(url))
      .WillOnce(Return(embedded_test_server()->GetURL(
          kMainHost, "/password/update_form_empty_fields.html")));

  // Navigate to the page again to trigger autofill.
  PasswordsNavigationObserver observer(WebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(observer.Wait());

  base::HistogramTester histogram_tester;
  password_change_service()->OfferPasswordChangeUi(url, u"test", u"pa$$word",
                                                   WebContents());
  password_change_service()
      ->GetPasswordChangeDelegate(WebContents())
      ->StartPasswordChangeFlow();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChange.UserHasPasswordSavedOnAPCLaunch", true,
      1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       CrossOriginNavigationDetected) {
  base::HistogramTester histogram_tester;

  SetPrivacyNoticeAcceptedPref();
  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(Return(https_test_server().GetURL(
          kMainHost, "/password/update_form_empty_fields.html")));
  EXPECT_CALL(*affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));
  EXPECT_CALL(*affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce(RunOnceCallback<1>(affiliations::AffiliatedFacets(), true));

  password_change_service()->OfferPasswordChangeUi(main_url, u"test",
                                                   u"pa$$word", WebContents());

  // Verify the delegate is created.
  base::WeakPtr<PasswordChangeDelegate> delegate =
      password_change_service()
          ->GetPasswordChangeDelegate(WebContents())
          ->AsWeakPtr();
  ASSERT_TRUE(delegate);

  // Verify delegate is waiting for change password form when password change
  // starts.
  delegate->StartPasswordChangeFlow();
  EXPECT_EQ(delegate->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForChangePasswordForm);

  EXPECT_TRUE(base::test::RunUntil([&delegate]() {
    return delegate->GetCurrentState() ==
           PasswordChangeDelegate::State::kChangingPassword;
  }));

  GURL url = https_test_server().GetURL(kDifferentHost,
                                        "/password/simple_password.html");
  (void)content::NavigateToURL(
      static_cast<PasswordChangeDelegateImpl*>(delegate.get())->executor(),
      url);

  EXPECT_TRUE(base::test::RunUntil([&delegate]() {
    return delegate->GetCurrentState() ==
           PasswordChangeDelegate::State::kPasswordChangeFailed;
  }));
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       OnTabCloseLogsUnexpectedFailure) {
  SetPrivacyNoticeAcceptedPref();
  content::WebContents* original_apc_flow_web_contents = WebContents();
  int original_apc_flow_tab_index =
      browser()->tab_strip_model()->GetIndexOfWebContents(
          original_apc_flow_web_contents);

  const GURL main_url = original_apc_flow_web_contents->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(Return(embedded_test_server()->GetURL("/password/done.html")));

  password_change_service()->OfferPasswordChangeUi(
      main_url, u"test", u"pa$$word", original_apc_flow_web_contents);
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(
          original_apc_flow_web_contents);
  delegate->StartPasswordChangeFlow();
  SetModelQualityLogsUploader();

  // Set the 'open form' quality log, so that when there is an interruption
  // the next step is set as FLOW_INTERRUPTED.
  static_cast<PasswordChangeDelegateImpl*>(delegate)
      ->logs_uploader()
      ->SetOpenFormQualityStatus(
          QualityStatus::
              PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);

  // Add an extra tab to prevent a dangling pointer when closing
  // the tab where the main flow is active.
  std::unique_ptr<content::WebContents> extra_web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  content::WebContents* new_active_web_contents = extra_web_contents.get();
  browser()->tab_strip_model()->AppendWebContents(std::move(extra_web_contents),
                                                  true /* foreground */);
  SetWebContents(new_active_web_contents);

  // Close the tab where the flow is active to trigger a
  // flow interruption log.
  browser()->tab_strip_model()->CloseWebContentsAt(
      original_apc_flow_tab_index, TabCloseTypes::CLOSE_USER_GESTURE);

  base::WeakPtr<PasswordChangeDelegate> delegate_weak_ptr =
      delegate->AsWeakPtr();
  EXPECT_TRUE(base::test::RunUntil(
      [&delegate_weak_ptr]() { return !delegate_weak_ptr; }));

  VerifyUniqueQualityLog(
      /*open_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      /* submit_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FLOW_INTERRUPTED,
      /*verify_submission_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*final_status=*/
      FinalModelStatus::FINAL_MODEL_STATUS_UNSPECIFIED);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       FlowInterruptedAfterOpenFormStep) {
  SetPrivacyNoticeAcceptedPref();
  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(Return(embedded_test_server()->GetURL("/password/done.html")));

  password_change_service()->OfferPasswordChangeUi(main_url, u"test",
                                                   u"pa$$word", WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  SetModelQualityLogsUploader();

  // Set the 'open form' quality log, so that when there is an interruption
  // the next step is set as FLOW_INTERRUPTED.
  static_cast<PasswordChangeDelegateImpl*>(delegate)
      ->logs_uploader()
      ->SetOpenFormQualityStatus(
          QualityStatus::
              PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);

  PasswordChangeUIController* ui_controller =
      static_cast<PasswordChangeDelegateImpl*>(delegate)->ui_controller();
  EXPECT_TRUE(base::test::RunUntil(
      [ui_controller]() { return ui_controller->toast_view(); }));
  base::WeakPtr<PasswordChangeDelegate> delegate_weak_ptr =
      delegate->AsWeakPtr();
  // Simulate clicking the "cancel" button on the UI toast.
  views::test::ButtonTestApi clicker(
      ui_controller->toast_view()->action_button());
  clicker.NotifyClick(ui::test::TestEvent());
  // Verify that the flow's state is "canceled".
  EXPECT_EQ(PasswordChangeDelegate::State::kCanceled,
            delegate->GetCurrentState());
  EXPECT_TRUE(base::test::RunUntil(
      [&delegate_weak_ptr]() { return !delegate_weak_ptr; }));

  VerifyUniqueQualityLog(
      /*open_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      /* submit_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FLOW_INTERRUPTED,
      /*verify_submission_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*final_status=*/
      FinalModelStatus::FINAL_MODEL_STATUS_UNSPECIFIED);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       FlowInterruptedAfterSubmitFormStep) {
  SetPrivacyNoticeAcceptedPref();
  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(Return(embedded_test_server()->GetURL("/password/done.html")));

  password_change_service()->OfferPasswordChangeUi(main_url, u"test",
                                                   u"pa$$word", WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  SetModelQualityLogsUploader();

  // Set the 'submit form' quality log, so that when there is an interruption
  // the next step is set as FLOW_INTERRUPTED.
  static_cast<PasswordChangeDelegateImpl*>(delegate)
      ->logs_uploader()
      ->SetSubmitFormQualityStatus(
          QualityStatus::
              PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);

  PasswordChangeUIController* ui_controller =
      static_cast<PasswordChangeDelegateImpl*>(delegate)->ui_controller();
  EXPECT_TRUE(base::test::RunUntil(
      [ui_controller]() { return ui_controller->toast_view(); }));
  base::WeakPtr<PasswordChangeDelegate> delegate_weak_ptr =
      delegate->AsWeakPtr();
  // Simulate clicking the "cancel" button on the UI toast.
  views::test::ButtonTestApi clicker(
      ui_controller->toast_view()->action_button());
  clicker.NotifyClick(ui::test::TestEvent());
  // Verify that the flow's state is "canceled".
  EXPECT_EQ(PasswordChangeDelegate::State::kCanceled,
            delegate->GetCurrentState());
  EXPECT_TRUE(base::test::RunUntil(
      [&delegate_weak_ptr]() { return !delegate_weak_ptr; }));

  VerifyUniqueQualityLog(
      /*open_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /* submit_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      /*verify_submission_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FLOW_INTERRUPTED,
      /*final_status=*/
      FinalModelStatus::FINAL_MODEL_STATUS_UNSPECIFIED);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       OtpDetectedfterSubmitFormStep) {
  SetPrivacyNoticeAcceptedPref();
  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(Return(embedded_test_server()->GetURL("/password/done.html")));

  password_change_service()->OfferPasswordChangeUi(main_url, u"test",
                                                   u"pa$$word", WebContents());
  SetModelQualityLogsUploader();
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();

  // Set the 'submit form' quality log, so that when there is an interruption
  // the next step is set as FLOW_INTERRUPTED.
  static_cast<PasswordChangeDelegateImpl*>(delegate)
      ->logs_uploader()
      ->SetSubmitFormQualityStatus(
          QualityStatus::
              PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
  base::WeakPtr<PasswordChangeDelegate> delegate_weak_ptr =
      delegate->AsWeakPtr();

  auto* delegate_impl = static_cast<PasswordChangeDelegateImpl*>(delegate);
  delegate->OnOtpFieldDetected(delegate_impl->executor());
  EXPECT_EQ(delegate->GetCurrentState(),
            PasswordChangeDelegate::State::kOtpDetected);
  delegate_impl->ui_controller()->CallOnDialogCanceledForTesting();
  EXPECT_TRUE(base::test::RunUntil(
      [&delegate_weak_ptr]() { return !delegate_weak_ptr; }));

  VerifyUniqueQualityLog(
      /*open_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /* submit_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      /*verify_submission_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_OTP_DETECTED,
      /*final_status=*/
      FinalModelStatus::FINAL_MODEL_STATUS_UNSPECIFIED);
}
