// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/password_manager_uitest_util.h"
#include "chrome/browser/password_manager/passwords_navigation_observer.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom-test-utils.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"
#include "components/password_manager/core/browser/http_auth_manager.h"
#include "components/password_manager/core/browser/http_auth_observer.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/password_manager/password_manager_signin_intercept_test_helper.h"
#include "chrome/browser/signin/dice_web_signin_interceptor.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#endif  // BUIDLFLAG(ENABLE_DICE_SUPPORT)

using autofill::ParsingResult;
using autofill::test::CreateFieldPrediction;
using base::ASCIIToUTF16;
using base::Feature;
using testing::_;
using testing::ElementsAre;
using testing::Field;
using testing::Pair;
using testing::SizeIs;

namespace password_manager {
namespace {

class PasswordManagerBrowserTest : public PasswordManagerBrowserTestBase {
 public:
  PasswordManagerBrowserTest() {
    // Turn off waiting for server predictions before filing. It makes filling
    // behaviour more deterministic. Filling with server predictions is tested
    // in PasswordFormManager unit tests.
    password_manager::PasswordFormManager::
        set_wait_for_server_predictions_for_filling(false);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PasswordManagerBrowserTestBase::SetUpCommandLine(command_line);

    // For the password form to be treated as the Gaia signin page.
    command_line->AppendSwitchASCII(
        switches::kGaiaUrl,
        https_test_server().GetURL("accounts.google.com", "/").spec());
  }

  ~PasswordManagerBrowserTest() override = default;
};

// A test fixture that injects an `ObservingAutofillClient` into newly created
// tabs to allow waiting for an Autofill popup to open.
class PasswordManagerAutofillPopupBrowserTest
    : public PasswordManagerBrowserTest {
 protected:
  ObservingAutofillClient& autofill_client() {
    return *autofill_client_injector_[WebContents()];
  }

 private:
  autofill::TestAutofillClientInjector<ObservingAutofillClient>
      autofill_client_injector_;
};

// This fixture enables communication to the Autofill crowdsourcing server, but
// denies any such requests.
class PasswordManagerVotingBrowserTest : public PasswordManagerBrowserTest {
 public:
  void SetUpOnMainThread() override {
    PasswordManagerBrowserTest::SetUpOnMainThread();
    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](content::URLLoaderInterceptor::RequestParams* params) {
              bool is_autofill_request =
                  params->url_request.url.spec().find(
                      "https://content-autofill.googleapis.com/") !=
                  std::string::npos;
              return is_autofill_request;
            }));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    PasswordManagerBrowserTest::TearDownOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      autofill::features::test::kAutofillServerCommunication};
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

// Test class for testing password manager with the BackForwardCache feature
// enabled. More info about the BackForwardCache, see:
// http://doc/1YrBKX_eFMA9KoYof-eVThT35jcTqWcH_rRxYbR5RapU
class PasswordManagerBackForwardCacheBrowserTest
    : public PasswordManagerBrowserTest {
 public:
  void SetUpOnMainThread() override {
    // TODO(crbug.com/40737060): Remove this and below after confirming
    // whether setup is completing.
    LOG(INFO) << "SetUpOnMainThread started.";
    host_resolver()->AddRule("*", "127.0.0.1");
    PasswordManagerBrowserTest ::SetUpOnMainThread();
    LOG(INFO) << "SetUpOnMainThread complete.";
  }

  bool IsGetCredentialsSuccessful() {
    return "success" ==
           content::EvalJs(WebContents()->GetPrimaryMainFrame(), R"(
      new Promise(resolve => {
        navigator.credentials.get({password: true, unmediated: true })
          .then(m => { resolve("success"); })
          .catch(()=> { resolve("error"); });
        });
    )");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
    PasswordManagerBrowserTest::SetUpCommandLine(command_line);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class MockHttpAuthObserver : public password_manager::HttpAuthObserver {
 public:
  MOCK_METHOD(void,
              OnAutofillDataAvailable,
              (const std::u16string& username, const std::u16string& password),
              (override));

  MOCK_METHOD(void, OnLoginModelDestroying, (), (override));
};

GURL GetFileURL(const char* filename) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("password").AppendASCII(filename);
  CHECK(base::PathExists(path));
  return net::FilePathToFileURL(path);
}

// Handles |request| to "/basic_auth". If "Authorization" header is present,
// responds with a non-empty HTTP 200 page (regardless of its value). Otherwise
// serves a Basic Auth challenge.
std::unique_ptr<net::test_server::HttpResponse> HandleTestAuthRequest(
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(request.relative_url, "/basic_auth",
                        base::CompareCase::SENSITIVE)) {
    return nullptr;
  }
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  if (base::Contains(request.headers, "Authorization")) {
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("Success!");
  } else {
    http_response->set_code(net::HTTP_UNAUTHORIZED);
    std::string realm = base::EndsWith(request.relative_url, "/empty_realm",
                                       base::CompareCase::SENSITIVE)
                            ? "\"\""
                            : "\"test realm\"";
    http_response->AddCustomHeader("WWW-Authenticate", "Basic realm=" + realm);
  }
  return http_response;
}

void TestPromptNotShown(const char* failure_message,
                        content::WebContents* web_contents) {
  SCOPED_TRACE(testing::Message(failure_message));

  PasswordsNavigationObserver observer(web_contents);
  std::string fill_and_submit =
      "document.getElementById('username_failed').value = 'temp';"
      "document.getElementById('password_failed').value = 'random';"
      "document.getElementById('failed_form').submit()";

  ASSERT_TRUE(content::ExecJs(web_contents, fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_FALSE(BubbleObserver(web_contents).IsSavePromptShownAutomatically());
}

// Generate HTML for a simple password form with the specified action URL.
std::string GeneratePasswordFormForAction(const GURL& action_url) {
  return "<form method='POST' action='" + action_url.spec() +
         "'"
         "      onsubmit='return true;' id='testform'>"
         "  <input type='password' id='password_field'>"
         "</form>";
}

// Inject an about:blank frame with a password form that uses the specified
// action URL into |web_contents|.
void InjectBlankFrameWithPasswordForm(content::WebContents* web_contents,
                                      const GURL& action_url) {
  std::string form_html = GeneratePasswordFormForAction(action_url);
  std::string inject_blank_frame_with_password_form =
      "var frame = document.createElement('iframe');"
      "frame.id = 'iframe';"
      "document.body.appendChild(frame);"
      "frame.contentDocument.body.innerHTML = \"" +
      form_html + "\"";
  ASSERT_TRUE(
      content::ExecJs(web_contents, inject_blank_frame_with_password_form));
}

// Inject an iframe with a password form that uses the specified action URL into
// |web_contents|.
void InjectFrameWithPasswordForm(content::WebContents* web_contents,
                                 const GURL& action_url) {
  std::string form_html = GeneratePasswordFormForAction(action_url);
  std::string inject_blank_frame_with_password_form =
      "var ifr = document.createElement('iframe');"
      "ifr.setAttribute('id', 'iframeResult');"
      "document.body.appendChild(ifr);"
      "ifr.contentWindow.document.open();"
      "ifr.contentWindow.document.write(\"" +
      form_html +
      "\");"
      "ifr.contentWindow.document.close();";
  ASSERT_TRUE(
      content::ExecJs(web_contents, inject_blank_frame_with_password_form));
}

// Fills in a fake password and submits the form in |frame|, waiting for the
// submit navigation to finish.  |action_url| is the form action URL to wait
// for.
void SubmitInjectedPasswordForm(content::WebContents* web_contents,
                                content::RenderFrameHost* frame,
                                const GURL& action_url) {
  std::string submit_form =
      "document.getElementById('password_field').value = 'pa55w0rd';"
      "document.getElementById('testform').submit();";
  PasswordsNavigationObserver observer(web_contents);
  observer.SetPathToWaitFor(action_url.path());
  ASSERT_TRUE(content::ExecJs(frame, submit_form));
  ASSERT_TRUE(observer.Wait());
}

void SetUrlAsTrustworthy(const std::string& url) {
  std::vector<std::string> rejected_patterns;
  network::SecureOriginAllowlist::GetInstance().SetAuxiliaryAllowlist(
      url, &rejected_patterns);
  // Check that the url was not rejected.
  EXPECT_THAT(rejected_patterns, testing::IsEmpty());
}

// Actual tests ---------------------------------------------------------------

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, PromptForNormalSubmit) {
  NavigateToFile("/password/password_form.html");

  // Fill a form and submit through a <input type="submit"> button. Nothing
  // special.
  PasswordsNavigationObserver observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());

  // Save the password and check the store.
  BubbleObserver bubble_observer(WebContents());
  EXPECT_TRUE(bubble_observer.IsSavePromptShownAutomatically());
  bubble_observer.AcceptSavePrompt();
  WaitForPasswordStore();

  CheckThatCredentialsStored("temp", "random");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, NoPromptIfFormReappeared) {
  NavigateToFile("/password/failed.html");
  TestPromptNotShown("normal form", WebContents());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptIfChangePasswordFormReappearedEmpty) {
  NavigateToFile("/password/update_form_empty_fields.html");
  // Fill a form and submit through a <input type="submit"> button. Nothing
  // special.
  PasswordsNavigationObserver observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('password').value = 'old_pass';"
      "document.getElementById('new_password_1').value = 'new_pass';"
      "document.getElementById('new_password_2').value = 'new_pass';"
      "document.getElementById('chg_submit_wo_username_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(BubbleObserver(WebContents()).IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       NoPromptIfFormReappearedWithPartsHidden) {
  NavigateToFile("/password/failed_partly_visible.html");
  TestPromptNotShown("partly visible form", WebContents());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       NoPromptIfFormReappearedInputOutsideFor) {
  NavigateToFile("/password/failed_input_outside.html");
  TestPromptNotShown("form with input outside", WebContents());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       NoPromptAfterCredentialsAPIPasswordStore) {
  NavigateToFile("/password/password_form.html");
  // Simulate the Credential Management API function store() is called and
  // PasswordManager instance is notified about that.
  ChromePasswordManagerClient::FromWebContents(WebContents())
      ->NotifyStorePasswordCalled();

  // Fill a form and submit through a <input type="submit"> button. The
  // renderer should not send "PasswordFormsParsed" messages after the page
  // was loaded.
  PasswordsNavigationObserver observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  BubbleObserver prompt_observer(WebContents());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForSubmitWithSameDocumentNavigation) {
  NavigateToFile("/password/password_navigate_before_submit.html");

  // Fill a form and submit through a <input type="submit"> button. Nothing
  // special. The form does an in-page navigation before submitting.
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       LoginSuccessWithUnrelatedForm) {
  // Log in, see a form on the landing page. That form is not related to the
  // login form (=has different input fields), so we should offer saving the
  // password.
  NavigateToFile("/password/password_form.html");

  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_unrelated').value = 'temp';"
      "document.getElementById('password_unrelated').value = 'random';"
      "document.getElementById('submit_unrelated').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, LoginFailed) {
  NavigateToFile("/password/password_form.html");

  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_failed').value = 'temp';"
      "document.getElementById('password_failed').value = 'random';"
      "document.getElementById('submit_failed').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForSubmitUsingJavaScript) {
  NavigateToFile("/password/password_form.html");

  // Fill a form and submit using <button> that calls submit() on the form.
  // This should work regardless of the type of element, as long as submit() is
  // called.
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, PromptForDynamicForm) {
  // Adding a PSL matching form is a workaround explained later.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  GURL psl_orogin = embedded_test_server()->GetURL("psl.example.com", "/");
  signin_form.signon_realm = psl_orogin.spec();
  signin_form.url = psl_orogin;
  signin_form.username_value = u"unused_username";
  signin_form.password_value = u"unused_password";
  password_store->AddLogin(signin_form);

  // Show the dynamic form.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "example.com", "/password/dynamic_password_form.html")));
  ASSERT_TRUE(content::ExecJs(
      WebContents(), "document.getElementById('create_form_button').click();"));

  // Blink has a timer for 0.3 seconds before it updates the browser with the
  // new dynamic form. We wait for the form being detected by observing the UI
  // state. The state changes due to the matching credential saved above. Later
  // the form submission is definitely noticed by the browser.
  BubbleObserver(WebContents()).WaitForManagementState();

  // Fill the dynamic password form and submit.
  PasswordsNavigationObserver observer(WebContents());
  std::string fill_and_submit =
      "document.dynamic_form.username.value = 'tempro';"
      "document.dynamic_form.password.value = 'random';"
      "document.dynamic_form.submit()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());

  EXPECT_TRUE(BubbleObserver(WebContents()).IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, NoPromptForNavigation) {
  NavigateToFile("/password/password_form.html");

  // Don't fill the password form, just navigate away. Shouldn't prompt.
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  ASSERT_TRUE(content::ExecJs(RenderFrameHost(),
                              "window.location.href = 'done.html';",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  ASSERT_TRUE(observer.Wait());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       NoPromptForSubFrameNavigation) {
  NavigateToFile("/password/multi_frames.html");

  // If you are filling out a password form in one frame and a different frame
  // navigates, this should not trigger the infobar.
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  observer.SetPathToWaitFor("/password/done.html");
  std::string fill =
      "var first_frame = document.getElementById('first_frame');"
      "var frame_doc = first_frame.contentDocument;"
      "frame_doc.getElementById('username_field').value = 'temp';"
      "frame_doc.getElementById('password_field').value = 'random';";
  std::string navigate_frame =
      "var second_iframe = document.getElementById('second_frame');"
      "second_iframe.contentWindow.location.href = 'done.html';";

  ASSERT_TRUE(content::ExecJs(WebContents(), fill));
  ASSERT_TRUE(content::ExecJs(WebContents(), navigate_frame));
  ASSERT_TRUE(observer.Wait());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       NoPromptForSameFormWithDifferentAction) {
  // Log in, see a form on the landing page. That form is related to the login
  // form (has a different action but has same input fields), so we should not
  // offer saving the password.
  NavigateToFile("/password/password_form.html");

  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_different_action').value = 'temp';"
      "document.getElementById('password_different_action').value = 'random';"
      "document.getElementById('submit_different_action').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, NoPromptForActionMutation) {
  NavigateToFile("/password/password_form_action_mutation.html");

  // Need to pay attention for a message that XHR has finished since there
  // is no navigation to wait for.
  content::DOMMessageQueue message_queue(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_action_mutation').value = 'temp';"
      "document.getElementById('password_action_mutation').value = 'random';"
      "document.getElementById('submit_action_mutation').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  std::string message;
  while (message_queue.WaitForMessage(&message)) {
    if (message == "\"XHR_FINISHED\"") {
      break;
    }
  }
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       NoPromptForFormWithEnteredUsername) {
  // Log in, see a form on the landing page. That form is not related to the
  // login form but has the same username as was entered previously, so we
  // should not offer saving the password.
  NavigateToFile("/password/password_form.html");

  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_contains_username').value = 'temp';"
      "document.getElementById('password_contains_username').value = 'random';"
      "document.getElementById('submit_contains_username').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForDifferentFormWithEmptyAction) {
  // Log in, see a form on the landing page. That form is not related to the
  // signin form. The signin and the form on the landing page have empty
  // actions, so we should offer saving the password.
  NavigateToFile("/password/navigate_to_same_url_empty_actions.html");

  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username').value = 'temp';"
      "document.getElementById('password').value = 'random';"
      "document.getElementById('submit-button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptAfterSubmitWithSubFrameNavigation) {
  NavigateToFile("/password/multi_frames.html");

  // Make sure that we prompt to save password even if a sub-frame navigation
  // happens first.
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  observer.SetPathToWaitFor("/password/done.html");
  std::string navigate_frame =
      "var second_iframe = document.getElementById('second_frame');"
      "second_iframe.contentWindow.location.href = 'other.html';";
  std::string fill_and_submit =
      "var first_frame = document.getElementById('first_frame');"
      "var frame_doc = first_frame.contentDocument;"
      "frame_doc.getElementById('username_field').value = 'temp';"
      "frame_doc.getElementById('password_field').value = 'random';"
      "frame_doc.getElementById('input_submit_button').click();";

  ASSERT_TRUE(content::ExecJs(WebContents(), navigate_frame));
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(
    PasswordManagerBrowserTest,
    NoPromptForFailedLoginFromMainFrameWithMultiFramesSameDocument) {
  NavigateToFile("/password/multi_frames.html");

  // Make sure that we don't prompt to save the password for a failed login
  // from the main frame with multiple frames in the same page.
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_failed').value = 'temp';"
      "document.getElementById('password_failed').value = 'random';"
      "document.getElementById('submit_failed').click();";

  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(
    PasswordManagerBrowserTest,
    NoPromptForFailedLoginFromSubFrameWithMultiFramesSameDocument) {
  NavigateToFile("/password/multi_frames.html");

  // Make sure that we don't prompt to save the password for a failed login
  // from a sub-frame with multiple frames in the same page.
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "var first_frame = document.getElementById('first_frame');"
      "var frame_doc = first_frame.contentDocument;"
      "frame_doc.getElementById('username_failed').value = 'temp';"
      "frame_doc.getElementById('password_failed').value = 'random';"
      "frame_doc.getElementById('submit_failed').click();";

  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  observer.SetPathToWaitFor("/password/failed.html");
  ASSERT_TRUE(observer.Wait());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, PromptForXHRSubmit) {
  NavigateToFile("/password/password_xhr_submit.html");

  // Verify that we show the save password prompt if a form returns false
  // in its onsubmit handler but instead logs in/navigates via XHR.
  // Note that calling 'submit()' on a form with javascript doesn't call
  // the onsubmit handler, so we click the submit button instead.
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForXHRSubmitWithoutNavigation) {
  NavigateToFile("/password/password_xhr_submit.html");

  // Need to pay attention for a message that XHR has finished since there
  // is no navigation to wait for.
  content::DOMMessageQueue message_queue(WebContents());

  // Verify that if XHR without navigation occurs and the form has been filled
  // out we try and save the password. Note that in general the submission
  // doesn't need to be via form.submit(), but for testing purposes it's
  // necessary since we otherwise ignore changes made to the value of these
  // fields by script.
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "navigate = false;"
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('submit_button').click();";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  std::string message;
  while (message_queue.WaitForMessage(&message)) {
    if (message == "\"XHR_FINISHED\"") {
      break;
    }
  }

  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForXHRSubmitWithoutNavigation_SignupForm) {
  NavigateToFile("/password/password_xhr_submit.html");

  // Need to pay attention for a message that XHR has finished since there
  // is no navigation to wait for.
  content::DOMMessageQueue message_queue(WebContents());

  // Verify that if XHR without navigation occurs and the form has been filled
  // out we try and save the password. Note that in general the submission
  // doesn't need to be via form.submit(), but for testing purposes it's
  // necessary since we otherwise ignore changes made to the value of these
  // fields by script.
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "navigate = false;"
      "document.getElementById('signup_username_field').value = 'temp';"
      "document.getElementById('signup_password_field').value = 'random';"
      "document.getElementById('confirmation_password_field').value = 'random';"
      "document.getElementById('signup_submit_button').click();";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  std::string message;
  while (message_queue.WaitForMessage(&message)) {
    if (message == "\"XHR_FINISHED\"") {
      break;
    }
  }

  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       NoPromptForXHRSubmitWithoutNavigationWithUnfilledForm) {
  NavigateToFile("/password/password_xhr_submit.html");

  // Need to pay attention for a message that XHR has finished since there
  // is no navigation to wait for.
  content::DOMMessageQueue message_queue(WebContents());

  // Verify that if XHR without navigation occurs and the form has NOT been
  // filled out we don't prompt.
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "navigate = false;"
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('submit_button').click();";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  std::string message;
  while (message_queue.WaitForMessage(&message)) {
    if (message == "\"XHR_FINISHED\"") {
      break;
    }
  }

  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(
    PasswordManagerBrowserTest,
    NoPromptForXHRSubmitWithoutNavigationWithUnfilledForm_SignupForm) {
  NavigateToFile("/password/password_xhr_submit.html");

  // Need to pay attention for a message that XHR has finished since there
  // is no navigation to wait for.
  content::DOMMessageQueue message_queue(WebContents());

  // Verify that if XHR without navigation occurs and the form has NOT been
  // filled out we don't prompt.
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "navigate = false;"
      "document.getElementById('signup_username_field').value = 'temp';"
      "document.getElementById('signup_submit_button').click();";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  std::string message;
  while (message_queue.WaitForMessage(&message)) {
    if (message == "\"XHR_FINISHED\"") {
      break;
    }
  }

  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, PromptForFetchSubmit) {
  NavigateToFile("/password/password_fetch_submit.html");

  // Verify that we show the save password prompt if a form returns false
  // in its onsubmit handler but instead logs in/navigates via Fetch.
  // Note that calling 'submit()' on a form with javascript doesn't call
  // the onsubmit handler, so we click the submit button instead.
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForFetchSubmitWithoutNavigation) {
  NavigateToFile("/password/password_fetch_submit.html");

  // Need to pay attention for a message that XHR has finished since there
  // is no navigation to wait for.
  content::DOMMessageQueue message_queue(WebContents());

  // Verify that if XHR without navigation occurs and the form has been filled
  // out we try and save the password. Note that in general the submission
  // doesn't need to be via form.submit(), but for testing purposes it's
  // necessary since we otherwise ignore changes made to the value of these
  // fields by script.
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "navigate = false;"
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('submit_button').click();";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  // This forces layout update.
  RunUntilInputProcessed(RenderFrameHost()->GetRenderWidgetHost());

  std::string message;
  while (message_queue.WaitForMessage(&message)) {
    if (message == "\"FETCH_FINISHED\"") {
      break;
    }
  }
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForFetchSubmitWithoutNavigation_SignupForm) {
  NavigateToFile("/password/password_fetch_submit.html");

  // Need to pay attention for a message that Fetch has finished since there
  // is no navigation to wait for.
  content::DOMMessageQueue message_queue(WebContents());

  // Verify that if Fetch without navigation occurs and the form has been filled
  // out we try and save the password. Note that in general the submission
  // doesn't need to be via form.submit(), but for testing purposes it's
  // necessary since we otherwise ignore changes made to the value of these
  // fields by script.
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "navigate = false;"
      "document.getElementById('signup_username_field').value = 'temp';"
      "document.getElementById('signup_password_field').value = 'random';"
      "document.getElementById('confirmation_password_field').value = 'random';"
      "document.getElementById('signup_submit_button').click();";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  // This forces layout update.
  RunUntilInputProcessed(RenderFrameHost()->GetRenderWidgetHost());
  std::string message;

  while (message_queue.WaitForMessage(&message)) {
    if (message == "\"FETCH_FINISHED\"") {
      break;
    }
  }
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(
    PasswordManagerBrowserTest,
    NoPromptForFetchSubmitWithoutNavigationWithUnfilledForm) {
  NavigateToFile("/password/password_fetch_submit.html");

  // Need to pay attention for a message that Fetch has finished since there
  // is no navigation to wait for.
  content::DOMMessageQueue message_queue(WebContents());

  // Verify that if Fetch without navigation occurs and the form has NOT been
  // filled out we don't prompt.
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "navigate = false;"
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('submit_button').click();";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  // This forces layout update.
  RunUntilInputProcessed(RenderFrameHost()->GetRenderWidgetHost());

  std::string message;
  while (message_queue.WaitForMessage(&message)) {
    if (message == "\"FETCH_FINISHED\"") {
      break;
    }
  }
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(
    PasswordManagerBrowserTest,
    NoPromptForFetchSubmitWithoutNavigationWithUnfilledForm_SignupForm) {
  NavigateToFile("/password/password_fetch_submit.html");

  // Need to pay attention for a message that Fetch has finished since there
  // is no navigation to wait for.
  content::DOMMessageQueue message_queue(WebContents());

  // Verify that if Fetch without navigation occurs and the form has NOT been
  // filled out we don't prompt.
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "navigate = false;"
      "document.getElementById('signup_username_field').value = 'temp';"
      "document.getElementById('signup_submit_button').click();";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  // This forces layout update.
  RunUntilInputProcessed(RenderFrameHost()->GetRenderWidgetHost());

  std::string message;
  while (message_queue.WaitForMessage(&message)) {
    if (message == "\"FETCH_FINISHED\"") {
      break;
    }
  }
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, NoPromptIfLinkClicked) {
  NavigateToFile("/password/password_form.html");

  // Verify that if the user takes a direct action to leave the page, we don't
  // prompt to save the password even if the form is already filled out.
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_click_link =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('link').click();";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_click_link));
  ASSERT_TRUE(observer.Wait());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerVotingBrowserTest,
                       VerifyPasswordGenerationUpload) {
  // The form should not be hosted on localhost to enable sending
  // crowdsourcing votes.
  const std::string kTestSignonRealm = "example.com";
  // This fixture is needed to allow password filling on page load.
  SetUrlAsTrustworthy(
      embedded_test_server()->GetURL(kTestSignonRealm, "/").spec());

  // Visit a signup form.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kTestSignonRealm,
                                                "/password/signup_form.html")));

  // Enter a password and save it.
  PasswordsNavigationObserver first_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('other_info').value = 'stuff';"
      "document.getElementById('username_field').value = 'my_username';"
      "document.getElementById('password_field').value = 'password';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));

  ASSERT_TRUE(first_observer.Wait());
  {
    base::HistogramTester histograms;
    BubbleObserver prompt_observer(WebContents());
    EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
    prompt_observer.AcceptSavePrompt();
    // One vote on saving a password.
    histograms.ExpectUniqueSample("Autofill.UploadEvent", 1, 1);
  }

  // Now navigate to a login form that has similar HTML markup.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     kTestSignonRealm, "/password/password_form.html")));

  // Simulate a user click to force an autofill of the form's DOM value, not
  // just the suggested value.
  content::SimulateMouseClick(WebContents(), 0,
                              blink::WebMouseEvent::Button::kLeft);
  WaitForElementValue("username_field", "my_username");
  WaitForElementValue("password_field", "password");

  // Submit the form and verify that there is no infobar (as the password
  // has already been saved).
  PasswordsNavigationObserver second_observer(WebContents());
  BubbleObserver second_prompt_observer(WebContents());
  std::string submit_form =
      "document.getElementById('input_submit_button').click()";
  {
    base::HistogramTester histograms;
    ASSERT_TRUE(content::ExecJs(WebContents(), submit_form));
    ASSERT_TRUE(second_observer.Wait());
    // One vote for credential reuse, one vote for first time login.
    histograms.ExpectUniqueSample("Autofill.UploadEvent", 1, 2);
  }
  EXPECT_FALSE(second_prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, PromptForSubmitFromIframe) {
  NavigateToFile("/password/password_submit_from_iframe.html");

  // Submit a form in an iframe, then cause the whole page to navigate without a
  // user gesture. We expect the save password prompt to be shown here, because
  // some pages use such iframes for login forms.
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "var iframe = document.getElementById('test_iframe');"
      "var iframe_doc = iframe.contentDocument;"
      "iframe_doc.getElementById('username_field').value = 'temp';"
      "iframe_doc.getElementById('password_field').value = 'random';"
      "iframe_doc.getElementById('submit_button').click()";

  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForInputElementWithoutName) {
  // Check that the prompt is shown for forms where input elements lack the
  // "name" attribute but the "id" is present.
  NavigateToFile("/password/password_form.html");

  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field_no_name').value = 'temp';"
      "document.getElementById('password_field_no_name').value = 'random';"
      "document.getElementById('input_submit_button_no_name').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForInputElementWithoutId) {
  // Check that the prompt is shown for forms where input elements lack the
  // "id" attribute but the "name" attribute is present.
  NavigateToFile("/password/password_form.html");

  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementsByName('username_field_no_id')[0].value = 'temp';"
      "document.getElementsByName('password_field_no_id')[0].value = 'random';"
      "document.getElementsByName('input_submit_button_no_id')[0].click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForInputElementWithoutIdAndName) {
  // Check that prompt is shown for forms where the input fields lack both
  // the "id" and the "name" attributes.
  NavigateToFile("/password/password_form.html");

  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "var form = document.getElementById('testform_elements_no_id_no_name');"
      "var username = form.children[0];"
      "username.value = 'temp';"
      "var password = form.children[1];"
      "password.value = 'random';"
      "form.children[2].click()";  // form.children[2] is the submit button.
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
  prompt_observer.AcceptSavePrompt();

  // Check that credentials are stored.
  WaitForPasswordStore();
  CheckThatCredentialsStored("temp", "random");
}

// Test for checking that no prompt is shown for URLs with file: scheme.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, NoPromptForFileSchemeURLs) {
  GURL url = GetFileURL("password_form.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click();";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       NoPromptForLandingPageWithHTTPErrorStatusCode) {
  // Check that no prompt is shown for forms where the landing page has
  // HTTP status 404.
  NavigateToFile("/password/password_form.html");

  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field_http_error').value = 'temp';"
      "document.getElementById('password_field_http_error').value = 'random';"
      "document.getElementById('input_submit_button_http_error').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, DeleteFrameBeforeSubmit) {
  NavigateToFile("/password/multi_frames.html");

  PasswordsNavigationObserver observer(WebContents());
  // Make sure we save some password info from an iframe and then destroy it.
  std::string save_and_remove =
      "var first_frame = document.getElementById('first_frame');"
      "var frame_doc = first_frame.contentDocument;"
      "frame_doc.getElementById('username_field').value = 'temp';"
      "frame_doc.getElementById('password_field').value = 'random';"
      "frame_doc.getElementById('input_submit_button').click();"
      "first_frame.parentNode.removeChild(first_frame);";
  // Submit from the main frame, but without navigating through the onsubmit
  // handler.
  std::string navigate_frame =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click();"
      "window.location.href = 'done.html';";

  ASSERT_TRUE(content::ExecJs(WebContents(), save_and_remove));
  ASSERT_TRUE(content::ExecJs(WebContents(), navigate_frame));
  ASSERT_TRUE(observer.Wait());
  // The only thing we check here is that there is no use-after-free reported.
}

class PasswordManagerOverwritePlaceholderTest
    : public PasswordManagerBrowserTest {
 public:
  PasswordManagerOverwritePlaceholderTest() {
    feature_list_.InitAndEnableFeature(
        features::kEnableOverwritingPlaceholderUsernames);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that login page with prefilled credentials is overwritten if server
// classified the placeholder value.
IN_PROC_BROWSER_TEST_F(PasswordManagerOverwritePlaceholderTest,
                       FillIfServerPredictionSaysUsernameIsPlaceholder) {
  // Add credentials to the password store.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm saved_form;
  const std::string kTestSignonRealm =
      embedded_test_server()->GetURL("example.com", "/").spec();
  saved_form.signon_realm = kTestSignonRealm;
  const GURL kFormUrl = embedded_test_server()->GetURL(
      "example.com", "/password/prefilled_username.html");
  saved_form.url = kFormUrl;
  saved_form.username_value = u"saved_username";
  saved_form.password_value = u"saved_password";
  password_store->AddLogin(saved_form);

  // This fixture is needed to allow filling on page load.
  SetUrlAsTrustworthy(kTestSignonRealm);

  password_manager::PasswordFormManager::
      set_wait_for_server_predictions_for_filling(true);

  // User navigates to the page.
  // The form should not be hosted on localhost to enable using server
  // predictions.
  PasswordsNavigationObserver observer(WebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kFormUrl));
  ASSERT_TRUE(observer.Wait());

  // Create server predictions.
  autofill::AutofillType::ServerPrediction username_prediction;
  username_prediction.server_predictions = {
      autofill::test::CreateFieldPrediction(autofill::FieldType::USERNAME)};
  username_prediction.may_use_prefilled_placeholder = true;
  autofill::AutofillType::ServerPrediction password_prediction;
  password_prediction.server_predictions = {
      autofill::test::CreateFieldPrediction(autofill::FieldType::PASSWORD)};
  // Simulate password manager receiving server predictions.
  password_manager::ContentPasswordManagerDriver* driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          WebContents()->GetPrimaryMainFrame());
  autofill::FormData form_data(
      *static_cast<const password_manager::PasswordManager*>(
           driver->GetPasswordManager())
           ->form_managers()[0]
           ->observed_form());
  driver->GetPasswordManager()->ProcessAutofillPredictions(
      driver, form_data,
      {{form_data.fields()[0].global_id(), std::move(username_prediction)},
       {form_data.fields()[1].global_id(), std::move(password_prediction)}});

  // Password Manager will not autofill on page load until user interacted with
  // the page.
  CheckElementValue("username_field", "some_username");
  CheckElementValue("password_field", "some_password");
  // After user clicks on the webpage, password manager will fill the password
  // form.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));
  WaitForElementValue("username_field",
                      base::UTF16ToUTF8(saved_form.username_value));
  WaitForElementValue("password_field",
                      base::UTF16ToUTF8(saved_form.password_value));
}

// Tests that prefilled credentials were not overwritten since there is no
// signal that the values are placeholders.
IN_PROC_BROWSER_TEST_F(PasswordManagerOverwritePlaceholderTest,
                       NonPlaceholderPasswordNotOverwritten) {
  // Add credentials to the password store.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm saved_form;
  const std::string kTestSignonRealm =
      embedded_test_server()->GetURL("example.com", "/").spec();
  saved_form.signon_realm = kTestSignonRealm;
  const GURL kFormUrl = embedded_test_server()->GetURL(
      "example.com", "/password/prefilled_username.html");
  saved_form.url = kFormUrl;
  saved_form.username_value = u"saved_username";
  saved_form.password_value = u"saved_password";
  password_store->AddLogin(saved_form);

  // This fixture is needed to allow filling on page load.
  SetUrlAsTrustworthy(kTestSignonRealm);

  password_manager::PasswordFormManager::
      set_wait_for_server_predictions_for_filling(true);

  // User navigates to the page.
  PasswordsNavigationObserver observer(WebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kFormUrl));
  ASSERT_TRUE(observer.Wait());

  // Password Manager will not autofill on page load until user interacted with
  // the page.
  CheckElementValue("username_field", "some_username");
  CheckElementValue("password_field", "some_password");
  // After user clicks on the webpage, password manager will try to fill the
  // password form.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));
  // Password manager doesn't fill the page.
  WaitForElementValue("username_field", "some_username");
  WaitForElementValue("password_field", "some_password");
}

// Tests that well-known placeholder values in the username field are
// overwritten on page load.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       FillIfUsernameIsPlaceholder) {
  // Add credentials to the password store.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm saved_form;
  const std::string kTestSignonRealm =
      embedded_test_server()->GetURL("example.com", "/").spec();
  saved_form.signon_realm = kTestSignonRealm;
  const GURL kFormUrl = embedded_test_server()->GetURL(
      "example.com", "/password/placeholder_username.html");
  saved_form.url = kFormUrl;
  saved_form.username_value = u"saved_username";
  saved_form.password_value = u"saved_password";
  password_store->AddLogin(saved_form);

  // This fixture is needed to allow filling on page load.
  SetUrlAsTrustworthy(kTestSignonRealm);

  password_manager::PasswordFormManager::
      set_wait_for_server_predictions_for_filling(true);

  // User navigates to the page.
  PasswordsNavigationObserver observer(WebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kFormUrl));
  ASSERT_TRUE(observer.Wait());

  // Password Manager will not autofill on page load until user interacted with
  // the page.
  CheckElementValue("username_field", "email");
  CheckElementValue("password_field", "prefilled_password");
  // After user clicks on the webpage, password manager will fill the password
  // form.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));
  WaitForElementValue("username_field",
                      base::UTF16ToUTF8(saved_form.username_value));
  WaitForElementValue("password_field",
                      base::UTF16ToUTF8(saved_form.password_value));
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       UsernameAndPasswordValueAccessible) {
  // At first let us save a credential to the password store.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.url = embedded_test_server()->base_url();
  signin_form.action = embedded_test_server()->base_url();
  signin_form.username_value = u"admin";
  signin_form.password_value = u"12345";
  password_store->AddLogin(signin_form);

  // Steps from https://crbug.com/337429#c37.
  // Navigate to the page, click a link that opens a second tab, reload the
  // first tab and observe that the password is accessible.
  NavigateToFile("/password/form_and_link.html");

  // Click on a link to open a new tab, then switch back to the first one.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  std::string click = "document.getElementById('testlink').click();";
  ASSERT_TRUE(content::ExecJs(WebContents(), click));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  browser()->tab_strip_model()->ActivateTabAt(0);

  // Reload the original page to have the saved credentials autofilled.
  PasswordsNavigationObserver reload_observer(WebContents());
  NavigateToFile("/password/form_and_link.html");
  ASSERT_TRUE(reload_observer.Wait());

  // Now check that the username and the password are not accessible yet.
  CheckElementValue("username_field", "");
  CheckElementValue("password_field", "");
  // Let the user interact with the page.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));
  // Wait until that interaction causes the username and the password value to
  // be revealed.
  WaitForElementValue("username_field", "admin");
  WaitForElementValue("password_field", "12345");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PasswordValueAccessibleOnSubmit) {
  // At first let us save a credential to the password store.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.url = embedded_test_server()->base_url();
  signin_form.action = embedded_test_server()->base_url();
  signin_form.username_value = u"admin";
  signin_form.password_value = u"random_secret";
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/form_and_link.html");

  PasswordsNavigationObserver submit_observer(WebContents());
  // Submit the form via a tap on the submit button.
  content::SimulateMouseClickOrTapElementWithId(WebContents(),
                                                "input_submit_button");
  ASSERT_TRUE(submit_observer.Wait());
  std::string query = WebContents()->GetLastCommittedURL().query();
  EXPECT_THAT(query, testing::HasSubstr("random_secret"));
}

// Test fix for crbug.com/338650.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       DontPromptForPasswordFormWithDefaultValue) {
  NavigateToFile("/password/password_form_with_default_value.html");

  // Don't prompt if we navigate away even if there is a password value since
  // it's not coming from the user.
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  NavigateToFile("/password/done.html");
  ASSERT_TRUE(observer.Wait());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       DontPromptForPasswordFormWithReadonlyPasswordField) {
  NavigateToFile("/password/password_form_with_password_readonly.html");

  // Fill a form and submit through a <input type="submit"> button. Nothing
  // special.
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptWhenEnableAutomaticPasswordSavingSwitchIsNotSet) {
  NavigateToFile("/password/password_form.html");

  // Fill a form and submit through a <input type="submit"> button.
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

// Test fix for crbug.com/368690.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, NoPromptWhenReloading) {
  NavigateToFile("/password/password_form.html");

  std::string fill =
      "document.getElementById('username_redirect').value = 'temp';"
      "document.getElementById('password_redirect').value = 'random';";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill));

  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  GURL url = embedded_test_server()->GetURL("/password/password_form.html");
  NavigateParams params(browser(), url, ::ui::PAGE_TRANSITION_RELOAD);
  ui_test_utils::NavigateToURL(&params);
  ASSERT_TRUE(observer.Wait());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

// Test that if a form gets dynamically added between the form parsing and
// rendering, and while the main frame still loads, it still is registered, and
// thus saving passwords from it works.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       FormsAddedBetweenParsingAndRendering) {
  NavigateToFile("/password/between_parsing_and_rendering.html");

  PasswordsNavigationObserver observer(WebContents());
  std::string submit =
      "document.getElementById('username').value = 'temp';"
      "document.getElementById('password').value = 'random';"
      "document.getElementById('submit-button').click();";
  ASSERT_TRUE(content::ExecJs(WebContents(), submit));
  ASSERT_TRUE(observer.Wait());

  EXPECT_TRUE(BubbleObserver(WebContents()).IsSavePromptShownAutomatically());
}

// Test that if a hidden form gets dynamically added between the form parsing
// and rendering, it still is registered, and autofilling works.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       HiddenFormAddedBetweenParsingAndRendering) {
  // At first let us save a credential to the password store.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.url = embedded_test_server()->base_url();
  signin_form.action = embedded_test_server()->base_url();
  signin_form.username_value = u"admin";
  signin_form.password_value = u"12345";
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/between_parsing_and_rendering.html?hidden");

  std::string show_form =
      "document.getElementsByTagName('form')[0].style.display = 'block'";
  ASSERT_TRUE(content::ExecJs(WebContents(), show_form));

  // Wait until the username is filled, to make sure autofill kicked in.
  WaitForElementValue("username", "admin");
  WaitForElementValue("password", "12345");
}

// https://crbug.com/713645
// Navigate to a page that can't load some of the subresources. Create a hidden
// form when the body is loaded. Make the form visible. Chrome should autofill
// the form.
// The fact that the form is hidden isn't super important but reproduces the
// actual bug.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, SlowPageFill) {
  // At first let us save a credential to the password store.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.url = embedded_test_server()->base_url();
  signin_form.action = embedded_test_server()->base_url();
  signin_form.username_value = u"admin";
  signin_form.password_value = u"12345";
  password_store->AddLogin(signin_form);

  GURL url =
      embedded_test_server()->GetURL("/password/infinite_password_form.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  // Wait for autofill.
  BubbleObserver bubble_observer(WebContents());
  bubble_observer.WaitForManagementState();

  // Show the form and make sure that the password was autofilled.
  std::string show_form =
      "document.getElementsByTagName('form')[0].style.display = 'block'";
  ASSERT_TRUE(content::ExecJs(WebContents(), show_form));

  CheckElementValue("username", "admin");
  CheckElementValue("password", "12345");
}

// Test that if there was no previous page load then the PasswordManagerDriver
// does not think that there were SSL errors on the current page. The test opens
// a new tab with a URL for which the embedded test server issues a basic auth
// challenge.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, NoLastLoadGoodLastLoad) {
  // We must use a new test server here because embedded_test_server() is
  // already started at this point and adding the request handler to it would
  // not be thread safe.
  net::EmbeddedTestServer http_test_server;

  // Teach the embedded server to handle requests by issuing the basic auth
  // challenge.
  http_test_server.RegisterRequestHandler(
      base::BindRepeating(&HandleTestAuthRequest));
  ASSERT_TRUE(http_test_server.Start());

  password_manager::TestPasswordStore* password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  ASSERT_TRUE(password_store->IsEmpty());

  // Navigate to a page requiring HTTP auth.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), http_test_server.GetURL("/basic_auth")));
  ASSERT_TRUE(base::test::RunUntil(
      []() { return LoginHandler::GetAllLoginHandlersForTest().size() == 1; }));

  // Offer valid credentials on the auth challenge.
  ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());
  LoginHandler* handler = LoginHandler::GetAllLoginHandlersForTest().front();
  ASSERT_TRUE(handler);
  PasswordsNavigationObserver nav_observer(WebContents());
  // Any username/password will work.
  handler->SetAuth(u"user", u"pwd");

  // The password manager should be working correctly.
  ASSERT_TRUE(nav_observer.Wait());
  WaitForPasswordStore();
  BubbleObserver bubble_observer(WebContents());
  EXPECT_TRUE(bubble_observer.IsSavePromptShownAutomatically());
  bubble_observer.AcceptSavePrompt();

  // Spin the message loop to make sure the password store had a chance to save
  // the password.
  WaitForPasswordStore();
  EXPECT_FALSE(password_store->IsEmpty());
}

// Fill out a form and click a button. The Javascript removes the form, creates
// a similar one with another action, fills it out and submits. Chrome can
// manage to detect the new one and create a complete matching
// PasswordFormManager. Otherwise, the all-but-action matching PFM should be
// used. Regardless of the internals the user sees the bubble in 100% cases.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PreferPasswordFormManagerWhichFinishedMatching) {
  NavigateToFile("/password/create_form_copy_on_submit.html");

  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string submit =
      "document.getElementById('username').value = 'overwrite_me';"
      "document.getElementById('password').value = 'random';"
      "document.getElementById('non-form-button').click();";
  ASSERT_TRUE(content::ExecJs(WebContents(), submit));
  ASSERT_TRUE(observer.Wait());

  WaitForPasswordStore();
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

// Tests whether a attempted submission of a malicious credentials gets blocked.
// This simulates a case which is described in http://crbug.com/571580.
IN_PROC_BROWSER_TEST_F(
    PasswordManagerBrowserTest,
    NoPromptForSeparateLoginFormWhenSwitchingFromHttpsToHttp) {
  std::string path = "/password/password_form.html";
  GURL https_url(https_test_server().GetURL(path));
  ASSERT_TRUE(https_url.SchemeIs(url::kHttpsScheme));

  PasswordsNavigationObserver form_observer(WebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), https_url));
  ASSERT_TRUE(form_observer.Wait());

  std::string fill_and_submit_redirect =
      "document.getElementById('username_redirect').value = 'user';"
      "document.getElementById('password_redirect').value = 'password';"
      "document.getElementById('submit_redirect').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit_redirect));

  PasswordsNavigationObserver redirect_observer(WebContents());
  redirect_observer.SetPathToWaitFor("/password/redirect.html");
  ASSERT_TRUE(redirect_observer.Wait());

  BubbleObserver prompt_observer(WebContents());
  prompt_observer.WaitForAutomaticSavePrompt();

  // Normally the redirect happens to done.html. Here an attack is simulated
  // that hijacks the redirect to a attacker controlled page.
  GURL http_url(
      embedded_test_server()->GetURL("/password/simple_password.html"));
  std::string attacker_redirect =
      "window.location.href = '" + http_url.spec() + "';";
  ASSERT_TRUE(content::ExecJs(RenderFrameHost(), attacker_redirect,
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  PasswordsNavigationObserver attacker_observer(WebContents());
  attacker_observer.SetPathToWaitFor("/password/simple_password.html");
  ASSERT_TRUE(attacker_observer.Wait());

  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());

  std::string fill_and_submit_attacker_form =
      "document.getElementById('username_field').value = 'attacker_username';"
      "document.getElementById('password_field').value = 'attacker_password';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit_attacker_form));

  PasswordsNavigationObserver done_observer(WebContents());
  done_observer.SetPathToWaitFor("/password/done.html");
  ASSERT_TRUE(done_observer.Wait());

  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
  prompt_observer.AcceptSavePrompt();

  // Wait for password store and check that credentials are stored.
  WaitForPasswordStore();
  CheckThatCredentialsStored("user", "password");
}

// Tests that after HTTP -> HTTPS migration the credential is autofilled.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       HttpMigratedCredentialAutofilled) {
  // Add an http credential to the password store.
  GURL https_origin = https_test_server().base_url();
  ASSERT_TRUE(https_origin.SchemeIs(url::kHttpsScheme));
  GURL::Replacements rep;
  rep.SetSchemeStr(url::kHttpScheme);
  GURL http_origin = https_origin.ReplaceComponents(rep);
  password_manager::PasswordForm http_form;
  http_form.signon_realm = http_origin.spec();
  http_form.url = http_origin;
  // Assume that the previous action was already HTTPS one matching the current
  // page.
  http_form.action = https_origin;
  http_form.username_value = u"user";
  http_form.password_value = u"12345";
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_store->AddLogin(http_form);

  PasswordsNavigationObserver form_observer(WebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server().GetURL("/password/password_form.html")));
  ASSERT_TRUE(form_observer.Wait());
  WaitForPasswordStore();

  // Let the user interact with the page, so that DOM gets modification events,
  // needed for autofilling fields.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));
  WaitForElementValue("username_field", "user");
  WaitForElementValue("password_field", "12345");
}

// Tests that obsolete HTTP credentials are moved when a site migrated to HTTPS
// and has HSTS enabled.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       ObsoleteHttpCredentialMovedOnMigrationToHstsSite) {
  // Add an http credential to the password store.
  GURL https_origin = https_test_server().base_url();
  ASSERT_TRUE(https_origin.SchemeIs(url::kHttpsScheme));
  GURL::Replacements rep;
  rep.SetSchemeStr(url::kHttpScheme);
  GURL http_origin = https_origin.ReplaceComponents(rep);
  password_manager::PasswordForm http_form;
  http_form.signon_realm = http_origin.spec();
  http_form.url = http_origin;
  http_form.username_value = u"user";
  http_form.password_value = u"12345";
  password_manager::TestPasswordStore* password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  password_store->AddLogin(http_form);

  // Treat the host of the HTTPS test server as HSTS.
  AddHSTSHost(https_test_server().host_port_pair().host());

  // Navigate to HTTPS page and trigger the migration.
  PasswordsNavigationObserver form_observer(WebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server().GetURL("/password/password_form.html")));
  ASSERT_TRUE(form_observer.Wait());

  // Issue the query for HTTPS credentials.
  WaitForPasswordStore();

  // Realize there are no HTTPS credentials and issue the query for HTTP
  // credentials instead.
  WaitForPasswordStore();

  // Sync with IO thread before continuing. This is necessary, because the
  // credential migration triggers a query for the HSTS state which gets
  // executed on the IO thread. The actual task is empty, because only the reply
  // is relevant. By the time the reply is executed it is guaranteed that the
  // migration is completed.
  base::RunLoop run_loop;
  content::GetIOThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE, base::BindOnce([]() {}), run_loop.QuitClosure());
  run_loop.Run();

  // Migration updates should touch the password store.
  WaitForPasswordStore();
  // Only HTTPS passwords should be present.
  EXPECT_THAT(password_store->stored_passwords(),
              ElementsAre(Pair(https_origin.spec(), SizeIs(1))));
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptWhenPasswordFormWithoutUsernameFieldSubmitted) {
  password_manager::TestPasswordStore* password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());

  EXPECT_TRUE(password_store->IsEmpty());

  NavigateToFile("/password/form_with_only_password_field.html");

  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string submit =
      "document.getElementById('password').value = 'password';"
      "document.getElementById('submit-button').click();";
  ASSERT_TRUE(content::ExecJs(WebContents(), submit));
  ASSERT_TRUE(observer.Wait());

  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
  prompt_observer.AcceptSavePrompt();

  WaitForPasswordStore();
  EXPECT_FALSE(password_store->IsEmpty());
}

// Test that if a form gets autofilled, then it gets autofilled on re-creation
// as well.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, ReCreatedFormsGetFilled) {
  // At first let us save a credential to the password store.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.url = embedded_test_server()->base_url();
  signin_form.action = embedded_test_server()->base_url();
  signin_form.username_value = u"temp";
  signin_form.password_value = u"random";
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/dynamic_password_form.html");
  const std::string create_form =
      "document.getElementById('create_form_button').click();";
  ASSERT_TRUE(content::ExecJs(WebContents(), create_form));
  // Wait until the username is filled, to make sure autofill kicked in.
  WaitForElementValue("username_id", "temp");

  // Now the form gets deleted and created again. It should get autofilled
  // again.
  const std::string delete_form =
      "var form = document.getElementById('dynamic_form_id');"
      "form.parentNode.removeChild(form);";
  ASSERT_TRUE(content::ExecJs(WebContents(), delete_form));
  ASSERT_TRUE(content::ExecJs(WebContents(), create_form));
  WaitForElementValue("username_id", "temp");
}

// Test that if the same dynamic form is created multiple times then all of them
// are autofilled.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, DuplicateFormsGetFilled) {
  // At first let us save a credential to the password store.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.url = embedded_test_server()->base_url();
  signin_form.action = embedded_test_server()->base_url();
  signin_form.username_value = u"temp";
  signin_form.password_value = u"random";
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/recurring_dynamic_form.html");
  ASSERT_TRUE(content::ExecJs(WebContents(), "addForm();"));
  // Wait until the username is filled, to make sure autofill kicked in.
  WaitForJsElementValue("document.body.children[0].children[0]", "temp");
  WaitForJsElementValue("document.body.children[0].children[1]", "random");

  // Add one more form.
  ASSERT_TRUE(content::ExecJs(WebContents(), "addForm();"));
  // Wait until the username is filled, to make sure autofill kicked in.
  WaitForJsElementValue("document.body.children[1].children[0]", "temp");
  WaitForJsElementValue("document.body.children[1].children[1]", "random");
}

// Test that an autofilled credential is deleted then the password manager
// doesn't try to resurrect it on navigation.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       DeletedPasswordIsNotRevived) {
  // At first let us save a credential to the password store.
  password_manager::TestPasswordStore* password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.url = embedded_test_server()->base_url();
  signin_form.action = embedded_test_server()->base_url();
  signin_form.username_value = u"admin";
  signin_form.password_value = u"1234";
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/password_form.html");
  // Let the user interact with the page.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));
  // Wait until that interaction causes the username and the password value to
  // be revealed.
  WaitForElementValue("username_field", "admin");

  // Now the credential is removed via the settings or the bubble.
  password_store->RemoveLogin(FROM_HERE, signin_form);
  WaitForPasswordStore();

  // Submit the form. It shouldn't revive the credential in the store.
  PasswordsNavigationObserver observer(WebContents());
  ASSERT_TRUE(content::ExecJs(
      WebContents(), "document.getElementById('input_submit_button').click()"));
  ASSERT_TRUE(observer.Wait());

  WaitForPasswordStore();
  EXPECT_TRUE(password_store->IsEmpty());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForPushStateWhenFormDisappears) {
  NavigateToFile("/password/password_push_state.html");

  // Verify that we show the save password prompt if 'history.pushState()'
  // is called after form submission is suppressed by, for example, calling
  // preventDefault() in a form's submit event handler.
  // Note that calling 'submit()' on a form with javascript doesn't call
  // the onsubmit handler, so we click the submit button instead.
  // Also note that the prompt will only show up if the form disappers
  // after submission
  PasswordsNavigationObserver observer(WebContents());
  observer.set_quit_on_entry_committed(true);
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

// Similar to the case above, but this time the form persists after
// 'history.pushState()'. And save password prompt should not show up
// in this case.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       NoPromptForPushStateWhenFormPersists) {
  NavigateToFile("/password/password_push_state.html");

  // Set |should_delete_testform| to false to keep submitted form visible after
  // history.pushsTate();
  PasswordsNavigationObserver observer(WebContents());
  observer.set_quit_on_entry_committed(true);
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "should_delete_testform = false;"
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

// The password manager should distinguish forms with empty actions. After
// successful login, the login form disappears, but the another one shouldn't be
// recognized as the login form. The save prompt should appear.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForPushStateWhenFormWithEmptyActionDisappears) {
  NavigateToFile("/password/password_push_state.html");

  PasswordsNavigationObserver observer(WebContents());
  observer.set_quit_on_entry_committed(true);
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('ea_username_field').value = 'temp';"
      "document.getElementById('ea_password_field').value = 'random';"
      "document.getElementById('ea_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

// Similar to the case above, but this time the form persists after
// 'history.pushState()'. The password manager should find the login form even
// if the action of the form is empty. Save password prompt should not show up.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForPushStateWhenFormWithEmptyActionPersists) {
  NavigateToFile("/password/password_push_state.html");

  PasswordsNavigationObserver observer(WebContents());
  observer.set_quit_on_entry_committed(true);
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "should_delete_testform = false;"
      "document.getElementById('ea_username_field').value = 'temp';"
      "document.getElementById('ea_password_field').value = 'random';"
      "document.getElementById('ea_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

// Current and target URLs contain different parameters and references. This
// test checks that parameters and references in origins are ignored for
// form origin comparison.
IN_PROC_BROWSER_TEST_F(
    PasswordManagerBrowserTest,
    PromptForPushStateWhenFormDisappears_ParametersInOrigins) {
  NavigateToFile("/password/password_push_state.html?login#r");

  PasswordsNavigationObserver observer(WebContents());
  observer.set_quit_on_entry_committed(true);
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "add_parameters_to_target_url = true;"
      "document.getElementById('pa_username_field').value = 'temp';"
      "document.getElementById('pa_password_field').value = 'random';"
      "document.getElementById('pa_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

// Similar to the case above, but this time the form persists after
// 'history.pushState()'. The password manager should find the login form even
// if target and current URLs contain different parameters or references.
// Save password prompt should not show up.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForPushStateWhenFormPersists_ParametersInOrigins) {
  NavigateToFile("/password/password_push_state.html?login#r");

  PasswordsNavigationObserver observer(WebContents());
  observer.set_quit_on_entry_committed(true);
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "should_delete_testform = false;"
      "add_parameters_to_target_url = true;"
      "document.getElementById('pa_username_field').value = 'temp';"
      "document.getElementById('pa_password_field').value = 'random';"
      "document.getElementById('pa_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerAutofillPopupBrowserTest,
                       InFrameNavigationDoesNotClearPopupState) {
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.username_value = u"temp";
  signin_form.password_value = u"random123";
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/password_form.html");
  // Trigger in page navigation.
  std::string in_page_navigate = "location.hash = '#blah';";
  ASSERT_TRUE(content::ExecJs(RenderFrameHost(), in_page_navigate,
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Click on the username field to display the popup.
  content::SimulateMouseClickOrTapElementWithId(WebContents(),
                                                "username_field");
  // Make sure that the popup is showing.
  autofill_client().WaitForAutofillPopup();
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, ChangePwdFormBubbleShown) {
  NavigateToFile("/password/password_form.html");

  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('chg_username_field').value = 'temp';"
      "document.getElementById('chg_password_field').value = 'random';"
      "document.getElementById('chg_new_password_1').value = 'random1';"
      "document.getElementById('chg_new_password_2').value = 'random1';"
      "document.getElementById('chg_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       ChangePwdFormPushStateBubbleShown) {
  NavigateToFile("/password/password_push_state.html");

  PasswordsNavigationObserver observer(WebContents());
  observer.set_quit_on_entry_committed(true);
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('chg_username_field').value = 'temp';"
      "document.getElementById('chg_password_field').value = 'random';"
      "document.getElementById('chg_new_password_1').value = 'random1';"
      "document.getElementById('chg_new_password_2').value = 'random1';"
      "document.getElementById('chg_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, NoPromptOnBack) {
  // Go to a successful landing page through submitting first, so that it is
  // reachable through going back, and the remembered page transition is form
  // submit. There is no need to submit non-empty strings.
  NavigateToFile("/password/password_form.html");

  PasswordsNavigationObserver dummy_submit_observer(WebContents());
  std::string just_submit =
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), just_submit));
  ASSERT_TRUE(dummy_submit_observer.Wait());

  // Now go to a page with a form again, fill the form, and go back instead of
  // submitting it.
  NavigateToFile("/password/dummy_submit.html");

  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  // The (dummy) submit is necessary to provisionally save the typed password.
  // A user typing in the password field would not need to submit to
  // provisionally save it, but the script cannot trigger that just by
  // assigning to the field's value.
  std::string fill_and_back =
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click();"
      "window.history.back();";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_back));
  ASSERT_TRUE(observer.Wait());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

// Regression test for http://crbug.com/452306
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       ChangingTextToPasswordFieldOnSignupForm) {
  NavigateToFile("/password/signup_form.html");

  // In this case, pretend that username_field is actually a password field
  // that starts as a text field to simulate placeholder.
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string change_and_submit =
      "document.getElementById('other_info').value = 'username';"
      "document.getElementById('username_field').type = 'password';"
      "document.getElementById('username_field').value = 'mypass';"
      "document.getElementById('password_field').value = 'mypass';"
      "document.getElementById('testform').submit();";
  ASSERT_TRUE(content::ExecJs(WebContents(), change_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

// Regression test for http://crbug.com/451631
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       SavingOnManyPasswordFieldsTest) {
  // Simulate Macy's registration page, which contains the normal 2 password
  // fields for confirming the new password plus 2 more fields for security
  // questions and credit card. Make sure that saving works correctly for such
  // sites.
  NavigateToFile("/password/many_password_signup_form.html");

  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'username';"
      "document.getElementById('password_field').value = 'mypass';"
      "document.getElementById('confirm_field').value = 'mypass';"
      "document.getElementById('security_answer').value = 'hometown';"
      "document.getElementById('SSN').value = '1234';"
      "document.getElementById('testform').submit();";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       SaveWhenIFrameDestroyedOnFormSubmit) {
  NavigateToFile("/password/frame_detached_on_submit.html");

  // Need to pay attention for a message that XHR has finished since there
  // is no navigation to wait for.
  content::DOMMessageQueue message_queue(WebContents());

  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "var iframe = document.getElementById('login_iframe');"
      "var frame_doc = iframe.contentDocument;"
      "frame_doc.getElementById('username_field').value = 'temp';"
      "frame_doc.getElementById('password_field').value = 'random';"
      "frame_doc.getElementById('submit_button').click();";

  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  std::string message;
  while (message_queue.WaitForMessage(&message)) {
    if (message == "\"SUBMISSION_FINISHED\"") {
      break;
    }
  }

  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

// TODO(crbug.com/360035859): Fix and re-enable.
IN_PROC_BROWSER_TEST_F(
    PasswordManagerBrowserTest,
    DISABLED_IFrameDetachedRightAfterFormSubmission_UpdateBubbleShown) {
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.password_value = u"pw";
  signin_form.username_value = u"temp";
  password_store->AddLogin(signin_form);
  WaitForPasswordStore();

  NavigateToFile("/password/frame_detached_after_submit.html");

  content::RenderFrameHost* iframe_rfh = nullptr;
  RenderFrameHost()->ForEachRenderFrameHost([&](content::RenderFrameHost* rfh) {
    if (!rfh->IsInPrimaryMainFrame()) {
      iframe_rfh = rfh;
      return;
    }
  });
  ASSERT_TRUE(iframe_rfh);

  BubbleObserver prompt_observer(WebContents());
  content::RenderFrameDeletedObserver iframe_observer(iframe_rfh);
  std::string fill_and_submit =
      "var iframe = document.getElementById('password_reset_iframe');"
      "var frame_doc = iframe.contentDocument;"
      "frame_doc.getElementById('password_field').value = 'random';"
      "frame_doc.getElementById('confirm_password_field').value = 'random';"
      "frame_doc.getElementById('input_submit_button').click();";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(iframe_observer.WaitUntilDeleted());

  prompt_observer.WaitForAutomaticUpdatePrompt();
  EXPECT_TRUE(prompt_observer.IsUpdatePromptShownAutomatically());
}

// Check that a username and password are filled into forms in iframes
// that don't share the security origin with the main frame, but have PSL
// matched origins.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PSLMatchedCrossSiteFillTest) {
  GURL main_frame_url = embedded_test_server()->GetURL(
      "www.foo.com", "/password/password_form_in_crosssite_iframe.html");
  PasswordsNavigationObserver observer(WebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));
  ASSERT_TRUE(observer.Wait());

  // Create an iframe and navigate cross-site.
  PasswordsNavigationObserver iframe_observer(WebContents());
  iframe_observer.SetPathToWaitFor("/password/crossite_iframe_content.html");
  GURL iframe_url = embedded_test_server()->GetURL(
      "abc.foo.com", "/password/crossite_iframe_content.html");
  std::string create_iframe =
      base::StringPrintf("create_iframe('%s');", iframe_url.spec().c_str());
  ASSERT_TRUE(content::ExecJs(RenderFrameHost(), create_iframe,
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  ASSERT_TRUE(iframe_observer.Wait());

  // Store a password for autofill later.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = iframe_url.DeprecatedGetOriginAsURL().spec();
  signin_form.url = iframe_url;
  signin_form.username_value = u"temp";
  signin_form.password_value = u"pa55w0rd";
  password_store->AddLogin(signin_form);
  WaitForPasswordStore();

  // Visit the form again.
  PasswordsNavigationObserver reload_observer(WebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));
  ASSERT_TRUE(reload_observer.Wait());

  PasswordsNavigationObserver iframe_observer_2(WebContents());
  iframe_observer_2.SetPathToWaitFor("/password/crossite_iframe_content.html");
  ASSERT_TRUE(content::ExecJs(RenderFrameHost(), create_iframe,
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  ASSERT_TRUE(iframe_observer_2.Wait());

  // Simulate the user interaction in the iframe which should trigger autofill.
  // Click in the middle of the frame to avoid the border.
  content::SimulateMouseClickOrTapElementWithId(WebContents(), "iframe");

  // Verify username and password have not been autofilled due to an insecure
  // origin.
  EXPECT_TRUE(content::EvalJs(RenderFrameHost(), "sendMessage('get_username');",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE)
                  .ExtractString()
                  .empty());

  EXPECT_TRUE(content::EvalJs(RenderFrameHost(), "sendMessage('get_password');",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE)
                  .ExtractString()
                  .empty());
}

// Check that a username and password are not filled in forms in iframes
// that don't have PSL matched origins.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PSLUnMatchedCrossSiteFillTest) {
  GURL main_frame_url = embedded_test_server()->GetURL(
      "www.foo.com", "/password/password_form_in_crosssite_iframe.html");
  PasswordsNavigationObserver observer(WebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));
  ASSERT_TRUE(observer.Wait());

  // Create an iframe and navigate cross-site.
  PasswordsNavigationObserver iframe_observer(WebContents());
  iframe_observer.SetPathToWaitFor("/password/crossite_iframe_content.html");
  GURL iframe_url = embedded_test_server()->GetURL(
      "www.bar.com", "/password/crossite_iframe_content.html");
  std::string create_iframe =
      base::StringPrintf("create_iframe('%s');", iframe_url.spec().c_str());
  ASSERT_TRUE(content::ExecJs(RenderFrameHost(), create_iframe,
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  ASSERT_TRUE(iframe_observer.Wait());

  // Store a password for autofill later.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = iframe_url.DeprecatedGetOriginAsURL().spec();
  signin_form.url = iframe_url;
  signin_form.username_value = u"temp";
  signin_form.password_value = u"pa55w0rd";
  password_store->AddLogin(signin_form);
  WaitForPasswordStore();

  // Visit the form again.
  PasswordsNavigationObserver reload_observer(WebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));
  ASSERT_TRUE(reload_observer.Wait());

  PasswordsNavigationObserver iframe_observer_2(WebContents());
  iframe_observer_2.SetPathToWaitFor("/password/crossite_iframe_content.html");
  ASSERT_TRUE(content::ExecJs(RenderFrameHost(), create_iframe,
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  ASSERT_TRUE(iframe_observer_2.Wait());

  // Simulate the user interaction in the iframe which should trigger autofill.
  // Click in the middle of the frame to avoid the border.
  content::SimulateMouseClickOrTapElementWithId(WebContents(), "iframe");

  // Verify username is not autofilled
  EXPECT_EQ("",
            content::EvalJs(RenderFrameHost(), "sendMessage('get_username');",
                            content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  // Verify password is not autofilled
  EXPECT_EQ("",
            content::EvalJs(RenderFrameHost(), "sendMessage('get_password');",
                            content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// Check that a password form in an iframe of same origin will not be
// filled in until user interact with the iframe.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       SameOriginIframeAutoFillTest) {
  // Visit the sign-up form to store a password for autofill later
  NavigateToFile("/password/password_form_in_same_origin_iframe.html");
  PasswordsNavigationObserver observer(WebContents());
  observer.SetPathToWaitFor("/password/done.html");

  std::string submit =
      "var ifrmDoc = document.getElementById('iframe').contentDocument;"
      "ifrmDoc.getElementById('username_field').value = 'temp';"
      "ifrmDoc.getElementById('password_field').value = 'pa55w0rd';"
      "ifrmDoc.getElementById('input_submit_button').click();";
  ASSERT_TRUE(content::ExecJs(WebContents(), submit));
  ASSERT_TRUE(observer.Wait());
  BubbleObserver prompt_observer(WebContents());
  prompt_observer.WaitForAutomaticSavePrompt();
  prompt_observer.AcceptSavePrompt();

  // Visit the form again
  PasswordsNavigationObserver reload_observer(WebContents());
  NavigateToFile("/password/password_form_in_same_origin_iframe.html");
  ASSERT_TRUE(reload_observer.Wait());

  // Verify password and username are not accessible yet.
  CheckElementValue("iframe", "username_field", "");
  CheckElementValue("iframe", "password_field", "");

  // Simulate the user interaction in the iframe which should trigger autofill.
  // Click in the middle of the username to avoid the border.
  ASSERT_TRUE(content::ExecJs(
      RenderFrameHost(),
      "var usernameRect = document.getElementById("
      "'iframe').contentDocument.getElementById('username_field')"
      ".getBoundingClientRect();",
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  int y =
      content::EvalJs(RenderFrameHost(),
                      "Math.floor(usernameRect.top + usernameRect.height / 2)",
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE)
          .ExtractInt();
  int x =
      content::EvalJs(RenderFrameHost(),
                      "Math.floor(usernameRect.left + usernameRect.width / 2)",
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE)
          .ExtractInt();

  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(x, y));
  // Verify username and password have been autofilled
  WaitForElementValue("iframe", "username_field", "temp");
  WaitForElementValue("iframe", "password_field", "pa55w0rd");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, ChangePwdNoAccountStored) {
  NavigateToFile("/password/password_form.html");

  // Fill a form and submit through a <input type="submit"> button.
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());

  std::string fill_and_submit =
      "document.getElementById('chg_password_wo_username_field').value = "
      "'old_pw';"
      "document.getElementById('chg_new_password_wo_username_1').value = "
      "'new_pw';"
      "document.getElementById('chg_new_password_wo_username_2').value = "
      "'new_pw';"
      "document.getElementById('chg_submit_wo_username_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  // No credentials stored before, so save bubble is shown.
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
  prompt_observer.AcceptSavePrompt();

  // Check that credentials are stored.
  WaitForPasswordStore();
  CheckThatCredentialsStored("", "new_pw");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, ChangePwd1AccountStored) {
  // At first let us save credentials to the PasswordManager.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.password_value = u"pw";
  signin_form.username_value = u"temp";
  password_store->AddLogin(signin_form);

  // Check that password update bubble is shown.
  NavigateToFile("/password/password_form.html");
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit_change_password =
      "document.getElementById('chg_password_wo_username_field').value = "
      "'random';"
      "document.getElementById('chg_new_password_wo_username_1').value = "
      "'new_pw';"
      "document.getElementById('chg_new_password_wo_username_2').value = "
      "'new_pw';"
      "document.getElementById('chg_submit_wo_username_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit_change_password));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsUpdatePromptShownAutomatically());

  // We emulate that the user clicks "Update" button.
  prompt_observer.AcceptUpdatePrompt();

  WaitForPasswordStore();
  CheckThatCredentialsStored("temp", "new_pw");
}

// This fixture disable autofill. If a password is autofilled, then all the
// Javascript changes are discarded and test below would not be able to feed a
// new password to the form.
class PasswordManagerBrowserTestWithAutofillDisabled
    : public PasswordManagerBrowserTest {
 public:
  PasswordManagerBrowserTestWithAutofillDisabled() {
    feature_list_.InitAndEnableFeature(features::kFillOnAccountSelect);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTestWithAutofillDisabled,
                       PasswordOverriddenUpdateBubbleShown) {
  // At first let us save credentials to the PasswordManager.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.username_value = u"temp";
  signin_form.password_value = u"pw";
  password_store->AddLogin(signin_form);

  // Check that password update bubble is shown.
  NavigateToFile("/password/password_form.html");
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'new_pw';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  // The stored password "pw" was overridden with "new_pw", so update prompt is
  // expected.
  EXPECT_TRUE(prompt_observer.IsUpdatePromptShownAutomatically());

  prompt_observer.AcceptUpdatePrompt();
  WaitForPasswordStore();
  CheckThatCredentialsStored("temp", "new_pw");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PasswordNotOverriddenUpdateBubbleNotShown) {
  // At first let us save credentials to the PasswordManager.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.username_value = u"temp";
  signin_form.password_value = u"pw";
  password_store->AddLogin(signin_form);

  // Check that password update bubble is shown.
  NavigateToFile("/password/password_form.html");
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'pw';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  // The stored password "pw" was not overridden, so update prompt is not
  // expected.
  EXPECT_FALSE(prompt_observer.IsUpdatePromptShownAutomatically());
  CheckThatCredentialsStored("temp", "pw");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       MultiplePasswordsWithPasswordSelectionEnabled) {
  NavigateToFile("/password/password_form.html");
  PasswordsNavigationObserver observer(WebContents());
  // It is important that these 3 passwords are different. Because if two of
  // them are the same, it is going to be treated as a password update and the
  // dropdown will not be shown.
  std::string fill_and_submit =
      "document.getElementById('chg_password_wo_username_field').value = "
      "'pass1';"
      "document.getElementById('chg_new_password_wo_username_1').value = "
      "'pass2';"
      "document.getElementById('chg_new_password_wo_username_2').value = "
      "'pass3';"
      "document.getElementById('chg_submit_wo_username_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  // 3 possible passwords are going to be shown in a dropdown when the password
  // selection feature is enabled. The first one will be selected as the main
  // password by default. All three will be in the |all_alternative_passwords|
  // list. The save password prompt is expected.
  BubbleObserver bubble_observer(WebContents());
  EXPECT_TRUE(bubble_observer.IsSavePromptShownAutomatically());
  EXPECT_EQ(u"pass1",
            ManagePasswordsUIController::FromWebContents(WebContents())
                ->GetPendingPassword()
                .password_value);
  EXPECT_THAT(
      ManagePasswordsUIController::FromWebContents(WebContents())
          ->GetPendingPassword()
          .all_alternative_passwords,
      ElementsAre(AllOf(Field("value", &AlternativeElement::value, u"pass1"),
                        Field("name", &AlternativeElement::name,
                              u"chg_password_wo_username_field")),
                  AllOf(Field("value", &AlternativeElement::value, u"pass2"),
                        Field("name", &AlternativeElement::name,
                              u"chg_new_password_wo_username_1")),
                  AllOf(Field("value", &AlternativeElement::value, u"pass3"),
                        Field("name", &AlternativeElement::name,
                              u"chg_new_password_wo_username_2"))));
  bubble_observer.AcceptSavePrompt();
  WaitForPasswordStore();
  CheckThatCredentialsStored("", "pass1");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       ChangePwdWhenTheFormContainNotUsernameTextfield) {
  // At first let us save credentials to the PasswordManager.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.password_value = u"pw";
  signin_form.username_value = u"temp";
  password_store->AddLogin(signin_form);

  // Check that password update bubble is shown.
  NavigateToFile("/password/password_form.html");
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit_change_password =
      "document.getElementById('chg_text_field').value = '3';"
      "document.getElementById('chg_password_withtext_field').value"
      " = 'random';"
      "document.getElementById('chg_new_password_withtext_username_1').value"
      " = 'new_pw';"
      "document.getElementById('chg_new_password_withtext_username_2').value"
      " = 'new_pw';"
      "document.getElementById('chg_submit_withtext_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit_change_password));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsUpdatePromptShownAutomatically());

  prompt_observer.AcceptUpdatePrompt();
  WaitForPasswordStore();
  CheckThatCredentialsStored("temp", "new_pw");
}

// Test whether the password form with the username and password fields having
// ambiguity in id attribute gets autofilled correctly.
IN_PROC_BROWSER_TEST_F(
    PasswordManagerBrowserTest,
    AutofillSuggestionsForPasswordFormWithAmbiguousIdAttribute) {
  // At first let us save credentials to the PasswordManager.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm login_form;
  login_form.signon_realm = embedded_test_server()->base_url().spec();
  login_form.action = embedded_test_server()->GetURL("/password/done.html");
  login_form.username_value = u"myusername";
  login_form.password_value = u"mypassword";
  password_store->AddLogin(login_form);

  // Now, navigate to the password form having ambiguous Ids for username and
  // password fields and verify whether username and password is autofilled.
  NavigateToFile("/password/ambiguous_password_form.html");

  // Let the user interact with the page, so that DOM gets modification events,
  // needed for autofilling fields.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));

  WaitForElementValue("ambiguous_form", 0 /* elements_index */, "myusername");
  WaitForElementValue("ambiguous_form", 1 /* elements_index */, "mypassword");
}

// Test whether the password form having username and password fields without
// name and id attribute gets autofilled correctly.
IN_PROC_BROWSER_TEST_F(
    PasswordManagerBrowserTest,
    AutofillSuggestionsForPasswordFormWithoutNameOrIdAttribute) {
  // At first let us save credentials to the PasswordManager.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm login_form;
  login_form.signon_realm = embedded_test_server()->base_url().spec();
  login_form.action = embedded_test_server()->GetURL("/password/done.html");
  login_form.username_value = u"myusername";
  login_form.password_value = u"mypassword";
  password_store->AddLogin(login_form);

  // Now, navigate to the password form having no Ids for username and password
  // fields and verify whether username and password is autofilled.
  NavigateToFile("/password/ambiguous_password_form.html");

  // Let the user interact with the page, so that DOM gets modification events,
  // needed for autofilling fields.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));

  WaitForElementValue("no_name_id_form", 0 /* elements_index */, "myusername");
  WaitForElementValue("no_name_id_form", 1 /* elements_index */, "mypassword");
}

// Test whether the change password form having username and password fields
// without name and id attribute gets autofilled correctly.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       AutofillSuggestionsForChangePwdWithEmptyNames) {
  // At first let us save credentials to the PasswordManager.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm login_form;
  login_form.signon_realm = embedded_test_server()->base_url().spec();
  login_form.action = embedded_test_server()->GetURL("/password/done.html");
  login_form.username_value = u"myusername";
  login_form.password_value = u"mypassword";
  password_store->AddLogin(login_form);

  // Now, navigate to the password form having no Ids for username and password
  // fields and verify whether username and password is autofilled.
  NavigateToFile("/password/ambiguous_password_form.html");

  // Let the user interact with the page, so that DOM gets modification events,
  // needed for autofilling fields.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));

  WaitForElementValue("change_pwd_but_no_autocomplete", 0 /* elements_index */,
                      "myusername");
  WaitForElementValue("change_pwd_but_no_autocomplete", 1 /* elements_index */,
                      "mypassword");

  std::string get_new_password =
      "document.getElementById("
      "  'change_pwd_but_no_autocomplete').elements[2].value;";
  EXPECT_EQ("", content::EvalJs(RenderFrameHost(), get_new_password,
                                content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// Test whether the change password form having username and password fields
// with empty names but having |autocomplete='current-password'| gets autofilled
// correctly.
IN_PROC_BROWSER_TEST_F(
    PasswordManagerBrowserTest,
    AutofillSuggestionsForChangePwdWithEmptyNamesAndAutocomplete) {
  // At first let us save credentials to the PasswordManager.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm login_form;
  login_form.signon_realm = embedded_test_server()->base_url().spec();
  login_form.action = embedded_test_server()->GetURL("/password/done.html");
  login_form.username_value = u"myusername";
  login_form.password_value = u"mypassword";
  password_store->AddLogin(login_form);

  // Now, navigate to the password form having no Ids for username and password
  // fields and verify whether username and password is autofilled.
  NavigateToFile("/password/ambiguous_password_form.html");

  // Let the user interact with the page, so that DOM gets modification events,
  // needed for autofilling fields.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));

  WaitForElementValue("change_pwd", 0 /* elements_index */, "myusername");
  WaitForElementValue("change_pwd", 1 /* elements_index */, "mypassword");

  std::string get_new_password =
      "document.getElementById('change_pwd').elements[2].value;";
  EXPECT_EQ("", content::EvalJs(RenderFrameHost(), get_new_password,
                                content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// Test whether the change password form having username and password fields
// with empty names but having only new password fields having
// |autocomplete='new-password'| atrribute do not get autofilled.
IN_PROC_BROWSER_TEST_F(
    PasswordManagerBrowserTest,
    AutofillSuggestionsForChangePwdWithEmptyNamesButOnlyNewPwdField) {
  // At first let us save credentials to the PasswordManager.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm login_form;
  login_form.signon_realm = embedded_test_server()->base_url().spec();
  login_form.action = embedded_test_server()->GetURL("/password/done.html");
  login_form.username_value = u"myusername";
  login_form.password_value = u"mypassword";
  password_store->AddLogin(login_form);

  // Now, navigate to the password form having no Ids for username and password
  // fields and verify whether username and password is autofilled.
  NavigateToFile("/password/ambiguous_password_form.html");

  // Let the user interact with the page, so that DOM gets modification events,
  // needed for autofilling fields.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));

  std::string get_username =
      "document.getElementById("
      "  'change_pwd_but_no_old_pwd').elements[0].value;";
  EXPECT_EQ("", content::EvalJs(RenderFrameHost(), get_username,
                                content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  std::string get_new_password =
      "document.getElementById("
      "  'change_pwd_but_no_old_pwd').elements[1].value;";
  EXPECT_EQ("", content::EvalJs(RenderFrameHost(), get_new_password,
                                content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  std::string get_retype_password =
      "document.getElementById("
      "  'change_pwd_but_no_old_pwd').elements[2].value;";
  EXPECT_EQ("", content::EvalJs(RenderFrameHost(), get_retype_password,
                                content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// When there are multiple HttpAuthObservers (e.g., multiple HTTP auth dialogs
// as in http://crbug.com/537823), ensure that credentials from PasswordStore
// distributed to them are filtered by the realm.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, BasicAuthSeparateRealms) {
  // We must use a new test server here because embedded_test_server() is
  // already started at this point and adding the request handler to it would
  // not be thread safe.
  net::EmbeddedTestServer http_test_server;
  http_test_server.RegisterRequestHandler(
      base::BindRepeating(&HandleTestAuthRequest));
  ASSERT_TRUE(http_test_server.Start());

  // Save credentials for "test realm" in the store.
  password_manager::TestPasswordStore* password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  password_manager::PasswordForm creds;
  creds.scheme = password_manager::PasswordForm::Scheme::kBasic;
  creds.signon_realm = http_test_server.base_url().spec() + "test realm";
  creds.password_value = u"pw";
  creds.username_value = u"temp";
  password_store->AddLogin(creds);
  WaitForPasswordStore();
  ASSERT_FALSE(password_store->IsEmpty());

  // In addition to the HttpAuthObserver created automatically for the HTTP
  // auth dialog, also create a mock observer, for a different realm.
  MockHttpAuthObserver mock_login_model_observer;
  HttpAuthManager* httpauth_manager =
      ChromePasswordManagerClient::FromWebContents(WebContents())
          ->GetHttpAuthManager();
  password_manager::PasswordForm other_form(creds);
  other_form.signon_realm = "https://example.com/other realm";
  httpauth_manager->SetObserverAndDeliverCredentials(&mock_login_model_observer,
                                                     other_form);
  // The mock observer should not receive the stored credentials.
  EXPECT_CALL(mock_login_model_observer, OnAutofillDataAvailable(_, _))
      .Times(0);

  // Now wait until the navigation to the test server causes a HTTP auth dialog
  // to appear.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), http_test_server.GetURL("/basic_auth")));
  ASSERT_TRUE(base::test::RunUntil(
      []() { return LoginHandler::GetAllLoginHandlersForTest().size() == 1; }));

  // The auth dialog caused a query to PasswordStore, make sure it was
  // processed.
  WaitForPasswordStore();

  httpauth_manager->DetachObserver(&mock_login_model_observer);
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, ProxyAuthFilling) {
  GURL test_page = embedded_test_server()->GetURL("/auth-basic");

  // Save credentials for "testrealm" in the store.
  password_manager::TestPasswordStore* password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  password_manager::PasswordForm creds;
  creds.scheme = password_manager::PasswordForm::Scheme::kBasic;
  creds.url = test_page;
  creds.signon_realm = embedded_test_server()->base_url().spec() + "testrealm";
  creds.password_value = u"pw";
  creds.username_value = u"temp";
  password_store->AddLogin(creds);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page));
  ASSERT_TRUE(base::test::RunUntil(
      []() { return LoginHandler::GetAllLoginHandlersForTest().size() == 1; }));

  BubbleObserver(WebContents()).WaitForManagementState();
}

// Test whether the password form which is loaded as hidden is autofilled
// correctly. This happens very often in situations when in order to sign-in the
// user clicks a sign-in button and a hidden passsword form becomes visible.
// This test differs from AutofillSuggestionsForProblematicPasswordForm in that
// the form is hidden and in that test only some fields are hidden.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       AutofillSuggestionsHiddenPasswordForm) {
  // At first let us save credentials to the PasswordManager.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm login_form;
  login_form.signon_realm = embedded_test_server()->base_url().spec();
  login_form.action = embedded_test_server()->GetURL("/password/done.html");
  login_form.username_value = u"myusername";
  login_form.password_value = u"mypassword";
  password_store->AddLogin(login_form);

  // Now, navigate to the hidden password form and verify whether username and
  // password is autofilled.
  NavigateToFile("/password/password_form.html");

  // Let the user interact with the page, so that DOM gets modification events,
  // needed for autofilling the password.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));

  WaitForElementValue("hidden_password_form_username", "myusername");
  WaitForElementValue("hidden_password_form_password", "mypassword");
}

// Test whether the password form with the problematic invisible password field
// gets autofilled correctly.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       AutofillSuggestionsForProblematicPasswordForm) {
  // At first let us save credentials to the PasswordManager.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm login_form;
  login_form.signon_realm = embedded_test_server()->base_url().spec();
  login_form.action = embedded_test_server()->GetURL("/password/done.html");
  login_form.username_value = u"myusername";
  login_form.password_value = u"mypassword";
  password_store->AddLogin(login_form);

  // Now, navigate to the password form with a hidden password field and verify
  // whether username and password is autofilled.
  NavigateToFile("/password/password_form.html");

  // Let the user interact with the page, so that DOM gets modification events,
  // needed for autofilling the password.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));

  WaitForElementValue("form_with_hidden_password_username", "myusername");
  WaitForElementValue("form_with_hidden_password_password", "mypassword");
}

// Test whether the password form with the problematic invisible password field
// in ambiguous password form gets autofilled correctly.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       AutofillSuggestionsForProblematicAmbiguousPasswordForm) {
  // At first let us save credentials to the PasswordManager.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm login_form;
  login_form.signon_realm = embedded_test_server()->base_url().spec();
  login_form.action = embedded_test_server()->GetURL("/password/done.html");
  login_form.username_value = u"myusername";
  login_form.password_value = u"mypassword";
  password_store->AddLogin(login_form);

  // Now, navigate to the password form having ambiguous Ids for username and
  // password fields and verify whether username and password is autofilled.
  NavigateToFile("/password/ambiguous_password_form.html");

  // Let the user interact with the page, so that DOM gets modification events,
  // needed for autofilling fields.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));

  WaitForElementValue("hidden_password_form", 0 /* elements_index */,
                      "myusername");
  WaitForElementValue("hidden_password_form", 2 /* elements_index */,
                      "mypassword");
}

// Check that the internals page contains logs from the renderer.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       DISABLED_InternalsPage_Renderer) {
  // The test is flaky with same-site back/forward cache (which is enabled by
  // default).
  // TODO(crbug.com/40808799): Investigate and fix this.
  content::DisableBackForwardCacheForTesting(
      WebContents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // Open the internals page.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://password-manager-internals"),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* internals_web_contents = WebContents();

  // The renderer is supposed to ask whether logging is available. To avoid
  // race conditions between the answer "Logging is available" arriving from
  // the browser and actual logging callsites reached in the renderer, open
  // first an arbitrary page to ensure that the renderer queries the
  // availability of logging and has enough time to receive the answer.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/password/done.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* forms_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Now navigate to another page, containing some forms, so that the renderer
  // attempts to log. It should be a different page than the current one,
  // because just reloading the current one sometimes confused the Wait() call
  // and lead to timeouts (https://crbug.com/804398).
  PasswordsNavigationObserver observer(forms_web_contents);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/password/password_form.html"),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_TRUE(observer.Wait());

  std::string find_logs =
      "var text = document.getElementById('log-entries').innerText;"
      "var logs_found = /PasswordAutofillAgent::/.test(text);"
      "logs_found;";
  EXPECT_EQ(true, content::EvalJs(internals_web_contents->GetPrimaryMainFrame(),
                                  find_logs,
                                  content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// Check that the internals page contains logs from the browser.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, InternalsPage_Browser) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://password-manager-internals"),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* internals_web_contents = WebContents();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/password/password_form.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  std::string find_logs =
      "var text = document.getElementById('log-entries').innerText;"
      "var logs_found = /PasswordManager::/.test(text);"
      "logs_found;";
  EXPECT_EQ(true, content::EvalJs(internals_web_contents->GetPrimaryMainFrame(),
                                  find_logs,
                                  content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// Tests that submitted credentials are saved on a password form without
// username element when there are no stored credentials.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PasswordRetryFormSaveNoUsernameCredentials) {
  // Check that password save bubble is shown.
  NavigateToFile("/password/password_form.html");
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('retry_password_field').value = 'pw';"
      "document.getElementById('retry_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
  prompt_observer.AcceptSavePrompt();

  WaitForPasswordStore();
  CheckThatCredentialsStored("", "pw");
}

// Tests that no bubble shown when a password form without username submitted
// and there is stored credentials with the same password.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PasswordRetryFormNoBubbleWhenPasswordTheSame) {
  // At first let us save credentials to the PasswordManager.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.username_value = u"temp";
  signin_form.password_value = u"pw";
  password_store->AddLogin(signin_form);
  signin_form.username_value = u"temp1";
  signin_form.password_value = u"pw1";
  password_store->AddLogin(signin_form);

  // Check that no password bubble is shown when the submitted password is the
  // same in one of the stored credentials.
  NavigateToFile("/password/password_form.html");
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('retry_password_field').value = 'pw';"
      "document.getElementById('retry_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
  EXPECT_FALSE(prompt_observer.IsUpdatePromptShownAutomatically());
}

// Tests that the update bubble shown when a password form without username is
// submitted and there are stored credentials but with different password.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PasswordRetryFormUpdateBubbleShown) {
  // At first let us save credentials to the PasswordManager.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.username_value = u"temp";
  signin_form.password_value = u"pw";
  password_store->AddLogin(signin_form);

  // Check that password update bubble is shown.
  NavigateToFile("/password/password_form.html");
  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('retry_password_field').value = 'new_pw';"
      "document.getElementById('retry_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  // The new password "new_pw" is used, so update prompt is expected.
  EXPECT_TRUE(prompt_observer.IsUpdatePromptShownAutomatically());

  prompt_observer.AcceptUpdatePrompt();

  WaitForPasswordStore();
  CheckThatCredentialsStored("temp", "new_pw");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       NoCrashWhenNavigatingWithOpenAccountPicker) {
  // Save credentials with 'skip_zero_click'.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.password_value = u"password";
  signin_form.username_value = u"user";
  signin_form.url = embedded_test_server()->base_url();
  signin_form.skip_zero_click = true;
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/password_form.html");

  // Call the API to trigger the notification to the client, which raises the
  // account picker dialog.
  ASSERT_TRUE(content::ExecJs(WebContents(),
                              "navigator.credentials.get({password: true})",
                              content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Navigate while the picker is open.
  NavigateToFile("/password/password_form.html");

  // No crash!
}

// Tests that the prompt to save the password is still shown if the fields have
// the "autocomplete" attribute set off.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       PromptForSubmitWithAutocompleteOff) {
  NavigateToFile("/password/password_autocomplete_off_test.html");

  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username').value = 'temp';"
      "document.getElementById('password').value = 'random';"
      "document.getElementById('submit').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
}

IN_PROC_BROWSER_TEST_F(
    PasswordManagerBrowserTest,
    SkipZeroClickNotToggledAfterSuccessfulSubmissionWithAPI) {
  // Save credentials with 'skip_zero_click'
  password_manager::TestPasswordStore* password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.password_value = u"password";
  signin_form.username_value = u"user";
  signin_form.url = embedded_test_server()->base_url();
  signin_form.skip_zero_click = true;
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/password_form.html");

  // Call the API to trigger the notification to the client.
  ASSERT_TRUE(content::ExecJs(
      WebContents(),
      "navigator.credentials.get({password: true, unmediated: true })",
      content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit_change_password =
      "document.getElementById('username_field').value = 'user';"
      "document.getElementById('password_field').value = 'password';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit_change_password));
  ASSERT_TRUE(observer.Wait());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());

  // Verify that the form's 'skip_zero_click' is not updated.
  auto& passwords_map = password_store->stored_passwords();
  ASSERT_EQ(1u, passwords_map.size());
  auto& passwords_vector = passwords_map.begin()->second;
  ASSERT_EQ(1u, passwords_vector.size());
  const password_manager::PasswordForm& form = passwords_vector[0];
  EXPECT_EQ(u"user", form.username_value);
  EXPECT_EQ(u"password", form.password_value);
  EXPECT_TRUE(form.skip_zero_click);
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       SkipZeroClickNotToggledAfterSuccessfulAutofill) {
  // Save credentials with 'skip_zero_click'
  password_manager::TestPasswordStore* password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.password_value = u"password";
  signin_form.username_value = u"user";
  signin_form.url = embedded_test_server()->base_url();
  signin_form.skip_zero_click = true;
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/password_form.html");

  // No API call.

  PasswordsNavigationObserver observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string fill_and_submit_change_password =
      "document.getElementById('username_field').value = 'user';"
      "document.getElementById('password_field').value = 'password';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit_change_password));
  ASSERT_TRUE(observer.Wait());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());

  // Verify that the form's 'skip_zero_click' is not updated.
  auto& passwords_map = password_store->stored_passwords();
  ASSERT_EQ(1u, passwords_map.size());
  auto& passwords_vector = passwords_map.begin()->second;
  ASSERT_EQ(1u, passwords_vector.size());
  const password_manager::PasswordForm& form = passwords_vector[0];
  EXPECT_EQ(u"user", form.username_value);
  EXPECT_EQ(u"password", form.password_value);
  EXPECT_TRUE(form.skip_zero_click);
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, ReattachWebContents) {
  auto detached_web_contents = content::WebContents::Create(
      content::WebContents::CreateParams(WebContents()->GetBrowserContext()));
  PasswordsNavigationObserver observer(detached_web_contents.get());
  detached_web_contents->GetController().LoadURL(
      embedded_test_server()->GetURL("/password/multi_frames.html"),
      content::Referrer(), ::ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
  ASSERT_TRUE(observer.Wait());
  // Ensure that there is at least one more frame created than just the main
  // frame.
  EXPECT_LT(1u,
            CollectAllRenderFrameHosts(detached_web_contents->GetPrimaryPage())
                .size());

  auto* tab_strip_model = browser()->tab_strip_model();
  // Check that the autofill and password manager driver factories are notified
  // about all frames, not just the main one. The factories should receive
  // messages for non-main frames, in particular
  // AutofillHostMsg_PasswordFormsParsed. If that were the first time the
  // factories hear about such frames, this would crash.
  tab_strip_model->AddWebContents(std::move(detached_web_contents), -1,
                                  ::ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                  AddTabTypes::ADD_ACTIVE);
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       FillWhenFormWithHiddenUsername) {
  // At first let us save a credential to the password store.
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.url = embedded_test_server()->base_url();
  signin_form.username_value = u"current_username";
  signin_form.password_value = u"current_username_password";
  password_store->AddLogin(signin_form);
  signin_form.username_value = u"last_used_username";
  signin_form.password_value = u"last_used_password";
  signin_form.date_last_used = base::Time::Now();
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/hidden_username.html");

  // Let the user interact with the page.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));

  // current_username is hardcoded in the invisible text on the page so
  // current_username_password should be filled rather than last_used_password.
  WaitForElementValue("password", "current_username_password");
}

// Harness for showing dialogs as part of the DialogBrowserTest suite.
// Test params:
//  - bool popup_views_enabled: whether feature AutofillExpandedPopupViews
//        is enabled for testing.
class PasswordManagerDialogBrowserTest
    : public SupportsTestDialog<PasswordManagerBrowserTestBase> {
 public:
  PasswordManagerDialogBrowserTest() = default;

  PasswordManagerDialogBrowserTest(const PasswordManagerDialogBrowserTest&) =
      delete;
  PasswordManagerDialogBrowserTest& operator=(
      const PasswordManagerDialogBrowserTest&) = delete;

  void ShowUi(const std::string& name) override {
    // Note regarding flakiness: LocationBarBubbleDelegateView::ShowForReason()
    // uses ShowInactive() unless the bubble is invoked with reason ==
    // USER_GESTURE. This means that, so long as these dialogs are not triggered
    // by gesture, the dialog does not attempt to take focus, and so should
    // never _lose_ focus in the test, which could cause flakes when tests are
    // run in parallel. LocationBarBubbles also dismiss on other events, but
    // only events in the WebContents. E.g. Rogue mouse clicks should not cause
    // the dialog to dismiss since they won't be sent via WebContents.
    // A user gesture is determined in browser_commands.cc by checking
    // ManagePasswordsUIController::IsAutomaticallyOpeningBubble(), but that's
    // set and cleared immediately while showing the bubble, so it can't be
    // checked here.
    NavigateToFile("/password/password_form.html");
    PasswordsNavigationObserver observer(WebContents());
    std::string fill_and_submit =
        "document.getElementById('username_field').value = 'temp';"
        "document.getElementById('password_field').value = 'random';"
        "document.getElementById('input_submit_button').click()";
    ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
    ASSERT_TRUE(observer.Wait());
  }
};

IN_PROC_BROWSER_TEST_F(PasswordManagerDialogBrowserTest, InvokeUi_normal) {
  ShowAndVerifyUi();
}

// Verify that password manager ignores passwords on forms injected into
// about:blank frames.  See https://crbug.com/756587.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, AboutBlankFramesAreIgnored) {
  // Start from a page without a password form.
  NavigateToFile("/password/other.html");

  // Add a blank iframe and then inject a password form into it.
  BubbleObserver prompt_observer(WebContents());
  GURL submit_url(embedded_test_server()->GetURL("/password/done.html"));
  InjectBlankFrameWithPasswordForm(WebContents(), submit_url);
  content::RenderFrameHost* frame =
      ChildFrameAt(WebContents()->GetPrimaryMainFrame(), 0);
  EXPECT_EQ(GURL(url::kAboutBlankURL), frame->GetLastCommittedURL());
  EXPECT_TRUE(frame->IsRenderFrameLive());
  EXPECT_FALSE(prompt_observer.IsSavePromptAvailable());

  // Fill in the password and submit the form.  This shouldn't bring up a save
  // password prompt and shouldn't result in a renderer kill.
  SubmitInjectedPasswordForm(WebContents(), frame, submit_url);
  EXPECT_TRUE(frame->IsRenderFrameLive());
  EXPECT_EQ(submit_url, frame->GetLastCommittedURL());
  EXPECT_FALSE(prompt_observer.IsSavePromptAvailable());
}

// Verify that password manager ignores passwords on forms injected into
// about:blank popups.  See https://crbug.com/756587.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, AboutBlankPopupsAreIgnored) {
  // Start from a page without a password form.
  NavigateToFile("/password/other.html");

  // Open an about:blank popup and inject the password form into it.
  ui_test_utils::TabAddedWaiter tab_add(browser());
  GURL submit_url(embedded_test_server()->GetURL("/password/done.html"));
  std::string form_html = GeneratePasswordFormForAction(submit_url);
  std::string open_blank_popup_with_password_form =
      "var w = window.open('about:blank');"
      "w.document.body.innerHTML = \"" +
      form_html + "\";";
  ASSERT_TRUE(
      content::ExecJs(WebContents(), open_blank_popup_with_password_form));
  tab_add.Wait();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* newtab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Submit the password form and check that there was no renderer kill and no
  BubbleObserver prompt_observer(WebContents());
  SubmitInjectedPasswordForm(newtab, newtab->GetPrimaryMainFrame(), submit_url);
  EXPECT_FALSE(prompt_observer.IsSavePromptAvailable());
  EXPECT_TRUE(newtab->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_EQ(submit_url, newtab->GetPrimaryMainFrame()->GetLastCommittedURL());
}

// Verify that previously saved passwords for about:blank frames are not used
// for autofill.  See https://crbug.com/756587.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       ExistingAboutBlankPasswordsAreNotUsed) {
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  signin_form.url = GURL(url::kAboutBlankURL);
  signin_form.signon_realm = "about:";
  GURL submit_url(embedded_test_server()->GetURL("/password/done.html"));
  signin_form.action = submit_url;
  signin_form.password_value = u"pa55w0rd";
  password_store->AddLogin(signin_form);

  // Start from a page without a password form.
  NavigateToFile("/password/other.html");

  // Inject an about:blank frame with password form.
  InjectBlankFrameWithPasswordForm(WebContents(), submit_url);
  content::RenderFrameHost* frame =
      ChildFrameAt(WebContents()->GetPrimaryMainFrame(), 0);
  EXPECT_EQ(GURL(url::kAboutBlankURL), frame->GetLastCommittedURL());

  // Simulate user interaction in the iframe which normally triggers
  // autofill. Click in the middle of the frame to avoid the border.
  content::SimulateMouseClickOrTapElementWithId(WebContents(), "iframe");

  // Verify password is not autofilled.  Blink has a timer for 0.3 seconds
  // before it updates the browser with the new dynamic form, so wait long
  // enough for this timer to fire before checking the password.  Note that we
  // can't wait for any other events here, because when the test passes, there
  // should be no password manager IPCs sent from the renderer to browser.
  EXPECT_EQ(
      "",
      content::EvalJs(
          frame,
          "new Promise(resolve => {"
          "    setTimeout(function() {"
          "        resolve(document.getElementById('password_field').value);"
          "    }, 1000);"
          "});",
          content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  EXPECT_TRUE(frame->IsRenderFrameLive());
}

// Verify that there is no renderer kill when filling out a password on a
// subframe with a data: URL.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       NoRendererKillWithDataURLFrames) {
  // Start from a page without a password form.
  NavigateToFile("/password/other.html");

  // Add a iframe with a data URL that has a password form.
  BubbleObserver prompt_observer(WebContents());
  GURL submit_url(embedded_test_server()->GetURL("/password/done.html"));
  std::string form_html = GeneratePasswordFormForAction(submit_url);
  std::string inject_data_frame_with_password_form =
      "var frame = document.createElement('iframe');\n"
      "frame.src = \"data:text/html," +
      form_html +
      "\";\n"
      "document.body.appendChild(frame);\n";
  ASSERT_TRUE(
      content::ExecJs(WebContents(), inject_data_frame_with_password_form));
  EXPECT_TRUE(content::WaitForLoadStop(WebContents()));
  content::RenderFrameHost* frame =
      ChildFrameAt(WebContents()->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(frame->GetLastCommittedURL().SchemeIs(url::kDataScheme));
  EXPECT_TRUE(frame->IsRenderFrameLive());
  EXPECT_FALSE(prompt_observer.IsSavePromptAvailable());

  // Fill in the password and submit the form.  This shouldn't bring up a save
  // password prompt and shouldn't result in a renderer kill.
  SubmitInjectedPasswordForm(WebContents(), frame, submit_url);
  // After navigation, the RenderFrameHost may change.
  frame = ChildFrameAt(WebContents()->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(frame->IsRenderFrameLive());
  EXPECT_EQ(submit_url, frame->GetLastCommittedURL());
  EXPECT_FALSE(prompt_observer.IsSavePromptAvailable());
}

// Verify that there is no renderer kill when filling out a password on a
// blob: URL.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       NoRendererKillWithBlobURLFrames) {
  // Start from a page without a password form.
  NavigateToFile("/password/other.html");

  GURL submit_url(embedded_test_server()->GetURL("/password/done.html"));
  std::string form_html = GeneratePasswordFormForAction(submit_url);
  std::string navigate_to_blob_url =
      "location.href = URL.createObjectURL(new Blob([\"" + form_html +
      "\"], { type: 'text/html' }));";
  PasswordsNavigationObserver observer(WebContents());
  ASSERT_TRUE(content::ExecJs(WebContents(), navigate_to_blob_url));
  ASSERT_TRUE(observer.Wait());

  // Fill in the password and submit the form.  This shouldn't bring up a save
  // password prompt and shouldn't result in a renderer kill.
  std::string fill_and_submit =
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('testform').submit();";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  EXPECT_FALSE(BubbleObserver(WebContents()).IsSavePromptAvailable());
}

// Test that for HTTP auth (i.e., credentials not put through web forms) the
// password manager works even though it should be disabled on the previous
// page.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, CorrectEntryForHttpAuth) {
  // The embedded_test_server() is already started at this point and adding
  // the request handler to it would not be thread safe. Therefore, use a new
  // server.
  net::EmbeddedTestServer http_test_server;

  // Teach the embedded server to handle requests by issuing the basic auth
  // challenge.
  http_test_server.RegisterRequestHandler(
      base::BindRepeating(&HandleTestAuthRequest));
  ASSERT_TRUE(http_test_server.Start());

  // Navigate to about:blank first. This is a page where password manager
  // should not work.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Navigate to a page requiring HTTP auth
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), http_test_server.GetURL("/basic_auth")));
  ASSERT_TRUE(base::test::RunUntil(
      []() { return LoginHandler::GetAllLoginHandlersForTest().size() == 1; }));

  PasswordsNavigationObserver nav_observer(WebContents());
  // Offer valid credentials on the auth challenge.
  ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());
  LoginHandler* handler = *LoginHandler::GetAllLoginHandlersForTest().begin();
  ASSERT_TRUE(handler);
  // Any username/password will work.
  handler->SetAuth(u"user", u"pwd");

  // The password manager should be working correctly.
  ASSERT_TRUE(nav_observer.Wait());
  WaitForPasswordStore();
  BubbleObserver bubble_observer(WebContents());
  EXPECT_TRUE(bubble_observer.IsSavePromptShownAutomatically());
}

// Test that if HTTP auth login (i.e., credentials not put through web forms)
// succeeds, and there is a blocklisted entry with the HTML PasswordForm::Scheme
// for that origin, then the bubble is shown.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       HTTPAuthRealmAfterHTMLBlocklistedIsNotBlocked) {
  // The embedded_test_server() is already started at this point and adding
  // the request handler to it would not be thread safe. Therefore, use a new
  // server.
  net::EmbeddedTestServer http_test_server;

  // Teach the embedded server to handle requests by issuing the basic auth
  // challenge.
  http_test_server.RegisterRequestHandler(
      base::BindRepeating(&HandleTestAuthRequest));
  ASSERT_TRUE(http_test_server.Start());

  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();

  password_manager::PasswordForm blocked_form;
  blocked_form.scheme = password_manager::PasswordForm::Scheme::kHtml;
  blocked_form.signon_realm = http_test_server.base_url().spec();
  blocked_form.url = http_test_server.base_url();
  blocked_form.blocked_by_user = true;
  password_store->AddLogin(blocked_form);

  // Navigate to a page requiring HTTP auth.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), http_test_server.GetURL("/basic_auth")));
  ASSERT_TRUE(base::test::RunUntil(
      []() { return LoginHandler::GetAllLoginHandlersForTest().size() == 1; }));

  PasswordsNavigationObserver nav_observer(WebContents());

  ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());
  LoginHandler* handler = *LoginHandler::GetAllLoginHandlersForTest().begin();
  ASSERT_TRUE(handler);
  // Any username/password will work.
  handler->SetAuth(u"user", u"pwd");
  ASSERT_TRUE(nav_observer.Wait());
  WaitForPasswordStore();
  EXPECT_TRUE(BubbleObserver(WebContents()).IsSavePromptShownAutomatically());
}

// Test that if HTML login succeeds, and there is a blocklisted entry
// with the HTTP auth PasswordForm::Scheme (i.e., credentials not put
// through web forms) for that origin, then the bubble is shown.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       HTMLLoginAfterHTTPAuthBlocklistedIsNotBlocked) {
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();

  password_manager::PasswordForm blocked_form;
  blocked_form.scheme = password_manager::PasswordForm::Scheme::kBasic;
  blocked_form.signon_realm =
      embedded_test_server()->base_url().spec() + "test realm";
  blocked_form.url = embedded_test_server()->base_url();
  blocked_form.blocked_by_user = true;
  password_store->AddLogin(blocked_form);

  NavigateToFile("/password/password_form.html");
  PasswordsNavigationObserver observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'pw';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  BubbleObserver bubble_observer(WebContents());
  EXPECT_TRUE(bubble_observer.IsSavePromptShownAutomatically());
}

// Tests that "blocklist site" feature works for the basic scenario.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       HTMLLoginAfterHTMLBlocklistedIsBlocklisted) {
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();

  password_manager::PasswordForm blocked_form;
  blocked_form.scheme = password_manager::PasswordForm::Scheme::kHtml;
  blocked_form.signon_realm = embedded_test_server()->base_url().spec();
  blocked_form.url = embedded_test_server()->base_url();
  blocked_form.blocked_by_user = true;
  password_store->AddLogin(blocked_form);

  NavigateToFile("/password/password_form.html");
  PasswordsNavigationObserver observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'pw';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());
  BubbleObserver bubble_observer(WebContents());
  EXPECT_FALSE(bubble_observer.IsSavePromptShownAutomatically());
  EXPECT_TRUE(bubble_observer.IsSavePromptAvailable());
}

// This test emulates what was observed in https://crbug.com/856543: Imagine the
// user stores a single username/password pair on origin A, and later submits a
// username-less password-reset form on origin B. In the bug, A and B were
// PSL-matches (different, but with the same eTLD+1), and Chrome ended up
// overwriting the old password with the new one. This test checks that update
// bubble is shown instead of silent update.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       NoSilentOverwriteOnPSLMatch) {
  // Store a password at origin A.
  const GURL url_A = embedded_test_server()->GetURL("abc.foo.com", "/");
  password_manager::TestPasswordStore* password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = url_A.DeprecatedGetOriginAsURL().spec();
  signin_form.url = url_A;
  signin_form.username_value = u"user";
  signin_form.password_value = u"oldpassword";
  password_store->AddLogin(signin_form);
  WaitForPasswordStore();

  // Visit origin B with a form only containing new- and confirmation-password
  // fields.
  GURL url_B = embedded_test_server()->GetURL(
      "www.foo.com", "/password/new_password_form.html");
  PasswordsNavigationObserver observer_B(WebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_B));
  ASSERT_TRUE(observer_B.Wait());

  // Fill in the new password and submit.
  GURL url_done =
      embedded_test_server()->GetURL("www.foo.com", "/password/done.html");
  PasswordsNavigationObserver observer_done(WebContents());
  observer_done.SetPathToWaitFor("/password/done.html");
  ASSERT_TRUE(content::ExecJs(
      RenderFrameHost(),
      "document.getElementById('new_p').value = 'new password';"
      "document.getElementById('conf_p').value = 'new password';"
      "document.getElementById('testform').submit();",
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  ASSERT_TRUE(observer_done.Wait());

  // Check that the password for origin A was not updated automatically and the
  // update bubble is shown instead.
  WaitForPasswordStore();  // Let the navigation take its effect on storing.
  ASSERT_THAT(password_store->stored_passwords(),
              ElementsAre(testing::Key(url_A.DeprecatedGetOriginAsURL())));
  CheckThatCredentialsStored("user", "oldpassword");
  BubbleObserver prompt_observer(WebContents());
  EXPECT_TRUE(prompt_observer.IsUpdatePromptShownAutomatically());

  // Check that the password is updated correctly if the user clicks Update.
  prompt_observer.AcceptUpdatePrompt();

  WaitForPasswordStore();
  // The stored credential has been updated with the new password.
  const auto& passwords_map = password_store->stored_passwords();
  ASSERT_THAT(passwords_map,
              ElementsAre(testing::Key(url_A.DeprecatedGetOriginAsURL())));
  for (const auto& credentials : passwords_map) {
    ASSERT_THAT(credentials.second, testing::SizeIs(1));
    EXPECT_EQ(u"user", credentials.second[0].username_value);
    EXPECT_EQ(u"new password", credentials.second[0].password_value);
  }
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       NoFillGaiaReauthenticationForm) {
  password_manager::TestPasswordStore* password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());

  // Visit Gaia reath page.
  const GURL url = https_test_server().GetURL("accounts.google.com",
                                              "/password/gaia_reath_form.html");
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = url.GetWithEmptyPath().spec();
  signin_form.url = url.GetWithEmptyPath();
  signin_form.username_value = u"user";
  signin_form.password_value = u"password123";
  password_store->AddLogin(signin_form);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Check that no autofill happened.
  content::SimulateMouseClick(WebContents(), 0,
                              blink::WebMouseEvent::Button::kLeft);
  CheckElementValue("identifier", "");
  CheckElementValue("password", "");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       NoFillGaiaWithSkipSavePasswordForm) {
  password_manager::TestPasswordStore* password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());

  // Visit Gaia form with ssp=1 as query (ssp stands for Skip Save Password).
  const GURL url = https_test_server().GetURL(
      "accounts.google.com", "/password/password_form.html?ssp=1");

  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = url.GetWithEmptyPath().spec();
  signin_form.url = url.GetWithEmptyPath();
  signin_form.username_value = u"user";
  signin_form.password_value = u"password123";
  password_store->AddLogin(signin_form);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Check that no autofill happened.
  content::SimulateMouseClick(WebContents(), 0,
                              blink::WebMouseEvent::Button::kLeft);
  CheckElementValue("username_field", "");
  CheckElementValue("password_field", "");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, FormDynamicallyChanged) {
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.username_value = u"temp";
  signin_form.password_value = u"pw";
  password_store->AddLogin(signin_form);

  // Check that password update bubble is shown.
  NavigateToFile("/password/simple_password.html");

  // Simulate that a script removes username/password elements and adds the
  // elements identical to them.
  ASSERT_TRUE(content::ExecJs(RenderFrameHost(),
                              "function replaceElement(id) {"
                              "  var elem = document.getElementById(id);"
                              "  var parent = elem.parentElement;"
                              "  var cloned_elem = elem.cloneNode();"
                              "  cloned_elem.value = '';"
                              "  parent.removeChild(elem);"
                              "  parent.appendChild(cloned_elem);"
                              "}"
                              "replaceElement('username_field');"
                              "replaceElement('password_field');",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Let the user interact with the page, so that DOM gets modification events,
  // needed for autofilling fields.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));

  WaitForElementValue("username_field", "temp");
  WaitForElementValue("password_field", "pw");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, ParserAnnotations) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      autofill::switches::kShowAutofillSignatures);
  NavigateToFile("/password/password_form.html");
  const char kGetAnnotation[] =
      "document.getElementById('%s').getAttribute('pm_parser_annotation');";

  EXPECT_EQ(
      "username_element",
      content::EvalJs(RenderFrameHost(),
                      base::StringPrintf(kGetAnnotation, "username_field"),
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  EXPECT_EQ(
      "password_element",
      content::EvalJs(RenderFrameHost(),
                      base::StringPrintf(kGetAnnotation, "password_field"),
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  EXPECT_EQ(
      "new_password_element",
      content::EvalJs(RenderFrameHost(),
                      base::StringPrintf(kGetAnnotation, "chg_new_password_1"),
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  EXPECT_EQ(
      "confirmation_password_element",
      content::EvalJs(RenderFrameHost(),
                      base::StringPrintf(kGetAnnotation, "chg_new_password_2"),
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// Test if |PasswordManager.FormVisited.PerProfileType| metrics are recorded as
// expected.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       ProfileTypeMetricSubmission) {
  base::HistogramTester histogram_tester;

  NavigateToFile("/password/simple_password.html");

  // Test if visit is properly recorded and submission is not marked.
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.FormVisited.PerProfileType",
      profile_metrics::BrowserProfileType::kRegular, 1);

  // Fill a form and submit through a <input type="submit"> button. Nothing
  // special.
  PasswordsNavigationObserver observer(WebContents());
  constexpr char kFillAndSubmit[] =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), kFillAndSubmit));
  ASSERT_TRUE(observer.Wait());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBackForwardCacheBrowserTest,
                       SavePasswordOnRestoredPage) {
  // Navigate to a page with a password form.
  NavigateToFile("/password/password_form.html");
  content::RenderFrameHostWrapper rfh(WebContents()->GetPrimaryMainFrame());

  // Navigate away so that the password form page is stored in the cache.
  ASSERT_TRUE(NavigateToURL(
      WebContents(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  ASSERT_EQ(rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Restore the cached page.
  ASSERT_TRUE(content::HistoryGoBack(WebContents()));
  ASSERT_EQ(rfh.get(), WebContents()->GetPrimaryMainFrame());

  // Fill out and submit the password form.
  PasswordsNavigationObserver observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());

  // Save the password and check the store.
  BubbleObserver bubble_observer(WebContents());
  ASSERT_TRUE(bubble_observer.IsSavePromptShownAutomatically());
  bubble_observer.AcceptSavePrompt();
  WaitForPasswordStore();

  CheckThatCredentialsStored("temp", "random");
}

// Test that if the credentials API is used, it makes the page ineligible for
// caching in the BackForwardCache.
//
// See where BackForwardCache::DisableForRenderFrameHost is called in
// chrome_password_manager_client.cc for explanation.
IN_PROC_BROWSER_TEST_F(PasswordManagerBackForwardCacheBrowserTest,
                       NotCachedIfCredentialsAPIUsed) {
  // Navigate to a page with a password form.
  NavigateToFile("/password/password_form.html");
  content::RenderFrameHostWrapper rfh(WebContents()->GetPrimaryMainFrame());

  // Use the password manager API, this should make the page uncacheable.
  ASSERT_TRUE(IsGetCredentialsSuccessful());

  // Navigate away.
  ASSERT_TRUE(NavigateToURL(
      WebContents(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  // The page should not have been cached.
  ASSERT_TRUE(rfh.WaitUntilRenderFrameDeleted());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBackForwardCacheBrowserTest,
                       CredentialsAPIOnlyCalledOnRestoredPage) {
  // Navigate to a page with a password form.
  NavigateToFile("/password/password_form.html");
  content::RenderFrameHostWrapper rfh(WebContents()->GetPrimaryMainFrame());

  // Navigate away.
  ASSERT_TRUE(NavigateToURL(
      WebContents(), embedded_test_server()->GetURL("b.com", "/title1.html")));
  ASSERT_EQ(rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Restore the cached page.
  ASSERT_TRUE(content::HistoryGoBack(WebContents()));
  ASSERT_EQ(rfh.get(), WebContents()->GetPrimaryMainFrame());

  // Make sure the password manager API works. Since it was never connected, it
  // shouldn't have been affected by the
  // ContentCredentialManager::DisconnectBinding call in
  // ChromePasswordManagerClient::DidFinishNavigation, (this GetCredentials call
  // will establish the mojo connection for the first time).
  ASSERT_TRUE(IsGetCredentialsSuccessful());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       DetectFormSubmissionOnIframe) {
  // Start from a page without a password form.
  NavigateToFile("/password/other.html");

  // Add a blank iframe and then inject a password form into it.
  BubbleObserver prompt_observer(WebContents());
  GURL current_url(embedded_test_server()->GetURL("/password/other.html"));
  GURL submit_url(embedded_test_server()->GetURL("/password/done.html"));
  InjectFrameWithPasswordForm(WebContents(), submit_url);
  content::RenderFrameHost* frame =
      ChildFrameAt(WebContents()->GetPrimaryMainFrame(), 0);
  EXPECT_EQ(GURL(url::kAboutBlankURL), frame->GetLastCommittedURL());
  EXPECT_EQ(submit_url.DeprecatedGetOriginAsURL(),
            frame->GetLastCommittedOrigin().GetURL());
  EXPECT_TRUE(frame->IsRenderFrameLive());
  EXPECT_FALSE(prompt_observer.IsSavePromptAvailable());

  // Fill in the password and submit the form. This should bring up a save
  // password prompt and shouldn't result in a renderer kill.
  SubmitInjectedPasswordForm(WebContents(), frame, submit_url);
  EXPECT_TRUE(frame->IsRenderFrameLive());
  EXPECT_TRUE(prompt_observer.IsSavePromptAvailable());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest,
                       ShowPasswordManagerNoBrowser) {
  // Create a WebContent without tab helpers so it has no associated browser.
  std::unique_ptr<content::WebContents> new_web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));

  // Verify that there is no browser.
  ASSERT_FALSE(chrome::FindBrowserWithTab(new_web_contents.get()));

  // Create ChromePasswordManagerClient for newly created web_contents.
  autofill::ChromeAutofillClient::CreateForWebContents(new_web_contents.get());
  ChromePasswordManagerClient::CreateForWebContents(new_web_contents.get());

  ChromePasswordManagerClient* client =
      ChromePasswordManagerClient::FromWebContents(new_web_contents.get());
  ASSERT_TRUE(client);
  ASSERT_NO_FATAL_FAILURE(client->NavigateToManagePasswordsPage(
      password_manager::ManagePasswordsReferrer::kPasswordsGoogleWebsite));
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTest, FormWithoutTextInputs) {
  base::HistogramTester histogram_tester;

  // Navigate to a page with a form without text inputs.
  NavigateToFile("/password/no_text_inputs.html");

  // Submit the form.
  PasswordsNavigationObserver observer(WebContents());
  std::string submit_pw_form =
      "document.getElementById('input_submit_button').click();";
  ASSERT_TRUE(content::ExecJs(WebContents(), submit_pw_form));
  ASSERT_TRUE(observer.Wait());

  // Verify that no form was seen on the browser side.
  histogram_tester.ExpectTotalCount(
      "PasswordManager.FormVisited.PerProfileType", 0);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// This test suite only applies to Gaia signin page, and checks that the
// signin interception bubble and the password bubbles never conflict.
class PasswordManagerBrowserTestWithSigninInterception
    : public PasswordManagerBrowserTest {
 public:
  PasswordManagerBrowserTestWithSigninInterception()
      : helper_(&https_test_server()) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PasswordManagerBrowserTest::SetUpCommandLine(command_line);
    helper_.SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    helper_.SetUpOnMainThread();
    PasswordManagerBrowserTest::SetUpOnMainThread();
  }

  void FillAndSubmitGaiaPassword() {
    PasswordsNavigationObserver observer(WebContents());
    std::string fill_and_submit = base::StringPrintf(
        "document.getElementById('username_field').value = '%s';"
        "document.getElementById('password_field').value = 'new_pw';"
        "document.getElementById('input_submit_button').click()",
        helper_.gaia_username().c_str());
    ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
    ASSERT_TRUE(observer.Wait());
  }

  // Gaia passwords can only be saved if they are a secondary account. Add
  // another dummy account in Chrome that acts as the primary.
  void SetupAccountsForSavingGaiaPassword() {
    CoreAccountId dummy_account = helper_.AddGaiaAccountToProfile(
        browser()->profile(), "dummy_email@example.com", "dummy_gaia_id");
    IdentityManagerFactory::GetForProfile(browser()->profile())
        ->GetPrimaryAccountMutator()
        ->SetPrimaryAccount(dummy_account, signin::ConsentLevel::kSignin);
  }

 protected:
  PasswordManagerSigninInterceptTestHelper helper_;
};

// Checks that password update suppresses signin interception.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTestWithSigninInterception,
                       InterceptionBubbleSuppressedByPasswordUpdate) {
  Profile* profile = browser()->profile();
  helper_.SetupProfilesForInterception(profile);
  // Prepopulate Gaia credentials to trigger an update bubble.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              profile, ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  helper_.StoreGaiaCredentials(password_store);

  helper_.NavigateToGaiaSigninPage(WebContents());

  // The stored password "pw" was overridden with "new_pw", so update prompt is
  // expected. Use the retry form, to avoid autofill.
  BubbleObserver prompt_observer(WebContents());

  PasswordsNavigationObserver observer(WebContents());
  std::string fill_and_submit =
      "document.getElementById('retry_password_field').value = 'new_pw';"
      "document.getElementById('retry_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());

  EXPECT_TRUE(prompt_observer.IsUpdatePromptShownAutomatically());

  // Complete the Gaia signin.
  CoreAccountId account_id = helper_.AddGaiaAccountToProfile(
      profile, helper_.gaia_email(), helper_.gaia_id());

  // Check that interception does not happen.
  base::HistogramTester histogram_tester;
  DiceWebSigninInterceptor* signin_interceptor =
      helper_.GetSigninInterceptor(profile);
  signin_interceptor->MaybeInterceptWebSignin(
      WebContents(), account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      /*is_new_account=*/true,
      /*is_sync_signin=*/false);
  EXPECT_FALSE(signin_interceptor->is_interception_in_progress());
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kAbortPasswordUpdate, 1);
}

// Checks that Gaia password can be saved when there is no interception.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTestWithSigninInterception,
                       SaveGaiaPassword) {
  SetupAccountsForSavingGaiaPassword();
  helper_.NavigateToGaiaSigninPage(WebContents());

  // Add the new password: triggers the save bubble.
  BubbleObserver prompt_observer(WebContents());
  FillAndSubmitGaiaPassword();
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());

  // Complete the Gaia signin.
  Profile* profile = browser()->profile();
  CoreAccountId account_id = helper_.AddGaiaAccountToProfile(
      profile, helper_.gaia_email(), helper_.gaia_id());

  // Check that interception does not happen.
  base::HistogramTester histogram_tester;
  DiceWebSigninInterceptor* signin_interceptor =
      helper_.GetSigninInterceptor(profile);
  signin_interceptor->MaybeInterceptWebSignin(
      WebContents(), account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      /*is_new_account=*/true,
      /*is_sync_signin=*/false);
  EXPECT_FALSE(signin_interceptor->is_interception_in_progress());
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kAbortProfileCreationDisallowed, 1);
}

// Checks that signin interception suppresses password save, if the form is
// processed before the signin completes.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTestWithSigninInterception,
                       SavePasswordSuppressedBeforeSignin) {
  Profile* profile = browser()->profile();
  helper_.SetupProfilesForInterception(profile);
  SetupAccountsForSavingGaiaPassword();
  helper_.NavigateToGaiaSigninPage(WebContents());

  // Add the new password, password bubble not triggered.
  BubbleObserver prompt_observer(WebContents());
  FillAndSubmitGaiaPassword();
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());

  // Complete the Gaia signin.
  CoreAccountId account_id = helper_.AddGaiaAccountToProfile(
      profile, helper_.gaia_email(), helper_.gaia_id());

  // Check that interception happens.
  base::HistogramTester histogram_tester;
  DiceWebSigninInterceptor* signin_interceptor =
      helper_.GetSigninInterceptor(profile);
  signin_interceptor->MaybeInterceptWebSignin(
      WebContents(), account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      /*is_new_account=*/true,
      /*is_sync_signin=*/false);
  EXPECT_TRUE(signin_interceptor->is_interception_in_progress());
}

// Checks that signin interception suppresses password save, if the form is
// processed after the signin completes.
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTestWithSigninInterception,
                       SavePasswordSuppressedAfterSignin) {
  Profile* profile = browser()->profile();
  helper_.SetupProfilesForInterception(profile);
  SetupAccountsForSavingGaiaPassword();
  helper_.NavigateToGaiaSigninPage(WebContents());

  // Complete the Gaia signin.
  CoreAccountId account_id = helper_.AddGaiaAccountToProfile(
      profile, helper_.gaia_email(), helper_.gaia_id());

  // Check that interception happens.
  base::HistogramTester histogram_tester;
  DiceWebSigninInterceptor* signin_interceptor =
      helper_.GetSigninInterceptor(profile);
  signin_interceptor->MaybeInterceptWebSignin(
      WebContents(), account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      /*is_new_account=*/true,
      /*is_sync_signin=*/false);
  EXPECT_TRUE(signin_interceptor->is_interception_in_progress());

  // Add the new password, password bubble not triggered.
  BubbleObserver prompt_observer(WebContents());
  FillAndSubmitGaiaPassword();
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}
#endif  // BUIDLFLAG(ENABLE_DICE_SUPPORT)

// This is for checking that we don't make unexpected calls to the password
// manager driver prior to activation and to permit checking that expected calls
// do happen after activation.
class MockPrerenderPasswordManagerDriver
    : public autofill::mojom::PasswordManagerDriverInterceptorForTesting {
 public:
  explicit MockPrerenderPasswordManagerDriver(
      password_manager::ContentPasswordManagerDriver* driver)
      : impl_(driver->ReceiverForTesting().SwapImplForTesting(this)) {
    DelegateToImpl();
  }
  MockPrerenderPasswordManagerDriver(
      const MockPrerenderPasswordManagerDriver&) = delete;
  MockPrerenderPasswordManagerDriver& operator=(
      const MockPrerenderPasswordManagerDriver&) = delete;
  ~MockPrerenderPasswordManagerDriver() override = default;

  autofill::mojom::PasswordManagerDriver* GetForwardingInterface() override {
    return impl_;
  }

  // autofill::mojom::PasswordManagerDriver
  MOCK_METHOD(void,
              PasswordFormsParsed,
              (const std::vector<autofill::FormData>& form_data),
              (override));
  MOCK_METHOD(void,
              PasswordFormsRendered,
              (const std::vector<autofill::FormData>& visible_form_data),
              (override));
  MOCK_METHOD(void,
              PasswordFormSubmitted,
              (const autofill::FormData& form_data),
              (override));
  MOCK_METHOD(void,
              InformAboutUserInput,
              (const autofill::FormData& form_data),
              (override));
  MOCK_METHOD(
      void,
      DynamicFormSubmission,
      (autofill::mojom::SubmissionIndicatorEvent submission_indication_event),
      (override));
  MOCK_METHOD(void,
              PasswordFormCleared,
              (const autofill::FormData& form_Data),
              (override));
  MOCK_METHOD(void,
              RecordSavePasswordProgress,
              (const std::string& log),
              (override));
  MOCK_METHOD(void, UserModifiedPasswordField, (), (override));
  MOCK_METHOD(void,
              UserModifiedNonPasswordField,
              (autofill::FieldRendererId renderer_id,
               const std::u16string& value,
               bool autocomplete_attribute_has_username,
               bool is_likely_otp),
              (override));
  MOCK_METHOD(void,
              ShowPasswordSuggestions,
              (const autofill::PasswordSuggestionRequest&),
              (override));
#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(void,
              ShowKeyboardReplacingSurface,
              (autofill::mojom::SubmissionReadinessState, bool),
              (override));
#endif
  MOCK_METHOD(void,
              CheckSafeBrowsingReputation,
              (const GURL& form_action, const GURL& frame_url),
              (override));
  MOCK_METHOD(void,
              FocusedInputChanged,
              (autofill::FieldRendererId focused_field_id,
               autofill::mojom::FocusedFieldType focused_field_type),
              (override));
  MOCK_METHOD(void,
              LogFirstFillingResult,
              (autofill::FormRendererId form_renderer_id, int32_t result),
              (override));

  void DelegateToImpl() {
    ON_CALL(*this, PasswordFormsParsed)
        .WillByDefault(
            [this](const std::vector<autofill::FormData>& form_data) {
              impl_->PasswordFormsParsed(form_data);
              RemoveWaitType(WAIT_FOR_PASSWORD_FORMS::WAIT_FOR_PARSED);
            });
    ON_CALL(*this, PasswordFormsRendered)
        .WillByDefault(
            [this](const std::vector<autofill::FormData>& visible_form_data) {
              impl_->PasswordFormsRendered(visible_form_data);
              RemoveWaitType(WAIT_FOR_PASSWORD_FORMS::WAIT_FOR_RENDERED);
            });
    ON_CALL(*this, PasswordFormSubmitted)
        .WillByDefault([this](const autofill::FormData& form_data) {
          impl_->PasswordFormSubmitted(form_data);
        });
    ON_CALL(*this, InformAboutUserInput)
        .WillByDefault([this](const autofill::FormData& form_data) {
          impl_->InformAboutUserInput(form_data);
        });
    ON_CALL(*this, DynamicFormSubmission)
        .WillByDefault([this](autofill::mojom::SubmissionIndicatorEvent
                                  submission_indication_event) {
          impl_->DynamicFormSubmission(submission_indication_event);
        });
    ON_CALL(*this, PasswordFormCleared)
        .WillByDefault([this](const autofill::FormData& form_Data) {
          impl_->PasswordFormCleared(form_Data);
        });
    ON_CALL(*this, RecordSavePasswordProgress)
        .WillByDefault([this](const std::string& log) {
          impl_->RecordSavePasswordProgress(log);
        });
    ON_CALL(*this, UserModifiedPasswordField).WillByDefault([this]() {
      impl_->UserModifiedPasswordField();
    });
    ON_CALL(*this, UserModifiedNonPasswordField)
        .WillByDefault([this](autofill::FieldRendererId renderer_id,
                              const std::u16string& value,
                              bool autocomplete_attribute_has_username,
                              bool is_likely_otp) {
          impl_->UserModifiedNonPasswordField(
              renderer_id, value, autocomplete_attribute_has_username,
              is_likely_otp);
        });
    ON_CALL(*this, ShowPasswordSuggestions)
        .WillByDefault(
            [this](const autofill::PasswordSuggestionRequest& request) {
              autofill::PasswordSuggestionRequest copy = request;
              copy.form_data = autofill::FormData();
              copy.username_field_index = 0;
              copy.password_field_index = 0;
              impl_->ShowPasswordSuggestions(copy);
            });
#if BUILDFLAG(IS_ANDROID)
    ON_CALL(*this, ShowKeyboardReplacingSurface)
        .WillByDefault([this](autofill::mojom::SubmissionReadinessState
                                  submission_readiness) {
          impl_->ShowKeyboardReplacingSurface(submission_readiness,
                                              /*is_webauthn=*/false);
        });
#endif
    ON_CALL(*this, CheckSafeBrowsingReputation)
        .WillByDefault([this](const GURL& form_action, const GURL& frame_url) {
          impl_->CheckSafeBrowsingReputation(form_action, frame_url);
        });
    ON_CALL(*this, FocusedInputChanged)
        .WillByDefault(
            [this](autofill::FieldRendererId focused_field_id,
                   autofill::mojom::FocusedFieldType focused_field_type) {
              impl_->FocusedInputChanged(focused_field_id, focused_field_type);
            });
    ON_CALL(*this, LogFirstFillingResult)
        .WillByDefault(
            [this](autofill::FormRendererId form_renderer_id, int32_t result) {
              impl_->LogFirstFillingResult(form_renderer_id, result);
            });
  }

  void WaitFor(uint32_t wait_type) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    wait_type_ = wait_type;
    run_loop.Run();
  }

  void WaitForPasswordFormParsedAndRendered() {
    WaitFor(WAIT_FOR_PASSWORD_FORMS::WAIT_FOR_PARSED |
            WAIT_FOR_PASSWORD_FORMS::WAIT_FOR_RENDERED);
  }

  enum WAIT_FOR_PASSWORD_FORMS {
    WAIT_FOR_NOTHING = 0,
    WAIT_FOR_PARSED = 1 << 0,    // Waits for PasswordFormsParsed().
    WAIT_FOR_RENDERED = 1 << 1,  // Waits for PasswordFormsRendered().
  };

 private:
  void RemoveWaitType(uint32_t arrived) {
    wait_type_ &= ~arrived;
    if (wait_type_ == WAIT_FOR_NOTHING && quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }
  base::OnceClosure quit_closure_;
  uint32_t wait_type_ = WAIT_FOR_NOTHING;
  raw_ptr<autofill::mojom::PasswordManagerDriver, AcrossTasksDanglingUntriaged>
      impl_ = nullptr;
};

class MockPrerenderPasswordManagerDriverInjector
    : public content::WebContentsObserver {
 public:
  explicit MockPrerenderPasswordManagerDriverInjector(
      content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~MockPrerenderPasswordManagerDriverInjector() override = default;

  MockPrerenderPasswordManagerDriver* GetMockForFrame(
      content::RenderFrameHost* rfh) {
    return static_cast<MockPrerenderPasswordManagerDriver*>(
        GetDriverForFrame(rfh)->ReceiverForTesting().impl());
  }

 private:
  password_manager::ContentPasswordManagerDriver* GetDriverForFrame(
      content::RenderFrameHost* rfh) {
    return password_manager::ContentPasswordManagerDriver::
        GetForRenderFrameHost(rfh);
  }

  // content::WebContentsObserver:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override {
    auto* rfh = navigation_handle->GetRenderFrameHost();
    if (navigation_handle->IsPrerenderedPageActivation() ||
        navigation_handle->IsSameDocument() ||
        rfh->GetLifecycleState() !=
            content::RenderFrameHost::LifecycleState::kPrerendering) {
      return;
    }
    mocks_.push_back(std::make_unique<
                     testing::StrictMock<MockPrerenderPasswordManagerDriver>>(
        GetDriverForFrame(navigation_handle->GetRenderFrameHost())));
  }

  std::vector<
      std::unique_ptr<testing::StrictMock<MockPrerenderPasswordManagerDriver>>>
      mocks_;
};

class PasswordManagerPrerenderBrowserTest : public PasswordManagerBrowserTest {
 public:
  PasswordManagerPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &PasswordManagerPrerenderBrowserTest::web_contents,
            base::Unretained(this))) {}
  ~PasswordManagerPrerenderBrowserTest() override = default;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    PasswordManagerBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Register requests handler before the server is started.
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleTestAuthRequest));

    PasswordManagerBrowserTest::SetUpOnMainThread();
  }

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

  void SendKey(::ui::KeyboardCode key,
               content::RenderFrameHost* render_frame_host) {
    blink::WebKeyboardEvent web_event(
        blink::WebKeyboardEvent::Type::kRawKeyDown,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());

    input::NativeWebKeyboardEvent event(web_event, gfx::NativeView());
    event.windows_key_code = key;
    render_frame_host->GetRenderWidgetHost()->ForwardKeyboardEvent(event);
  }

  // Adds a tab with ChromePasswordManagerClient.
  // Note that it doesn't use CustomManagePasswordsUIController and it's not
  // useful to test UI. After calling this,
  // PasswordManagerBrowserTest::WebContents() is not available.
  void GetNewTabWithPasswordManagerClient() {
    content::WebContents* preexisting_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    std::unique_ptr<content::WebContents> owned_web_contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(browser()->profile()));
    ASSERT_TRUE(owned_web_contents.get());

    // ManagePasswordsUIController needs ChromePasswordManagerClient for
    // logging.
    autofill::ChromeAutofillClient::CreateForWebContents(
        owned_web_contents.get());
    ChromePasswordManagerClient::CreateForWebContents(owned_web_contents.get());
    ASSERT_TRUE(
        ChromePasswordManagerClient::FromWebContents(owned_web_contents.get()));
    ManagePasswordsUIController::CreateForWebContents(owned_web_contents.get());
    ASSERT_TRUE(
        ManagePasswordsUIController::FromWebContents(owned_web_contents.get()));
    ASSERT_FALSE(owned_web_contents.get()->IsLoading());
    browser()->tab_strip_model()->AppendWebContents(
        std::move(owned_web_contents), true);
    if (preexisting_tab) {
      ClearWebContentsPtr();
      browser()->tab_strip_model()->CloseWebContentsAt(
          0, TabCloseTypes::CLOSE_NONE);
    }
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

// Tests that the prerender doesn't proceed HTTP auth login and once the page
// is loaded as the primary page the prompt is shown. As the page is
// not loaded from the prerender, it also checks if it's not activated from the
// prerender.
IN_PROC_BROWSER_TEST_F(PasswordManagerPrerenderBrowserTest,
                       ChromePasswordManagerClientInPrerender) {
  MockPrerenderPasswordManagerDriverInjector injector(WebContents());

  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::test::PrerenderHostRegistryObserver registry_observer(
      *WebContents());
  auto prerender_url = embedded_test_server()->GetURL("/basic_auth");

  // Loads a page requiring HTTP auth in the prerender.
  prerender_helper()->AddPrerenderAsync(prerender_url);

  // Ensure that the prerender has started.
  registry_observer.WaitForTrigger(prerender_url);
  content::FrameTreeNodeId prerender_id =
      prerender_helper()->GetHostForUrl(prerender_url);
  EXPECT_TRUE(prerender_id);
  content::test::PrerenderHostObserver host_observer(*WebContents(),
                                                     prerender_id);
  // PrerenderHost is destroyed by net::INVALID_AUTH_CREDENTIALS and it stops
  // prerendering.
  host_observer.WaitForDestroyed();

  BubbleObserver bubble_observer(WebContents());
  EXPECT_FALSE(bubble_observer.IsSavePromptShownAutomatically());

  // Navigates the primary page to the URL.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  ASSERT_TRUE(base::test::RunUntil(
      []() { return LoginHandler::GetAllLoginHandlersForTest().size() == 1; }));

  PasswordsNavigationObserver nav_observer(WebContents());
  // Offer valid credentials on the auth challenge.
  ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());
  LoginHandler* handler = *LoginHandler::GetAllLoginHandlersForTest().begin();
  EXPECT_TRUE(handler);
  // Any username/password will work.
  handler->SetAuth(u"user", u"pwd");

  // The password manager should be working correctly.
  ASSERT_TRUE(nav_observer.Wait());
  WaitForPasswordStore();
  EXPECT_TRUE(bubble_observer.IsSavePromptShownAutomatically());

  // Make sure that the prerender was not activated.
  EXPECT_FALSE(host_observer.was_activated());
}

// Tests that saving password doesn't work in the prerendering.
IN_PROC_BROWSER_TEST_F(PasswordManagerPrerenderBrowserTest,
                       SavePasswordInPrerender) {
  MockPrerenderPasswordManagerDriverInjector injector(WebContents());
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  auto prerender_url =
      embedded_test_server()->GetURL("/password/password_form.html");
  // Loads a page in the prerender.
  content::FrameTreeNodeId host_id =
      prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*WebContents(), host_id);
  content::RenderFrameHost* render_frame_host =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);
  // Fills a form and submits through a <input type="submit"> button.
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecJs(render_frame_host, fill_and_submit));
  // Since navigation from a prerendering page is disallowed, prerendering is
  // canceled. This also means that we should never make any calls to the mocked
  // driver. Since we've already set an expectation of no calls, this will be
  // checked implicitly when the injector (and consequently, the mock) is
  // destroyed.
  host_observer.WaitForDestroyed();
  BubbleObserver bubble_observer(WebContents());
  EXPECT_FALSE(bubble_observer.IsSavePromptShownAutomatically());

  // Navigates the primary page to the URL.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  // Makes sure that the page is not from the prerendering.
  EXPECT_FALSE(host_observer.was_activated());

  // After loading the primary page, try to submit the password.
  PasswordsNavigationObserver observer(WebContents());
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_and_submit));
  ASSERT_TRUE(observer.Wait());

  // Saves the password and checks the store.
  EXPECT_TRUE(bubble_observer.IsSavePromptShownAutomatically());
  bubble_observer.AcceptSavePrompt();

  WaitForPasswordStore();
  CheckThatCredentialsStored("temp", "random");
}

// Tests that Mojo messages in prerendering are deferred from the render to
// the PasswordManagerDriver until activation.
IN_PROC_BROWSER_TEST_F(PasswordManagerPrerenderBrowserTest,
                       MojoDeferringInPrerender) {
  GetNewTabWithPasswordManagerClient();
  MockPrerenderPasswordManagerDriverInjector injector(web_contents());

  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  auto prerender_url =
      embedded_test_server()->GetURL("/password/password_form.html");
  // Loads a page in the prerender.
  content::FrameTreeNodeId host_id =
      prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  content::RenderFrameHost* render_frame_host =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);

  auto* mock = injector.GetMockForFrame(render_frame_host);
  testing::Mock::VerifyAndClearExpectations(mock);

  // We expect that messages will be sent to the driver, post-activation.
  EXPECT_CALL(*mock, PasswordFormsParsed).Times(1);
  EXPECT_CALL(*mock, PasswordFormsRendered).Times(1);

  // Navigates the primary page to the URL.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  // Makes sure that the page is activated from the prerendering.
  EXPECT_TRUE(host_observer.was_activated());

  mock->WaitForPasswordFormParsedAndRendered();
}

/// Inject the mock driver when navigation happens in main frame.
class MockPasswordManagerDriverInjector : public content::WebContentsObserver {
 public:
  explicit MockPasswordManagerDriverInjector(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~MockPasswordManagerDriverInjector() override = default;

  MockPrerenderPasswordManagerDriver* GetMockForFrame(
      content::RenderFrameHost* rfh) {
    return static_cast<MockPrerenderPasswordManagerDriver*>(
        GetDriverForFrame(rfh)->ReceiverForTesting().impl());
  }

 private:
  password_manager::ContentPasswordManagerDriver* GetDriverForFrame(
      content::RenderFrameHost* rfh) {
    return password_manager::ContentPasswordManagerDriver::
        GetForRenderFrameHost(rfh);
  }

  // content::WebContentsObserver:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override {
    mocks_.push_back(std::make_unique<
                     testing::StrictMock<MockPrerenderPasswordManagerDriver>>(
        GetDriverForFrame(navigation_handle->GetRenderFrameHost())));
  }

  std::vector<std::unique_ptr<MockPrerenderPasswordManagerDriver>> mocks_;
};

// Test class for testing password manager with CredentiallessIframe enabled.
class PasswordManagerCredentiallessIframeTest
    : public PasswordManagerBrowserTest {
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    PasswordManagerBrowserTest::SetUpOnMainThread();
  }
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableBlinkTestFeatures);
    PasswordManagerBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(PasswordManagerCredentiallessIframeTest, NoFormsSeen) {
  GURL main_frame_url = embedded_test_server()->GetURL(
      "/password/password_form_in_credentialless_iframe.html");
  MockPasswordManagerDriverInjector injector(WebContents());
  PasswordsNavigationObserver nav_observer(WebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));
  ASSERT_TRUE(nav_observer.Wait());

  content::RenderFrameHost* main_rfh = WebContents()->GetPrimaryMainFrame();
  // Check behavior on a normal iframe:
  {
    ASSERT_TRUE(content::ExecJs(
        main_rfh, R"(create_iframe('/empty.html', 'iframe', false);)"));
    content::RenderFrameHost* child_rfh = ChildFrameAt(main_rfh, 0);
    ASSERT_NE(child_rfh, nullptr);
    MockPrerenderPasswordManagerDriver* mock =
        injector.GetMockForFrame(child_rfh);
    ASSERT_NE(mock, nullptr);
    EXPECT_CALL(*mock, PasswordFormsParsed).Times(1);
    ASSERT_TRUE(
        content::ExecJs(child_rfh, "window.parent.inject_form(document);"));
    mock->WaitFor(MockPrerenderPasswordManagerDriver::WAIT_FOR_PASSWORD_FORMS::
                      WAIT_FOR_PARSED);
  }

  // Check what happens when using a credentialless iframe instead:
  {
    ASSERT_TRUE(content::ExecJs(
        main_rfh, "create_iframe('/empty.html', 'iframe', true);"));
    content::RenderFrameHost* child_rfh =
        ChildFrameAt(WebContents()->GetPrimaryMainFrame(), 1);
    ASSERT_NE(child_rfh, nullptr);
    MockPrerenderPasswordManagerDriver* mock =
        injector.GetMockForFrame(child_rfh);
    ASSERT_NE(mock, nullptr);
    EXPECT_CALL(*mock, PasswordFormsParsed).Times(0);
    ASSERT_TRUE(
        content::ExecJs(child_rfh, "window.parent.inject_form(document);"));
    base::RunLoop().RunUntilIdle();
  }
}

IN_PROC_BROWSER_TEST_F(PasswordManagerCredentiallessIframeTest,
                       DisablePasswordManagerOnCredentiallessIframe) {
  GURL base_url = https_test_server().GetURL("a.test", "/");
  GURL main_frame_url = https_test_server().GetURL(
      "a.test", "/password/password_form_in_credentialless_iframe.html");
  GURL form_url = https_test_server().GetURL(
      "a.test", "/password/crossite_iframe_content.html");

  // 1. Store the username/password
  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = base_url.spec();
  signin_form.url = base_url;
  signin_form.action = base_url;
  signin_form.username_value = u"temp";
  signin_form.password_value = u"pa55w0rd";
  password_store->AddLogin(signin_form);

  // 2. Load the form again, from a normal and a credentialless iframe.
  PasswordsNavigationObserver reload_observer(WebContents());
  reload_observer.SetPathToWaitFor(
      "/password/password_form_in_credentialless_iframe.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));
  ASSERT_TRUE(reload_observer.Wait());
  EXPECT_TRUE(content::ExecJs(
      WebContents(),
      content::JsReplace("create_iframe($1, 'normal', false); ", form_url)));
  EXPECT_TRUE(content::ExecJs(
      WebContents(),
      content::JsReplace("create_iframe($1, 'credentialless', true); ",
                         form_url)));
  content::WaitForLoadStop(WebContents());
  content::RenderFrameHost* iframe_normal =
      ChildFrameAt(WebContents()->GetPrimaryMainFrame(), 0);
  content::RenderFrameHost* iframe_credentialless =
      ChildFrameAt(WebContents()->GetPrimaryMainFrame(), 1);

  EXPECT_EQ("pa55w0rd",
            content::EvalJs(iframe_normal,
                            "window.parent.check_password(document);"));
  EXPECT_EQ("not found",
            content::EvalJs(iframe_credentialless,
                            "window.parent.check_password(document);"));

  // 3. Navigate the normal iframe to be a credentialless iframe.
  EXPECT_TRUE(content::ExecJs(
      WebContents(),
      "document.getElementById('normal').credentialless = true"));
  EXPECT_TRUE(content::ExecJs(
      WebContents(),
      content::JsReplace("document.getElementById('normal').src = $1",
                         form_url)));
  content::WaitForLoadStop(WebContents());
  iframe_normal = ChildFrameAt(WebContents()->GetPrimaryMainFrame(), 0);
  EXPECT_EQ("not found",
            content::EvalJs(iframe_normal,
                            "window.parent.check_password(document);"));
}

}  // namespace

}  // namespace password_manager
