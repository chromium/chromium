// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_test_base.h"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/passwords_navigation_observer.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/password_store_results_observer.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test_utils.h"
#include "net/cert/cert_verify_result.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/transport_security_state.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/switches.h"

namespace {

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

enum ReturnCodes {  // Possible results of the JavaScript code.
  RETURN_CODE_OK,
  RETURN_CODE_NO_ELEMENT,
  RETURN_CODE_WRONG_VALUE,
  RETURN_CODE_INVALID,
};

}  // namespace

BubbleObserver::BubbleObserver(content::WebContents* web_contents)
    : passwords_ui_controller_(
          ManagePasswordsUIController::FromWebContents(web_contents)) {}

bool BubbleObserver::IsBubbleDisplayedAutomatically() const {
  return passwords_ui_controller_->IsShowingBubble();
}

bool BubbleObserver::IsSavePromptAvailable() const {
  return passwords_ui_controller_->GetState() ==
         password_manager::ui::PENDING_PASSWORD_STATE;
}

bool BubbleObserver::IsUpdatePromptAvailable() const {
  return passwords_ui_controller_->GetState() ==
         password_manager::ui::PENDING_PASSWORD_UPDATE_STATE;
}

bool BubbleObserver::IsSavePromptShownAutomatically() const {
  return IsSavePromptAvailable() && IsBubbleDisplayedAutomatically();
}

bool BubbleObserver::IsUpdatePromptShownAutomatically() const {
  return IsUpdatePromptAvailable() && IsBubbleDisplayedAutomatically();
}

void BubbleObserver::Hide() const {
  passwords_ui_controller_->OnBubbleHidden();
}

void BubbleObserver::AcceptSavePrompt() const {
  ASSERT_TRUE(IsSavePromptAvailable());
  passwords_ui_controller_->SavePassword(
      passwords_ui_controller_->GetPendingPassword().username_value,
      passwords_ui_controller_->GetPendingPassword().password_value);
  EXPECT_FALSE(IsSavePromptAvailable());
}

void BubbleObserver::AcceptUpdatePrompt() const {
  ASSERT_TRUE(IsUpdatePromptAvailable());
  passwords_ui_controller_->SavePassword(
      passwords_ui_controller_->GetPendingPassword().username_value,
      passwords_ui_controller_->GetPendingPassword().password_value);
  EXPECT_FALSE(IsUpdatePromptAvailable());
}

void BubbleObserver::WaitForAccountChooser() const {
  WaitForState(password_manager::ui::CREDENTIAL_REQUEST_STATE);
}

void BubbleObserver::WaitForInactiveState() const {
  WaitForState(password_manager::ui::INACTIVE_STATE);
}

void BubbleObserver::WaitForManagementState() const {
  WaitForState(password_manager::ui::MANAGE_STATE);
}

void BubbleObserver::WaitForAutomaticSavePrompt() const {
  WaitForState(password_manager::ui::PENDING_PASSWORD_STATE);
}

void BubbleObserver::WaitForAutomaticUpdatePrompt() const {
  WaitForState(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
}

bool BubbleObserver::WaitForFallbackForSaving() const {
  if (passwords_ui_controller_->GetState() ==
      password_manager::ui::PENDING_PASSWORD_STATE) {
    return !IsBubbleDisplayedAutomatically();
  }

  if (base::test::RunUntil([this]() {
        return passwords_ui_controller_->GetState() ==
               password_manager::ui::PENDING_PASSWORD_STATE;
      })) {
    EXPECT_FALSE(IsBubbleDisplayedAutomatically());
    return true;
  }
  return false;
}

void BubbleObserver::WaitForSaveUnsyncedCredentialsPrompt() const {
  WaitForState(
      password_manager::ui::WILL_DELETE_UNSYNCED_ACCOUNT_PASSWORDS_STATE);
}

void BubbleObserver::WaitForState(
    password_manager::ui::State target_state) const {
  auto IsTargetStateObserved = [this, target_state]() {
    const bool should_wait_for_automatic_prompt =
        target_state == password_manager::ui::PENDING_PASSWORD_STATE ||
        target_state == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE;
    return target_state == passwords_ui_controller_->GetState() &&
           (!should_wait_for_automatic_prompt ||
            IsBubbleDisplayedAutomatically());
  };
  if (IsTargetStateObserved()) {
    return;
  }

  EXPECT_TRUE(base::test::RunUntil(IsTargetStateObserved));
}

PasswordManagerBrowserTestBase::PasswordManagerBrowserTestBase()
    : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

PasswordManagerBrowserTestBase::~PasswordManagerBrowserTestBase() = default;

void PasswordManagerBrowserTestBase::SetUp() {
  ASSERT_TRUE(https_test_server().InitializeAndListen());
  CertVerifierBrowserTest::SetUp();
}

void PasswordManagerBrowserTestBase::SetUpOnMainThread() {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Setup HTTPS server serving files from standard test directory.
  static constexpr base::FilePath::CharType kDocRoot[] =
      FILE_PATH_LITERAL("chrome/test/data");
  https_test_server().ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
  https_test_server().StartAcceptingConnections();

  // Setup the mock host resolver
  host_resolver()->AddRule("*", "127.0.0.1");

  // Whitelist all certs for the HTTPS server.
  auto cert = https_test_server().GetCertificate();
  net::CertVerifyResult verify_result;
  verify_result.cert_status = 0;
  verify_result.verified_cert = cert;
  mock_cert_verifier()->AddResultForCert(cert.get(), verify_result, net::OK);

  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
}

void PasswordManagerBrowserTestBase::ClearWebContentsPtr() {
  web_contents_ = nullptr;
}

void PasswordManagerBrowserTestBase::TearDownOnMainThread() {
  ClearWebContentsPtr();
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
}

void PasswordManagerBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  CertVerifierBrowserTest::SetUpCommandLine(command_line);
  // Some builders are flaky due to slower loading interacting
  // with deferred commits.
  command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
}

// static
void PasswordManagerBrowserTestBase::WaitForPasswordStore(Browser* browser) {
  scoped_refptr<password_manager::PasswordStoreInterface>
      profile_password_store = ProfilePasswordStoreFactory::GetForProfile(
          browser->profile(), ServiceAccessType::IMPLICIT_ACCESS);
  password_manager::PasswordStoreResultsObserver profile_syncer;
  profile_password_store->GetAllLoginsWithAffiliationAndBrandingInformation(
      profile_syncer.GetWeakPtr());
  profile_syncer.WaitForResults();

  scoped_refptr<password_manager::PasswordStoreInterface>
      account_password_store = AccountPasswordStoreFactory::GetForProfile(
          browser->profile(), ServiceAccessType::IMPLICIT_ACCESS);
  if (account_password_store) {
    password_manager::PasswordStoreResultsObserver account_syncer;
    account_password_store->GetAllLoginsWithAffiliationAndBrandingInformation(
        account_syncer.GetWeakPtr());
    account_syncer.WaitForResults();
  }
}

content::WebContents* PasswordManagerBrowserTestBase::WebContents() const {
  return web_contents_;
}

void PasswordManagerBrowserTestBase::SetWebContents(
    content::WebContents* web_contents) {
  web_contents_ = web_contents;
}

content::RenderFrameHost* PasswordManagerBrowserTestBase::RenderFrameHost()
    const {
  return WebContents()->GetPrimaryMainFrame();
}

void PasswordManagerBrowserTestBase::NavigateToFile(const std::string& path) {
  ASSERT_EQ(web_contents_,
            browser()->tab_strip_model()->GetActiveWebContents());
  PasswordsNavigationObserver observer(WebContents());
  GURL url = embedded_test_server()->GetURL(path);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(observer.Wait());
}

void PasswordManagerBrowserTestBase::WaitForElementValue(
    const std::string& element_id,
    const std::string& expected_value) {
  WaitForElementValue("null", element_id, expected_value);
}

void PasswordManagerBrowserTestBase::WaitForElementValue(
    const std::string& iframe_id,
    const std::string& element_id,
    const std::string& expected_value) {
  SCOPED_TRACE(::testing::Message()
               << "iframe_id=" << iframe_id << ", element_id=" << element_id
               << ", expected_value=" << expected_value);
  const std::string value_check_function = base::StringPrintf(
      "function valueCheck() {"
      "  if (%s)"
      "    var element = document.getElementById("
      "        '%s').contentDocument.getElementById('%s');"
      "  else "
      "    var element = document.getElementById('%s');"
      "  return element && element.value == '%s';"
      "}",
      iframe_id.c_str(), iframe_id.c_str(), element_id.c_str(),
      element_id.c_str(), expected_value.c_str());
  const std::string script =
      value_check_function +
      base::StringPrintf(
          "new Promise(resolve => {"
          "  if (valueCheck()) {"
          "    /* Spin the event loop with setTimeout. */"
          "    setTimeout(() => resolve(%d), 0);"
          "  } else {"
          "    if (%s)"
          "      var element = document.getElementById("
          "          '%s').contentDocument.getElementById('%s');"
          "    else "
          "      var element = document.getElementById('%s');"
          "    if (!element)"
          "      resolve(%d);"
          "    element.onchange = function() {"
          "      if (valueCheck()) {"
          "        /* Spin the event loop with setTimeout. */"
          "        setTimeout(() => resolve(%d), 0);"
          "      } else {"
          "        resolve(%d);"
          "      }"
          // Since this test uses promises, rather than
          // domAutomationController.send, it is not possible for future
          // 'change' events to cause flakiness or wrong script results, since
          // each promise can only settle (i.e.  fulfill or reject) at most
          // once. However, to ensure that the first onchange event is the only
          // one that can be the result of this script, we still clear out the
          // onchange listener.
          "      element.onchange = undefined;"
          "    };"
          "  }"
          "});",
          RETURN_CODE_OK, iframe_id.c_str(), iframe_id.c_str(),
          element_id.c_str(), element_id.c_str(), RETURN_CODE_NO_ELEMENT,
          RETURN_CODE_OK, RETURN_CODE_WRONG_VALUE);
  EXPECT_EQ(RETURN_CODE_OK,
            content::EvalJs(RenderFrameHost(), script,
                            content::EXECUTE_SCRIPT_NO_USER_GESTURE))
      << "element_id = " << element_id
      << ", expected_value = " << expected_value;
}

void PasswordManagerBrowserTestBase::WaitForElementValue(
    const std::string& form_id,
    size_t elements_index,
    const std::string& expected_value) {
  const std::string element_selector =
      base::StringPrintf("document.getElementById('%s').elements['%zu']",
                         form_id.c_str(), elements_index);
  WaitForJsElementValue(element_selector, expected_value);
}

void PasswordManagerBrowserTestBase::WaitForJsElementValue(
    const std::string& element_selector,
    const std::string& expected_value) {
  SCOPED_TRACE(::testing::Message() << "element_selector=" << element_selector
                                    << ", expected_value=" << expected_value);
  const std::string value_check_function = base::StringPrintf(
      "function valueCheck() {"
      "  var element = %s;"
      "  return element && element.value == '%s';"
      "}",
      element_selector.c_str(), expected_value.c_str());
  const std::string script =
      value_check_function +
      base::StringPrintf(
          "new Promise(resolve => {"
          "  if (valueCheck()) {"
          "    /* Spin the event loop with setTimeout. */"
          "    setTimeout(() => resolve(%d), 0);"
          "  } else {"
          "    var element = %s;"
          "    if (!element)"
          "      resolve(%d);"
          "    element.onchange = function() {"
          "      if (valueCheck()) {"
          "        /* Spin the event loop with setTimeout. */"
          "        setTimeout(() => resolve(%d), 0);"
          "      } else {"
          "        resolve(%d);"
          "      }"
          // Since this test uses promises, rather than
          // domAutomationController.send, it is not possible for future
          // 'change' events to cause flakiness or wrong script results, since
          // each promise can only settle (i.e.  fulfill or reject) at most
          // once. However, to ensure that the first onchange event is the only
          // one that can be the result of this script, we still clear out the
          // onchange listener.
          "      element.onchange = undefined;"
          "    };"
          "  }"
          "});",
          RETURN_CODE_OK, element_selector.c_str(), RETURN_CODE_NO_ELEMENT,
          RETURN_CODE_OK, RETURN_CODE_WRONG_VALUE);
  EXPECT_EQ(RETURN_CODE_OK,
            content::EvalJs(RenderFrameHost(), script,
                            content::EXECUTE_SCRIPT_NO_USER_GESTURE))
      << "element_selector = " << element_selector
      << ", expected_value = " << expected_value;
}

void PasswordManagerBrowserTestBase::WaitForPasswordStore() {
  WaitForPasswordStore(browser());
}

void PasswordManagerBrowserTestBase::CheckElementValue(
    const std::string& element_id,
    const std::string& expected_value) {
  CheckElementValue("null", element_id, expected_value);
}

void PasswordManagerBrowserTestBase::CheckElementValue(
    const std::string& iframe_id,
    const std::string& element_id,
    const std::string& expected_value) {
  EXPECT_EQ(expected_value, GetElementValue(iframe_id, element_id))
      << "element_id = " << element_id;
}

std::string PasswordManagerBrowserTestBase::GetElementValue(
    const std::string& iframe_id,
    const std::string& element_id) {
  const std::string value_get_script = base::StringPrintf(
      "if (%s)"
      "  var element = document.getElementById("
      "      '%s').contentDocument.getElementById('%s');"
      "else "
      "  var element = document.getElementById('%s');"
      "var value = element ? element.value : 'element not found';"
      "value;",
      iframe_id.c_str(), iframe_id.c_str(), element_id.c_str(),
      element_id.c_str());
  return content::EvalJs(RenderFrameHost(), value_get_script,
                         content::EXECUTE_SCRIPT_NO_USER_GESTURE)
      .ExtractString();
}

void PasswordManagerBrowserTestBase::SetUpInProcessBrowserTestFixture() {
  CertVerifierBrowserTest::SetUpInProcessBrowserTestFixture();
  create_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(
              base::BindRepeating([](content::BrowserContext* context) {
                // Set up a TestSyncService which will happily return
                // "everything is active" so that password generation is
                // considered enabled.
                SyncServiceFactory::GetInstance()->SetTestingFactory(
                    context, base::BindRepeating(&BuildTestSyncService));

                ProfilePasswordStoreFactory::GetInstance()->SetTestingFactory(
                    context,
                    base::BindRepeating(&password_manager::BuildPasswordStore<
                                        content::BrowserContext,
                                        password_manager::TestPasswordStore>));

                // It's fine to override unconditionally, GetForProfile() will
                // still return null if account storage is disabled.
                AccountPasswordStoreFactory::GetInstance()->SetTestingFactory(
                    context, base::BindRepeating(
                                 &password_manager::BuildPasswordStoreWithArgs<
                                     content::BrowserContext,
                                     password_manager::TestPasswordStore,
                                     password_manager::IsAccountStore>,
                                 password_manager::IsAccountStore(true)));
              }));
}

void PasswordManagerBrowserTestBase::AddHSTSHost(const std::string& host) {
  network::mojom::NetworkContext* network_context =
      browser()->profile()->GetDefaultStoragePartition()->GetNetworkContext();
  base::Time expiry = base::Time::Now() + base::Days(1000);
  bool include_subdomains = false;
  base::RunLoop run_loop;
  network_context->AddHSTS(host, expiry, include_subdomains,
                           run_loop.QuitClosure());
  run_loop.Run();
}

void PasswordManagerBrowserTestBase::CheckThatCredentialsStored(
    const std::string& username,
    const std::string& password,
    std::optional<password_manager::PasswordForm::Type> type) {
  SCOPED_TRACE(::testing::Message() << username << ", " << password);
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  auto& passwords_map = password_store->stored_passwords();
  ASSERT_EQ(1u, passwords_map.size());
  auto& passwords_vector = passwords_map.begin()->second;
  ASSERT_EQ(1u, passwords_vector.size());
  const password_manager::PasswordForm& form = passwords_vector[0];
  EXPECT_EQ(base::ASCIIToUTF16(username), form.username_value);
  EXPECT_EQ(base::ASCIIToUTF16(password), form.password_value);
  if (type.has_value()) {
    EXPECT_EQ(type.value(), form.type);
  }
}
