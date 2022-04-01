// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

const char kTestFilePath[] =
    "/notifications/notification_permission_checker.html";
const char kTesterHost[] = "notification.com";
const char kIsolatedEmbedderHost[] = "isolated.com";
const char kEmbedderHost[] = "normal.com";

// A ChromeContentBrowserClient that returns a non-default
// StoragePartitionConfig for the given Origin.
class StoragePartitioningChromeContentBrowserClient
    : public ChromeContentBrowserClient {
 public:
  explicit StoragePartitioningChromeContentBrowserClient(
      const std::string& partitioned_host)
      : partitioned_host_(partitioned_host) {}

  ~StoragePartitioningChromeContentBrowserClient() override = default;

  content::StoragePartitionConfig GetStoragePartitionConfigForSite(
      content::BrowserContext* browser_context,
      const GURL& site) override {
    if (site.host() == partitioned_host_) {
      return content::StoragePartitionConfig::Create(
          browser_context, partitioned_host_, /*partition_name=*/"",
          /*in_memory=*/false);
    }
    return ChromeContentBrowserClient::GetStoragePartitionConfigForSite(
        browser_context, site);
  }

 private:
  std::string partitioned_host_;
};

}  // namespace

class NotificationPermissionBrowserTest : public InProcessBrowserTest {
 public:
  NotificationPermissionBrowserTest()
      : partitioning_client_(kIsolatedEmbedderHost) {}

  NotificationPermissionBrowserTest(const NotificationPermissionBrowserTest&) =
      delete;
  NotificationPermissionBrowserTest& operator=(
      const NotificationPermissionBrowserTest&) = delete;
  ~NotificationPermissionBrowserTest() override {
    CHECK_EQ(&partitioning_client_,
             SetBrowserClientForTesting(original_client_));
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory(server_root_);
    EXPECT_TRUE(https_server_->Start());

    original_client_ = SetBrowserClientForTesting(&partitioning_client_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

 protected:
  void GrantNotificationPermissionForTest(const GURL& url) const {
    NotificationPermissionContext::UpdatePermission(
        browser()->profile(), url.DeprecatedGetOriginAsURL(),
        CONTENT_SETTING_ALLOW);
  }

  std::string GetNotificationPermissionState(
      const content::ToRenderFrameHost& adapter) {
    return RunJs(adapter, "getNotificationPermission()");
  }

  std::string GetServiceWorkerNotificationPermissionState(
      const content::ToRenderFrameHost& adapter) {
    return RunJs(adapter, "getServiceWorkerNotificationPermission()");
  }

  std::string QueryNotificationPermissionState(
      const content::ToRenderFrameHost& adapter) {
    return RunJs(adapter, "queryNotificationPermission()");
  }

  std::string QueryServiceWorkerNotificationPermissionState(
      const content::ToRenderFrameHost& adapter) {
    return RunJs(adapter, "queryServiceWorkerNotificationPermission()");
  }

  std::string RequestNotificationPermission(
      const content::ToRenderFrameHost& adapter) {
    return RunJs(adapter, "requestNotificationPermission()");
  }

  std::string GetPushPermissionState(
      const content::ToRenderFrameHost& adapter) {
    return RunJs(adapter, "getPushPermission()");
  }

  std::string GetServiceWorkerPushPermissionState(
      const content::ToRenderFrameHost& adapter) {
    return RunJs(adapter, "getServiceWorkerPushPermission()");
  }

  std::string RequestPushPermission(const content::ToRenderFrameHost& adapter) {
    return RunJs(adapter, "requestPushPermission()");
  }

  GURL TesterUrl() const {
    return https_server_->GetURL(kTesterHost, kTestFilePath);
  }

  GURL IsolatedEmbedderUrl() const {
    return https_server_->GetURL(kIsolatedEmbedderHost, kTestFilePath);
  }

  GURL EmbedderUrl() const {
    return https_server_->GetURL(kEmbedderHost, kTestFilePath);
  }

  content::WebContents* GetActiveWebContents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* CreateChildIframe(
      content::RenderFrameHost* parent_rfh,
      const GURL& iframe_src) {
    // For now assume this is the only child iframe.
    EXPECT_FALSE(ChildFrameAt(parent_rfh, 0));

    content::TestNavigationObserver navigation_observer(GetActiveWebContents());
    EXPECT_TRUE(ExecJs(
        parent_rfh,
        content::JsReplace("const iframe = document.createElement('iframe');"
                           "iframe.id = 'child_iframe';"
                           "iframe.src = $1;"
                           "document.body.appendChild(iframe);",
                           iframe_src)));
    navigation_observer.Wait();
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
    EXPECT_EQ(net::OK, navigation_observer.last_net_error_code());
    EXPECT_EQ(iframe_src, navigation_observer.last_navigation_url());

    content::RenderFrameHost* iframe = ChildFrameAt(parent_rfh, 0);
    EXPECT_TRUE(iframe);
    return iframe;
  }

 private:
  std::string RunJs(const content::ToRenderFrameHost& adapter,
                    const std::string& js) {
    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        adapter.render_frame_host(), js, &result));
    return result;
  }

  const base::FilePath server_root_{FILE_PATH_LITERAL("chrome/test/data")};
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  content::ContentMockCertVerifier mock_cert_verifier_;

  raw_ptr<content::ContentBrowserClient> original_client_ = nullptr;
  StoragePartitioningChromeContentBrowserClient partitioning_client_;
};

// Tests that notification permissions aren't delegated to an embedded frame
// as other permissions are. If 'example.com' was granted notification
// permissions by the user when it was a top-level frame, then it retains that
// permission when iframed in another page, regardless of the other page's
// permission status.
IN_PROC_BROWSER_TEST_F(NotificationPermissionBrowserTest,
                       PermissionNotDelegated) {
  GrantNotificationPermissionForTest(TesterUrl());

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), TesterUrl()));
  EXPECT_EQ("granted", GetNotificationPermissionState(GetActiveWebContents()));
  EXPECT_EQ("granted", GetServiceWorkerNotificationPermissionState(
                           GetActiveWebContents()));

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), EmbedderUrl()));
  EXPECT_EQ("default", GetNotificationPermissionState(GetActiveWebContents()));

  content::RenderFrameHost* iframe =
      CreateChildIframe(GetActiveWebContents()->GetMainFrame(), TesterUrl());
  EXPECT_EQ(TesterUrl(), iframe->GetLastCommittedURL());
  EXPECT_EQ("granted", GetNotificationPermissionState(iframe));
  EXPECT_EQ("granted", GetServiceWorkerNotificationPermissionState(iframe));
  EXPECT_EQ("granted", GetPushPermissionState(iframe));
  EXPECT_EQ("granted", GetServiceWorkerPushPermissionState(iframe));
}

// Tests that iframes not using their normal StoragePartition don't have
// notification permission, even if they would have permission outside of an
// isolated app.
IN_PROC_BROWSER_TEST_F(NotificationPermissionBrowserTest,
                       IframesInNonDefaultPartitionDontGetPermission) {
  GrantNotificationPermissionForTest(TesterUrl());

  // Verify that TesterUrl() has notification/push permission.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), TesterUrl()));
  content::RenderFrameHost* main_frame = GetActiveWebContents()->GetMainFrame();
  EXPECT_EQ("granted", GetNotificationPermissionState(main_frame));
  EXPECT_EQ("granted", GetServiceWorkerNotificationPermissionState(main_frame));
  EXPECT_EQ("granted", QueryNotificationPermissionState(main_frame));
  EXPECT_EQ("granted",
            QueryServiceWorkerNotificationPermissionState(main_frame));
  EXPECT_EQ("granted", GetPushPermissionState(main_frame));
  EXPECT_EQ("granted", GetServiceWorkerPushPermissionState(main_frame));

  // Load a site that uses a dedicated StoragePartition and verify that it has
  // default notification/push permissions.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), IsolatedEmbedderUrl()));
  main_frame = GetActiveWebContents()->GetMainFrame();
  EXPECT_EQ("default", GetNotificationPermissionState(main_frame));
  EXPECT_EQ("denied", GetServiceWorkerNotificationPermissionState(main_frame));
  EXPECT_EQ("prompt", QueryNotificationPermissionState(main_frame));
  EXPECT_EQ("prompt",
            QueryServiceWorkerNotificationPermissionState(main_frame));
  EXPECT_EQ("prompt", GetPushPermissionState(main_frame));
  EXPECT_EQ("prompt", GetServiceWorkerPushPermissionState(main_frame));

  // Load TesterUrl() in an iframe inside the dedicated StoragePartition page.
  // Even though TesterUrl() has notification/push permission when in a main
  // frame, it shouldn't when it's embedded in a different StoragePartition.
  content::RenderFrameHost* iframe =
      CreateChildIframe(GetActiveWebContents()->GetMainFrame(), TesterUrl());
  EXPECT_EQ(TesterUrl(), iframe->GetLastCommittedURL());
  EXPECT_EQ("denied", GetNotificationPermissionState(iframe));
  EXPECT_EQ("denied", GetServiceWorkerNotificationPermissionState(iframe));
  EXPECT_EQ("denied", QueryNotificationPermissionState(iframe));
  EXPECT_EQ("denied", QueryServiceWorkerNotificationPermissionState(iframe));
  EXPECT_EQ("denied", RequestNotificationPermission(iframe));
  EXPECT_EQ("denied", GetPushPermissionState(iframe));
  EXPECT_EQ("denied", GetServiceWorkerPushPermissionState(iframe));
  EXPECT_EQ("NotAllowedError: Registration failed - permission denied",
            RequestPushPermission(iframe));
}
