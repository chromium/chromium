// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_change_service.h"
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
#include "chrome/browser/ui/passwords/bubble_controllers/password_change/password_change_info_bubble_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
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

using affiliations::AffiliationService;
using affiliations::MockAffiliationService;
using PasswordChangeOutcome = ::optimization_guide::proto::
    PasswordChangeSubmissionData_PasswordChangeOutcome;
using PasswordChangeErrorCase = ::optimization_guide::proto::
    PasswordChangeSubmissionData_PasswordChangeErrorCase;
using OptimizationGuideModelExecutionError = optimization_guide::
    OptimizationGuideModelExecutionError::ModelExecutionError;
using ::testing::_;
using ::testing::An;
using ::testing::Contains;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::WithArg;
using SubmissionOutcome = PasswordChangeSubmissionVerifier::SubmissionOutcome;
using QualityStatus = optimization_guide::proto::
    PasswordChangeQuality_StepQuality_SubmissionStatus;
using SubmissionOutcome = PasswordChangeSubmissionVerifier::SubmissionOutcome;
using optimization_guide::TestModelQualityLogsUploaderService;
using FinalModelStatus = optimization_guide::proto::FinalModelStatus;

const char kPasswordChangeSubmissionOutcomeHistogram[] =
    "PasswordManager.PasswordChangeSubmissionOutcome";
const char kMainHost[] = "example.com";
const char kChangePasswordURL[] = "https://example.com/password/";

namespace {

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
  return std::make_unique<testing::NiceMock<MockAffiliationService>>();
}

std::unique_ptr<KeyedService> CreateOptimizationService(
    content::BrowserContext* context) {
  return std::make_unique<
      testing::NiceMock<MockOptimizationGuideKeyedService>>();
}

// Verifies that |test_ukm_recorder| recorder has a single entry called |entry|
// and returns it.
const ukm::mojom::UkmEntry* GetMetricEntry(
    const ukm::TestUkmRecorder& test_ukm_recorder,
    std::string_view entry) {
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>
      ukm_entries = test_ukm_recorder.GetEntriesByName(entry);
  EXPECT_EQ(1u, ukm_entries.size());
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

  void VerifyUniqueQualityLog(FinalModelStatus final_status,
                              QualityStatus quality_status) {
    const std::vector<
        std::unique_ptr<optimization_guide::proto::LogAiDataRequest>>& logs =
        logs_uploader().uploaded_logs();
    ASSERT_EQ(1u, logs.size());
    EXPECT_EQ(logs[0]
                  ->mutable_password_change_submission()
                  ->mutable_quality()
                  ->final_model_status(),
              final_status);
    EXPECT_EQ(logs[0]
                  ->mutable_password_change_submission()
                  ->mutable_quality()
                  ->verify_submission()
                  .status(),
              quality_status);
  }

  void SetPrivacyNoticeAcceptedPref() {
    ON_CALL(*mock_optimization_guide_keyed_service(),
            ShouldFeatureBeCurrentlyEnabledForUser(
                optimization_guide::UserVisibleFeatureKey::
                    kPasswordChangeSubmission))
        .WillByDefault(testing::Return(true));
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
              ASSERT_TRUE(
                  password_change_request.page_context().has_ax_tree_data());
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

  void CheckPasswordsSavedOnFailure(const std::string& username,
                                    const std::string& new_password) {
    scoped_refptr<password_manager::TestPasswordStore> password_store =
        static_cast<password_manager::TestPasswordStore*>(
            ProfilePasswordStoreFactory::GetForProfile(
                browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
                .get());
    const std::vector<password_manager::PasswordForm>& passwords_vector =
        password_store->stored_passwords().begin()->second;
    // Check if |username| + |new password| is stored
    bool found_username_with_new_password = false;
    // Check if |empty username| + |new password| is stored
    bool found_empty_username_with_new_password = false;

    for (const auto& form : passwords_vector) {
      if (form.username_value == base::ASCIIToUTF16(username) &&
          form.password_value == base::ASCIIToUTF16(new_password)) {
        found_username_with_new_password = true;
      } else if (form.username_value.empty() &&
                 form.password_value == base::ASCIIToUTF16(new_password)) {
        found_empty_username_with_new_password = true;
      }
    }

    EXPECT_FALSE(found_username_with_new_password);
    EXPECT_TRUE(found_empty_username_with_new_password);
  }

  void StartPasswordChange(const GURL& url,
                           const std::u16string& username,
                           const std::u16string& password,
                           content::WebContents* web_contents) {
    password_change_service()->OfferPasswordChangeUi(url, username, password,
                                                     web_contents);
    // Close the leak detection bubble and simulate that it was accepted.
    PasswordBubbleViewBase::CloseCurrentBubble();
    password_change_service()
        ->GetPasswordChangeDelegate(web_contents)
        ->StartPasswordChangeFlow();
  }

 private:
  base::CallbackListSubscription create_services_subscription_;
  base::WeakPtrFactory<PasswordChangeBrowserTest> weak_ptr_factory_{this};
};

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       PasswordChangeDoesNotStartUntilPrivacyNoticeAccepted) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  // Assert that there is a single tab.
  ASSERT_EQ(1, tab_strip->count());
  ASSERT_FALSE(
      password_change_service()->GetPasswordChangeDelegate(WebContents()));

  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(GURL(kChangePasswordURL)));

  StartPasswordChange(main_url, u"test", u"password", WebContents());

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
  EXPECT_EQ(GURL(kChangePasswordURL), web_contents->GetURL());
  EXPECT_EQ(delegate->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForChangePasswordForm);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       ChangePasswordFormIsFilledAutomatically) {
  SetPrivacyNoticeAcceptedPref();

  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields_no_submit.html")));

  StartPasswordChange(main_url, u"test", u"pa$$word", WebContents());

  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());

  auto* web_contents =
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
      .WillOnce(testing::Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields.html")));

  StartPasswordChange(main_url, u"test", u"pa$$word", WebContents());

  // Verify the delegate is created and it's currently waiting for change
  // password form.
  base::WeakPtr<PasswordChangeDelegate> delegate =
      password_change_service()
          ->GetPasswordChangeDelegate(WebContents())
          ->AsWeakPtr();
  ASSERT_TRUE(delegate);
  delegate->AddObserver(&observer);
  EXPECT_EQ(PasswordChangeDelegate::State::kWaitingForChangePasswordForm,
            delegate->GetCurrentState());

  // Verify observer is invoked when the state changes.
  EXPECT_CALL(observer,
              OnStateChanged(PasswordChangeDelegate::State::kChangingPassword));

  auto web_contents = static_cast<PasswordChangeDelegateImpl*>(delegate.get())
                          ->executor()
                          ->GetWeakPtr();
  // Start observing web_contents where password change happens.
  SetWebContents(web_contents.get());
  PasswordsNavigationObserver navigation_observer(web_contents.get());
  EXPECT_TRUE(navigation_observer.Wait());

  // Wait and verify the old password is filled correctly.
  WaitForElementValue("password", "pa$$word");
  EXPECT_EQ(PasswordChangeDelegate::State::kChangingPassword,
            delegate->GetCurrentState());

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
      .WillOnce(testing::Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields_no_submit.html")));

  StartPasswordChange(main_url, u"test", u"pa$$word", WebContents());

  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());

  // Start observing web_contents where password change happens.
  SetWebContents(
      static_cast<PasswordChangeDelegateImpl*>(delegate)->executor());
  PasswordsNavigationObserver observer(WebContents());
  EXPECT_TRUE(observer.Wait());
  WaitForElementValue("password", "pa$$word");

  // Verify generated password is pre-saved.
  WaitForPasswordStore();
  auto generated_password = delegate->GetGeneratedPassword();
  EXPECT_EQ(base::UTF16ToUTF8(generated_password),
            GetElementValue(/*iframe_id=*/"null", "new_password_1"));
  CheckThatCredentialsStored(
      /*username=*/"", base::UTF16ToUTF8(generated_password));
}

// Verify that after password change is stopped, password change delegate is not
// returned.
IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, StopPasswordChange) {
  SetPrivacyNoticeAcceptedPref();

  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(
          embedded_test_server()->GetURL("/password/done.html")));

  StartPasswordChange(main_url, u"test", u"pa$$word", WebContents());

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
      .WillOnce(testing::Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields.html")));

  StartPasswordChange(main_url, u"test", u"pa$$word", WebContents());
  MockPasswordChangeOutcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME);

  base::WeakPtr<PasswordChangeDelegate> delegate =
      password_change_service()
          ->GetPasswordChangeDelegate(WebContents())
          ->AsWeakPtr();
  EXPECT_TRUE(base::test::RunUntil([delegate]() {
    return delegate->GetCurrentState() ==
           PasswordChangeDelegate::State::kPasswordSuccessfullyChanged;
  }));
  CheckThatCredentialsStored(
      /*username=*/"test", base::UTF16ToUTF8(delegate->GetGeneratedPassword()),
      password_manager::PasswordForm::Type::kChangeSubmission);

  delegate->Stop();
  EXPECT_TRUE(base::test::RunUntil([&delegate]() {
    // Delegate's destructor is called async, so this is needed before checking
    // the metrics report.
    return delegate == nullptr;
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
  histogram_tester.ExpectTotalCount(
      "PasswordManager.ChangePasswordFormDetectionTime", 1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetMetricEntry(
          test_ukm_recorder,
          ukm::builders::PasswordManager_PasswordChangeSubmissionOutcome::
              kEntryName),
      ukm::builders::PasswordManager_PasswordChangeSubmissionOutcome::
          kPasswordChangeSubmissionOutcomeName,
      static_cast<int>(SubmissionOutcome::kSuccess));
  VerifyUniqueQualityLog(
      FinalModelStatus::FINAL_MODEL_STATUS_SUCCESS,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, OldPasswordIsUpdated) {
  base::HistogramTester histograms;
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
      .WillOnce(testing::Return(embedded_test_server()->GetURL(
          kMainHost, "/password/update_form_empty_fields.html")));

  StartPasswordChange(url, form.username_value, form.password_value,
                      WebContents());
  MockPasswordChangeOutcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME);

  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  EXPECT_TRUE(base::test::RunUntil([delegate]() {
    return delegate->GetCurrentState() ==
           PasswordChangeDelegate::State::kPasswordSuccessfullyChanged;
  }));

  // Verify saved password is updated.
  WaitForPasswordStore();
  CheckThatCredentialsStored(
      base::UTF16ToUTF8(form.username_value),
      base::UTF16ToUTF8(delegate->GetGeneratedPassword()),
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
      .WillOnce(testing::Return(embedded_test_server()->GetURL(
          kMainHost, "/password/update_form_empty_fields.html")));

  StartPasswordChange(origin, form.username_value, form.password_value,
                      WebContents());
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

  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
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
      .WillOnce(testing::Return(embedded_test_server()->GetURL(
          kMainHost, "/password/update_form_empty_fields.html")));

  StartPasswordChange(origin, form.username_value, form.password_value,
                      WebContents());

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
  CheckPasswordsSavedOnFailure(
      base::UTF16ToUTF8(form.username_value),
      base::UTF16ToUTF8(delegate->GetGeneratedPassword()));

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
      FinalModelStatus::FINAL_MODEL_STATUS_FAILURE,
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FAILURE_STATUS);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       SignInCheckBubbleIsHiddenWhenStateIsUpdated) {
  SetPrivacyNoticeAcceptedPref();
  const GURL main_url = WebContents()->GetLastCommittedURL();
  const GURL change_password_url =
      embedded_test_server()->GetURL("/password/update_form_empty_fields.html");

  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(change_password_url));

  StartPasswordChange(main_url, u"test", u"pa$$word", WebContents());
  // Verify the delegate is created and it's currently waiting for change
  // password form.
  auto* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  ASSERT_TRUE(delegate);

  PasswordBubbleViewBase::ShowBubble(
      WebContents(), LocationBarBubbleDelegateView::USER_GESTURE);
  auto* bubble_controller = static_cast<PasswordChangeInfoBubbleController*>(
      PasswordBubbleViewBase::manage_password_bubble()->GetController());
  ASSERT_EQ(
      bubble_controller->GetTitle(),
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UI_SIGN_IN_CHECK_TITLE));
  ASSERT_EQ(url_formatter::FormatUrlForSecurityDisplay(change_password_url),
            bubble_controller->GetDisplayOrigin());

  // Wait until the state is changed from `kWaitingForChangePasswordForm` to any
  // other state. The bubble should disappear then.
  ASSERT_TRUE(base::test::RunUntil([delegate]() {
    return delegate->GetCurrentState() !=
           PasswordChangeDelegate::State::kWaitingForChangePasswordForm;
  }));
  PasswordBubbleViewBase* bubble =
      PasswordBubbleViewBase::manage_password_bubble();
  ASSERT_FALSE(bubble);
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, OpenTabWithPasswordChange) {
  SetPrivacyNoticeAcceptedPref();

  const GURL main_url = WebContents()->GetLastCommittedURL();
  const GURL change_password_url =
      embedded_test_server()->GetURL("/password/update_form_empty_fields.html");

  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(change_password_url));
  StartPasswordChange(main_url, u"test", u"password", WebContents());

  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());

  EXPECT_EQ(0, tab_strip->active_index());
  password_change_service()
      ->GetPasswordChangeDelegate(WebContents())
      ->OpenPasswordChangeTab();

  EXPECT_EQ(2, tab_strip->count());
  EXPECT_EQ(1, tab_strip->active_index());
}
#endif

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       PrivacyNoticeDisplayedAutomatically) {
  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields.html")));

  BubbleObserver prompt_observer(WebContents());
  StartPasswordChange(main_url, u"test", u"pa$$word", WebContents());

  EXPECT_EQ(PasswordChangeDelegate::State::kWaitingForAgreement,
            password_change_service()
                ->GetPasswordChangeDelegate(WebContents())
                ->GetCurrentState());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return prompt_observer.IsBubbleDisplayedAutomatically(); }));
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       SuccessfulDialogDisplayedAutomatically) {
  SetPrivacyNoticeAcceptedPref();
  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields.html")));

  BubbleObserver prompt_observer(WebContents());
  StartPasswordChange(main_url, u"test", u"pa$$word", WebContents());

  MockPasswordChangeOutcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME);

  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  EXPECT_TRUE(base::test::RunUntil([delegate]() {
    return delegate->GetCurrentState() ==
           PasswordChangeDelegate::State::kPasswordSuccessfullyChanged;
  }));
  // Now bubble should automatically appear.
  EXPECT_TRUE(prompt_observer.IsBubbleDisplayedAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       FailureDialogDisplayedAutomatically) {
  SetPrivacyNoticeAcceptedPref();
  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields.html")));
  BubbleObserver prompt_observer(WebContents());

  StartPasswordChange(main_url, u"test", u"pa$$word", WebContents());
  MockPasswordChangeOutcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_UNSUCCESSFUL_OUTCOME);

  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  ASSERT_TRUE(base::test::RunUntil([delegate]() {
    return delegate->GetCurrentState() ==
           PasswordChangeDelegate::State::kPasswordChangeFailed;
  }));
  // Now bubble should automatically appear.
  EXPECT_TRUE(prompt_observer.IsBubbleDisplayedAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       LeakCheckBubbleDisplayedAutomatically) {
  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields.html")));
  BubbleObserver prompt_observer(WebContents());

  password_change_service()->OfferPasswordChangeUi(main_url, u"test",
                                                   u"pa$$word", WebContents());

  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  EXPECT_EQ(delegate->GetCurrentState(),
            PasswordChangeDelegate::State::kOfferingPasswordChange);
  // Now bubble should automatically appear.
  EXPECT_TRUE(prompt_observer.IsBubbleDisplayedAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       BubbleIsNotDisplayedWhenSwitchedToDifferentTab) {
  SetPrivacyNoticeAcceptedPref();
  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields.html")));

  // Open new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/password/done.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  BubbleObserver prompt_observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Start password change in the old tab
  StartPasswordChange(main_url, u"test", u"pa$$word", WebContents());
  MockPasswordChangeOutcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME);

  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  auto* web_contents =
      static_cast<PasswordChangeDelegateImpl*>(delegate)->executor();
  PasswordsNavigationObserver password_change_page_observer(web_contents);
  EXPECT_TRUE(password_change_page_observer.Wait());

  EXPECT_TRUE(base::test::RunUntil([delegate]() {
    return delegate->GetCurrentState() ==
           PasswordChangeDelegate::State::kPasswordSuccessfullyChanged;
  }));
  // Even after password change is finished no bubble is shown.
  EXPECT_FALSE(prompt_observer.IsBubbleDisplayedAutomatically());
}

IN_PROC_BROWSER_TEST_F(
    PasswordChangeBrowserTest,
    SuccessfulDialogDisplayedAutomaticallyEvenAfterTheCheckIsFinished) {
  SetPrivacyNoticeAcceptedPref();
  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(embedded_test_server()->GetURL(
          "/password/update_form_empty_fields.html")));
  StartPasswordChange(main_url, u"test", u"pa$$word", WebContents());

  // Add and activate a new tab to verify the success dialog will be displayed
  // automatically when focusing the old tab again.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/password/done.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  MockPasswordChangeOutcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME);

  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  EXPECT_TRUE(base::test::RunUntil([delegate]() {
    return delegate->GetCurrentState() ==
           PasswordChangeDelegate::State::kPasswordSuccessfullyChanged;
  }));

  // Verify that activating the original tab shows a bubble automatically.
  BubbleObserver prompt_observer(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(prompt_observer.IsBubbleDisplayedAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, OTPDetectionHaltsTheFlow) {
  SetPrivacyNoticeAcceptedPref();
  const GURL main_url = WebContents()->GetLastCommittedURL();
  EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
      .WillOnce(testing::Return(
          embedded_test_server()->GetURL("/password/done.html")));

  StartPasswordChange(main_url, u"test", u"pa$$word", WebContents());

  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  ASSERT_TRUE(delegate);
  EXPECT_EQ(PasswordChangeDelegate::State::kWaitingForChangePasswordForm,
            delegate->GetCurrentState());

  BubbleObserver prompt_observer(WebContents());

  delegate->OnOtpFieldDetected(
      static_cast<PasswordChangeDelegateImpl*>(delegate)->executor());

  EXPECT_EQ(PasswordChangeDelegate::State::kOtpDetected,
            delegate->GetCurrentState());
  EXPECT_TRUE(prompt_observer.IsBubbleDisplayedAutomatically());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
}
