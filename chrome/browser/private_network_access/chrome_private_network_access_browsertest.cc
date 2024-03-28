// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/memory/ref_counted.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/device_notifications/device_pinned_notification_renderer.h"
#include "chrome/browser/device_notifications/device_status_icon_renderer.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/private_network_access/chrome_private_network_device_chooser.h"
#include "chrome/browser/private_network_access/chrome_private_network_device_delegate.h"
#include "chrome/browser/private_network_access/private_network_device_browser_test_utils.h"
#include "chrome/browser/private_network_access/private_network_device_chooser_controller.h"
#include "chrome/browser/private_network_access/private_network_device_permission_context.h"
#include "chrome/browser/private_network_access/private_network_device_permission_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/chooser_bubble_testapi.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/ip_address_space.mojom-forward.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"
#include "third_party/blink/public/common/origin_trials/trial_token_result.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/mojom/private_network_device/private_network_device.mojom.h"

namespace {

using ::content::JsReplace;
using ::testing::Return;

using Type = NewAcceptedDeviceType;

constexpr std::string_view kUserAcceptedPrivateNetworkDeviceHistogramName =
    "Security.PrivateNetworkAccess.PermissionNewAcceptedDeviceType";

constexpr char kPrivateHost[] = "a.test";
constexpr char kPublicPath[] =
    "/private_network_access/no-favicon-treat-as-public-address.html";

// Path to a response that passes Private Network Access checks.
constexpr char kPnaLocalDevicePath[] = "/private_network_access/get-device";

class ChromePrivateNetworkAccessTestBase : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    secure_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    insecure_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(secure_server_.Start());
    ASSERT_TRUE(insecure_server_.Start());
    test_content_browser_client_.SetAsBrowserClient();
    host_resolver()->AddRule(kPrivateHost, "127.0.0.1");

    GURL url = SecureURL("/simple_page.html");
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  }

  void TearDownOnMainThread() override {
    test_content_browser_client_.UnsetAsBrowserClient();
  }

  GURL SecureURL(const std::string& path) {
    return secure_server_.GetURL("127.0.0.1", path);
  }

  GURL InsecureURL(const std::string& path) {
    return insecure_server_.GetURL(kPrivateHost, path);
  }

  net::EmbeddedTestServer secure_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  net::EmbeddedTestServer insecure_server_{net::EmbeddedTestServer::TYPE_HTTP};

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  TestPNAContentBrowserClient test_content_browser_client_;
};
class ChromePrivateNetworkAccessDisablePermissionFeatureTest
    : public ChromePrivateNetworkAccessTestBase {
 public:
  ChromePrivateNetworkAccessDisablePermissionFeatureTest() {
    feature_list_.InitWithFeatures(
        {features::kBlockInsecurePrivateNetworkRequests,
         features::kBlockInsecurePrivateNetworkRequestsFromPrivate,
         features::kPrivateNetworkAccessSendPreflights},
        {network::features::kPrivateNetworkAccessPermissionPrompt});
  }
};

IN_PROC_BROWSER_TEST_F(ChromePrivateNetworkAccessDisablePermissionFeatureTest,
                       RequestDevices) {
  GURL url = SecureURL(kPublicPath);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(false,
            content::EvalJs(web_contents,
                            JsReplace(
                                R"(fetch($1, {targetAddressSpace: 'local'})
                          .then(
                            response => response.ok,
                            error => {
                              console.log('Error fetching ' +$1, error);
                              return false;
                            })
                          .catch(e => e.toString()))",
                                InsecureURL(kPnaLocalDevicePath))));
}

class ChromePrivateNetworkAccessTest
    : public ChromePrivateNetworkAccessTestBase {
 public:
  ChromePrivateNetworkAccessTest() {
    feature_list_.InitWithFeatures(
        {network::features::kPrivateNetworkAccessPermissionPrompt,
         features::kBlockInsecurePrivateNetworkRequests,
         features::kBlockInsecurePrivateNetworkRequestsFromPrivate,
         features::kPrivateNetworkAccessSendPreflights},
        {});
  }
};

IN_PROC_BROWSER_TEST_F(ChromePrivateNetworkAccessTest, RequestDevices) {
  base::HistogramTester histogram_tester;
  GURL url = SecureURL(kPublicPath);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(true,
            content::EvalJs(web_contents,
                            JsReplace(
                                R"(fetch($1, {targetAddressSpace: 'local'})
                          .then(
                            response => response.ok,
                            error => {
                              console.log('Error fetching ' +$1, error);
                              return false;
                            })
                          .catch(e => e.toString()))",
                                InsecureURL(kPnaLocalDevicePath))));
  histogram_tester.ExpectUniqueSample(
      kUserAcceptedPrivateNetworkDeviceHistogramName, Type::kValidDevice, 1);

  EXPECT_EQ(false, content::EvalJs(web_contents,
                                   JsReplace(
                                       R"(fetch($1)
                          .then(
                            response => response.ok,
                            error => {
                              console.log('Error fetching ' +$1, error);
                              return false;
                            })
                          .catch(e => e.toString()))",
                                       InsecureURL(kPnaLocalDevicePath))));

  EXPECT_EQ(false, content::EvalJs(
                       web_contents,
                       JsReplace(R"(fetch($1, {targetAddressSpace: 'private'})
                          .then(
                            response => response.ok,
                            error => {
                              console.log('Error fetching ' +$1, error);
                              return false;
                            })
                          .catch(e => e.toString()))",
                                 InsecureURL(kPnaLocalDevicePath))));

  // Crash the renderer process.
  content::RenderProcessHost* process =
      web_contents->GetPrimaryMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();

  // Reload tab.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));

  // Fetch the same private network device again.
  EXPECT_EQ(true, content::EvalJs(
                      web_contents,
                      JsReplace(R"(fetch($1, {targetAddressSpace: 'local'})
                        .then(
                          response => response.ok,
                          error => {
                            console.log('Error fetching ' +$1, error);
                            return false;
                            })
                        .catch(e => e.toString()))",
                                InsecureURL(kPnaLocalDevicePath))));
  histogram_tester.ExpectUniqueSample(
      kUserAcceptedPrivateNetworkDeviceHistogramName, Type::kValidDevice, 1);
}

}  // namespace
