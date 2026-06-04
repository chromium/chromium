// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_browsertest.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest, StopPausedTask) {
  TaskId task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());

  // Open a second tab to prevent the browser from closing when the active tab
  // is closed.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Navigate the active tab to a new page.
  ASSERT_TRUE(chrome_test_utils::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("/actor/blank.html")));

  {
    actor::ActorTask* task = actor_keyed_service()->GetTask(task_id);
    actor::AddTabToTask(*active_tab(), *task);

    task->Pause(/*from_actor=*/false);
    CHECK(!task->IsCompleted());
  }
  base::RunLoop run_loop;
  auto discard = active_tab()->RegisterWillDetach(base::BindRepeating(
      [](base::RepeatingClosure run_loop_closure, tabs::TabInterface* tab,
         tabs::TabInterface::DetachReason reason) { run_loop_closure.Run(); },
      run_loop.QuitClosure()));
  active_tab()->Close();
  run_loop.Run();

  {
    base::RunLoop run_loop2;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop2.QuitClosure());
    run_loop2.Run();
  }

  // The task should be destroyed.
  EXPECT_FALSE(actor_keyed_service()->GetTask(task_id));
}

IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       ObserveTabOnceCleanupOnClose) {
  TaskId task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());

  actor::ActorTask* task = actor_keyed_service()->GetTask(task_id);
  ASSERT_TRUE(task);

  // Open a second tab to observe.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  tabs::TabInterface* second_tab =
      browser()->tab_strip_model()->GetTabAtIndex(1);
  ASSERT_TRUE(second_tab);
  tabs::TabHandle second_tab_handle = second_tab->GetHandle();

  // Observe the second tab.
  task->ObserveTabOnce(second_tab_handle);

  // Verify it is in LastActedTabs.
  EXPECT_TRUE(task->GetLastActedTabs().contains(second_tab_handle));

  // Close the second tab.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);

  // Flush message loop.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // Verify it is NO LONGER in LastActedTabs.
  EXPECT_FALSE(task->GetLastActedTabs().contains(second_tab_handle));
}

}  // namespace
}  // namespace actor
