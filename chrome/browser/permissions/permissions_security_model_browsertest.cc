// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/embedder_support/switches.h"
#include "components/permissions/features.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "content/public/test/browser_test.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "url/gurl.h"

namespace {

// Helper for CreateBlobURL() and CreateFilesystemURL().
// ASSERT_* macros can only be used in functions returning void.
void AssertResultIsString(const content::EvalJsResult& result) {
  // Verify no error.
  ASSERT_EQ("", result.error);
  // We could use result.value.is_string(), but this logs the actual type in
  // case of mismatch.
  ASSERT_EQ(base::Value::Type::STRING, result.value.type()) << result.value;
}

// Creates a blob containing dummy HTML, then returns its URL.
// Executes javascript to do so in |rfh|.
GURL CreateBlobURL(content::RenderFrameHost* rfh) {
  content::EvalJsResult result = content::EvalJs(rfh, R"(
    const blob = new Blob(["foo"], {type: "text/html"});
    URL.createObjectURL(blob)
  )");

  AssertResultIsString(result);
  return GURL(result.ExtractString());
}

// Tests of permissions behavior for an inheritance and embedding of an origin.
// Test fixtures are run with and without the `PermissionsRevisedOriginHandling`
// flag.
class PermissionsSecurityModelBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  PermissionsSecurityModelBrowserTest() {
    feature_list_.InitWithFeatureState(
        permissions::features::kRevisedOriginHandling, GetParam());
    geolocation_overrider_ =
        std::make_unique<device::ScopedGeolocationOverrider>(0, 0);
  }
  PermissionsSecurityModelBrowserTest(
      const PermissionsSecurityModelBrowserTest&) = delete;
  PermissionsSecurityModelBrowserTest& operator=(
      const PermissionsSecurityModelBrowserTest&) = delete;
  ~PermissionsSecurityModelBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        embedder_support::kDisablePopupBlocking);
  }

  content::WebContents* OpenPopup(Browser* browser, const GURL& url) const {
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    content::ExecuteScriptAsync(
        contents, content::JsReplace("window.open($1, '', '[]');", url));
    Browser* popup = ui_test_utils::WaitForBrowserToOpen();
    EXPECT_NE(popup, browser);
    content::WebContents* popup_contents =
        popup->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(WaitForRenderFrameReady(popup_contents->GetMainFrame()));
    WaitForLoadStop(popup_contents);
    return popup_contents;
  }

  bool IsRevisedOriginHandlingEnabled() { return GetParam(); }

  void VeriftyPermissions(content::WebContents* opener_or_embedder_contents,
                          content::RenderFrameHost* test_rfh,
                          std::string& check_permission,
                          std::string& request_permission,
                          bool is_notification = false) {
    ASSERT_FALSE(content::EvalJs(opener_or_embedder_contents, check_permission)
                     .value.GetBool());
    ASSERT_FALSE(content::EvalJs(test_rfh, check_permission).value.GetBool());

    const bool is_embedder =
        test_rfh->IsDescendantOf(opener_or_embedder_contents->GetMainFrame());

    permissions::PermissionRequestManager* manager =
        permissions::PermissionRequestManager::FromWebContents(
            opener_or_embedder_contents);
    std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
        std::make_unique<permissions::MockPermissionPromptFactory>(manager);

    // Enable auto-accept of a permission request.
    bubble_factory->set_response_type(
        permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

    // Request permission on the opener or embedder contents.
    EXPECT_EQ("granted",
              content::EvalJs(opener_or_embedder_contents, request_permission,
                              content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                  .ExtractString());
    EXPECT_EQ(1, bubble_factory->TotalRequestCount());

    // Disable auto-accept of a permission request.
    bubble_factory->set_response_type(
        permissions::PermissionRequestManager::AutoResponseType::NONE);

    EXPECT_TRUE(content::EvalJs(opener_or_embedder_contents, check_permission)
                    .value.GetBool());

    // Verify permissions on the test RFH.
    {
      // If `test_rfh` is not a descendant of `opener_or_embedder_contents`, in
      // other words if `test_rfh` was created via `Window.open()`, permissions
      // are propagated from an opener WebContents only if
      // `RevisedOriginHandlingEnabled` is enabled.
      const bool expect_granted =
          IsRevisedOriginHandlingEnabled() || is_embedder;

      EXPECT_EQ(expect_granted,
                content::EvalJs(test_rfh, check_permission).value.GetBool());
    }

    // Request permission on the test RFH.
    {
      // If `test_rfh` is not a descendant of `opener_or_embedder_contents`,
      // permission request is allowed only for Notifications. If
      // `RevisedOriginHandlingEnabled` is enabled, permission request allowed
      // for all `ContentSettingsType`.
      const bool expect_granted =
          IsRevisedOriginHandlingEnabled() || is_embedder || is_notification;

      EXPECT_EQ(expect_granted ? "granted" : "failure",
                content::EvalJs(test_rfh, request_permission,
                                content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                    .ExtractString());
    }

    // There should not be the 2nd prompt.
    EXPECT_EQ(1, bubble_factory->TotalRequestCount());
  }

  void TestNotifications(content::WebContents* opener_or_embedder_contents,
                         content::RenderFrameHost* test_rfh) {
    std::string kCheckNotifications = R"((async () => {
       const PermissionStatus = await navigator.permissions.query({name:
       'notifications'}); return PermissionStatus.state === 'granted';
      })();)";

    std::string kRequestNotifications = R"(
        Notification.requestPermission(
            _ => domAutomationController.send('granted'),
            _ => domAutomationController.send('failure'));
        )";

    VeriftyPermissions(opener_or_embedder_contents, test_rfh,
                       kCheckNotifications, kRequestNotifications,
                       /*is_notification=*/true);
  }

  void TestGeolocation(content::WebContents* opener_or_embedder_contents,
                       content::RenderFrameHost* test_rfh) {
    std::string kCheckGeolocation = R"((async () => {
       const PermissionStatus = await navigator.permissions.query({name:
       'geolocation'}); return PermissionStatus.state === 'granted';
      })();)";

    std::string kRequestGeolocation = R"(
          navigator.geolocation.getCurrentPosition(
            _ => domAutomationController.send('granted'),
            _ => domAutomationController.send('failure'));
        )";

    VeriftyPermissions(opener_or_embedder_contents, test_rfh, kCheckGeolocation,
                       kRequestGeolocation);
  }

  void TestCamera(content::WebContents* opener_or_embedder_contents,
                  content::RenderFrameHost* test_rfh) {
    std::string kCheckCamera = R"((async () => {
     const PermissionStatus =
        await navigator.permissions.query({name: 'camera'});
     return PermissionStatus.state === 'granted';
    })();)";

    std::string kRequestCamera = R"(
    var constraints = { video: true };
    navigator.mediaDevices.getUserMedia(constraints).then(function(stream) {
        domAutomationController.send('granted');
    })
    .catch(function(err) {
        domAutomationController.send('failure');
    });
    )";

    VeriftyPermissions(opener_or_embedder_contents, test_rfh, kCheckCamera,
                       kRequestCamera);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PermissionsSecurityModelBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(PermissionsSecurityModelBrowserTest,
                       EmbedIframeAboutBlank) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/iframe_about_blank.html"));
  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  ASSERT_TRUE(main_rfh);

  content::WebContents* embedder_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* about_blank_iframe =
      content::FrameMatchingPredicate(
          embedder_contents, base::BindRepeating(&content::FrameMatchesName,
                                                 "about_blank_iframe"));
  ASSERT_TRUE(about_blank_iframe);

  TestNotifications(embedder_contents, about_blank_iframe);
  TestGeolocation(embedder_contents, about_blank_iframe);
  TestCamera(embedder_contents, about_blank_iframe);
}

IN_PROC_BROWSER_TEST_P(PermissionsSecurityModelBrowserTest,
                       WindowOpenAboutBlank) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* opener_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(opener_contents);

  content::WebContents* popup_contents =
      OpenPopup(browser(), GURL("about:blank"));
  ASSERT_TRUE(popup_contents);

  TestNotifications(opener_contents, popup_contents->GetMainFrame());
  TestGeolocation(opener_contents, popup_contents->GetMainFrame());
  TestCamera(opener_contents, popup_contents->GetMainFrame());
}

// `about:srcdoc` supports only embedder WebContents, hence no test for opener.
IN_PROC_BROWSER_TEST_P(PermissionsSecurityModelBrowserTest, EmbedIframeSrcDoc) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/iframe_srcdoc.html"));
  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  ASSERT_TRUE(main_rfh);

  content::WebContents* embedder_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* srcdoc_iframe = content::FrameMatchingPredicate(
      embedder_contents,
      base::BindRepeating(&content::FrameMatchesName, "srcdoc_iframe"));
  ASSERT_TRUE(srcdoc_iframe);

  TestNotifications(embedder_contents, srcdoc_iframe);
  TestGeolocation(embedder_contents, srcdoc_iframe);
  TestCamera(embedder_contents, srcdoc_iframe);
}

IN_PROC_BROWSER_TEST_P(PermissionsSecurityModelBrowserTest, EmbedIframeBlob) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/iframe_blob.html"));
  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  ASSERT_TRUE(main_rfh);

  content::WebContents* embedder_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* blob_iframe_rfh = content::FrameMatchingPredicate(
      embedder_contents,
      base::BindRepeating(&content::FrameMatchesName, "blob_iframe"));
  ASSERT_TRUE(blob_iframe_rfh);
  EXPECT_TRUE(blob_iframe_rfh->GetLastCommittedURL().SchemeIsBlob());

  TestNotifications(embedder_contents, blob_iframe_rfh);
  TestGeolocation(embedder_contents, blob_iframe_rfh);
  TestCamera(embedder_contents, blob_iframe_rfh);
}

IN_PROC_BROWSER_TEST_P(PermissionsSecurityModelBrowserTest, WindowOpenBlob) {
  if (GetParam()) {
    // Blob iframe on an opener contents does not work if
    // `kRevisedOriginHandling` feature enabled.
    // TODO(crbug.com/698985): Remove when the bug is fixed.
    return;
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), url, 1));
  content::WebContents* opener_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(opener_contents);

  content::WebContents* blob_popup_contents =
      OpenPopup(browser(), CreateBlobURL(opener_contents->GetMainFrame()));
  ASSERT_TRUE(blob_popup_contents);

  EXPECT_TRUE(blob_popup_contents->GetLastCommittedURL().SchemeIsBlob());

  TestNotifications(opener_contents, blob_popup_contents->GetMainFrame());
  TestGeolocation(opener_contents, blob_popup_contents->GetMainFrame());
  TestCamera(opener_contents, blob_popup_contents->GetMainFrame());
}

}  // anonymous namespace
