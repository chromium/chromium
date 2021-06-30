// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/util/memory_pressure/fake_memory_pressure_monitor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace performance_manager {
namespace policies {

namespace {

// WebContentsObserver used in the BFCachePolicy browser tests.
class LenientBFCacheTestObserver : public content::WebContentsObserver {
 public:
  explicit LenientBFCacheTestObserver(content::WebContents* contents)
      : content::WebContentsObserver(contents) {}
  LenientBFCacheTestObserver(const LenientBFCacheTestObserver&) = delete;
  LenientBFCacheTestObserver(LenientBFCacheTestObserver&&) = delete;
  LenientBFCacheTestObserver& operator=(const LenientBFCacheTestObserver&) =
      delete;
  LenientBFCacheTestObserver& operator=(LenientBFCacheTestObserver&&) = delete;
  ~LenientBFCacheTestObserver() override = default;

  MOCK_METHOD1(RenderFrameDeleted, void(content::RenderFrameHost*));
};
using MockObserver = ::testing::StrictMock<LenientBFCacheTestObserver>;

class BFCachePolicyBrowserTest : public InProcessBrowserTest {
 public:
  ~BFCachePolicyBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kBackForwardCache,
          {{"TimeToLiveInBackForwardCacheInSeconds", "3600"}}}},
        {features::kBackForwardCacheMemoryControls});

    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* top_frame_host() {
    return web_contents()->GetMainFrame();
  }

  // Init |rfh_a_| and |rfh_b_|, after calling this function |rfh_a_| will be in
  // the BFCache and rfh_b_ will be active.
  void InitWithOnePageInBFCache() {
    GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
    GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

    EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);

    // Navigate to A.
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
    rfh_a_ = top_frame_host();

    // Navigate to B.
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
    EXPECT_EQ(rfh_a_->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);
    rfh_b_ = top_frame_host();
  }

  base::test::ScopedFeatureList feature_list_;

  content::RenderFrameHost* rfh_a_ = nullptr;
  content::RenderFrameHost* rfh_b_ = nullptr;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(BFCachePolicyBrowserTest,
                       CacheFlushedWhenTabBackgrounded) {
  InitWithOnePageInBFCache();

  MockObserver obs(web_contents());
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(obs, RenderFrameDeleted(rfh_a_))
      .WillOnce(::testing::Invoke([&]() { std::move(quit_closure).Run(); }));

  // Backgrounding the page should flush the BFCache.
  web_contents()->WasHidden();
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(BFCachePolicyBrowserTest,
                       CacheFlushedOnModerateMemoryPressure) {
  util::test::FakeMemoryPressureMonitor fake_memory_pressure_monitor;

  InitWithOnePageInBFCache();

  MockObserver obs(web_contents());
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(obs, RenderFrameDeleted(rfh_a_))
      .WillOnce(::testing::Invoke([&]() { std::move(quit_closure).Run(); }));

  // A moderate memory pressure signal should flush the BFCache.
  fake_memory_pressure_monitor.SetAndNotifyMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_MODERATE);

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(BFCachePolicyBrowserTest,
                       CacheFlushedOnCriticalMemoryPressure) {
  util::test::FakeMemoryPressureMonitor fake_memory_pressure_monitor;

  InitWithOnePageInBFCache();

  MockObserver obs(web_contents());

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(obs, RenderFrameDeleted(rfh_a_))
      .WillOnce(::testing::Invoke([&]() { std::move(quit_closure).Run(); }));

  // A critical memory pressure signal should flush the BFCache.
  fake_memory_pressure_monitor.SetAndNotifyMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_CRITICAL);

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(BFCachePolicyBrowserTest,
                       TabHiddenDuringBackNavigation) {
  util::test::FakeMemoryPressureMonitor fake_memory_pressure_monitor;

  InitWithOnePageInBFCache();

  MockObserver obs(web_contents());
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(obs, RenderFrameDeleted(rfh_b_))
      .WillOnce(::testing::Invoke([&]() { std::move(quit_closure).Run(); }));

  web_contents()->GetController().GoBack();
  // Make the tab backgrounded before the back navigation completes. |rfh_a_|
  // should become the active frame and the cache should be flushed (i.e.
  // |rfh_b_| should be deleted).
  web_contents()->WasHidden();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  run_loop.Run();
  EXPECT_EQ(rfh_a_, top_frame_host());
}

}  // namespace policies
}  // namespace performance_manager
