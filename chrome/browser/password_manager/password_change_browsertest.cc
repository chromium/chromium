// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <utility>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_change_service.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_change/change_password_form_filling_submission_helper.h"
#include "chrome/browser/password_manager/password_change/change_password_form_finder.h"
#include "chrome/browser/password_manager/password_change/login_state_checker.h"
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
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
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
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::WithArg;
using FinalModelStatus = ::optimization_guide::proto::FinalModelStatus;
using PasswordChangeOutcome = ::optimization_guide::proto::
    PasswordChangeSubmissionData_PasswordChangeOutcome;
using QualityStatus = ::optimization_guide::proto::
    PasswordChangeQuality_StepQuality_SubmissionStatus;

constexpr char kMainHost[] = "example.com";
constexpr char kDifferentHost[] = "foo.com";

class TestAutofillManager : public autofill::BrowserAutofillManager {
 public:
  explicit TestAutofillManager(autofill::ContentAutofillDriver* driver)
      : BrowserAutofillManager(driver) {}

  testing::AssertionResult WaitForFormsSeen(int min_num_awaited_calls) {
    return forms_seen_waiter_.Wait(min_num_awaited_calls);
  }

 private:
  autofill::TestAutofillManagerWaiter forms_seen_waiter_{
      *this,
      {autofill::AutofillManagerEvent::kFormsSeen}};
};

std::unique_ptr<KeyedService> CreateTestAffiliationService(
    content::BrowserContext* context) {
  auto affiliation_service =
      std::make_unique<NiceMock<MockAffiliationService>>();
  ON_CALL(*affiliation_service, GetPSLExtensions)
      .WillByDefault(RunOnceCallbackRepeatedly<0>(std::vector<std::string>()));
  ON_CALL(*affiliation_service, GetAffiliationsAndBranding)
      .WillByDefault(
          RunOnceCallbackRepeatedly<1>(affiliations::AffiliatedFacets(), true));
  return std::move(affiliation_service);
}

std::unique_ptr<KeyedService> CreateOptimizationService(
    content::BrowserContext* context) {
  auto opt_guide_keyed_service =
      std::make_unique<NiceMock<MockOptimizationGuideKeyedService>>();
  auto logs_uploader = std::make_unique<TestModelQualityLogsUploaderService>(
      g_browser_process->local_state());
  opt_guide_keyed_service->SetModelQualityLogsUploaderServiceForTesting(
      std::move(logs_uploader));
  return opt_guide_keyed_service;
}

password_manager::PasswordForm CreatePasswordForm(
    const GURL& url,
    const std::u16string& username,
    const std::u16string& password) {
  password_manager::PasswordForm form;
  form.url = GURL(url);
  form.signon_realm = url.GetWithEmptyPath().spec();
  form.username_value = username;
  form.password_value = password;
  return form;
}

void NavigateToURL(content::WebContents* web_contents, const GURL& url) {
  ASSERT_TRUE(content::NavigateToURL(web_contents, url));
}

}  // namespace

class PasswordChangeBrowserTest : public PasswordManagerBrowserTestBase {
 public:
  PasswordChangeBrowserTest() {
    // TODO (crbug.com/439496997): Fix the test to work with this feature flag
    // default value.
    scoped_feature_list_.InitWithFeatures(
        // kShowDomNodeIDs is required in order to extract the dom_node_id for
        // the submission step.
        {autofill::features::debug::kShowDomNodeIDs,
         password_manager::features::kStopLoginCheckOnFailedLogin},
        {});
  }

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

  void VerifyUniqueQualityLog(QualityStatus login_check_status,
                              QualityStatus open_form_status,
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
    EXPECT_EQ(quality.logged_in_check().status(), login_check_status);
    EXPECT_EQ(quality.open_form().status(), open_form_status);
    EXPECT_EQ(quality.submit_form().status(), submit_form_status);
    EXPECT_EQ(quality.verify_submission().status(), verify_submission_status);
    EXPECT_EQ(quality.final_model_status(), final_status);
  }

  void SetPrivacyNoticeAcceptedPref() {
    ON_CALL(*mock_optimization_guide_keyed_service(),
            ShouldFeatureBeCurrentlyEnabledForUser(
                optimization_guide::UserVisibleFeatureKey::
                    kPasswordChangeSubmission))
        .WillByDefault(Return(true));
  }

  void SetChangePasswordUrl(const std::string& url) {
    const GURL main_url = WebContents()->GetLastCommittedURL();
    EXPECT_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
        .WillOnce(Return(embedded_test_server()->GetURL(url)));
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

  ChromePasswordManagerClient* client() const {
    return ChromePasswordManagerClient::FromWebContents(WebContents());
  }

  TestAutofillManager* GetAutofillManager() {
    return autofill_manager_injector_[WebContents()->GetPrimaryMainFrame()];
  }

  int GetDomNodeId(const std::string& element_id) {
    const std::string value_get_script = base::StringPrintf(
        "var element = document.getElementById('%s');"
        "var value = element ? Number(element.getAttribute(\"dom-node-id\")) : "
        "-1;"
        "value;",
        element_id.c_str());
    return content::EvalJs(RenderFrameHost(), value_get_script,
                           content::EXECUTE_SCRIPT_NO_USER_GESTURE)
        .ExtractInt();
  }

  void MockLoginOutcome(LoginCheckResult outcome) {
    base::RunLoop run_loop;
    MockOptimizationGuideKeyedService* optimization_service =
        mock_optimization_guide_keyed_service();
    optimization_guide::OptimizationGuideModelExecutionResultCallback callback;
    EXPECT_CALL(*optimization_service,
                ExecuteModel(optimization_guide::ModelBasedCapabilityKey::
                                 kPasswordChangeSubmission,
                             _, _, _))
        .WillOnce(DoAll(
            testing::Invoke(&run_loop, &base::RunLoop::Quit),
            WithArg<3>([&](auto callback) {
              optimization_guide::proto::PasswordChangeResponse response;
              switch (outcome) {
                case LoginCheckResult::kLoggedIn:
                  response.mutable_is_logged_in_data()->set_is_logged_in(true);
                  break;
                case LoginCheckResult::kLoggedOut:
                  response.mutable_is_logged_in_data()->set_is_logged_in(false);
                  break;
                case LoginCheckResult::kError:
                  response.mutable_is_logged_in_data()->set_is_logged_in(false);
                  response.mutable_is_logged_in_data()->set_error_case(
                      optimization_guide::proto::IsLoggedInResponseData::
                          ErrorCase::
                              IsLoggedInResponseData_ErrorCase_LOGIN_FAILED);
                  break;
              }
              auto result =
                  optimization_guide::OptimizationGuideModelExecutionResult(
                      optimization_guide::AnyWrapProto(response),
                      /*execution_info=*/nullptr);
              std::move(callback).Run(std::move(result), /*log_entry=*/nullptr);
            })));
    run_loop.Run();
    // The previous EXPECT posts the limitation that there must not be more
    // calls to ExecuteModel after login check. This causes flakiness in many
    // tests, that proceed with filling and submitting the form. So this
    // should allow further calls to ExecuteModel for the following steps, which
    // may or may not follow depending on the test.
    testing::Mock::VerifyAndClearExpectations(
        mock_optimization_guide_keyed_service());
  }

  void MockSuccessfulSubmitButtonClick(PasswordChangeDelegate* delegate) {
    SetWebContents(
        static_cast<PasswordChangeDelegateImpl*>(delegate)->executor());

    base::RunLoop run_loop;
    MockOptimizationGuideKeyedService* optimization_service =
        mock_optimization_guide_keyed_service();
    optimization_guide::OptimizationGuideModelExecutionResultCallback callback;
    EXPECT_CALL(*optimization_service,
                ExecuteModel(optimization_guide::ModelBasedCapabilityKey::
                                 kPasswordChangeSubmission,
                             _, _, _))
        .WillOnce(DoAll(
            testing::Invoke(&run_loop, &base::RunLoop::Quit),
            WithArg<3>([&](auto callback) {
              optimization_guide::proto::PasswordChangeResponse response;
              response.mutable_submit_form_data()->set_dom_node_id_to_click(
                  GetDomNodeId("chg_submit_wo_username_button"));
              auto result =
                  optimization_guide::OptimizationGuideModelExecutionResult(
                      optimization_guide::AnyWrapProto(response),
                      /*execution_info=*/nullptr);
              std::move(callback).Run(std::move(result), /*log_entry=*/nullptr);
            })));
    run_loop.Run();
    SetWebContents(browser()->tab_strip_model()->GetWebContentsAt(0));
  }

  void MockPasswordChangeOutcome(PasswordChangeOutcome outcome) {
    base::RunLoop run_loop;
    MockOptimizationGuideKeyedService* optimization_service =
        mock_optimization_guide_keyed_service();
    EXPECT_CALL(*optimization_service,
                ExecuteModel(optimization_guide::ModelBasedCapabilityKey::
                                 kPasswordChangeSubmission,
                             _, _, _))
        .WillOnce(DoAll(
            testing::Invoke(&run_loop, &base::RunLoop::Quit),
            WithArg<3>([outcome](auto callback) {
              optimization_guide::proto::PasswordChangeResponse response;
              response.mutable_outcome_data()->set_submission_outcome(outcome);
              auto result =
                  optimization_guide::OptimizationGuideModelExecutionResult(
                      optimization_guide::AnyWrapProto(response),
                      /*execution_info=*/nullptr);
              std::move(callback).Run(std::move(result), /*log_entry=*/nullptr);
            })));
    run_loop.Run();
  }

  autofill::FormData CreateSimpleOtp() {
    content::RenderFrameHost* rfh = WebContents()->GetPrimaryMainFrame();
    autofill::LocalFrameToken frame_token(rfh->GetFrameToken().value());
    autofill::FormData form;
    form.set_url(GURL("https://www.foo.com"));
    form.set_renderer_id(autofill::test::MakeFormRendererId());
    autofill::FormFieldData field = {autofill::test::CreateTestFormField(
        "some_label", "some_name", "some_value",
        autofill::FormControlType::kInputText)};

    form.set_fields({field});
    return autofill::test::CreateFormDataForFrame(form, frame_token);
  }

  void AddOtpToThePage() {
    // Inject the form because otherwise it cannot be guaranteed that the OTP
    // field is classified as such.
    auto form = CreateSimpleOtp();
    auto form_structure = std::make_unique<autofill::FormStructure>(form);
    const std::vector<autofill::FieldType> field_types = {
        autofill::ONE_TIME_CODE};
    autofill::test_api(*form_structure)
        .SetFieldTypes(/*heuristic_types=*/field_types,
                       /*server_types=*/field_types);
    autofill::test_api(*form_structure).AssignSections();
    autofill::test_api(*GetAutofillManager())
        .AddSeenFormStructure(std::move(form_structure));
    autofill::test_api(*GetAutofillManager()).OnFormsParsed({form});

    ASSERT_TRUE(
        GetAutofillManager()->FindCachedFormById(form.fields()[0].global_id()));

    // Notify observers manually as this would typically happen during parsing
    // but the step is skipped when using the Test APIs.
    GetAutofillManager()->NotifyObservers(
        &TestAutofillManager::Observer::OnFieldTypesDetermined,
        form.global_id(),
        TestAutofillManager::Observer::FieldTypeSource::
            kHeuristicsOrAutocomplete);
  }

 private:
  autofill::test::AutofillBrowserTestEnvironment autofill_environment_;
  base::CallbackListSubscription create_services_subscription_;
  autofill::TestAutofillManagerInjector<TestAutofillManager>
      autofill_manager_injector_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::WeakPtrFactory<PasswordChangeBrowserTest> weak_ptr_factory_{this};
};

// Flaky: crbug.com/456247817
#if BUILDFLAG(IS_LINUX)
#define MAYBE_ChangePasswordFormIsFilledAutomatically \
  DISABLED_ChangePasswordFormIsFilledAutomatically
#else
#define MAYBE_ChangePasswordFormIsFilledAutomatically \
  ChangePasswordFormIsFilledAutomatically
#endif  // BUILDFLAG(IS_LINUX)
IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       MAYBE_ChangePasswordFormIsFilledAutomatically) {
  SetPrivacyNoticeAcceptedPref();
  SetChangePasswordUrl("/password/update_form_empty_fields_no_submit.html");

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  MockLoginOutcome(LoginCheckResult::kLoggedIn);

  content::WebContents* web_contents =
      static_cast<PasswordChangeDelegateImpl*>(delegate)->executor();
  // Start observing web_contents where password change happens.
  SetWebContents(web_contents);
  PasswordsNavigationObserver observer(web_contents);
  EXPECT_TRUE(observer.Wait());

  std::string generated_password = base::UTF16ToUTF8(
      static_cast<PasswordChangeDelegateImpl*>(delegate)->generated_password());

  // Verify all the fields are filled correctly.
  WaitForElementValue("new_password_2", generated_password);
  WaitForElementValue("new_password_1", generated_password);
  WaitForElementValue("password", "pa$$word");
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, GeneratedPasswordIsPreSaved) {
  SetPrivacyNoticeAcceptedPref();
  SetChangePasswordUrl("/password/update_form_empty_fields_no_submit.html");

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  MockLoginOutcome(LoginCheckResult::kLoggedIn);

  // Start observing web_contents where password change happens.
  auto* delegate_impl = static_cast<PasswordChangeDelegateImpl*>(delegate);
  SetWebContents(delegate_impl->executor());
  PasswordsNavigationObserver observer(WebContents());
  EXPECT_TRUE(observer.Wait());

  // Verify generated password is pre-saved.
  WaitForElementValue("password", "pa$$word");
  WaitForPasswordStore();
  CheckThatCredentialsStored(
      /*username=*/"test", "pa$$word",
      base::UTF16ToUTF8(static_cast<PasswordChangeDelegateImpl*>(delegate)
                            ->generated_password()));
}

// Verify that after password change is stopped, password change delegate is not
// returned.
IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, StopPasswordChange) {
  SetPrivacyNoticeAcceptedPref();
  SetChangePasswordUrl("/password/done.html");

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
  ASSERT_TRUE(
      password_change_service()->GetPasswordChangeDelegate(WebContents()));

  password_change_service()->GetPasswordChangeDelegate(WebContents())->Stop();
  EXPECT_FALSE(
      password_change_service()->GetPasswordChangeDelegate(WebContents()));
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, NewPasswordIsSaved) {
  SetPrivacyNoticeAcceptedPref();
  SetChangePasswordUrl("/password/update_form_empty_fields.html");

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  MockLoginOutcome(LoginCheckResult::kLoggedIn);
  MockSuccessfulSubmitButtonClick(delegate);
  MockPasswordChangeOutcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME);

  EXPECT_EQ(PasswordChangeDelegate::State::kPasswordSuccessfullyChanged,
            delegate->GetCurrentState());
  WaitForPasswordStore();
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
  VerifyUniqueQualityLog(
      /*login_check_status=*/QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      /*open_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_STEP_SKIPPED,
      /*submit_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      /*verify_submission_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      /*final_status=*/
      FinalModelStatus::FINAL_MODEL_STATUS_SUCCESS);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, OldPasswordIsUpdated) {
  SetPrivacyNoticeAcceptedPref();

  // Add an existing password for this site.
  ASSERT_TRUE(content::NavigateToURL(
      WebContents(),
      embedded_test_server()->GetURL("/password/simple_password.html")));
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm form = CreatePasswordForm(
      WebContents()->GetLastCommittedURL(), u"test", u"pa$$word");
  password_store->AddLogin(form);
  WaitForPasswordStore();

  SetChangePasswordUrl("/password/update_form_empty_fields.html");
  password_change_service()->OfferPasswordChangeUi(form, WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  MockLoginOutcome(LoginCheckResult::kLoggedIn);
  MockSuccessfulSubmitButtonClick(delegate);
  MockPasswordChangeOutcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME);

  EXPECT_EQ(PasswordChangeDelegate::State::kPasswordSuccessfullyChanged,
            delegate->GetCurrentState());

  // Verify saved password is updated.
  WaitForPasswordStore();
  CheckThatCredentialsStored(
      base::UTF16ToUTF8(form.username_value),
      base::UTF16ToUTF8(static_cast<PasswordChangeDelegateImpl*>(delegate)
                            ->generated_password()),
      base::UTF16ToUTF8(form.password_value),
      password_manager::PasswordForm::Type::kChangeSubmission);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, OpenTabWithPasswordChange) {
  SetPrivacyNoticeAcceptedPref();
  SetChangePasswordUrl("/password/update_form_empty_fields.html");

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  MockLoginOutcome(LoginCheckResult::kLoggedIn);

  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(tab_strip->count(), 1);

  EXPECT_EQ(tab_strip->active_index(), 0);
  delegate->OpenPasswordChangeTab();
  // Stop the flow as this what happens in reality when user chooses to see a
  // hidden tab.
  delegate->Stop();

  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_EQ(tab_strip->active_index(), 1);

  EXPECT_FALSE(ChromePasswordManagerClient::FromWebContents(WebContents())
                   ->apply_client_side_prediction_override_for_testing());
  EXPECT_TRUE(ChromePasswordManagerClient::FromWebContents(
                  tab_strip->GetActiveWebContents())
                  ->apply_client_side_prediction_override_for_testing());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       LeakCheckDialogWithPrivacyNoticeDisplayed) {
  SetChangePasswordUrl("/password/update_form_empty_fields.html");

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
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
  SetChangePasswordUrl("/password/update_form_empty_fields.html");

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  MockLoginOutcome(LoginCheckResult::kLoggedIn);
  MockPasswordChangeOutcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_UNSUCCESSFUL_OUTCOME);

  EXPECT_EQ(PasswordChangeDelegate::State::kPasswordChangeFailed,
            delegate->GetCurrentState());

  EXPECT_TRUE(static_cast<PasswordChangeDelegateImpl*>(delegate)
                  ->ui_controller()
                  ->dialog_widget()
                  ->IsVisible());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       LeakCheckDialogWithoutPrivacyNoticeDisplayed) {
  SetPrivacyNoticeAcceptedPref();
  SetChangePasswordUrl("/password/update_form_empty_fields.html");

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());

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
  SetChangePasswordUrl("/password/done.html");

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  MockLoginOutcome(LoginCheckResult::kLoggedIn);
  EXPECT_EQ(delegate->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForChangePasswordForm);

  auto* delegate_impl = static_cast<PasswordChangeDelegateImpl*>(delegate);
  delegate_impl->OnOtpFieldDetected();

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
      /*login_check_status=*/QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      /*open_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_OTP_DETECTED,
      /*submit_form_status=*/
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
  SetChangePasswordUrl("/password/done.html");

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  EXPECT_TRUE(delegate);
  delegate->StartPasswordChangeFlow();
  MockLoginOutcome(LoginCheckResult::kLoggedIn);

  PasswordChangeUIController* ui_controller =
      static_cast<PasswordChangeDelegateImpl*>(delegate)->ui_controller();
  EXPECT_TRUE(ui_controller->toast_view());
  // Verify action button is present and visible.
  EXPECT_TRUE(ui_controller->toast_view()->close_button());
  EXPECT_TRUE(ui_controller->toast_view()->close_button()->GetVisible());

  // Click action button, this should cancel the flow.
  views::test::ButtonTestApi clicker(
      ui_controller->toast_view()->close_button());
  clicker.NotifyClick(ui::test::TestEvent());

  EXPECT_EQ(PasswordChangeDelegate::State::kCanceled,
            delegate->GetCurrentState());

  // Verify toast is displayed.
  EXPECT_TRUE(ui_controller->toast_view());
  // Action button navigates to the password change tab
  EXPECT_TRUE(ui_controller->toast_view()->action_button()->GetVisible());

  // The quality log is uploaded in the destructor.
  base::WeakPtr<PasswordChangeDelegate> delegate_weak_ptr =
      delegate->AsWeakPtr();
  EXPECT_TRUE(base::test::RunUntil(
      [&delegate_weak_ptr]() { return !delegate_weak_ptr; }));

  VerifyUniqueQualityLog(
      /*login_check_status=*/QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      /*open_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FLOW_INTERRUPTED,
      /*submit_form_status=*/
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
  SetChangePasswordUrl("/password/update_form_empty_fields.html");

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  MockLoginOutcome(LoginCheckResult::kLoggedIn);

  MockSuccessfulSubmitButtonClick(delegate);
  MockPasswordChangeOutcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME);
  EXPECT_EQ(PasswordChangeDelegate::State::kPasswordSuccessfullyChanged,
            delegate->GetCurrentState());

  // Verify toast is displayed.
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
  ASSERT_TRUE(content::NavigateToURL(
      WebContents(),
      embedded_test_server()->GetURL("/password/simple_password.html")));

  SetPrivacyNoticeAcceptedPref();
  SetChangePasswordUrl("/password/update_form_empty_fields.html");

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  MockLoginOutcome(LoginCheckResult::kLoggedIn);
  MockSuccessfulSubmitButtonClick(delegate);
  MockPasswordChangeOutcome(
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME);

  EXPECT_EQ(PasswordChangeDelegate::State::kPasswordSuccessfullyChanged,
            delegate->GetCurrentState());
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
                       CrossOriginNavigationDetected) {
  SetPrivacyNoticeAcceptedPref();
  SetChangePasswordUrl("/password/update_form_empty_fields.html");

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());

  // Verify the delegate is created.
  base::WeakPtr<PasswordChangeDelegate> delegate =
      password_change_service()
          ->GetPasswordChangeDelegate(WebContents())
          ->AsWeakPtr();
  ASSERT_TRUE(delegate);

  // Verify delegate is waiting for change password form when password change
  // starts.
  delegate->StartPasswordChangeFlow();
  MockLoginOutcome(LoginCheckResult::kLoggedIn);
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

  delegate->Stop();
  EXPECT_TRUE(base::test::RunUntil([&delegate]() {
    // Delegate's destructor is called async, so this is needed before checking
    // the metrics report.
    return delegate == nullptr;
  }));

  VerifyUniqueQualityLog(
      /*login_check_status=*/QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      /*open_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_STEP_SKIPPED,
      /*submit_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_CROSE_ORIGIN_NAVIGATION,
      /*verify_submission_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*final_status=*/
      FinalModelStatus::FINAL_MODEL_STATUS_UNSPECIFIED);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       CrossOriginNavigationDetectedBeforeStartingTheFlow) {
  SetPrivacyNoticeAcceptedPref();
  SetChangePasswordUrl("/password/update_form_empty_fields.html");

  AddOtpToThePage();

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());

  // Verify the delegate is created.
  PasswordChangeDelegateImpl* delegate =
      static_cast<PasswordChangeDelegateImpl*>(
          password_change_service()->GetPasswordChangeDelegate(WebContents()));
  base::WeakPtr<PasswordChangeDelegate> delegate_weak_ptr =
      delegate->AsWeakPtr();
  ASSERT_TRUE(delegate);
  GURL url = https_test_server().GetURL(kDifferentHost,
                                        "/password/simple_password.html");
  // Navigate away from the page to a different domain. The flow should be
  // stopped.
  ASSERT_TRUE(content::NavigateToURL(WebContents(), url));

  EXPECT_TRUE(base::test::RunUntil(
      [&delegate_weak_ptr]() { return !delegate_weak_ptr; }));
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       OnTabCloseLogsUnexpectedFailure) {
  SetPrivacyNoticeAcceptedPref();
  SetChangePasswordUrl("/password/done.html");

  int original_apc_flow_tab_index =
      browser()->tab_strip_model()->GetIndexOfWebContents(WebContents());

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  MockLoginOutcome(LoginCheckResult::kLoggedIn);

  EXPECT_TRUE(base::test::RunUntil([delegate]() {
    return delegate->GetCurrentState() ==
           PasswordChangeDelegate::State::kWaitingForChangePasswordForm;
  }));

  // Add an extra tab to prevent a dangling pointer when closing
  // the tab where the main flow is active.
  std::unique_ptr<content::WebContents> extra_web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  content::WebContents* new_active_web_contents = extra_web_contents.get();
  browser()->tab_strip_model()->AppendWebContents(std::move(extra_web_contents),
                                                  true /* foreground */);
  SetWebContents(new_active_web_contents);

  // Closing the tab where the flow is active triggers a flow interruption log.
  browser()->tab_strip_model()->CloseWebContentsAt(
      original_apc_flow_tab_index, TabCloseTypes::CLOSE_USER_GESTURE);

  base::WeakPtr<PasswordChangeDelegate> delegate_weak_ptr =
      delegate->AsWeakPtr();
  EXPECT_TRUE(base::test::RunUntil(
      [&delegate_weak_ptr]() { return !delegate_weak_ptr; }));

  VerifyUniqueQualityLog(
      /*login_check_status=*/QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      /*open_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FLOW_INTERRUPTED,
      /*submit_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*verify_submission_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*final_status=*/
      FinalModelStatus::FINAL_MODEL_STATUS_UNSPECIFIED);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       FlowInterruptedDuringOpenFormStep) {
  SetPrivacyNoticeAcceptedPref();
  SetChangePasswordUrl("/password/done.html");

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  MockLoginOutcome(LoginCheckResult::kLoggedIn);

  PasswordChangeUIController* ui_controller =
      static_cast<PasswordChangeDelegateImpl*>(delegate)->ui_controller();
  EXPECT_TRUE(base::test::RunUntil(
      [ui_controller]() { return ui_controller->toast_view(); }));
  base::WeakPtr<PasswordChangeDelegate> delegate_weak_ptr =
      delegate->AsWeakPtr();
  // Simulate clicking the "cancel" button on the UI toast.
  views::test::ButtonTestApi clicker(
      ui_controller->toast_view()->close_button());
  clicker.NotifyClick(ui::test::TestEvent());
  // Verify that the flow's state is "canceled".
  EXPECT_EQ(PasswordChangeDelegate::State::kCanceled,
            delegate->GetCurrentState());
  EXPECT_TRUE(base::test::RunUntil(
      [&delegate_weak_ptr]() { return !delegate_weak_ptr; }));

  VerifyUniqueQualityLog(
      /*login_check_status=*/QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      /*open_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FLOW_INTERRUPTED,
      /*submit_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*verify_submission_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*final_status=*/
      FinalModelStatus::FINAL_MODEL_STATUS_UNSPECIFIED);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       FlowInterruptedAfterOpenFormStep) {
  SetPrivacyNoticeAcceptedPref();
  SetChangePasswordUrl("/password/update_form_empty_fields.html");

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  MockLoginOutcome(LoginCheckResult::kLoggedIn);

  EXPECT_TRUE(base::test::RunUntil([delegate]() {
    return delegate->GetCurrentState() ==
           PasswordChangeDelegate::State::kChangingPassword;
  }));

  PasswordChangeUIController* ui_controller =
      static_cast<PasswordChangeDelegateImpl*>(delegate)->ui_controller();
  EXPECT_TRUE(base::test::RunUntil(
      [ui_controller]() { return ui_controller->toast_view(); }));
  base::WeakPtr<PasswordChangeDelegate> delegate_weak_ptr =
      delegate->AsWeakPtr();
  // Simulate clicking the "cancel" button on the UI toast.
  views::test::ButtonTestApi clicker(
      ui_controller->toast_view()->close_button());
  clicker.NotifyClick(ui::test::TestEvent());
  // Verify that the flow's state is "canceled".
  EXPECT_EQ(PasswordChangeDelegate::State::kCanceled,
            delegate->GetCurrentState());
  EXPECT_TRUE(base::test::RunUntil(
      [&delegate_weak_ptr]() { return !delegate_weak_ptr; }));

  VerifyUniqueQualityLog(
      /*login_check_status=*/QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      /*open_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_STEP_SKIPPED,
      /*submit_form_status=*/
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
  SetChangePasswordUrl("/password/update_form_empty_fields.html");

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  MockLoginOutcome(LoginCheckResult::kLoggedIn);
  MockSuccessfulSubmitButtonClick(delegate);

  EXPECT_EQ(PasswordChangeDelegate::State::kChangingPassword,
            delegate->GetCurrentState());
  // Cancel the flow.
  delegate->CancelPasswordChangeFlow();
  EXPECT_EQ(PasswordChangeDelegate::State::kCanceled,
            delegate->GetCurrentState());

  // Logs are uploaded on destruction.
  base::WeakPtr<PasswordChangeDelegate> delegate_weak_ptr =
      delegate->AsWeakPtr();
  EXPECT_TRUE(base::test::RunUntil(
      [&delegate_weak_ptr]() { return !delegate_weak_ptr; }));

  VerifyUniqueQualityLog(
      /*login_check_status=*/QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      /*open_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_STEP_SKIPPED,
      /*submit_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      /*verify_submission_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FLOW_INTERRUPTED,
      /*final_status=*/
      FinalModelStatus::FINAL_MODEL_STATUS_UNSPECIFIED);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       OtpDetectedDuringSubmitFormStep) {
  SetPrivacyNoticeAcceptedPref();
  SetChangePasswordUrl("/password/update_form_empty_fields.html");

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  MockLoginOutcome(LoginCheckResult::kLoggedIn);

  EXPECT_TRUE(base::test::RunUntil([delegate]() {
    return delegate->GetCurrentState() ==
           PasswordChangeDelegate::State::kChangingPassword;
  }));

  base::WeakPtr<PasswordChangeDelegate> delegate_weak_ptr =
      delegate->AsWeakPtr();

  auto* delegate_impl = static_cast<PasswordChangeDelegateImpl*>(delegate);
  delegate_impl->OnOtpFieldDetected();
  EXPECT_EQ(delegate->GetCurrentState(),
            PasswordChangeDelegate::State::kOtpDetected);
  delegate_impl->ui_controller()->CallOnDialogCanceledForTesting();
  EXPECT_TRUE(base::test::RunUntil(
      [&delegate_weak_ptr]() { return !delegate_weak_ptr; }));

  VerifyUniqueQualityLog(
      /*login_check_status=*/QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      /*open_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_STEP_SKIPPED,
      /*submit_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_OTP_DETECTED,
      /*verify_submission_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*final_status=*/
      FinalModelStatus::FINAL_MODEL_STATUS_UNSPECIFIED);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       OtpDetectedDuringVerificationStep) {
  SetPrivacyNoticeAcceptedPref();
  SetChangePasswordUrl("/password/update_form_empty_fields.html");

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  MockLoginOutcome(LoginCheckResult::kLoggedIn);
  MockSuccessfulSubmitButtonClick(delegate);

  base::WeakPtr<PasswordChangeDelegate> delegate_weak_ptr =
      delegate->AsWeakPtr();

  auto* delegate_impl = static_cast<PasswordChangeDelegateImpl*>(delegate);
  delegate_impl->OnOtpFieldDetected();
  EXPECT_EQ(delegate->GetCurrentState(),
            PasswordChangeDelegate::State::kOtpDetected);
  delegate_impl->ui_controller()->CallOnDialogCanceledForTesting();
  EXPECT_TRUE(base::test::RunUntil(
      [&delegate_weak_ptr]() { return !delegate_weak_ptr; }));

  VerifyUniqueQualityLog(
      /*login_check_status=*/QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      /*open_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_STEP_SKIPPED,
      /*submit_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      /*verify_submission_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_OTP_DETECTED,
      /*final_status=*/
      FinalModelStatus::FINAL_MODEL_STATUS_UNSPECIFIED);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest, OpenTabWhenLoggedOut) {
  SetChangePasswordUrl("/password/update_form_empty_fields.html");
  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
  auto* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();

  auto* delegate_impl = static_cast<PasswordChangeDelegateImpl*>(delegate);
  // Verify that the background tab was not created yet.
  EXPECT_FALSE(delegate_impl->executor());
  EXPECT_TRUE(delegate_impl->login_checker());
  EXPECT_EQ(delegate->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForChangePasswordForm);

  MockLoginOutcome(LoginCheckResult::kLoggedOut);
  EXPECT_EQ(delegate->GetCurrentState(),
            PasswordChangeDelegate::State::kLoginFormDetected);
  delegate->Stop();

  // When a user is not logged in, we still open a new tab with the
  // change password URL, so there should be two tabs after.
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
  delegate->OpenPasswordChangeTab();
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
  auto* change_password_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_EQ(change_password_contents->GetVisibleURL(),
            embedded_test_server()->GetURL(
                "/password/update_form_empty_fields.html"));
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       UserIsLoggedInOnSecondAttempt) {
  SetChangePasswordUrl("/password/done.html");

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
  auto* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  MockLoginOutcome(LoginCheckResult::kLoggedOut);
  EXPECT_EQ(delegate->GetCurrentState(),
            PasswordChangeDelegate::State::kLoginFormDetected);

  // Post a task to navigate properly capture request with MockLoginOutcome();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&NavigateToURL, WebContents(),
                                embedded_test_server()->GetURL(
                                    kMainHost, "/password/done.html")));
  MockLoginOutcome(LoginCheckResult::kLoggedIn);
  EXPECT_EQ(delegate->GetCurrentState(),
            PasswordChangeDelegate::State::kWaitingForChangePasswordForm);
  // Stop the flow to check the correct state of the quality log.
  delegate->Stop();
  // The quality log is uploaded in the destructor.
  base::WeakPtr<PasswordChangeDelegate> delegate_weak_ptr =
      delegate->AsWeakPtr();
  EXPECT_TRUE(base::test::RunUntil(
      [&delegate_weak_ptr]() { return !delegate_weak_ptr; }));
  VerifyUniqueQualityLog(
      /*login_check_status=*/QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS,
      /*open_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*submit_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*verify_submission_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*final_status=*/
      FinalModelStatus::FINAL_MODEL_STATUS_UNSPECIFIED);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       LoginCheckRespondedWithError) {
  SetChangePasswordUrl("/password/done.html");
  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
  auto* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());

  delegate->StartPasswordChangeFlow();

  // Verify that password change fails if login check ends with an error.
  MockLoginOutcome(LoginCheckResult::kError);
  EXPECT_EQ(delegate->GetCurrentState(),
            PasswordChangeDelegate::State::kChangePasswordFormNotFound);
  // Stop the flow to check the correct state of the quality log.
  delegate->Stop();

  base::WeakPtr<PasswordChangeDelegate> delegate_weak_ptr =
      delegate->AsWeakPtr();
  // The quality log is uploaded in the destructor.
  EXPECT_TRUE(base::test::RunUntil(
      [&delegate_weak_ptr]() { return !delegate_weak_ptr; }));
  VerifyUniqueQualityLog(
      /*login_check_status=*/QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNEXPECTED_STATE,
      /*open_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*submit_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*verify_submission_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*final_status=*/
      FinalModelStatus::FINAL_MODEL_STATUS_UNSPECIFIED);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTest,
                       FlowInterruptedBeforeLoginCheck) {
  SetPrivacyNoticeAcceptedPref();
  SetChangePasswordUrl("/password/done.html");

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  EXPECT_TRUE(delegate);
  delegate->StartPasswordChangeFlow();
  PasswordChangeUIController* ui_controller =
      static_cast<PasswordChangeDelegateImpl*>(delegate)->ui_controller();
  EXPECT_TRUE(ui_controller->toast_view());
  // Verify action button is present and visible.
  EXPECT_TRUE(ui_controller->toast_view()->close_button());
  EXPECT_TRUE(ui_controller->toast_view()->close_button()->GetVisible());

  // Click action button, this should cancel the flow.
  // Which is counted as an interruption in the quality logs.
  views::test::ButtonTestApi clicker(
      ui_controller->toast_view()->close_button());
  clicker.NotifyClick(ui::test::TestEvent());
  EXPECT_EQ(PasswordChangeDelegate::State::kCanceled,
            delegate->GetCurrentState());

  // The quality log is uploaded in the destructor.
  base::WeakPtr<PasswordChangeDelegate> delegate_weak_ptr =
      delegate->AsWeakPtr();
  EXPECT_TRUE(base::test::RunUntil(
      [&delegate_weak_ptr]() { return !delegate_weak_ptr; }));

  VerifyUniqueQualityLog(
      /*login_check_status=*/QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FLOW_INTERRUPTED,
      /*open_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*submit_form_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*verify_submission_status=*/
      QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_UNKNOWN_STATUS,
      /*final_status=*/
      FinalModelStatus::FINAL_MODEL_STATUS_UNSPECIFIED);
}

class PasswordChangeBrowserTestShowHiddenTab
    : public PasswordChangeBrowserTest {
 public:
  PasswordChangeBrowserTestShowHiddenTab() {
    scoped_feature_list_.InitAndEnableFeature(
        password_manager::features::kRunPasswordChangeInBackgroundTab);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PasswordChangeBrowserTestShowHiddenTab,
                       ShowHiddenTabDuringPasswordChange) {
  SetPrivacyNoticeAcceptedPref();
  SetChangePasswordUrl("/password/update_form_empty_fields_no_submit.html");

  password_change_service()->OfferPasswordChangeUi(
      CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                         u"pa$$word"),
      WebContents());
  PasswordChangeDelegate* delegate =
      password_change_service()->GetPasswordChangeDelegate(WebContents());
  delegate->StartPasswordChangeFlow();
  MockLoginOutcome(LoginCheckResult::kLoggedIn);

  TabStripModel* tab_strip = browser()->tab_strip_model();
  // Assert that password change tab is opened.
  ASSERT_EQ(tab_strip->count(), 2);
}
