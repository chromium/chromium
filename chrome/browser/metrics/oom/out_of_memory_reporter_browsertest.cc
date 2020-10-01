// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/oom/out_of_memory_reporter.h"

#include <set>
#include <utility>

#include "base/macros.h"
#include "base/optional.h"
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
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

using content::JsReplace;
using content::NavigationController;
using content::ScopedAllowRendererCrashes;
using content::TestNavigationObserver;
using content::WebContents;
using ui_test_utils::NavigateToURL;

// No current reliable way to determine OOM on Linux/Mac. Sanitizers also
// interfere with the exit code on OOM, making this detection unreliable.
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) || \
    defined(ADDRESS_SANITIZER)
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
  ~MAYBE_OutOfMemoryReporterBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUp();
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

 protected:
  base::Optional<GURL> last_oom_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MAYBE_OutOfMemoryReporterBrowserTest);
};

IN_PROC_BROWSER_TEST_F(MAYBE_OutOfMemoryReporterBrowserTest, MemoryExhaust) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  OutOfMemoryReporter::FromWebContents(web_contents)->AddObserver(this);

  const GURL crash_url = embedded_test_server()->GetURL("/title1.html");
  NavigateToURL(browser(), crash_url);

  // Careful, this doesn't actually commit the navigation. So, navigating to
  // this URL will cause an OOM associated with the previous committed URL.
  ScopedAllowRendererCrashes allow_renderer_crashes(
      browser()->tab_strip_model()->GetActiveWebContents());
  NavigateToURL(browser(), GURL(content::kChromeUIMemoryExhaustURL));
  EXPECT_EQ(crash_url, last_oom_url_.value());
}

// No current reliable way to determine OOM on Linux/Mac. Sanitizers also
// interfere with the exit code on OOM, making this detection unreliable.
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) || \
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
    MAYBE_OutOfMemoryReporterBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Since a portal element is navigated to without a gesture from the user, an
// OOM inside an un-activated portal should not cause an OOM report.
IN_PROC_BROWSER_TEST_F(MAYBE_PortalOutOfMemoryReporterBrowserTest,
                       NotReportedForPortal) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  OutOfMemoryReporter::FromWebContents(web_contents)->AddObserver(this);

  const GURL url(embedded_test_server()->GetURL("/portal/activate.html"));
  const GURL memory_exhaust_url(content::kChromeUIMemoryExhaustURL);

  // Navigate the main web contents to a page with a <portal> element.
  ui_test_utils::NavigateToURL(browser(), url);
  std::vector<WebContents*> inner_web_contents =
      web_contents->GetInnerWebContents();
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
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  OutOfMemoryReporter::FromWebContents(web_contents)->AddObserver(this);

  const GURL main_url(embedded_test_server()->GetURL("/portal/activate.html"));
  const GURL crash_url(
      embedded_test_server()->GetURL("/portal/activate-portal.html"));
  const GURL memory_exhaust_url(content::kChromeUIMemoryExhaustURL);

  // Navigate the main web contents to a page with a <portal> element.
  ui_test_utils::NavigateToURL(browser(), main_url);
  std::vector<WebContents*> inner_web_contents =
      web_contents->GetInnerWebContents();
  ASSERT_EQ(1u, inner_web_contents.size());

  // Both the portal and the main frame will add this class as an observer.
  WebContents* portal_contents = inner_web_contents[0];
  OutOfMemoryReporter::FromWebContents(portal_contents)->AddObserver(this);

  // Activate the portal - this is a user-gesture gated signal and means the
  // user intended to visit the portaled content so we should report an OOM
  // now.
  ASSERT_TRUE(ExecJs(web_contents, "activate();"));

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
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();

  EXPECT_EQ(crash_url, last_oom_url_.value());
}
