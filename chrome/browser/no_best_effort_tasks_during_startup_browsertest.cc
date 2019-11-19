// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_closure.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_task_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class NoBestEffortTasksDuringStartupTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest:
  void PreRunTestOnMainThread() override {
    // This test must run before PreRunTestOnMainThread() sets startup as
    // complete.
    TestNoBestEffortTasksDuringStartup();

    InProcessBrowserTest::PreRunTestOnMainThread();
  }

  void TestNoBestEffortTasksDuringStartup() {
    EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());

    base::RunLoop run_loop;
    auto barrier = base::BarrierClosure(2, run_loop.QuitClosure());

    // Thread pool task.
    base::PostTask(
        FROM_HERE, {base::ThreadPool(), base::TaskPriority::BEST_EFFORT},
        base::BindLambdaForTesting([&]() {
          EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
          barrier.Run();
        }));

    // UI thread task.
    base::PostTask(
        FROM_HERE,
        {content::BrowserThread::UI, base::TaskPriority::BEST_EFFORT},
        base::BindLambdaForTesting([&]() {
          EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
          barrier.Run();
        }));

    run_loop.Run();
  }
};

}  // namespace

// Verify that BEST_EFFORT tasks don't run until startup is complete.
IN_PROC_BROWSER_TEST_F(NoBestEffortTasksDuringStartupTest,
                       NoBestEffortTasksDuringStartup) {
  // The body of the test is in the TestNoBestEffortTasksDuringStartup() method
  // called from PreRunTestOnMainThread().
}
