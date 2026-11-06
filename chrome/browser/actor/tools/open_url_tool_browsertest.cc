// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/open_url_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/actor.mojom.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor {

namespace {

// Parameterized on `open_in_new_tab`, so behavior shared by both modes is
// tested in each.
class ActorOpenUrlToolBrowserTest : public ActorToolsTest,
                                    public testing::WithParamInterface<bool> {
 public:
  ActorOpenUrlToolBrowserTest() {
    // Require confirmation for navigations to new origins so that tests fail
    // if the tool's origin grant isn't applied.
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kGlicCrossOriginNavigationGating,
        {{kGlicConfirmNavigationToNewOrigins.name, "true"}});
  }
  ~ActorOpenUrlToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  bool OpenInNewTab() const { return GetParam(); }

  std::unique_ptr<ToolRequest> MakeOpenUrlRequest(tabs::TabInterface& tab,
                                                  const GURL& url) {
    return std::make_unique<OpenUrlToolRequest>(
        ActorSurfaceHandle::From(tab.GetHandle()), url, OpenInNewTab());
  }

  // Returns the tab that the tool navigated: `source_tab` in current-tab mode,
  // or the new tab otherwise. In new-tab mode, also checks that exactly one
  // foreground tab was created in `source_tab`'s window, that it was added to
  // the task, and that `source_tab` still shows `source_url`.
  tabs::TabInterface* GetNavigatedTab(tabs::TabInterface* source_tab,
                                      const GURL& source_url,
                                      int initial_tab_count) {
    if (!OpenInNewTab()) {
      EXPECT_EQ(tab_list()->GetTabCount(), initial_tab_count);
      return source_tab;
    }
    EXPECT_EQ(tab_list()->GetTabCount(), initial_tab_count + 1);
    tabs::TabInterface* new_tab = tab_list()->GetActiveTab();
    if (!new_tab) {
      ADD_FAILURE() << "No active tab";
      return nullptr;
    }
    EXPECT_NE(new_tab, source_tab);
    EXPECT_EQ(new_tab->GetBrowserWindowInterface(),
              source_tab->GetBrowserWindowInterface());
    EXPECT_TRUE(actor_task().HasTab(new_tab->GetHandle()));
    EXPECT_EQ(source_tab->GetContents()->GetLastCommittedURL(), source_url);
    return new_tab;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

std::string OpenUrlModeName(const testing::TestParamInfo<bool>& info) {
  return info.param ? "NewTab" : "CurrentTab";
}

INSTANTIATE_TEST_SUITE_P(All,
                         ActorOpenUrlToolBrowserTest,
                         testing::Bool(),
                         &OpenUrlModeName);

// The URL is opened in the given tab, or in a new foreground tab in the given
// tab's window that is added to the task.
IN_PROC_BROWSER_TEST_P(ActorOpenUrlToolBrowserTest, OpenUrl) {
  const GURL url_start = embedded_https_test_server().GetURL(
      "example.com", "/actor/blank.html?start");
  const GURL url_target = embedded_https_test_server().GetURL(
      "example.com", "/actor/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_start));
  const int initial_tab_count = tab_list()->GetTabCount();
  tabs::TabInterface* source_tab = active_tab();

  std::unique_ptr<ToolRequest> action =
      MakeOpenUrlRequest(*source_tab, url_target);
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  tabs::TabInterface* navigated_tab =
      GetNavigatedTab(source_tab, url_start, initial_tab_count);
  ASSERT_TRUE(navigated_tab);
  EXPECT_EQ(navigated_tab->GetContents()->GetLastCommittedURL(), url_target);
}

// A cross-origin destination is allowed under navigation gating, because the
// tool grants the destination's origin.
IN_PROC_BROWSER_TEST_P(ActorOpenUrlToolBrowserTest, CrossOrigin) {
  const GURL url_start =
      embedded_https_test_server().GetURL("/actor/blank.html?start");
  const GURL url_target = embedded_https_test_server().GetURL(
      "example.com", "/actor/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_start));
  const int initial_tab_count = tab_list()->GetTabCount();
  tabs::TabInterface* source_tab = active_tab();

  std::unique_ptr<ToolRequest> action =
      MakeOpenUrlRequest(*source_tab, url_target);
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  tabs::TabInterface* navigated_tab =
      GetNavigatedTab(source_tab, url_start, initial_tab_count);
  ASSERT_TRUE(navigated_tab);
  EXPECT_EQ(navigated_tab->GetContents()->GetLastCommittedURL(), url_target);
}

// If the navigation fails, the error is reported. In new-tab mode, the new tab
// is left open and stays in the task.
IN_PROC_BROWSER_TEST_P(ActorOpenUrlToolBrowserTest, NavigationFails) {
  const GURL url_start = embedded_https_test_server().GetURL(
      "example.com", "/actor/blank.html?start");
  const GURL url_target =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_start));
  const int initial_tab_count = tab_list()->GetTabCount();
  tabs::TabInterface* source_tab = active_tab();

  std::unique_ptr<content::URLLoaderInterceptor> interceptor =
      content::URLLoaderInterceptor::SetupRequestFailForURL(url_target,
                                                            net::ERR_FAILED);

  std::unique_ptr<ToolRequest> action =
      MakeOpenUrlRequest(*source_tab, url_target);
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kNavigateCommittedErrorPage);

  tabs::TabInterface* navigated_tab =
      GetNavigatedTab(source_tab, url_start, initial_tab_count);
  ASSERT_TRUE(navigated_tab);
  EXPECT_TRUE(
      navigated_tab->GetContents()->GetPrimaryMainFrame()->IsErrorDocument());
}

// An invalid URL is rejected without navigating or creating a tab.
IN_PROC_BROWSER_TEST_P(ActorOpenUrlToolBrowserTest, InvalidUrl) {
  const GURL url_start = embedded_https_test_server().GetURL(
      "example.com", "/actor/blank.html?start");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_start));
  const int initial_tab_count = tab_list()->GetTabCount();

  std::unique_ptr<ToolRequest> action =
      MakeOpenUrlRequest(*active_tab(), GURL("not a url"));
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kArgumentsInvalid);

  EXPECT_EQ(tab_list()->GetTabCount(), initial_tab_count);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url_start);
}

class ActorOpenUrlToolNoGatingBrowserTest : public ActorOpenUrlToolBrowserTest {
 public:
  ActorOpenUrlToolNoGatingBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        kGlicCrossOriginNavigationGating);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ActorOpenUrlToolNoGatingBrowserTest,
                         testing::Bool(),
                         &OpenUrlModeName);

// Without navigation gating, a blocked destination fails validation without
// navigating or creating a tab.
IN_PROC_BROWSER_TEST_P(ActorOpenUrlToolNoGatingBrowserTest, BlockedUrl) {
  const GURL url_start =
      embedded_https_test_server().GetURL("/actor/blank.html?start");
  const GURL url_target = embedded_https_test_server().GetURL(
      "blocked.example.com", "/actor/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_start));
  const int initial_tab_count = tab_list()->GetTabCount();

  std::unique_ptr<ToolRequest> action =
      MakeOpenUrlRequest(*active_tab(), url_target);
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kUrlBlocked);

  EXPECT_EQ(tab_list()->GetTabCount(), initial_tab_count);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url_start);
}

}  // namespace

}  // namespace actor
