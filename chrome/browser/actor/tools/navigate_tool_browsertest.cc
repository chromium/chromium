// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

using base::test::TestFuture;
using content::ChildFrameAt;
using content::TestNavigationManager;
using content::WaitForCopyableViewInWebContents;
using content::WaitForDOMContentLoaded;

namespace actor {

namespace {

class ActorNavigateToolBrowserTest : public ActorToolsTest {
 public:
  ActorNavigateToolBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{kGlicCrossOriginNavigationGating});
  }
  ~ActorNavigateToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Basic test of the NavigateTool.
IN_PROC_BROWSER_TEST_F(ActorNavigateToolBrowserTest, NavigateTool) {
  const GURL url_start =
      embedded_test_server()->GetURL("/actor/blank.html?start");
  const GURL url_target =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_start));

  std::unique_ptr<ToolRequest> action =
      MakeNavigateRequest(*active_tab(), url_target.spec());
  ActResultFuture result_success;
  actor_task().Act(ToRequestList(action), result_success.GetCallback());
  ExpectOkResult(result_success);

  EXPECT_EQ(web_contents()->GetURL(), url_target);
}

// Ensure that when navigating to a new document, the navigate tool delays
// completion until the new page has fired the load event.
IN_PROC_BROWSER_TEST_F(ActorNavigateToolBrowserTest,
                       NavigateTool_DelaysUntilLoad) {
  const GURL url_first =
      embedded_test_server()->GetURL("/actor/simple_iframe.html?start");
  const GURL url_second =
      embedded_test_server()->GetURL("/actor/simple_iframe.html?target");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  const GURL url_subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)
          ->GetLastCommittedURL();

  TestNavigationManager subframe_manager(web_contents(), url_subframe);
  TestNavigationManager main_manager(web_contents(), url_second);

  std::unique_ptr<ToolRequest> action =
      MakeNavigateRequest(*active_tab(), url_second.spec());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());

  // Wait for the main frame navigation to finish and for the main document to
  // reach DOMContentLoaded and for a frame to be presented.
  ASSERT_TRUE(main_manager.WaitForNavigationFinished());
  ASSERT_TRUE(WaitForDOMContentLoaded(main_frame()));
  WaitForCopyableViewInWebContents(web_contents());

  // Prevent the subframe response from being processed.
  ASSERT_TRUE(subframe_manager.WaitForResponse());

  EXPECT_FALSE(result.IsReady());
  TinyWait();
  EXPECT_FALSE(result.IsReady());
  ASSERT_FALSE(web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame());

  // Unblocking the subframe response will allow the page to fire the load event
  // and complete the tool request.
  ASSERT_TRUE(subframe_manager.WaitForNavigationFinished());
  ExpectOkResult(result);
}

IN_PROC_BROWSER_TEST_F(ActorNavigateToolBrowserTest,
                       NavigateTool_TargetUrlRestriction) {
  const GURL url_start =
      embedded_https_test_server().GetURL("/actor/blank.html?start");
  const GURL url_target = embedded_https_test_server().GetURL(
      "blocked.example.com", "/actor/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_start));

  std::unique_ptr<ToolRequest> action =
      MakeNavigateRequest(*active_tab(), url_target.spec());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kUrlBlocked);

  EXPECT_EQ(web_contents()->GetURL(), url_start);
}

// Test that the navigate tool correctly adds the acted on tab to the task's set
// of tabs.
IN_PROC_BROWSER_TEST_F(ActorNavigateToolBrowserTest,
                       NavigateTool_RecordActingOnTask) {
  ASSERT_TRUE(actor_task().GetTabs().empty());

  const GURL url_target =
      embedded_test_server()->GetURL("/actor/blank.html?target");

  std::unique_ptr<ToolRequest> action =
      MakeNavigateRequest(*active_tab(), url_target.spec());
  ActResultFuture result_success;
  actor_task().Act(ToRequestList(action), result_success.GetCallback());
  ExpectOkResult(result_success);

  EXPECT_EQ(web_contents()->GetURL(), url_target);

  EXPECT_EQ(actor_task().GetTabs().size(), 1ul);
  EXPECT_TRUE(actor_task().GetTabs().contains(active_tab()->GetHandle()));
}

class ActorNavigateToolRequestBrowserTest
    : public ActorToolsTest,
      public ::testing::WithParamInterface<
          bool /* enable GlicNavigateToolUseOpaqueInitiator */> {
 public:
  ActorNavigateToolRequestBrowserTest() {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{kGlicNavigateToolUseOpaqueInitiator},
          /*disabled_features=*/{kGlicCrossOriginNavigationGating});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{kGlicNavigateToolUseOpaqueInitiator,
                                 kGlicCrossOriginNavigationGating});
    }
  }

  ~ActorNavigateToolRequestBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();

    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);

    embedded_https_test_server().RegisterRequestMonitor(base::BindRepeating(
        &ActorNavigateToolRequestBrowserTest::MonitorRequest,
        base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  void MonitorRequest(const net::test_server::HttpRequest& request) {
    if (filter_relative_url_ == request.relative_url) {
      last_request_headers_ = request.headers;
    }
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::test_server::HttpRequest::HeaderMap last_request_headers_;
  std::string filter_relative_url_;
};

// Ensure that when NavigateTool triggers a navigation, whether an opaque
// initiator origin is used for the navigation request should control whether
// SameSite=strict cookies are sent.
IN_PROC_BROWSER_TEST_P(ActorNavigateToolRequestBrowserTest,
                       OpaqueInitiatorChangesStrictCookieBehavior) {
  const GURL url =
      embedded_https_test_server().GetURL("a.test", "/actor/blank.html");

  content::BrowserContext* browser_context =
      web_contents()->GetBrowserContext();

  ASSERT_TRUE(content::SetCookie(
      browser_context, url,
      "strict-cookie=hello-from-strict; SameSite=strict; Secure"));
  ASSERT_TRUE(content::SetCookie(
      browser_context, url, "lax-cookie=hello-from-lax; SameSite=lax; Secure"));
  ASSERT_TRUE(
      content::SetCookie(browser_context, url,
                         "none-cookie=hello-from-none; SameSite=none; Secure"));

  // Filter out spurious requests like the favicon request from our monitoring.
  filter_relative_url_ = "/actor/blank.html";
  std::unique_ptr<ToolRequest> action =
      MakeNavigateRequest(*active_tab(), url.spec());
  ActResultFuture result_success;
  actor_task().Act(ToRequestList(action), result_success.GetCallback());
  ExpectOkResult(result_success);

  EXPECT_EQ(web_contents()->GetURL(), url);

  std::string cookies = last_request_headers_["cookie"];
  if (GetParam()) {
    EXPECT_EQ("lax-cookie=hello-from-lax; none-cookie=hello-from-none",
              cookies);
  } else {
    EXPECT_EQ(
        "strict-cookie=hello-from-strict; lax-cookie=hello-from-lax; "
        "none-cookie=hello-from-none",
        cookies);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ActorNavigateToolRequestBrowserTest,
                         ::testing::Values(false, true));

}  // namespace
}  // namespace actor
