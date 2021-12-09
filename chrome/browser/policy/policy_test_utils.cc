// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_test_utils.h"

#include "base/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/net/safe_search_util.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/policy_constants.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/variations/variations_params_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using content::BrowserThread;
using testing::_;
using testing::Return;

namespace policy {

const base::FilePath::CharType kTestExtensionsDir[] =
    FILE_PATH_LITERAL("extensions");

void GetTestDataDirectory(base::FilePath* test_data_directory) {
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_TEST_DATA, test_data_directory));
}

PolicyTest::PolicyTest() = default;

PolicyTest::~PolicyTest() = default;

void PolicyTest::SetUp() {
  InProcessBrowserTest::SetUp();
}

void PolicyTest::SetUpInProcessBrowserTestFixture() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch("noerrdialogs");
  provider_.SetDefaultReturns(true /* is_initialization_complete_return */,
                              true /* is_first_policy_load_complete_return */);
  BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
}

void PolicyTest::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
}

void PolicyTest::SetUpCommandLine(base::CommandLine* command_line) {
  variations::testing::VariationParamsManager::AppendVariationParams(
      "ReportCertificateErrors", "ShowAndPossiblySend",
      {{"sendingThreshold", "1.0"}}, command_line);
}

scoped_refptr<const extensions::Extension> PolicyTest::LoadUnpackedExtension(
    const base::FilePath::StringType& name) {
  base::FilePath extension_path(ui_test_utils::GetTestFilePath(
      base::FilePath(kTestExtensionsDir), base::FilePath(name)));
  extensions::ChromeTestExtensionLoader loader(browser()->profile());
  return loader.LoadExtension(extension_path);
}

void PolicyTest::UpdateProviderPolicy(const PolicyMap& policy) {
  PolicyMap policy_with_defaults = policy.Clone();
#if defined(OS_CHROMEOS)
  SetEnterpriseUsersDefaults(&policy_with_defaults);
#endif
  provider_.UpdateChromePolicy(policy_with_defaults);
  DCHECK(base::CurrentThread::Get());
  base::RunLoop loop;
  loop.RunUntilIdle();
}

// static
void PolicyTest::SetPolicy(PolicyMap* policies,
                           const char* key,
                           absl::optional<base::Value> value) {
  policies->Set(key, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                POLICY_SOURCE_CLOUD, std::move(value), nullptr);
}

void PolicyTest::ApplySafeSearchPolicy(
    absl::optional<base::Value> legacy_safe_search,
    absl::optional<base::Value> google_safe_search,
    absl::optional<base::Value> legacy_youtube,
    absl::optional<base::Value> youtube_restrict) {
  PolicyMap policies;
  SetPolicy(&policies, key::kForceSafeSearch, std::move(legacy_safe_search));
  SetPolicy(&policies, key::kForceGoogleSafeSearch,
            std::move(google_safe_search));
  SetPolicy(&policies, key::kForceYouTubeSafetyMode, std::move(legacy_youtube));
  SetPolicy(&policies, key::kForceYouTubeRestrict, std::move(youtube_restrict));
  UpdateProviderPolicy(policies);
}

// static
GURL PolicyTest::GetExpectedSearchURL(bool expect_safe_search) {
  std::string expected_url("http://google.com/");
  if (expect_safe_search) {
    expected_url += "?" +
                    std::string(safe_search_util::kSafeSearchSafeParameter) +
                    "&" + safe_search_util::kSafeSearchSsuiParameter;
  }
  return GURL(expected_url);
}

// static
void PolicyTest::CheckSafeSearch(Browser* browser,
                                 bool expect_safe_search,
                                 const std::string& url) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(web_contents);
  ui_test_utils::SendToOmniboxAndSubmit(browser, url);
  observer.Wait();
  OmniboxEditModel* model =
      browser->window()->GetLocationBar()->GetOmniboxView()->model();
  EXPECT_TRUE(model->CurrentMatch(nullptr).destination_url.is_valid());
  EXPECT_EQ(GetExpectedSearchURL(expect_safe_search), web_contents->GetURL());
}

// static
bool PolicyTest::FetchSubresource(content::WebContents* web_contents,
                                  const GURL& url) {
  std::string script(
      "var xhr = new XMLHttpRequest();"
      "xhr.open('GET', '");
  script += url.spec() +
            "', true);"
            "xhr.onload = function (e) {"
            "  if (xhr.readyState === 4) {"
            "    window.domAutomationController.send(xhr.status === 200);"
            "  }"
            "};"
            "xhr.onerror = function () {"
            "  window.domAutomationController.send(false);"
            "};"
            "xhr.send(null)";
  bool xhr_result = false;
  bool execute_result =
      content::ExecuteScriptAndExtractBool(web_contents, script, &xhr_result);
  return xhr_result && execute_result;
}

bool PolicyTest::IsShowingInterstitial(content::WebContents* tab) {
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  if (!helper) {
    return false;
  }
  return helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting() !=
         nullptr;
}

void PolicyTest::WaitForInterstitial(content::WebContents* tab) {
  ASSERT_TRUE(IsShowingInterstitial(tab));
  ASSERT_TRUE(WaitForRenderFrameReady(tab->GetMainFrame()));
}

void PolicyTest::FlushBlocklistPolicy() {
  // Updates of the URLBlocklist are done on IO, after building the blocklist
  // on the blocking pool, which is initiated from IO.
  content::RunAllPendingInMessageLoop(BrowserThread::IO);
  content::RunAllTasksUntilIdle();
  content::RunAllPendingInMessageLoop(BrowserThread::IO);
}

}  // namespace policy
