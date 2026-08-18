// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "chrome/browser/task_manager/mock_web_contents_task_manager.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tags_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace task_manager {

namespace {

const char kTestPage1[] = "/devtools/debugger_test_page.html";
const char kTestPage2[] = "/devtools/navigate_back.html";

}  // namespace

// Defines a browser test for testing that DevTools WebContents are being tagged
// properly by a DevToolsTag and that the TagsManager records these tags. It
// will also test that the WebContentsTaskProvider will be able to provide the
// appropriate DevToolsTask.
class DevToolsTagTest : public InProcessBrowserTest {
 public:
  DevToolsTagTest()
      : devtools_window_(nullptr) {
    CHECK(embedded_test_server()->Start());
  }

  DevToolsTagTest(const DevToolsTagTest&) = delete;
  DevToolsTagTest& operator=(const DevToolsTagTest&) = delete;
  ~DevToolsTagTest() override = default;

  void LoadTestPage(const std::string& test_page) {
    GURL url = embedded_test_server()->GetURL(test_page);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  }

  void OpenDevToolsWindow(bool is_docked) {
    devtools_window_ = DevToolsWindowTesting::OpenDevToolsWindowSync(
        browser()->GetTabStripModel()->GetWebContentsAt(0), is_docked);
  }

  void CloseDevToolsWindow() {
    DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window_);
  }

  size_t tracked_tags_count() const {
    size_t count = 0;
    for (const auto& tag :
         WebContentsTagsManager::GetInstance()->tracked_tags()) {
      if (tag->web_contents()->GetVisibleURL().host() !=
          chrome::kChromeUIOmniboxPopupHost) {
        count++;
      }
    }
    return count;
  }

 private:
  // TODO(https://crbug.com/423465927): Explore a better approach to make the
  // existing tests run with the prewarm feature enabled.
  test::ScopedPrewarmFeatureList scoped_prewarm_feature_list_{
      test::ScopedPrewarmFeatureList::PrewarmState::kDisabled};
  raw_ptr<DevToolsWindow, AcrossTasksDanglingUntriaged> devtools_window_;
};

// Tests that opening a DevToolsWindow will result in tagging its main
// WebContents and that tag will be recorded by the TagsManager.
IN_PROC_BROWSER_TEST_F(DevToolsTagTest, DISABLED_TagsManagerRecordsATag) {
  // Browser tests start with a single tab.
  EXPECT_EQ(1U, tracked_tags_count());

  // Navigating the same tab to the test page won't change the number of tracked
  // tags. No devtools yet.
  LoadTestPage(kTestPage1);
  EXPECT_EQ(1U, tracked_tags_count());

  // Test both docked and undocked devtools.
  OpenDevToolsWindow(true);
  EXPECT_EQ(2U, tracked_tags_count());
  CloseDevToolsWindow();
  EXPECT_EQ(1U, tracked_tags_count());

  // For the undocked devtools there will be two tags one for the main contents
  // and one for the toolbox contents
  OpenDevToolsWindow(false);
  EXPECT_EQ(3U, tracked_tags_count());
  CloseDevToolsWindow();
  EXPECT_EQ(1U, tracked_tags_count());
}

IN_PROC_BROWSER_TEST_F(DevToolsTagTest, DevToolsTaskIsProvided) {
  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.NonToolTasks().empty());
  // Browser tests start with a single tab.
  EXPECT_EQ(1U, tracked_tags_count());

  task_manager.StartObserving();

  // The pre-existing tab is provided.
  EXPECT_EQ(1U, task_manager.NonToolTasks().size());

  LoadTestPage(kTestPage1);
  EXPECT_EQ(1U, tracked_tags_count());
  EXPECT_EQ(1U, task_manager.NonToolTasks().size());

  OpenDevToolsWindow(true);
  EXPECT_EQ(2U, tracked_tags_count());
  auto tasks = task_manager.NonToolTasks();
  ASSERT_EQ(2U, tasks.size());

  const Task* task = tasks.back();
  EXPECT_EQ(Task::RENDERER, task->GetType());

  // Navigating to a new page will not change the id of the devtools main
  // WebContents (its js may update its title).
  const int64_t task_id = task->task_id();
  LoadTestPage(kTestPage2);
  EXPECT_TRUE(base::test::RunUntil([task] {
    return task->title().find(u"navigate_back.html") != std::u16string::npos;
  }));
  EXPECT_EQ(2U, tracked_tags_count());
  tasks = task_manager.NonToolTasks();
  if (content::CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    // When ProactivelySwapBrowsingInstance or RenderDocument is enabled on
    // same-site main frame navigations, the navigation above will result in a
    // new RenderFrameHost, so the DevTools task will move (but still exist
    // in the tasks list).
    EXPECT_NE(task_id, tasks.back()->task_id());
    EXPECT_NE(task, tasks.back());
    EXPECT_EQ(task_id, tasks[0]->task_id());
    EXPECT_EQ(task, tasks[0]);
  } else {
    EXPECT_EQ(task_id, tasks.back()->task_id());
    EXPECT_EQ(task, tasks.back());
  }
  EXPECT_NE(tasks[0]->title(), tasks[1]->title());
  // If back/forward cache is enabled, the task for the previous page
  // will still be around.
  EXPECT_EQ(
      content::BackForwardCache::IsBackForwardCacheFeatureEnabled() ? 3U : 2U,
      tasks.size());

  // Close the DevTools window.
  CloseDevToolsWindow();
  EXPECT_EQ(1U, tracked_tags_count());

  EXPECT_EQ(
      content::BackForwardCache::IsBackForwardCacheFeatureEnabled() ? 2U : 1U,
      task_manager.NonToolTasks().size());
}

}  // namespace task_manager
