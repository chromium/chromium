// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/device_info.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/keyboard_accessory/android/password_accessory_controller.h"
#include "chrome/browser/password_manager/android/password_generation_controller.h"
#include "chrome/browser/password_manager/android/password_manager_test_utils_bridge.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/factories/account_password_store_factory.h"
#include "chrome/browser/password_manager/factories/profile_password_store_factory.h"
#include "chrome/browser/password_manager/passwords_navigation_observer.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/touch_to_fill_password_generation_controller.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/realtime_reporting_test_environment.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/password_store_results_observer.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kEnrollmentToken[] = "fake-enrollment-token";
constexpr char kEnrollmentTokenPolicyName[] = "CloudManagementEnrollmentToken";
constexpr char kClientId[] = "fake_client_id";

}  // namespace

class PasswordManagerAndroidBrowserTestBase : public AndroidBrowserTest {
 public:
  PasswordManagerAndroidBrowserTestBase()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // Set a GMS Core version that is guaranteed to provide full UPM support.
    // This ensures that calls to the password store are derministically
    // routed to the android backend.
    base::android::device_info::set_gms_version_code_for_test(
        base::NumberToString(password_manager::GetSplitStoresUpmMinVersion()));
    // See crbug.com/331746629. The login database on Android will be
    // deprecated soon. So create a fake backend on GMS Core for password
    // storing.
    SetUpGmsCoreFakeBackends();
  }
  ~PasswordManagerAndroidBrowserTestBase() override = default;

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void SetUpOnMainThread() override {
    // Map all out-going DNS lookups to the local server. This must be used in
    // conjunction with switches::kIgnoreCertificateErrors to work.
    host_resolver()->AddRule("*", "127.0.0.1");

    // Setup HTTPS server serving files from standard test directory.
    static constexpr base::FilePath::CharType kDocRoot[] =
        FILE_PATH_LITERAL("chrome/test/data");
    https_server_.ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
    ASSERT_TRUE(https_server_.Start());
  }

  void NavigateToFile(const std::string& file_path) {
    PasswordsNavigationObserver observer(GetActiveWebContents());
    EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                       https_server_.GetURL(file_path)));
    ASSERT_TRUE(observer.Wait());
  }

  const GURL& base_url() const { return https_server_.base_url(); }

  GURL GetURL(const std::string& file_path) const {
    return https_server_.GetURL(file_path);
  }

  void WaitForHistogram(const std::string& histogram_name,
                        const base::HistogramTester& histogram_tester) {
    // Continue if histogram was already recorded.
    if (base::StatisticsRecorder::FindHistogram(histogram_name)) {
      return;
    }

    // Else, wait until the histogram is recorded.
    base::RunLoop run_loop;
    auto histogram_observer = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        histogram_name,
        base::BindLambdaForTesting(
            [&](std::string_view histogram_name, uint64_t name_hash,
                base::HistogramBase::Sample32 sample) { run_loop.Quit(); }));
    run_loop.Run();
  }

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

  void WaitForPasswordStores() {
    scoped_refptr<password_manager::PasswordStoreInterface>
        profile_password_store = ProfilePasswordStoreFactory::GetForProfile(
            GetProfile(), ServiceAccessType::IMPLICIT_ACCESS);
    password_manager::PasswordStoreResultsObserver profile_syncer;
    profile_password_store->GetAllLoginsWithAffiliationAndBrandingInformation(
        profile_syncer.GetWeakPtr());
    profile_syncer.WaitForResults();

    scoped_refptr<password_manager::PasswordStoreInterface>
        account_password_store = AccountPasswordStoreFactory::GetForProfile(
            GetProfile(), ServiceAccessType::IMPLICIT_ACCESS);
    if (account_password_store) {
      password_manager::PasswordStoreResultsObserver account_syncer;
      account_password_store->GetAllLoginsWithAffiliationAndBrandingInformation(
          account_syncer.GetWeakPtr());
      account_syncer.WaitForResults();
    }
  }

 private:
  autofill::test::AutofillBrowserTestEnvironment environment_;
  net::EmbeddedTestServer https_server_;
};

class PasswordManagerAndroidBrowserTest
    : public PasswordManagerAndroidBrowserTestBase,
      public testing::WithParamInterface<bool> {};

IN_PROC_BROWSER_TEST_P(PasswordManagerAndroidBrowserTest,
                       TriggerFormSubmission) {
  base::HistogramTester uma_recorder;

  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          GetProfile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();

  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = base_url().spec();
  signin_form.url = base_url();
  signin_form.action = base_url();
  signin_form.username_value = u"username";
  signin_form.password_value = u"password";
  password_store->AddLogin(password_manager::FromPasswordForm(signin_form));
  WaitForPasswordStores();

  bool has_form_tag = GetParam();
  NavigateToFile(has_form_tag ? "/password/simple_password.html"
                              : "/password/no_form_element.html");

  password_manager::ContentPasswordManagerDriver* driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          GetActiveWebContents()->GetPrimaryMainFrame());

  // There should be only one form with two fields in the test html.
  ASSERT_EQ(static_cast<const password_manager::PasswordManager*>(
                driver->GetPasswordManager())
                ->form_managers()
                .size(),
            1u);

  PasswordsNavigationObserver observer(GetActiveWebContents());
  observer.SetPathToWaitFor("/password/done.html");

  // A user taps the username field.
  ASSERT_TRUE(
      content::ExecJs(GetActiveWebContents(),
                      "document.getElementById('username_field').focus();"));

  // Because on some simulator bots, renderer may take longer time to finish
  // the "focus()" call.
  content::MainThreadFrameObserver frame_observer(
      GetActiveWebContents()->GetRenderWidgetHostView()->GetRenderWidgetHost());
  frame_observer.Wait();

  // A user accepts a credential in TouchToFill. That fills in the credential
  // and submits it.
  base::test::TestFuture<bool> completion_future;
  driver->FillSuggestion(u"username", u"password",
                         completion_future.GetCallback());
  ASSERT_TRUE(completion_future.Wait());

  // TouchToFill tracking starts after filling the form.
  ChromePasswordManagerClient::FromWebContents(GetActiveWebContents())
      ->StartSubmissionTrackingAfterTouchToFill(u"username");

  driver->TriggerFormSubmission();

  ASSERT_TRUE(observer.Wait());

  // Wait for the histogram to be ready to reduce flakiness.
  WaitForHistogram("PasswordManager.TouchToFill.TimeToSuccessfulLogin",
                   uma_recorder);
  uma_recorder.ExpectTotalCount(
      "PasswordManager.TouchToFill.TimeToSuccessfulLogin", 1);
}

// Tests that manual password generation can be triggered on the text field if
// field's name contains "password".
IN_PROC_BROWSER_TEST_P(PasswordManagerAndroidBrowserTest,
                       TriggerPasswordGenerationOnTextField) {
  password_manager::PasswordFormManager::
      set_wait_for_server_predictions_for_filling(false);

  NavigateToFile("/password/sign_in_with_password_type_text.html");

  password_manager::ContentPasswordManagerDriver* driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          GetActiveWebContents()->GetPrimaryMainFrame());

  // After parsing, text field is considered as manual generation eligible
  // field.
  EXPECT_TRUE(base::test::RunUntil([driver]() {
    return driver->GetPasswordGenerationHelper()
               ->GenerationEnabledFieldsForTests()
               .size() == 1;
  }));
  const base::flat_set<autofill::FieldRendererId>& generation_enabled_fields =
      driver->GetPasswordGenerationHelper()->GenerationEnabledFieldsForTests();
  autofill::FieldRendererId password_field_renderer_id =
      *generation_enabled_fields.begin();

  // A user taps on the password field. JS call updates the last focused field
  // for `PasswordAutofillAgent`. `PasswordGenerationAgent` uses the last
  // focused field info to fill the manually generated password.
  // TODO: crbug.com/372635030 - Replace JS script and get rid of with
  // `ChromePasswordManagerClient::FocusedInputChanged` once
  // `SimulateMouseClickOrTapElementWithId` call starts creation of keyboard
  // accessory.
  ASSERT_TRUE(
      content::ExecJs(GetActiveWebContents(),
                      "document.getElementById('password_field').focus();"));
  // Wait because on some emulator bots the renderer may take longer time to
  // finish the "focus()" call.
  content::MainThreadFrameObserver frame_observer(
      GetActiveWebContents()->GetRenderWidgetHostView()->GetRenderWidgetHost());
  frame_observer.Wait();
  ChromePasswordManagerClient::FromWebContents(GetActiveWebContents())
      ->FocusedInputChanged(
          driver, password_field_renderer_id,
          autofill::mojom::FocusedFieldType::kFillableNonSearchField);
  // User generates the password manually.
  PasswordAccessoryController* password_accessory =
      PasswordAccessoryController::GetIfExisting(GetActiveWebContents());
  ASSERT_TRUE(password_accessory);
  password_accessory->OnOptionSelected(
      autofill::AccessoryAction::GENERATE_PASSWORD_MANUAL);

  PasswordGenerationController* password_generation_controller =
      PasswordGenerationController::GetIfExisting(GetActiveWebContents());

  // Wait till password generation bottomsheet is shown to the user.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return password_generation_controller
        ->GetTouchToFillGenerationControllerForTesting();
  }));
  TouchToFillPasswordGenerationController* touch_to_fill_generation_controller =
      password_generation_controller
          ->GetTouchToFillGenerationControllerForTesting();

  touch_to_fill_generation_controller->OnGeneratedPasswordAccepted(u"Password");

  EXPECT_TRUE(
      content::EvalJs(
          GetActiveWebContents(),
          "document.getElementById('password_field').value === \"Password\"")
          .ExtractBool());
}

class PasswordManagerEnterpriseReportingAndroidBrowserTest
    : public PasswordManagerAndroidBrowserTestBase {
 public:
  PasswordManagerEnterpriseReportingAndroidBrowserTest() {
    storage_.SetEnrollmentToken(kEnrollmentToken);
    storage_.SetClientId(kClientId);

    reporting_environment_ =
        enterprise_connectors::test::RealtimeReportingTestEnvironment::Create(
            /*enabled_event_names=*/{"loginEvent"},
            /*enabled_opt_in_events=*/{{"loginEvent", {"*"}}});
    CHECK(reporting_environment_);
  }

  void SetUpInProcessBrowserTestFixture() override {
    PasswordManagerAndroidBrowserTestBase::SetUpInProcessBrowserTestFixture();
    policy::ChromeBrowserPolicyConnector::EnableCommandLineSupportForTesting();
  }

  void SetUp() override {
    ASSERT_TRUE(reporting_environment_->Start());
    PasswordManagerAndroidBrowserTestBase::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PasswordManagerAndroidBrowserTestBase::SetUpCommandLine(command_line);
    base::CommandLine reporting_command_line =
        base::CommandLine::FromArgvWithoutProgram(
            reporting_environment_->GetArguments());
    command_line->AppendArguments(reporting_command_line,
                                  /*include_program=*/false);
    command_line->AppendSwitchASCII(
        "policy", base::StrCat({"{\"", kEnrollmentTokenPolicyName, "\":\"",
                                kEnrollmentToken, "\"}"}));
  }

  enterprise_connectors::test::RealtimeReportingTestServer* reporting_server() {
    return reporting_environment_->reporting_server();
  }

 private:
  policy::FakeBrowserDMTokenStorage storage_;
  std::unique_ptr<enterprise_connectors::test::RealtimeReportingTestEnvironment>
      reporting_environment_;
};

IN_PROC_BROWSER_TEST_F(PasswordManagerEnterpriseReportingAndroidBrowserTest,
                       LoginEventReported) {
  base::HistogramTester uma_recorder;

  NavigateToFile("/password/simple_password.html");

  PasswordsNavigationObserver observer(GetActiveWebContents());
  observer.SetPathToWaitFor("/password/done.html");

  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'user@domain.com';"
      "document.getElementById('password_field').value = 'password';"
      "document.getElementById('input_submit_button').click();";
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());

  WaitForHistogram("Enterprise.ReportingEventUploadSuccess", uma_recorder);
  uma_recorder.ExpectUniqueSample(
      "Enterprise.ReportingEventUploadSuccess",
      enterprise_connectors::EnterpriseReportingEventType::kLoginEvent, 1);
  uma_recorder.ExpectTotalCount("Enterprise.ReportingEventUploadFailure", 0);

  auto reports = reporting_server()->GetUploadedReports();
  ASSERT_EQ(1u, reports.size());
  EXPECT_EQ("Android", reports[0].device().os_platform());
  ASSERT_EQ(1, reports[0].events_size());
  EXPECT_EQ(reports[0].events(0).login_event().url(),
            GetURL("/password/simple_password.html").spec());
  EXPECT_TRUE(base::EndsWith(
      reports[0].events(0).login_event().login_user_name(), "@domain.com"));
}

INSTANTIATE_TEST_SUITE_P(VariateFormElementPresence,
                         PasswordManagerAndroidBrowserTest,
                         testing::Bool());
