// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/load_and_extract_content_tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor {

namespace {

class ActorLoadAndExtractContentToolBrowserTest : public ActorToolsTest {
 public:
  ActorLoadAndExtractContentToolBrowserTest() = default;
  ~ActorLoadAndExtractContentToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    browser()->window()->Show();
    browser()->window()->Activate();
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &ActorLoadAndExtractContentToolBrowserTest::HandleStallRequest,
        base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  std::unique_ptr<net::test_server::HttpResponse> HandleStallRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().path() != "/stall") {
      return nullptr;
    }
    return std::make_unique<net::test_server::HungResponse>();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      kGlicActorLoadAndExtractContentTool};
};

// An observer that watches for new tabs and their navigations.
//
// This class attaches a `SingleTabNavigationWatcher` to every new tab inserted
// into the `TabStripModel`. It collects the URLs of the first successful
// navigation in each of those tabs.
class TabNavigationObserver : public TabStripModelObserver {
 public:
  explicit TabNavigationObserver(Browser* browser)
      : tab_strip_model_(browser->tab_strip_model()),
        initial_tab_count_(tab_strip_model_->count()) {
    tab_strip_model_->AddObserver(this);
  }

  ~TabNavigationObserver() override { tab_strip_model_->RemoveObserver(this); }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kInserted) {
      for (const auto& contents : change.GetInsert()->contents) {
        tabs_added_count_++;
        // Create a watcher for the new tab to track its navigation.
        web_contents_observers_.push_back(
            std::make_unique<SingleTabNavigationWatcher>(contents.contents,
                                                         this));
      }
      if (wait_for_added_tabs_run_loop_ &&
          tabs_added_count_ >= expected_tabs_to_add_) {
        wait_for_added_tabs_run_loop_->Quit();
      }
    }
  }

  void OnNavigationComplete(const GURL& url) { navigated_urls_.push_back(url); }

  void WaitForAddedTabs(int count) {
    if (tabs_added_count_ >= count) {
      return;
    }
    expected_tabs_to_add_ = count;
    wait_for_added_tabs_run_loop_ = std::make_unique<base::RunLoop>();
    wait_for_added_tabs_run_loop_->Run();
  }

  // If the tool is working correctly, it should closed all the tabs it opened.
  void VerifyTabCountRestored() const {
    EXPECT_EQ(tab_strip_model_->count(), initial_tab_count_);
  }

  int tabs_added_count() const { return tabs_added_count_; }
  std::vector<GURL> navigated_urls() const { return navigated_urls_; }

 private:
  // Observes a single WebContents for its committed navigations.
  class SingleTabNavigationWatcher : public content::WebContentsObserver {
   public:
    SingleTabNavigationWatcher(content::WebContents* web_contents,
                               TabNavigationObserver* owner)
        : content::WebContentsObserver(web_contents), owner_(owner) {}

    void DidFinishNavigation(
        content::NavigationHandle* navigation_handle) override {
      // We only care about the primary main frame's committed navigations.
      // We ignore error pages to ensure we capture the intended destination.
      if (navigation_handle->IsInPrimaryMainFrame() &&
          navigation_handle->HasCommitted() &&
          !navigation_handle->IsErrorPage()) {
        owner_->OnNavigationComplete(navigation_handle->GetURL());
      }
    }

   private:
    raw_ptr<TabNavigationObserver> owner_;
  };

  const raw_ptr<TabStripModel> tab_strip_model_;
  const int initial_tab_count_;
  int tabs_added_count_ = 0;
  int expected_tabs_to_add_ = 0;
  std::unique_ptr<base::RunLoop> wait_for_added_tabs_run_loop_;
  std::vector<GURL> navigated_urls_;
  // Holds the observers for each individual tab.
  std::vector<std::unique_ptr<SingleTabNavigationWatcher>>
      web_contents_observers_;
};

// Verifies that the tool works with multiple valid URLs, returns an `OK`
// result, and correctly manages (opens and closes) browser tabs.
IN_PROC_BROWSER_TEST_F(ActorLoadAndExtractContentToolBrowserTest, Success) {
  const GURL url1 = embedded_test_server()->GetURL("/actor/simple.html?1");
  const GURL url2 = embedded_test_server()->GetURL("/actor/simple.html?2");
  std::vector<GURL> expected_urls = {url1, url2};

  std::unique_ptr<ToolRequest> request =
      std::make_unique<LoadAndExtractContentToolRequest>(expected_urls);
  ASSERT_NE(request, nullptr);

  TabNavigationObserver observer(browser());

  ActResultFuture result;
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectOkResult(result);

  // TODO(b/478282022): Verify the tool returns the correct extracted content
  // (APCs) once implemented. The content should match the text in
  // `simple.html`.

  // The tool should have opened two tabs.
  EXPECT_EQ(observer.tabs_added_count(), expected_urls.size());

  // The tool should have successfully navigated in the opened tabs.
  std::vector<GURL> actual_urls = observer.navigated_urls();
  EXPECT_THAT(actual_urls, testing::UnorderedElementsAreArray(expected_urls));

  // The tool should have closed the tabs it opened.
  observer.VerifyTabCountRestored();
}

// Verifies that the tool works with a single valid URL.
IN_PROC_BROWSER_TEST_F(ActorLoadAndExtractContentToolBrowserTest, SingleURL) {
  const GURL url = embedded_test_server()->GetURL("/actor/simple.html");
  std::vector<GURL> expected_urls = {url};

  std::unique_ptr<ToolRequest> request =
      std::make_unique<LoadAndExtractContentToolRequest>(expected_urls);

  TabNavigationObserver observer(browser());

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(observer.tabs_added_count(), 1);
  EXPECT_THAT(observer.navigated_urls(), testing::ElementsAre(url));
  observer.VerifyTabCountRestored();
}

// Checks if the tool can handle HTTP redirects, following them to the final
// destination.
IN_PROC_BROWSER_TEST_F(ActorLoadAndExtractContentToolBrowserTest, Redirect) {
  const GURL destination_url =
      embedded_test_server()->GetURL("/actor/simple.html");
  const GURL redirect_url = embedded_test_server()->GetURL(
      "/server-redirect?" + destination_url.spec());
  std::vector<GURL> urls = {redirect_url};

  std::unique_ptr<ToolRequest> request =
      std::make_unique<LoadAndExtractContentToolRequest>(urls);

  TabNavigationObserver observer(browser());

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(observer.tabs_added_count(), 1);
  EXPECT_THAT(observer.navigated_urls(), testing::ElementsAre(destination_url));
  observer.VerifyTabCountRestored();
}

// Tests behavior with a URL that leads to a navigation error (e.g., 404 Not
// Found).
IN_PROC_BROWSER_TEST_F(ActorLoadAndExtractContentToolBrowserTest,
                       NavigationError) {
  const GURL url = embedded_test_server()->GetURL("/non-existent");
  std::vector<GURL> urls = {url};

  std::unique_ptr<ToolRequest> request =
      std::make_unique<LoadAndExtractContentToolRequest>(urls);

  TabNavigationObserver observer(browser());

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kNavigateCommittedErrorPage);

  EXPECT_EQ(observer.tabs_added_count(), 1);
  EXPECT_THAT(observer.navigated_urls(), testing::IsEmpty());
  observer.VerifyTabCountRestored();
}

// Confirms the tool returns `kArgumentsInvalid` when given an empty list of
// URLs.
IN_PROC_BROWSER_TEST_F(ActorLoadAndExtractContentToolBrowserTest,
                       EmptyURLList) {
  std::vector<GURL> urls = {};

  std::unique_ptr<ToolRequest> request =
      std::make_unique<LoadAndExtractContentToolRequest>(urls);

  TabNavigationObserver observer(browser());

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kArgumentsInvalid);

  EXPECT_EQ(observer.tabs_added_count(), 0);
  EXPECT_THAT(observer.navigated_urls(), testing::IsEmpty());
  observer.VerifyTabCountRestored();
}

// Provides a list with both a valid URL and one that causes a navigation error.
// It should return an error and properly close *all* opened tabs.
IN_PROC_BROWSER_TEST_F(ActorLoadAndExtractContentToolBrowserTest,
                       MixedSuccessAndNavigationError) {
  const GURL valid_url = embedded_test_server()->GetURL("/actor/simple.html");
  const GURL invalid_url = embedded_test_server()->GetURL("/non-existent");
  std::vector<GURL> urls = {valid_url, invalid_url};

  std::unique_ptr<ToolRequest> request =
      std::make_unique<LoadAndExtractContentToolRequest>(urls);

  TabNavigationObserver observer(browser());

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  // Returns the first error encountered.
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kNavigateCommittedErrorPage);

  EXPECT_EQ(observer.tabs_added_count(), 2);
  EXPECT_THAT(observer.navigated_urls(), testing::ElementsAre(valid_url));
  observer.VerifyTabCountRestored();
}

// Simulates a tab being closed by an external actor while the tool is running.
IN_PROC_BROWSER_TEST_F(ActorLoadAndExtractContentToolBrowserTest,
                       TabClosedPrematurely) {
  const GURL url = embedded_test_server()->GetURL("/actor/simple.html");
  std::unique_ptr<ToolRequest> request =
      std::make_unique<LoadAndExtractContentToolRequest>(
          std::vector<GURL>{url});

  TabNavigationObserver observer(browser());

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  observer.WaitForAddedTabs(1);

  // Close the newly added tab. The initial tab is at index 0.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);

  ExpectErrorResult(result, mojom::ActionResultCode::kTabWentAway);
}

IN_PROC_BROWSER_TEST_F(ActorLoadAndExtractContentToolBrowserTest,
                       ToolDeletedImmediatelyAfterInvocation) {
  const GURL url = embedded_test_server()->GetURL("/actor/simple.html");
  std::unique_ptr<ToolRequest> request =
      std::make_unique<LoadAndExtractContentToolRequest>(
          std::vector<GURL>{url});

  TabNavigationObserver observer(browser());

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());

  // Delete the task, which deletes the tool.
  actor_keyed_service().StopTask(task_id_,
                                 ActorTask::StoppedReason::kStoppedByUser);

  observer.VerifyTabCountRestored();

  // The callback should be called with kTaskWentAway when the task is stopped.
  ExpectErrorResult(result, mojom::ActionResultCode::kTaskWentAway);
}

IN_PROC_BROWSER_TEST_F(ActorLoadAndExtractContentToolBrowserTest,
                       ToolDeletedAfterTabAdded) {
  const GURL url = embedded_test_server()->GetURL("/actor/simple.html");
  std::unique_ptr<ToolRequest> request =
      std::make_unique<LoadAndExtractContentToolRequest>(
          std::vector<GURL>{url});

  TabNavigationObserver observer(browser());

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());

  observer.WaitForAddedTabs(1);

  // Delete the task, which deletes the tool.
  actor_keyed_service().StopTask(task_id_,
                                 ActorTask::StoppedReason::kStoppedByUser);

  // The tool's destructor should close the tab that was opened.
  EXPECT_EQ(observer.tabs_added_count(), 1);
  observer.VerifyTabCountRestored();

  // The callback should be called with kTaskWentAway when the task is stopped.
  ExpectErrorResult(result, mojom::ActionResultCode::kTaskWentAway);
}

// Simulates the entire browser window being closed while the tool is running.
IN_PROC_BROWSER_TEST_F(ActorLoadAndExtractContentToolBrowserTest,
                       WindowClosedPrematurely) {
  const GURL url = embedded_test_server()->GetURL("/actor/simple.html");
  std::vector<GURL> urls = {url};

  // Create a second browser window for the tool to operate in.
  Browser* second_browser = CreateBrowser(browser()->profile());
  ASSERT_TRUE(second_browser);

  // We need a way to ensure the tool uses the second browser, the tool
  // currently uses the active window.
  // TODO(b/478282022): Update this test to use a specific window ID once the
  // tool supports that.
  second_browser->window()->Show();

  std::unique_ptr<ToolRequest> request =
      std::make_unique<LoadAndExtractContentToolRequest>(urls);

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());

  // Close the active window.
  CloseBrowserSynchronously(second_browser);

  ExpectErrorResult(result, mojom::ActionResultCode::kWindowWentAway);
}

class ActorLoadAndExtractContentToolTimeoutBrowserTest
    : public ActorLoadAndExtractContentToolBrowserTest {
 public:
  ActorLoadAndExtractContentToolTimeoutBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kGlicActorLoadAndExtractContentTool, {{"timeout", "1ms"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Simulates a page that takes too long to load to verify that the tool times
// out gracefully.
IN_PROC_BROWSER_TEST_F(ActorLoadAndExtractContentToolTimeoutBrowserTest,
                       Timeout) {
  const GURL url = embedded_test_server()->GetURL("/stall");
  std::vector<GURL> urls = {url};

  std::unique_ptr<ToolRequest> request =
      std::make_unique<LoadAndExtractContentToolRequest>(urls);

  TabNavigationObserver observer(browser());

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());

  ExpectErrorResult(result, mojom::ActionResultCode::kToolTimeout);
  observer.VerifyTabCountRestored();
  EXPECT_EQ(observer.tabs_added_count(), 1);
  EXPECT_THAT(observer.navigated_urls(), testing::IsEmpty());
}

// Simulates a user cancelling a navigation before it completes.
IN_PROC_BROWSER_TEST_F(ActorLoadAndExtractContentToolBrowserTest,
                       NavigationCancelled) {
  const GURL url = embedded_test_server()->GetURL("/stall");
  std::vector<GURL> urls = {url};

  std::unique_ptr<ToolRequest> request =
      std::make_unique<LoadAndExtractContentToolRequest>(urls);

  TabNavigationObserver observer(browser());

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());

  observer.WaitForAddedTabs(1);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);

  // Use a NavigationManager to ensure we catch it before it commits.
  content::TestNavigationManager navigation_manager(web_contents, url);

  // Stop the navigation while it's stalled.
  web_contents->Stop();

  ExpectErrorResult(result,
                    mojom::ActionResultCode::kNavigateCommittedErrorPage);
  observer.VerifyTabCountRestored();
  EXPECT_EQ(observer.tabs_added_count(), 1);
  EXPECT_THAT(observer.navigated_urls(), testing::IsEmpty());
}

// Tests the tool object being deleted while a page is in the middle of
// navigating.
IN_PROC_BROWSER_TEST_F(ActorLoadAndExtractContentToolBrowserTest,
                       ToolDeletedDuringNavigation) {
  const GURL url = embedded_test_server()->GetURL("/actor/simple.html");
  std::vector<GURL> urls = {url};

  std::unique_ptr<ToolRequest> request =
      std::make_unique<LoadAndExtractContentToolRequest>(urls);

  TabNavigationObserver observer(browser());

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());

  // Wait for the tab to be created but potentially before navigation completes.
  observer.WaitForAddedTabs(1);

  // Delete the task immediately.
  actor_keyed_service().StopTask(task_id_,
                                 ActorTask::StoppedReason::kStoppedByUser);

  observer.VerifyTabCountRestored();
  ExpectErrorResult(result, mojom::ActionResultCode::kTaskWentAway);
}

// TODO(b/478282478): Add a test to simulate a failure during content
// extraction and verify the resulting error.

}  // namespace

}  // namespace actor
