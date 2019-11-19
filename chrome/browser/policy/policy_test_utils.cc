// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_test_utils.h"

#include "base/message_loop/message_loop_current.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/chrome_screenshot_grabber.h"
#include "chrome/browser/ui/ash/chrome_screenshot_grabber_test_observer.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
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
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/transport_security_state.h"
#include "services/network/public/mojom/network_service_test.mojom.h"

#if defined(OS_CHROMEOS)
#include "ui/snapshot/screenshot_grabber.h"
#endif

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
  EXPECT_CALL(provider_, IsInitializationComplete(_))
      .WillRepeatedly(Return(true));
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

void PolicyTest::SetScreenshotPolicy(bool enabled) {
  PolicyMap policies;
  policies.Set(key::kDisableScreenshots, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(!enabled), nullptr);
  UpdateProviderPolicy(policies);
}

void PolicyTest::SetShouldRequireCTForTesting(bool* required) {
  if (content::IsOutOfProcessNetworkService()) {
    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
    content::GetNetworkService()->BindTestInterface(
        network_service_test.BindNewPipeAndPassReceiver());
    network::mojom::NetworkServiceTest::ShouldRequireCT required_ct;
    if (!required) {
      required_ct = network::mojom::NetworkServiceTest::ShouldRequireCT::RESET;
    } else {
      required_ct =
          *required
              ? network::mojom::NetworkServiceTest::ShouldRequireCT::REQUIRE
              : network::mojom::NetworkServiceTest::ShouldRequireCT::
                    DONT_REQUIRE;
    }

    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    network_service_test->SetShouldRequireCT(required_ct);
    return;
  }

  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&net::TransportSecurityState::SetShouldRequireCTForTesting,
                     required));
}

#if defined(OS_CHROMEOS)
class QuitMessageLoopAfterScreenshot
    : public ChromeScreenshotGrabberTestObserver {
 public:
  explicit QuitMessageLoopAfterScreenshot(base::OnceClosure done)
      : done_(std::move(done)) {}
  void OnScreenshotCompleted(ui::ScreenshotResult screenshot_result,
                             const base::FilePath& screenshot_path) override {
    base::PostTaskAndReply(FROM_HERE, {BrowserThread::IO}, base::DoNothing(),
                           std::move(done_));
  }

  ~QuitMessageLoopAfterScreenshot() override {}

 private:
  base::OnceClosure done_;
};

void PolicyTest::TestScreenshotFile(bool enabled) {
  base::RunLoop run_loop;
  QuitMessageLoopAfterScreenshot observer_(run_loop.QuitClosure());

  ChromeScreenshotGrabber* grabber = ChromeScreenshotGrabber::Get();
  grabber->test_observer_ = &observer_;
  SetScreenshotPolicy(enabled);
  grabber->HandleTakeScreenshotForAllRootWindows();
  run_loop.Run();

  grabber->test_observer_ = nullptr;
}
#endif  // defined(OS_CHROMEOS)

scoped_refptr<const extensions::Extension> PolicyTest::LoadUnpackedExtension(
    const base::FilePath::StringType& name) {
  base::FilePath extension_path(ui_test_utils::GetTestFilePath(
      base::FilePath(kTestExtensionsDir), base::FilePath(name)));
  extensions::ChromeTestExtensionLoader loader(browser()->profile());
  return loader.LoadExtension(extension_path);
}

void PolicyTest::UpdateProviderPolicy(const PolicyMap& policy) {
  PolicyMap policy_with_defaults;
  policy_with_defaults.CopyFrom(policy);
#if defined(OS_CHROMEOS)
  SetEnterpriseUsersDefaults(&policy_with_defaults);
#endif
  provider_.UpdateChromePolicy(policy_with_defaults);
  DCHECK(base::MessageLoopCurrent::Get());
  base::RunLoop loop;
  loop.RunUntilIdle();
}

void PolicyTest::PerformClick(int x, int y) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  blink::WebMouseEvent click_event(
      blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  click_event.button = blink::WebMouseEvent::Button::kLeft;
  click_event.click_count = 1;
  click_event.SetPositionInWidget(x, y);
  contents->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(click_event);
  click_event.SetType(blink::WebInputEvent::kMouseUp);
  contents->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(click_event);
}

void PolicyTest::SetPolicy(PolicyMap* policies,
                           const char* key,
                           std::unique_ptr<base::Value> value) {
  if (value) {
    policies->Set(key, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_CLOUD, std::move(value), nullptr);
  } else {
    policies->Erase(key);
  }
}

void PolicyTest::ApplySafeSearchPolicy(
    std::unique_ptr<base::Value> legacy_safe_search,
    std::unique_ptr<base::Value> google_safe_search,
    std::unique_ptr<base::Value> legacy_youtube,
    std::unique_ptr<base::Value> youtube_restrict) {
  PolicyMap policies;
  SetPolicy(&policies, key::kForceSafeSearch, std::move(legacy_safe_search));
  SetPolicy(&policies, key::kForceGoogleSafeSearch,
            std::move(google_safe_search));
  SetPolicy(&policies, key::kForceYouTubeSafetyMode, std::move(legacy_youtube));
  SetPolicy(&policies, key::kForceYouTubeRestrict, std::move(youtube_restrict));
  UpdateProviderPolicy(policies);
}

#if defined(OS_CHROMEOS)
void PolicyTest::SetEnableFlag(const keyboard::KeyboardEnableFlag& flag) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  keyboard_client->SetEnableFlag(flag);
}

void PolicyTest::ClearEnableFlag(const keyboard::KeyboardEnableFlag& flag) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  keyboard_client->ClearEnableFlag(flag);
}
#endif  // defined(OS_CHROMEOS)

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
void PolicyTest::CheckYouTubeRestricted(
    int youtube_restrict_mode,
    const std::map<GURL, net::HttpRequestHeaders>& urls_requested,
    const GURL& url) {
  auto iter = urls_requested.find(url);
  ASSERT_TRUE(iter != urls_requested.end());
  std::string header;
  iter->second.GetHeader(safe_search_util::kYouTubeRestrictHeaderName, &header);
  if (youtube_restrict_mode == safe_search_util::YOUTUBE_RESTRICT_OFF) {
    EXPECT_TRUE(header.empty());
  } else if (youtube_restrict_mode ==
             safe_search_util::YOUTUBE_RESTRICT_MODERATE) {
    EXPECT_EQ(header, safe_search_util::kYouTubeRestrictHeaderValueModerate);
  } else if (youtube_restrict_mode ==
             safe_search_util::YOUTUBE_RESTRICT_STRICT) {
    EXPECT_EQ(header, safe_search_util::kYouTubeRestrictHeaderValueStrict);
  }
}

// static
void PolicyTest::CheckAllowedDomainsHeader(
    const std::string& allowed_domain,
    const std::map<GURL, net::HttpRequestHeaders>& urls_requested,
    const GURL& url) {
  auto iter = urls_requested.find(url);
  ASSERT_TRUE(iter != urls_requested.end());
  if (allowed_domain.empty()) {
    EXPECT_TRUE(
        !iter->second.HasHeader(safe_search_util::kGoogleAppsAllowedDomains));
    return;
  }

  std::string header;
  iter->second.GetHeader(safe_search_util::kGoogleAppsAllowedDomains, &header);
  EXPECT_EQ(header, allowed_domain);
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

int PolicyTest::IsExtendedReportingCheckboxVisibleOnInterstitial() {
  const std::string command = base::StringPrintf(
      "var node = document.getElementById('extended-reporting-opt-in');"
      "if (node) {"
      "  window.domAutomationController.send(node.offsetWidth > 0 || "
      "      node.offsetHeight > 0 ? %d : %d);"
      "} else {"
      // The node should be present but not visible, so trigger an error
      // by sending false if it's not present.
      "  window.domAutomationController.send(%d);"
      "}",
      security_interstitials::CMD_TEXT_FOUND,
      security_interstitials::CMD_TEXT_NOT_FOUND,
      security_interstitials::CMD_ERROR);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(tab);
  int result = 0;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(tab->GetMainFrame(), command,
                                                  &result));
  return result;
}

void PolicyTest::SendInterstitialCommand(
    content::WebContents* tab,
    security_interstitials::SecurityInterstitialCommand command) {
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
      ->CommandReceived(base::NumberToString(command));
  return;
}

}  // namespace policy
