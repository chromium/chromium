// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<net::test_server::HttpResponse> HandleSlowNoCache(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url.find("/slow-no-cache") == std::string::npos) {
    return nullptr;
  }
  double delay = 1.0f;
  GURL request_url = request.GetURL();
  if (request_url.has_query()) {
    delay = std::atof(request_url.GetQuery().c_str());
  }

  auto http_response = std::make_unique<net::test_server::DelayedHttpResponse>(
      base::Seconds(delay));
  http_response->set_content_type("text/plain");
  http_response->set_content(base::StringPrintf("waited %.1f seconds", delay));
  http_response->AddCustomHeader("Cache-Control",
                                 "no-store, no-cache, must-revalidate");
  http_response->AddCustomHeader("Pragma", "no-cache");
  return http_response;
}

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
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindLambdaForTesting([&]() {
          EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
          barrier.Run();
        }));

    // UI thread task.
    content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostTask(
            FROM_HERE, base::BindLambdaForTesting([&]() {
              EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
              barrier.Run();
            }));

    run_loop.Run();
  }
};

class NoBestEffortTasksDuringSlowStartupTest : public InProcessBrowserTest {
 public:
  NoBestEffortTasksDuringSlowStartupTest() {
    feature_list_.InitAndEnableFeature(
        features::kImprovedStartupBestEffortDelay);
    // Prevents opening about:blank on launch. We want the only loading page to
    // be the slow URL we append to the command line, so that we can reliably
    // control the startup duration. Otherwise, about:blank would load instantly
    // and trigger the first paint/idle, unblocking BEST_EFFORT tasks early.
    set_open_about_blank_on_browser_launch(false);
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleSlowNoCache));
    GURL slow_url = embedded_test_server()->GetURL("/slow-no-cache?7");
    base::CommandLine::ForCurrentProcess()->AppendArg(slow_url.spec());
    embedded_test_server()->StartAcceptingConnections();
    InProcessBrowserTest::SetUp();
  }

  void PreRunTestOnMainThread() override {
    TestNoBestEffortTasksDuringSlowStartup();
    InProcessBrowserTest::PreRunTestOnMainThread();
  }

  void TestNoBestEffortTasksDuringSlowStartup() {
    EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());

    base::RunLoop run_loop;
    auto barrier = base::BarrierClosure(2, run_loop.QuitClosure());

    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindLambdaForTesting([&]() {
          EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
          barrier.Run();
        }));

    content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostTask(
            FROM_HERE, base::BindLambdaForTesting([&]() {
              EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
              barrier.Run();
            }));

    base::TimeTicks start_time = base::TimeTicks::Now();
    run_loop.Run();
    base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
    EXPECT_GE(elapsed, base::Seconds(6));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class NoBestEffortTasksDuringSessionRestoreStartupTest
    : public InProcessBrowserTest,
      public SessionRestoreObserver {
 public:
  NoBestEffortTasksDuringSessionRestoreStartupTest() {
    feature_list_.InitAndEnableFeature(
        features::kImprovedStartupBestEffortDelay);
    // Prevents opening about:blank on launch. We want the only loading page to
    // be the slow URL, so that we can reliably control the startup duration.
    // Otherwise, about:blank would load instantly and trigger the first
    // paint/idle, unblocking BEST_EFFORT tasks early.
    set_open_about_blank_on_browser_launch(false);
  }

  ~NoBestEffortTasksDuringSessionRestoreStartupTest() override {
    SessionRestore::RemoveObserver(this);
  }

  // SessionRestoreObserver:
  void OnSessionRestoreFinishedLoadingTabs() override {
    restore_finished_ = true;
    SessionRestore::RemoveObserver(this);
  }

  void SetUp() override {
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleSlowNoCache));

    base::FilePath user_data_dir =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            "user-data-dir");
    ASSERT_FALSE(user_data_dir.empty());
    base::FilePath port_file =
        user_data_dir.AppendASCII("test_server_port.txt");

    int port = 0;
    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    if (std::string(test_info->name()) == "Restore") {
      std::string port_str;
      if (base::ReadFileToString(port_file, &port_str)) {
        base::StringToInt(port_str, &port);
      }
    }

    ASSERT_TRUE(embedded_test_server()->InitializeAndListen(port));

    if (std::string(test_info->name()) == "PRE_Restore") {
      std::string port_str =
          base::NumberToString(embedded_test_server()->port());
      ASSERT_TRUE(base::WriteFile(port_file, port_str));
    }

    embedded_test_server()->StartAcceptingConnections();
    InProcessBrowserTest::SetUp();
  }

  GURL GetTestURL() const {
    return embedded_test_server()->GetURL("/title1.html");
  }

  GURL GetSlowURL() const {
    return embedded_test_server()->GetURL("/slow-no-cache?7");
  }

  void PreRunTestOnMainThread() override {
    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    if (std::string(test_info->name()) == "Restore") {
      TestNoBestEffortTasksDuringSessionRestore();
    }
    InProcessBrowserTest::PreRunTestOnMainThread();
  }

  void TestNoBestEffortTasksDuringSessionRestore() {
    EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());

    SessionRestore::AddObserver(this);

    base::RunLoop run_loop;
    auto barrier = base::BarrierClosure(2, run_loop.QuitClosure());

    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindLambdaForTesting([this, barrier]() {
          EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
          EXPECT_TRUE(restore_finished_);
          barrier.Run();
        }));

    content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostTask(
            FROM_HERE, base::BindLambdaForTesting([this, barrier]() {
              EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
              EXPECT_TRUE(restore_finished_);
              barrier.Run();
            }));

    run_loop.Run();

    BrowserWindowInterface* restored_browser =
        GetLastActiveBrowserWindowInterfaceWithAnyProfile();
    ASSERT_TRUE(restored_browser);
    EXPECT_EQ(2, restored_browser->GetTabStripModel()->count());
  }

 protected:
  std::atomic<bool> restore_finished_{false};

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

// Verify that BEST_EFFORT tasks don't run until startup is complete.
IN_PROC_BROWSER_TEST_F(NoBestEffortTasksDuringStartupTest,
                       NoBestEffortTasksDuringStartup) {
  // The body of the test is in the TestNoBestEffortTasksDuringStartup() method
  // called from PreRunTestOnMainThread().
}

// Verify that BEST_EFFORT tasks are delayed until paint even during a slow
// startup.
IN_PROC_BROWSER_TEST_F(NoBestEffortTasksDuringSlowStartupTest,
                       NoBestEffortTasksDuringSlowStartup) {}

// Session restore tests.
IN_PROC_BROWSER_TEST_F(NoBestEffortTasksDuringSessionRestoreStartupTest,
                       PRE_Restore) {
  SessionStartupPref pref(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(browser()->profile(), pref);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSlowURL()));
  chrome::AddTabAt(browser(), GetSlowURL(), -1, true);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetSlowURL(), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
}

IN_PROC_BROWSER_TEST_F(NoBestEffortTasksDuringSessionRestoreStartupTest,
                       Restore) {}
