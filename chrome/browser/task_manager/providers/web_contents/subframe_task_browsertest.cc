// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/task_manager/mock_web_contents_task_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_manager {

namespace {

// URL of a test page on a.com that has two cross-site iframes to b.com and
// c.com.
const char kCrossSitePageUrl[] = "/cross-site/a.com/iframe_cross_site.html";

// URL of a test page on a.com that has no cross-site iframes.
const char kSimplePageUrl[] = "/cross-site/a.com/title2.html";

std::u16string GetExpectedSubframeTitlePrefix() {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_SUBFRAME_PREFIX,
                                    std::u16string());
}

std::u16string PrefixExpectedBFCacheTitle(const std::string& title,
                                          bool is_subframe) {
  const auto msg_id = is_subframe
                          ? IDS_TASK_MANAGER_BACK_FORWARD_CACHE_SUBFRAME_PREFIX
                          : IDS_TASK_MANAGER_BACK_FORWARD_CACHE_PREFIX;
  return l10n_util::GetStringFUTF16(msg_id, base::UTF8ToUTF16(title));
}

std::u16string PrefixExpectedTabTitle(const std::string& title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_TAB_PREFIX,
                                    base::UTF8ToUTF16(title));
}

}  // namespace

// A test for OOPIFs and how they show up in the task manager as
// SubframeTasks.
class SubframeTaskBrowserTest : public InProcessBrowserTest {
 public:
  SubframeTaskBrowserTest() = default;
  SubframeTaskBrowserTest(const SubframeTaskBrowserTest&) = delete;
  SubframeTaskBrowserTest& operator=(const SubframeTaskBrowserTest&) = delete;
  ~SubframeTaskBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    content::SetupCrossSiteRedirector(embedded_test_server());
    embedded_test_server()->StartAcceptingConnections();
  }

  void NavigateTo(const char* page_url) const {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(page_url)));
  }
};

// Makes sure that, if sites are isolated, the task manager will show the
// expected SubframeTasks, and they will be shown as running on different
// processes as expected.
IN_PROC_BROWSER_TEST_F(SubframeTaskBrowserTest, TaskManagerShowsSubframeTasks) {
  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());
  task_manager.StartObserving();

  // Currently only the about:blank page.
  ASSERT_EQ(1U, task_manager.tasks().size());
  const Task* about_blank_task = task_manager.tasks().front();
  EXPECT_EQ(Task::RENDERER, about_blank_task->GetType());
  EXPECT_EQ(PrefixExpectedTabTitle("about:blank"), about_blank_task->title());

  NavigateTo(kCrossSitePageUrl);

  // Whether sites are isolated or not, we expect to have at least one tab
  // contents task.
  ASSERT_GE(task_manager.tasks().size(), 1U);
  const Task* cross_site_task = task_manager.tasks().front();
  EXPECT_EQ(Task::RENDERER, cross_site_task->GetType());
  EXPECT_EQ(PrefixExpectedTabTitle("cross-site iframe test"),
            cross_site_task->title());

  if (!content::AreAllSitesIsolatedForTesting()) {
    // Sites are not isolated. No SubframeTasks are expected, just the above
    // task.
    ASSERT_EQ(1U, task_manager.tasks().size());
  } else {
    // Sites are isolated. We expect, in addition to the above task, two more
    // SubframeTasks, one for b.com and another for c.com.
    ASSERT_EQ(3U, task_manager.tasks().size());
    const Task* subframe_task_1 = task_manager.tasks()[1];
    const Task* subframe_task_2 = task_manager.tasks()[2];

    EXPECT_EQ(Task::RENDERER, subframe_task_1->GetType());
    EXPECT_EQ(Task::RENDERER, subframe_task_2->GetType());

    EXPECT_TRUE(base::StartsWith(subframe_task_1->title(),
                                 GetExpectedSubframeTitlePrefix(),
                                 base::CompareCase::INSENSITIVE_ASCII));
    EXPECT_TRUE(base::StartsWith(subframe_task_2->title(),
                                 GetExpectedSubframeTitlePrefix(),
                                 base::CompareCase::INSENSITIVE_ASCII));

    // All tasks must be running on different processes.
    EXPECT_NE(subframe_task_1->process_id(), subframe_task_2->process_id());
    EXPECT_NE(subframe_task_1->process_id(), cross_site_task->process_id());
    EXPECT_NE(subframe_task_2->process_id(), cross_site_task->process_id());
  }

  // If we navigate to the simple page on a.com which doesn't have cross-site
  // iframes, we expect not to have any SubframeTasks, except if the previous
  // page is saved in the back-forward cache.
  NavigateTo(kSimplePageUrl);

  ASSERT_EQ(
      content::BackForwardCache::IsBackForwardCacheFeatureEnabled() ? 4U : 1U,
      task_manager.tasks().size());

  const auto& tasks = task_manager.tasks();
  // Main page and two cross-origin iframes.
  if (content::BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    EXPECT_EQ(
        PrefixExpectedBFCacheTitle("http://a.com/", /*is_subframe=*/false),
        tasks[0]->title());
    EXPECT_EQ(PrefixExpectedBFCacheTitle("http://b.com/",
                                         /*is_subframe=*/true),
              tasks[1]->title());
    EXPECT_EQ(PrefixExpectedBFCacheTitle("http://c.com/",
                                         /*is_subframe=*/true),
              tasks[2]->title());
  }
  // When navigation to |kSimplePageUrl| happens, tasks are first created for
  // page a.com and two cross-origin iframes b.com and c.com from
  // |RenderFrameHostStateChange|, then the task for |kSimplePageUrl| is created
  // from |DidFinishNavigation| when the navigation completes. Thus |.back()|.
  const Task* simple_page_task = tasks.back();
  EXPECT_EQ(Task::RENDERER, simple_page_task->GetType());
  EXPECT_EQ(PrefixExpectedTabTitle("Title Of Awesomeness"),
            simple_page_task->title());
}

// Allows listening to unresponsive task events.
class HungWebContentsTaskManager : public MockWebContentsTaskManager {
 public:
  HungWebContentsTaskManager() : unresponsive_task_(nullptr) {}
  void TaskUnresponsive(Task* task) override { unresponsive_task_ = task; }

  Task* unresponsive_task() { return unresponsive_task_; }

 private:
  raw_ptr<Task, DanglingUntriaged> unresponsive_task_;
};

// If sites are isolated, makes sure that subframe tasks can react to
// unresponsive renderers.
IN_PROC_BROWSER_TEST_F(SubframeTaskBrowserTest, TaskManagerHungSubframe) {
  // This test only makes sense if we have subframe processes.
  if (!content::AreAllSitesIsolatedForTesting())
    return;

  HungWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());
  task_manager.StartObserving();

  NavigateTo(kCrossSitePageUrl);

  // We expect SubframeTasks for b.com and c.com, in either order.
  ASSERT_EQ(3U, task_manager.tasks().size());
  const Task* subframe_task_1 = task_manager.tasks()[1];
  const Task* subframe_task_2 = task_manager.tasks()[2];

  EXPECT_EQ(Task::RENDERER, subframe_task_1->GetType());
  EXPECT_EQ(Task::RENDERER, subframe_task_2->GetType());

  EXPECT_TRUE(base::StartsWith(subframe_task_1->title(),
                               GetExpectedSubframeTitlePrefix(),
                               base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_TRUE(base::StartsWith(subframe_task_2->title(),
                               GetExpectedSubframeTitlePrefix(),
                               base::CompareCase::INSENSITIVE_ASCII));

  // Nothing should have hung yet.
  EXPECT_EQ(nullptr, task_manager.unresponsive_task());

  // Simulate a hang in one of the subframe processes.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* subframe1 = ChildFrameAt(web_contents, 0);
  ASSERT_TRUE(subframe1);
  SimulateUnresponsiveRenderer(web_contents,
                               subframe1->GetView()->GetRenderWidgetHost());

  // Verify task_observer saw one of the two subframe tasks.  (There's a race,
  // so it could be either one.)
  Task* unresponsive_task = task_manager.unresponsive_task();
  EXPECT_NE(nullptr, unresponsive_task);
  EXPECT_TRUE(unresponsive_task == subframe_task_1 ||
              unresponsive_task == subframe_task_2);
}

}  // namespace task_manager
