// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/oom/out_of_memory_reporter.h"

#include <set>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

using content::ExecuteScriptAsync;
using content::JsReplace;
using content::NavigationController;
using content::RenderFrameHost;
using content::ScopedAllowRendererCrashes;
using content::TestNavigationObserver;
using content::WebContents;
using content::test::PrerenderHostObserver;
using content::test::PrerenderTestHelper;
using ui_test_utils::NavigateToURL;

// No current reliable way to determine OOM on Linux/Mac. Sanitizers also
// interfere with the exit code on OOM, making this detection unreliable.
// TODO(crbug.com/1304695): Fix flakiness on Windows.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN) || defined(ADDRESS_SANITIZER)
#define MAYBE_OutOfMemoryReporterBrowserTest \
  DISABLED_OutOfMemoryReporterBrowserTest
#else
#define MAYBE_OutOfMemoryReporterBrowserTest OutOfMemoryReporterBrowserTest
#endif
class MAYBE_OutOfMemoryReporterBrowserTest
    : public InProcessBrowserTest,
      public OutOfMemoryReporter::Observer {
 public:
  MAYBE_OutOfMemoryReporterBrowserTest() = default;

  MAYBE_OutOfMemoryReporterBrowserTest(
      const MAYBE_OutOfMemoryReporterBrowserTest&) = delete;
  MAYBE_OutOfMemoryReporterBrowserTest& operator=(
      const MAYBE_OutOfMemoryReporterBrowserTest&) = delete;

  ~MAYBE_OutOfMemoryReporterBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Disable stack traces during this test since DbgHelp is unreliable in
    // low-memory conditions (see crbug.com/692564).
    command_line->AppendSwitch(switches::kDisableInProcessStackTraces);
  }

  // OutOfMemoryReporter::Observer:
  void OnForegroundOOMDetected(const GURL& url,
                               ukm::SourceId source_id) override {
    last_oom_url_ = url;
  }

  WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  absl::optional<GURL> last_oom_url_;
};

IN_PROC_BROWSER_TEST_F(MAYBE_OutOfMemoryReporterBrowserTest, MemoryExhaust) {
  OutOfMemoryReporter::FromWebContents(web_contents())->AddObserver(this);

  const GURL crash_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(browser(), crash_url));

  // Careful, this doesn't actually commit the navigation. So, navigating to
  // this URL will cause an OOM associated with the previous committed URL.
  ScopedAllowRendererCrashes allow_renderer_crashes(web_contents());
  ASSERT_TRUE(NavigateToURL(browser(), GURL(blink::kChromeUIMemoryExhaustURL)));
  EXPECT_EQ(crash_url, last_oom_url_.value());
}

// No current reliable way to determine OOM on Linux/Mac. Sanitizers also
// interfere with the exit code on OOM, making this detection unreliable.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    defined(ADDRESS_SANITIZER)
#define MAYBE_PortalOutOfMemoryReporterBrowserTest \
  DISABLED_PortalOutOfMemoryReporterBrowserTest
#else
#define MAYBE_PortalOutOfMemoryReporterBrowserTest \
  PortalOutOfMemoryReporterBrowserTest
#endif
class MAYBE_PortalOutOfMemoryReporterBrowserTest
    : public MAYBE_OutOfMemoryReporterBrowserTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kPortals,
                              blink::features::kPortalsCrossOrigin},
        /*disabled_features=*/{});
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Since a portal element is navigated to without a gesture from the user, an
// OOM inside an un-activated portal should not cause an OOM report.
IN_PROC_BROWSER_TEST_F(MAYBE_PortalOutOfMemoryReporterBrowserTest,
                       NotReportedForPortal) {
  OutOfMemoryReporter::FromWebContents(web_contents())->AddObserver(this);

  const GURL url(embedded_test_server()->GetURL("/portal/activate.html"));
  const GURL memory_exhaust_url(blink::kChromeUIMemoryExhaustURL);

  // Navigate the main web contents to a page with a <portal> element.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  std::vector<WebContents*> inner_web_contents =
      web_contents()->GetInnerWebContents();
  ASSERT_EQ(1u, inner_web_contents.size());

  // Both the portal and the main frame will add this class as an observer.
  WebContents* portal_contents = inner_web_contents[0];
  OutOfMemoryReporter::FromWebContents(portal_contents)->AddObserver(this);

  // Navigate the portal to the internal OOM crash page. Normally a portal can
  // only be navigated via script but we have to cheat here a bit since script
  // navigations can't access the crash page.
  ScopedAllowRendererCrashes allow_renderer_crashes(portal_contents);
  TestNavigationObserver nav_observer(portal_contents);
  NavigationController::LoadURLParams params(memory_exhaust_url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  portal_contents->GetController().LoadURLWithParams(params);
  nav_observer.Wait();

  // Wait a short amount of time to ensure the OOM report isn't delayed.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();

  // Ensure we didn't get an OOM report from either web contents.
  EXPECT_FALSE(last_oom_url_.has_value());
}

// Now try a crash that occurs in a portal after activation. Since this should
// behave the same as any other top-level browsing context, we now expect to
// record the OOM crash.
IN_PROC_BROWSER_TEST_F(MAYBE_PortalOutOfMemoryReporterBrowserTest,
                       ReportForActivatedPortal) {
  OutOfMemoryReporter::FromWebContents(web_contents())->AddObserver(this);

  const GURL main_url(embedded_test_server()->GetURL("/portal/activate.html"));
  const GURL crash_url(
      embedded_test_server()->GetURL("/portal/activate-portal.html"));
  const GURL memory_exhaust_url(blink::kChromeUIMemoryExhaustURL);

  // Navigate the main web contents to a page with a <portal> element.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  std::vector<WebContents*> inner_web_contents =
      web_contents()->GetInnerWebContents();
  ASSERT_EQ(1u, inner_web_contents.size());

  // Both the portal and the main frame will add this class as an observer.
  WebContents* portal_contents = inner_web_contents[0];
  OutOfMemoryReporter::FromWebContents(portal_contents)->AddObserver(this);

  // Activate the portal - this is a user-gesture gated signal and means the
  // user intended to visit the portaled content so we should report an OOM
  // now.
  ASSERT_TRUE(ExecJs(web_contents(), "activate();"));

  // Navigate the now-activated portal to the internal OOM crash page.
  ScopedAllowRendererCrashes allow_renderer_crashes(portal_contents);
  TestNavigationObserver nav_observer(portal_contents);
  NavigationController::LoadURLParams params(memory_exhaust_url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  portal_contents->GetController().LoadURLWithParams(params);
  nav_observer.Wait();

  // Wait a short amount of time to ensure the OOM report isn't delayed.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();

  EXPECT_EQ(crash_url, last_oom_url_.value());
}

// No current reliable way to determine OOM on Linux/Mac. Sanitizers also
// interfere with the exit code on OOM, making this detection unreliable.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    defined(ADDRESS_SANITIZER)
#define MAYBE_OutOfMemoryReporterPrerenderBrowserTest \
  DISABLED_OutOfMemoryReporterPrerenderBrowserTest
#else
#define MAYBE_OutOfMemoryReporterPrerenderBrowserTest \
  OutOfMemoryReporterPrerenderBrowserTest
#endif
class MAYBE_OutOfMemoryReporterPrerenderBrowserTest
    : public MAYBE_OutOfMemoryReporterBrowserTest {
 public:
  MAYBE_OutOfMemoryReporterPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &MAYBE_OutOfMemoryReporterPrerenderBrowserTest::web_contents,
            base::Unretained(this))) {}

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    MAYBE_OutOfMemoryReporterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    MAYBE_OutOfMemoryReporterBrowserTest::SetUpOnMainThread();
  }

 protected:
  PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(MAYBE_OutOfMemoryReporterPrerenderBrowserTest,
                       NotReportedOnPrerenderPage) {
  OutOfMemoryReporter::FromWebContents(web_contents())->AddObserver(this);

  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  const GURL prerender_url(embedded_test_server()->GetURL("/title1.html"));

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(browser(), url));

  // Start a prerender.
  int host_id = prerender_helper_.AddPrerender(prerender_url);
  ASSERT_NE(prerender_helper_.GetHostForUrl(prerender_url),
            RenderFrameHost::kNoFrameTreeNodeId);

  RenderFrameHost* prerender_rfh =
      prerender_helper_.GetPrerenderedMainFrameHost(host_id);

  ScopedAllowRendererCrashes allow_renderer_crashes(
      prerender_rfh->GetProcess());
  PrerenderHostObserver host_observer(*web_contents(), host_id);
  // Exhaust renderer process memory of the prerendered page. We execute script
  // that does as similar thing as blink::kChromeUIMemoryExhaustURL because
  // there are various throttles to prevent loading chrome:// URLs for
  // prerendering.
  ExecuteScriptAsync(prerender_rfh,
                     "const x = [];"
                     "while (true) { x.push(new Array(10000000).fill(0)); }");
  host_observer.WaitForDestroyed();

  // Ensure we didn't get an OOM report of the prerendered page.
  EXPECT_FALSE(last_oom_url_.has_value());
}

IN_PROC_BROWSER_TEST_F(MAYBE_OutOfMemoryReporterPrerenderBrowserTest,
                       ReportedOnActivatedPrerenderPage) {
  OutOfMemoryReporter::FromWebContents(web_contents())->AddObserver(this);

  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  const GURL prerender_url(embedded_test_server()->GetURL("/title1.html"));
  const GURL memory_exhaust_url(blink::kChromeUIMemoryExhaustURL);

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(browser(), url));

  // Start a prerender.
  prerender_helper_.AddPrerender(prerender_url);
  ASSERT_NE(prerender_helper_.GetHostForUrl(prerender_url),
            RenderFrameHost::kNoFrameTreeNodeId);

  // Activate the prerendered page.
  prerender_helper_.NavigatePrimaryPage(prerender_url);
  ASSERT_EQ(web_contents()->GetURL(), prerender_url);

  // Exhaust renderer process memory of the activated page.
  ScopedAllowRendererCrashes allow_renderer_crashes(web_contents());
  ASSERT_TRUE(NavigateToURL(browser(), memory_exhaust_url));

  // Ensure OOM is reported with the activated page.
  EXPECT_EQ(prerender_url, last_oom_url_.value());
}
