// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/task_manager/mock_web_contents_task_manager.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tags_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
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

  ~DevToolsTagTest() override {}

  void LoadTestPage(const std::string& test_page) {
    GURL url = embedded_test_server()->GetURL(test_page);
    ui_test_utils::NavigateToURL(browser(), url);
  }

  void OpenDevToolsWindow(bool is_docked) {
    devtools_window_ = DevToolsWindowTesting::OpenDevToolsWindowSync(
        browser()->tab_strip_model()->GetWebContentsAt(0), is_docked);
  }

  void CloseDevToolsWindow() {
    DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window_);
  }

  WebContentsTagsManager* tags_manager() const {
    return WebContentsTagsManager::GetInstance();
  }

 private:
  DevToolsWindow* devtools_window_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsTagTest);
};

// Tests that opening a DevToolsWindow will result in tagging its main
// WebContents and that tag will be recorded by the TagsManager.
IN_PROC_BROWSER_TEST_F(DevToolsTagTest, TagsManagerRecordsATag) {
  // Browser tests start with a single tab.
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());

  // Navigating the same tab to the test page won't change the number of tracked
  // tags. No devtools yet.
  LoadTestPage(kTestPage1);
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());

  // Test both docked and undocked devtools.
  OpenDevToolsWindow(true);
  EXPECT_EQ(2U, tags_manager()->tracked_tags().size());
  CloseDevToolsWindow();
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());

  // For the undocked devtools there will be two tags one for the main contents
  // and one for the toolbox contents
  OpenDevToolsWindow(false);
  EXPECT_EQ(3U, tags_manager()->tracked_tags().size());
  CloseDevToolsWindow();
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());
}

IN_PROC_BROWSER_TEST_F(DevToolsTagTest, DevToolsTaskIsProvided) {
  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());
  // Browser tests start with a single tab.
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());

  task_manager.StartObserving();

  // The pre-existing tab is provided.
  EXPECT_EQ(1U, task_manager.tasks().size());

  LoadTestPage(kTestPage1);
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());
  EXPECT_EQ(1U, task_manager.tasks().size());

  OpenDevToolsWindow(true);
  EXPECT_EQ(2U, tags_manager()->tracked_tags().size());
  ASSERT_EQ(2U, task_manager.tasks().size());

  const Task* task = task_manager.tasks().back();
  EXPECT_EQ(Task::RENDERER, task->GetType());

  // Navigating to a new page will not change the id of the devtools main
  // WebContents (its js may update its title).
  const int64_t task_id = task->task_id();
  LoadTestPage(kTestPage2);
  EXPECT_EQ(2U, tags_manager()->tracked_tags().size());
  if (content::CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    // When ProactivelySwapBrowsingInstance or RenderDocument is enabled on
    // same-site main frame navigations, the navigation above will result in a
    // new RenderFrameHost, so the DevTools task will move (but still exist
    // in the tasks list).
    EXPECT_NE(task_id, task_manager.tasks().back()->task_id());
    EXPECT_NE(task, task_manager.tasks().back());
    EXPECT_EQ(task_id, task_manager.tasks()[0]->task_id());
    EXPECT_EQ(task, task_manager.tasks()[0]);
  } else {
    EXPECT_EQ(task_id, task_manager.tasks().back()->task_id());
    EXPECT_EQ(task, task_manager.tasks().back());
  }
  EXPECT_NE(task_manager.tasks()[0]->title(),
            task_manager.tasks()[1]->title());

  CloseDevToolsWindow();
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());
  EXPECT_EQ(1U, task_manager.tasks().size());
}

}  // namespace task_manager
