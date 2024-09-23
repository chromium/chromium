// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/notifications/non_persistent_notification_handler.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace {

using Permission = ukm::builders::Permission;

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

    EXPECT_EQ("iframe loaded",
              EvalJs(parent_rfh, content::JsReplace(R"(
                new Promise((resolve, reject) => {
                  const iframe = document.createElement('iframe');
                  iframe.id = 'child_iframe';
                  iframe.src = $1;
                  iframe.onload = _ => { resolve('iframe loaded') };
                  iframe.onerror = e => { reject(e) };
                  document.body.appendChild(iframe);
                }))",
                                                    iframe_src)));

    content::RenderFrameHost* iframe = ChildFrameAt(parent_rfh, 0);
    EXPECT_TRUE(iframe);
    EXPECT_EQ(iframe_src, iframe->GetLastCommittedURL());
    return iframe;
  }

 private:
  const base::FilePath server_root_{FILE_PATH_LITERAL("chrome/test/data")};
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  content::ContentMockCertVerifier mock_cert_verifier_;

  raw_ptr<content::ContentBrowserClient> original_client_ = nullptr;
  StoragePartitioningChromeContentBrowserClient partitioning_client_;
};

// Tests that undelegated permissions which have their default/prompt value on
// an origin are automatically denied in documents from that origin when
// loaded as a cross-origin iframe.
IN_PROC_BROWSER_TEST_F(NotificationPermissionBrowserTest,
                       UndelegatedPermissionDeniedIfNotGrantedToOrigin) {
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), TesterUrl()));
  content::RenderFrameHost* main_frame =
      GetActiveWebContents()->GetPrimaryMainFrame();
  EXPECT_EQ("default", EvalJs(main_frame, "getNotificationPermission()"));
  EXPECT_EQ("denied",
            EvalJs(main_frame, "getServiceWorkerNotificationPermission()"));

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), EmbedderUrl()));
  main_frame = GetActiveWebContents()->GetPrimaryMainFrame();
  EXPECT_EQ("default", EvalJs(main_frame, "getNotificationPermission()"));

  content::RenderFrameHost* iframe = CreateChildIframe(main_frame, TesterUrl());
  EXPECT_EQ("denied", EvalJs(iframe, "getNotificationPermission()"));
  EXPECT_EQ("denied",
            EvalJs(iframe, "getServiceWorkerNotificationPermission()"));
  EXPECT_EQ("denied", EvalJs(iframe, "getPushPermission()"));
  // TODO(crbug.com/40254041): This should return 'denied'.
  EXPECT_EQ("prompt", EvalJs(iframe, "getServiceWorkerPushPermission()"));
}

// Tests that undelegated permissions aren't delegated to an embedded frame
// as other permissions are. If 'example.com' was granted notification
// permissions by the user when it was a top-level frame, then it retains that
// permission when iframed in another page, regardless of the other page's
// permission status.
IN_PROC_BROWSER_TEST_F(NotificationPermissionBrowserTest,
                       UndelegatedPermissionsAreNotDelegated) {
  GrantNotificationPermissionForTest(TesterUrl());

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), TesterUrl()));
  content::RenderFrameHost* main_frame =
      GetActiveWebContents()->GetPrimaryMainFrame();
  EXPECT_EQ("granted", EvalJs(main_frame, "getNotificationPermission()"));
  EXPECT_EQ("granted",
            EvalJs(main_frame, "getServiceWorkerNotificationPermission()"));

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), EmbedderUrl()));
  main_frame = GetActiveWebContents()->GetPrimaryMainFrame();
  EXPECT_EQ("default", EvalJs(main_frame, "getNotificationPermission()"));
  EXPECT_EQ("denied",
            EvalJs(main_frame, "getServiceWorkerNotificationPermission()"));
  EXPECT_EQ("prompt", EvalJs(main_frame, "getPushPermission()"));
  // TODO(crbug.com/40254041): This should return 'denied'.
  EXPECT_EQ("prompt", EvalJs(main_frame, "getServiceWorkerPushPermission()"));

  content::RenderFrameHost* iframe = CreateChildIframe(main_frame, TesterUrl());
  EXPECT_EQ("granted", EvalJs(iframe, "getNotificationPermission()"));
  EXPECT_EQ("granted",
            EvalJs(iframe, "getServiceWorkerNotificationPermission()"));
  EXPECT_EQ("granted", EvalJs(iframe, "getPushPermission()"));
  EXPECT_EQ("granted", EvalJs(iframe, "getServiceWorkerPushPermission()"));
}

// Tests that iframes not using their normal StoragePartition don't have
// notification permission, even if they would have permission outside of an
// isolated app.
IN_PROC_BROWSER_TEST_F(NotificationPermissionBrowserTest,
                       IframesInNonDefaultPartitionDontGetPermission) {
  GrantNotificationPermissionForTest(TesterUrl());

  // Verify that TesterUrl() has notification/push permission.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), TesterUrl()));
  content::RenderFrameHost* main_frame =
      GetActiveWebContents()->GetPrimaryMainFrame();
  EXPECT_EQ("granted", EvalJs(main_frame, "getNotificationPermission()"));
  EXPECT_EQ("granted",
            EvalJs(main_frame, "getServiceWorkerNotificationPermission()"));
  EXPECT_EQ("granted", EvalJs(main_frame, "queryNotificationPermission()"));
  EXPECT_EQ("granted",
            EvalJs(main_frame, "queryServiceWorkerNotificationPermission()"));
  EXPECT_EQ("granted", EvalJs(main_frame, "getPushPermission()"));
  EXPECT_EQ("granted", EvalJs(main_frame, "getServiceWorkerPushPermission()"));

  // Load a site that uses a dedicated StoragePartition and verify that it has
  // default notification/push permissions.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), IsolatedEmbedderUrl()));
  main_frame = GetActiveWebContents()->GetPrimaryMainFrame();
  EXPECT_EQ("default", EvalJs(main_frame, "getNotificationPermission()"));
  EXPECT_EQ("denied",
            EvalJs(main_frame, "getServiceWorkerNotificationPermission()"));
  EXPECT_EQ("prompt", EvalJs(main_frame, "queryNotificationPermission()"));
  EXPECT_EQ("prompt",
            EvalJs(main_frame, "queryServiceWorkerNotificationPermission()"));
  EXPECT_EQ("prompt", EvalJs(main_frame, "getPushPermission()"));
  EXPECT_EQ("prompt", EvalJs(main_frame, "getServiceWorkerPushPermission()"));

  // Load TesterUrl() in an iframe inside the dedicated StoragePartition page.
  // Even though TesterUrl() has notification/push permission when in a main
  // frame, it shouldn't when it's embedded in a different StoragePartition.
  content::RenderFrameHost* iframe = CreateChildIframe(main_frame, TesterUrl());
  EXPECT_EQ("denied", EvalJs(iframe, "getNotificationPermission()"));
  EXPECT_EQ("denied",
            EvalJs(iframe, "getServiceWorkerNotificationPermission()"));
  EXPECT_EQ("denied", EvalJs(iframe, "queryNotificationPermission()"));
  EXPECT_EQ("denied",
            EvalJs(iframe, "queryServiceWorkerNotificationPermission()"));
  EXPECT_EQ("denied", EvalJs(iframe, "requestNotificationPermission()"));
  EXPECT_EQ("denied", EvalJs(iframe, "getPushPermission()"));
  EXPECT_EQ("denied", EvalJs(iframe, "getServiceWorkerPushPermission()"));
  EXPECT_EQ(
      "a JavaScript error: \"NotAllowedError: "
      "Registration failed - permission denied\"\n",
      EvalJs(iframe, "requestPushPermission()").error);
}

// Test that the Notifications.NonPersistentNotificationThirdPartyCount metric
// triggers in third-party contexts. Note: This test doesn't exactly fit with
// the others in this class, but the helper methods here are exactly what we
// needed and this test will be removed once the metric is removed.
IN_PROC_BROWSER_TEST_F(NotificationPermissionBrowserTest,
                       NonPersistentNotificationThirdPartyCountMetricTest) {
  GrantNotificationPermissionForTest(TesterUrl());

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), TesterUrl()));
  content::RenderFrameHost* main_frame =
      GetActiveWebContents()->GetPrimaryMainFrame();

  base::HistogramTester histogram_tester;
  const std::string histogram_name =
      "Notifications.NonPersistentNotificationThirdPartyCount";

  const std::string show_notification_js = R"(new Promise((resolve) => {
     const notification = new Notification("done");
     notification.onshow = () => {
       const title = notification.title;
       notification.close();
       resolve(title);
     };
   });)";

  EXPECT_EQ("done", EvalJs(main_frame, show_notification_js));

  histogram_tester.ExpectBucketCount(histogram_name, false, 1);
  histogram_tester.ExpectBucketCount(histogram_name, true, 0);

  content::RenderFrameHost* iframe = CreateChildIframe(main_frame, TesterUrl());

  EXPECT_EQ("done", EvalJs(iframe, show_notification_js));

  histogram_tester.ExpectBucketCount(histogram_name, false, 2);
  histogram_tester.ExpectBucketCount(histogram_name, true, 0);

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), EmbedderUrl()));
  main_frame = GetActiveWebContents()->GetPrimaryMainFrame();
  iframe = CreateChildIframe(main_frame, TesterUrl());

  EXPECT_EQ("done", EvalJs(iframe, show_notification_js));

  histogram_tester.ExpectBucketCount(histogram_name, false, 2);
  histogram_tester.ExpectBucketCount(histogram_name, true, 1);
}

// Tests that non-persistent notifications (i.e. doesn't use
// the Push API) records PermissionUsage and Notification UKMs.
IN_PROC_BROWSER_TEST_F(NotificationPermissionBrowserTest,
                       NonPersistentNotificationRecordsUkms) {
  GrantNotificationPermissionForTest(TesterUrl());

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), TesterUrl()));
  content::RenderFrameHost* main_frame =
      GetActiveWebContents()->GetPrimaryMainFrame();

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  const std::string show_notification_js = R"(new Promise((resolve) => {
     const notification = new Notification("done");
     notification.onshow = () => {
       const title = notification.title;
       notification.close();
       resolve(title);
     };
   });)";

  EXPECT_EQ("done", EvalJs(main_frame, show_notification_js));
  const auto usage_entries = ukm_recorder.GetEntriesByName("PermissionUsage");
  ASSERT_EQ(1u, usage_entries.size());
}

IN_PROC_BROWSER_TEST_F(NotificationPermissionBrowserTest,
                       DisablePermissionRecordsUkms) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto* manager = permissions::PermissionRequestManager::FromWebContents(
      GetActiveWebContents());
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), TesterUrl()));
  permissions::MockPermissionRequest req(
      permissions::RequestType::kNotifications);
  manager->AddRequest(GetActiveWebContents()->GetPrimaryMainFrame(), &req);
  bubble_factory->WaitForPermissionBubble();
  manager->Accept();

  GrantNotificationPermissionForTest(TesterUrl());

  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<NonPersistentNotificationHandler>();
  handler->DisableNotifications(browser()->profile(), TesterUrl());

  const auto action_entries = ukm_recorder.GetEntriesByName("Permission");
  ASSERT_EQ(2u, action_entries.size());

  // The revocation event uses the notification source_id type
  EXPECT_EQ(ukm::SourceIdType::NOTIFICATION_ID,
            ukm::GetSourceIdType(action_entries[1]->source_id));

  // Expect one GRANT and one REVOKE event
  ukm_recorder.ExpectEntryMetric(
      action_entries[0], Permission::kActionName,
      static_cast<int>(permissions::PermissionAction::GRANTED));
  ukm_recorder.ExpectEntryMetric(
      action_entries[1], Permission::kActionName,
      static_cast<int>(permissions::PermissionAction::REVOKED));
}
