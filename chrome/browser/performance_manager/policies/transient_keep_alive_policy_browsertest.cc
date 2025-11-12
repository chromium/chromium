// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/transient_keep_alive_policy.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/performance_manager/policies/urgent_page_discarding_policy.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace performance_manager::policies {

class TransientKeepAlivePolicyBrowserTest : public InProcessBrowserTest {
 public:
  ~TransientKeepAlivePolicyBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Set up the feature with a short, testable duration and a small count as
    // well as the kTrackEmptyRendererProcessesForReuse to be able to pick up
    // "free" and "empty" renderer process hosts for reuse.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kTransientKeepAlivePolicy,
          {{"duration", "1s"}, {"count", "2"}}},
         {::features::kTrackEmptyRendererProcessesForReuse, {}}},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Disable BFCache discarding to avoid interference.
    content::DisableBackForwardCacheForTesting(
        contents, content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

    // Disable tab discarding to avoid interference.
    policies::UrgentPageDiscardingPolicy::DisableForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the keep-alive expires after the configured duration
// and that the RenderProcessHost is subsequently destroyed.
IN_PROC_BROWSER_TEST_F(TransientKeepAlivePolicyBrowserTest,
                       KeepAliveReleasedAfterTimeoutAndProcessDies) {
  const GURL kUrl1 = embedded_test_server()->GetURL("a.com", "/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("b.com", "/title2.html");

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create the first process.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl1));
  content::RenderProcessHost* rph1 =
      contents->GetPrimaryMainFrame()->GetProcess();
  auto rph1_id = rph1->GetID();

  // Set up a watcher to wait for this RPH to be destroyed.
  // We must set this up *before* we trigger the action.
  content::RenderProcessHostWatcher watcher(
      rph1, content::RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);

  // Create a second process, making the first one empty.
  // This starts our policy's 1-second keep-alive timer.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl2));

  // The histogram guarantees our policy has released its ref count.
  // Now, we must wait for the browser to *actually* destroy the RPH,
  // which happens asynchronously.
  watcher.Wait();

  // As a final check, verify it's no longer findable by its ID.
  // This is the safe way to check that the `rph1` pointer is "null".
  EXPECT_EQ(nullptr, content::RenderProcessHost::FromID(rph1_id));
}

// Tests that a process is reused if a navigation occurs before timeout.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ProcessIsReused DISABLED_ProcessIsReused
#else
#define MAYBE_ProcessIsReused ProcessIsReused
#endif
IN_PROC_BROWSER_TEST_F(TransientKeepAlivePolicyBrowserTest,
                       MAYBE_ProcessIsReused) {
  const GURL kUrl1 = embedded_test_server()->GetURL("a.com", "/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("b.com", "/title2.html");

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // 1. Create `rph1`
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl1));
  content::RenderProcessHost* rph1 =
      contents->GetPrimaryMainFrame()->GetProcess();

  // 2. Navigate away, making `rph1` empty and starting its timer.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl2));
  EXPECT_NE(rph1, contents->GetPrimaryMainFrame()->GetProcess());

  // 3. Navigate back to the first site *before* the 1s timer expires.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl1));

  // 4. Check that the RPH is the same one.
  content::RenderProcessHost* rph2 =
      contents->GetPrimaryMainFrame()->GetProcess();
  EXPECT_EQ(rph1, rph2);
}

}  // namespace performance_manager::policies
