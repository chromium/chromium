// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/permissions/chrome_permissions_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_tasks/public/features.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"

class PermissionDelegationBrowserTest : public InProcessBrowserTest {
 public:
  PermissionDelegationBrowserTest()
      : geolocation_overrider_(
            std::make_unique<device::ScopedGeolocationOverrider>(0, 0)) {}

  PermissionDelegationBrowserTest(const PermissionDelegationBrowserTest&) =
      delete;
  PermissionDelegationBrowserTest& operator=(
      const PermissionDelegationBrowserTest&) = delete;

  ~PermissionDelegationBrowserTest() override = default;

  void SetUpOnMainThread() override {
    permissions::PermissionRequestManager* manager =
        permissions::PermissionRequestManager::FromWebContents(
            GetWebContents());
    mock_permission_prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(manager);

    embedded_https_test_server().ServeFilesFromSourceDirectory(
        GetChromeTestDataDir());
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(&embedded_https_test_server());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  void TearDownOnMainThread() override {
    mock_permission_prompt_factory_.reset();
  }

  permissions::MockPermissionPromptFactory* prompt_factory() {
    return mock_permission_prompt_factory_.get();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  std::unique_ptr<permissions::MockPermissionPromptFactory>
      mock_permission_prompt_factory_;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;
};

IN_PROC_BROWSER_TEST_F(PermissionDelegationBrowserTest, DelegatedToTwoFrames) {
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  // Main frame is on a.com, iframe 1 is on b.com and iframe 2 is on c.com.
  GURL main_frame_url =
      embedded_https_test_server().GetURL("a.com", "/two_iframes_blank.html");
  GURL iframe_url_1 =
      embedded_https_test_server().GetURL("b.com", "/simple.html");
  GURL iframe_url_2 =
      embedded_https_test_server().GetURL("c.com", "/simple.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));
  content::RenderFrameHost* main_frame =
      GetWebContents()->GetPrimaryMainFrame();

  // Delegate permission to both frames.
  EXPECT_TRUE(content::ExecJs(
      main_frame,
      "document.getElementById('iframe1').allow = 'geolocation *';"));
  EXPECT_TRUE(content::ExecJs(
      main_frame,
      "document.getElementById('iframe2').allow = 'geolocation *';"));

  // Load the iframes.
  EXPECT_TRUE(
      content::NavigateIframeToURL(GetWebContents(), "iframe1", iframe_url_1));
  EXPECT_TRUE(
      content::NavigateIframeToURL(GetWebContents(), "iframe2", iframe_url_2));

  content::RenderFrameHost* frame_1 = content::FrameMatchingPredicate(
      GetWebContents()->GetPrimaryPage(),
      base::BindRepeating(&content::FrameMatchesName, "iframe1"));
  EXPECT_NE(nullptr, frame_1);
  content::RenderFrameHost* frame_2 = content::FrameMatchingPredicate(
      GetWebContents()->GetPrimaryPage(),
      base::BindRepeating(&content::FrameMatchesName, "iframe2"));
  EXPECT_NE(nullptr, frame_2);

  // Request permission from the first iframe.
  EXPECT_EQ(true, content::EvalJs(
                      frame_1,
                      "new Promise(resolve => {"
                      "  navigator.geolocation.getCurrentPosition(function(){ "
                      "    resolve(true); });"
                      "});"));

  // A prompt should have been shown with the top level origin rather than the
  // iframe origin.
  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());
  EXPECT_TRUE(prompt_factory()->RequestOriginSeen(
      main_frame_url.DeprecatedGetOriginAsURL()));
  EXPECT_FALSE(prompt_factory()->RequestOriginSeen(
      iframe_url_1.DeprecatedGetOriginAsURL()));
  EXPECT_FALSE(prompt_factory()->RequestOriginSeen(
      iframe_url_2.DeprecatedGetOriginAsURL()));

  // Request permission from the second iframe. Because it was granted to the
  // top level frame, it should also be granted to this iframe and there should
  // be no prompt.
  EXPECT_EQ(true, content::EvalJs(
                      frame_2,
                      "new Promise(resolve => {"
                      "  navigator.geolocation.getCurrentPosition(function(){ "
                      "    resolve(true); });"
                      "});"));
  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());

  // Request permission from the top level frame. It should already be granted
  // to this iframe and there should be no prompt.
  EXPECT_EQ(true, content::EvalJs(
                      main_frame,
                      "new Promise(resolve => {"
                      "  navigator.geolocation.getCurrentPosition(function(){ "
                      "    resolve(true); });"
                      "});"));
  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());
}

class ContextualTasksPermissionDelegationBrowserTest
    : public PermissionDelegationBrowserTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      contextual_tasks::kContextualTasks};
};

IN_PROC_BROWSER_TEST_F(ContextualTasksPermissionDelegationBrowserTest,
                       GetEmbeddingOriginOverride) {
  GURL contextual_tasks_url(chrome::kChromeUIContextualTasksURL);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), contextual_tasks_url));
  content::WebContents* outer_web_contents = GetWebContents();

  // Wait for the WebUI to initialize and spawn the inner <webview> guest
  // WebContents.
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return !outer_web_contents->GetInnerWebContents().empty(); }));

  content::WebContents* inner_web_contents =
      outer_web_contents->GetInnerWebContents()[0];
  ASSERT_TRUE(inner_web_contents);
  EXPECT_EQ(outer_web_contents, inner_web_contents->GetOuterWebContents());

  // Verify that GetEmbeddingOriginOverride correctly traverses from the inner
  // <webview> guest WebContents up to the outer WebUI WebContents.
  GURL requesting_origin("https://example.com");
  std::optional<GURL> override_origin =
      ChromePermissionsClient::GetInstance()->GetEmbeddingOriginOverride(
          requesting_origin, inner_web_contents);

  ASSERT_TRUE(override_origin.has_value());
  EXPECT_EQ(contextual_tasks_url, *override_origin);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPermissionDelegationBrowserTest,
                       RequestPermissionInWebView) {
  GURL default_ai_url =
      contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile())
          ->GetDefaultAiPageUrl();
  std::string expected_host(default_ai_url.host());

  // Intercept the default AI page load to prevent ERR_CONNECTION_REFUSED and
  // allow JavaScript to execute.
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](std::string expected_host,
         content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.host() == expected_host) {
          content::URLLoaderInterceptor::WriteResponse(
              "HTTP/1.1 200 OK\nContent-Type: text/html\n\n",
              "<html><body>Test Guest Page</body></html>",
              params->client.get());
          return true;
        }
        return false;
      },
      expected_host));

  GURL contextual_tasks_url(chrome::kChromeUIContextualTasksURL);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), contextual_tasks_url));
  content::WebContents* outer_web_contents = GetWebContents();

  // Wait for the WebUI to initialize and spawn the inner <webview> guest
  // WebContents.
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return !outer_web_contents->GetInnerWebContents().empty(); }));

  // The guest (inner) web contents should load a zero state page.
  content::WebContents* inner_web_contents =
      outer_web_contents->GetInnerWebContents()[0];
  ASSERT_TRUE(inner_web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(inner_web_contents));

  // Trigger a geolocation permission request from the guest WebContents.
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_EQ(true, content::EvalJs(
                      inner_web_contents->GetPrimaryMainFrame(),
                      "new Promise(resolve => {"
                      "  navigator.geolocation.getCurrentPosition(function(){ "
                      "    resolve(true); });"
                      "});"));

  // Verify that the permission request is shown with the canonicalized DSE
  // origin.
  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());
  EXPECT_TRUE(prompt_factory()->RequestOriginSeen(
      default_ai_url.DeprecatedGetOriginAsURL()));
}
