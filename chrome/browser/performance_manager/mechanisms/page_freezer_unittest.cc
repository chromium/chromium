// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/page_freezer.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_permission_controller.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager {
namespace mechanism {

namespace {

static constexpr char kUrl[] = "https://www.foo.com/";

void FlushUIThreadTasks() {
  // Post a single task and wait for it to finish. This will ensure that any
  // tasks not yet run but posted prior to this task have been dispatched.
  base::RunLoop run_loop;
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                               run_loop.QuitClosure());
  run_loop.Run();
}

void MaybeFreezePageNode(content::WebContents* content) {
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PageNode> page_node,
             base::OnceClosure quit_closure) {
            EXPECT_TRUE(page_node);
            mechanism::PageFreezer freezer;
            freezer.MaybeFreezePageNode(page_node.get());
            std::move(quit_closure).Run();
          },
          PerformanceManager::GetPrimaryPageNodeForWebContents(content),
          std::move(quit_closure)));
  run_loop.Run();

  // Allow the bounce back to the UI thread to run; it will have been scheduled
  // but not yet necessarily processed if the PM is also running on the UI
  // thread.
  FlushUIThreadTasks();
}

void UnfreezePageNode(content::WebContents* content) {
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PageNode> page_node,
             base::OnceClosure quit_closure) {
            EXPECT_TRUE(page_node);
            mechanism::PageFreezer freezer;
            freezer.UnfreezePageNode(page_node.get());
            std::move(quit_closure).Run();
          },
          PerformanceManager::GetPrimaryPageNodeForWebContents(content),
          std::move(quit_closure)));
  run_loop.Run();

  // Allow the bounce back to the UI thread to run; it will have been scheduled
  // but not yet necessarily processed if the PM is also running on the UI
  // thread.
  FlushUIThreadTasks();
}

}  // namespace

class PageFreezerTest : public ChromeRenderViewHostTestHarness {
 public:
  PageFreezerTest() = default;
  ~PageFreezerTest() override = default;
  PageFreezerTest(const PageFreezerTest& other) = delete;
  PageFreezerTest& operator=(const PageFreezerTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    pm_harness_.SetUp();
  }

  void TearDown() override {
    pm_harness_.TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 private:
  performance_manager::PerformanceManagerTestHarnessHelper pm_harness_;
};

TEST_F(PageFreezerTest, FreezeAndUnfreezePage) {
  SetContents(CreateTestWebContents());

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  EXPECT_TRUE(web_contents_tester);
  web_contents_tester->NavigateAndCommit(GURL(kUrl));

  web_contents()->WasHidden();

  MaybeFreezePageNode(web_contents());
  EXPECT_TRUE(web_contents_tester->IsPageFrozen());

  UnfreezePageNode(web_contents());
  EXPECT_FALSE(web_contents_tester->IsPageFrozen());
}

TEST_F(PageFreezerTest, CantFreezePageWithNotificationPermission) {
  SetContents(CreateTestWebContents());

  // Allow permissions.
  GetBrowserContext()->SetPermissionControllerForTesting(
      std::make_unique<content::MockPermissionController>());

  NavigateAndCommit(GURL(kUrl));

  // Try to freeze the page node, this should fail.
  MaybeFreezePageNode(web_contents());
  EXPECT_FALSE(content::WebContentsTester::For(web_contents())->IsPageFrozen());
}

}  // namespace mechanism
}  // namespace performance_manager
