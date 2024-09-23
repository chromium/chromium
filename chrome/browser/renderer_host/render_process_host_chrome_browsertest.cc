// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "media/base/media_switches.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if BUILDFLAG(IS_MAC)
#include "content/public/browser/browser_child_process_host.h"
#endif  // BUILDFLAG(IS_MAC)

using content::WebContents;

namespace {

int RenderProcessHostCount() {
  return content::RenderProcessHost::GetCurrentRenderProcessCountForTesting();
}

WebContents* FindFirstDevToolsContents() {
  for (content::WebContents* web_contents : content::GetAllWebContents()) {
    if (web_contents->GetURL().SchemeIs(content::kChromeDevToolsScheme) &&
        web_contents->GetPrimaryMainFrame()
            ->GetProcess()
            ->IsInitializedAndNotDead()) {
      return web_contents;
    }
  }
  return nullptr;
}

// TODO(rvargas) crbug.com/417532: Remove this code.
base::Process ProcessFromHandle(base::ProcessHandle handle) {
#if BUILDFLAG(IS_WIN)
  if (handle == GetCurrentProcess())
    return base::Process::Current();

  base::ProcessHandle out_handle;
  if (!::DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(),
                         &out_handle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
    return base::Process();
  }
  handle = out_handle;
#endif  // BUILDFLAG(IS_WIN)
  return base::Process(handle);
}

// Returns true if the priority of `process` is kBestEffort.
bool IsProcessBackgrounded(const base::Process& process) {
#if BUILDFLAG(IS_MAC)
  return process.GetPriority(
             content::BrowserChildProcessHost::GetPortProvider()) ==
         base::Process::Priority::kBestEffort;
#else
  return process.GetPriority() == base::Process::Priority::kBestEffort;
#endif
}

}  // namespace

class ChromeRenderProcessHostTest : public extensions::ExtensionBrowserTest {
 public:
  ChromeRenderProcessHostTest() {}

  ChromeRenderProcessHostTest(const ChromeRenderProcessHostTest&) = delete;
  ChromeRenderProcessHostTest& operator=(const ChromeRenderProcessHostTest&) =
      delete;

  // Show a tab, activating the current one if there is one, and wait for
  // the renderer process to be created or foregrounded, returning the
  // WebContents associated with the tab.
  WebContents* ShowSingletonTab(const GURL& page) {
    ::ShowSingletonTab(browser(), page);
    WebContents* wc = browser()->tab_strip_model()->GetActiveWebContents();
    CHECK(wc->GetURL() == page);

    WaitForLauncherThread();
    WaitForMessageProcessing(wc);
    return wc;
  }

  // Loads the given url in a new background tab and returns the WebContents
  // associated with the tab.
  WebContents* OpenBackgroundTab(const GURL& page) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), page, WindowOpenDisposition::NEW_BACKGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

    TabStripModel* tab_strip = browser()->tab_strip_model();
    WebContents* wc =
        tab_strip->GetWebContentsAt(tab_strip->active_index() + 1);
    CHECK(wc->GetVisibleURL() == page);

    WaitForLauncherThread();
    WaitForMessageProcessing(wc);
    return wc;
  }

  // Ensures that the backgrounding / foregrounding gets a chance to run.
  void WaitForLauncherThread() {
    base::RunLoop run_loop;
    content::GetProcessLauncherTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }

  // Implicitly waits for the renderer process associated with the specified
  // WebContents to process outstanding IPC messages by running some JavaScript
  // and waiting for the result.
  void WaitForMessageProcessing(WebContents* wc) {
    ASSERT_EQ(true, content::EvalJs(wc, "true;"));
  }

  // When we hit the max number of renderers, verify that the way we do process
  // sharing behaves correctly.  In particular, this test is verifying that even
  // when we hit the max process limit, that renderers of each type will wind up
  // in a process of that type, even if that means creating a new process.
  void TestProcessOverflow() {
    int tab_count = 1;
    int host_count = 1;
    WebContents* tab1 = nullptr;
    WebContents* tab2 = nullptr;
    content::RenderProcessHost* rph1 = nullptr;
    content::RenderProcessHost* rph2 = nullptr;
    content::RenderProcessHost* rph3 = nullptr;

    const extensions::Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII("options_page"));

    content::RenderFrameDeletedObserver before_webui_obs(
        content::ConvertToRenderFrameHost(
            browser()->tab_strip_model()->GetActiveWebContents()));

    // Change the first tab to be the omnibox page (WebUI).
    GURL omnibox(chrome::kChromeUIOmniboxURL);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), omnibox));

    // The host objects from the page before the WebUI navigation stick around
    // until the old renderer cleans up and ACKs, which may happen later than
    // the navigation in the WebUI's renderer. So wait for that.
    before_webui_obs.WaitUntilDeleted();

    EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
    tab1 = browser()->tab_strip_model()->GetWebContentsAt(tab_count - 1);
    rph1 = tab1->GetPrimaryMainFrame()->GetProcess();
    EXPECT_EQ(omnibox, tab1->GetURL());
    EXPECT_EQ(host_count, RenderProcessHostCount());

    // Create a new normal tab with a data URL. It should be in its own process.
    GURL page1("data:text/html,hello world1");

    ui_test_utils::TabAddedWaiter add_tab1(browser());
    ::ShowSingletonTab(browser(), page1);
    add_tab1.Wait();

    tab_count++;
    host_count++;
    EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
    tab1 = browser()->tab_strip_model()->GetWebContentsAt(tab_count - 1);
    rph2 = tab1->GetPrimaryMainFrame()->GetProcess();
    EXPECT_EQ(tab1->GetURL(), page1);
    EXPECT_EQ(host_count, RenderProcessHostCount());
    EXPECT_NE(rph1, rph2);

    // Create another data URL tab.  With Site Isolation, this will require its
    // own process, but without Site Isolation, it can share the previous
    // process.
    GURL page2("data:text/html,hello world2");
    ui_test_utils::TabAddedWaiter add_tab2(browser());
    ::ShowSingletonTab(browser(), page2);
    add_tab2.Wait();
    tab_count++;
    if (content::AreAllSitesIsolatedForTesting())
      host_count++;
    EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
    tab2 = browser()->tab_strip_model()->GetWebContentsAt(tab_count - 1);
    EXPECT_EQ(tab2->GetURL(), page2);
    EXPECT_EQ(host_count, RenderProcessHostCount());
    if (content::AreAllSitesIsolatedForTesting())
      EXPECT_NE(tab2->GetPrimaryMainFrame()->GetProcess(), rph2);
    else
      EXPECT_EQ(tab2->GetPrimaryMainFrame()->GetProcess(), rph2);

    // Create another WebUI tab.  Each WebUI tab should get a separate process
    // because of origin locking.
    // Note: intentionally create this tab after the normal tabs to exercise bug
    // 43448 where extension and WebUI tabs could get combined into normal
    // renderers.
    GURL history(chrome::kChromeUIHistoryURL);
    ui_test_utils::TabAddedWaiter add_tab3(browser());
    ::ShowSingletonTab(browser(), history);
    add_tab3.Wait();
    tab_count++;
    host_count++;
    EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
    tab2 = browser()->tab_strip_model()->GetWebContentsAt(tab_count - 1);
    EXPECT_EQ(tab2->GetURL(), GURL(history));
    EXPECT_EQ(host_count, RenderProcessHostCount());
    EXPECT_NE(tab2->GetPrimaryMainFrame()->GetProcess(), rph1);

    // Create an extension tab.  It should be in its own process.
    GURL extension_url("chrome-extension://" + extension->id());
    ui_test_utils::TabAddedWaiter add_tab4(browser());
    ::ShowSingletonTab(browser(), extension_url);

    add_tab4.Wait();
    tab_count++;
    host_count++;
    EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
    tab1 = browser()->tab_strip_model()->GetWebContentsAt(tab_count - 1);
    rph3 = tab1->GetPrimaryMainFrame()->GetProcess();
    EXPECT_EQ(tab1->GetURL(), extension_url);
    EXPECT_EQ(host_count, RenderProcessHostCount());
    EXPECT_NE(rph1, rph3);
    EXPECT_NE(rph2, rph3);
  }
};

class ChromeRenderProcessHostTestWithCommandLine
    : public ChromeRenderProcessHostTest {
 public:
  ChromeRenderProcessHostTestWithCommandLine() = default;

  ChromeRenderProcessHostTestWithCommandLine(
      const ChromeRenderProcessHostTestWithCommandLine&) = delete;
  ChromeRenderProcessHostTestWithCommandLine& operator=(
      const ChromeRenderProcessHostTestWithCommandLine&) = delete;

  ~ChromeRenderProcessHostTestWithCommandLine() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeRenderProcessHostTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kRendererProcessLimit, "1");
  }
};

IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTest, ProcessPerTab) {
  // Set max renderers to 1 to force running out of processes.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  base::CommandLine& parsed_command_line =
      *base::CommandLine::ForCurrentProcess();
  parsed_command_line.AppendSwitch(switches::kProcessPerTab);

  int tab_count = 1;
  int host_count = 1;

  content::RenderFrameDeletedObserver before_webui_obs(
      content::ConvertToRenderFrameHost(
          browser()->tab_strip_model()->GetActiveWebContents()));

  // Change the first tab to be a WebUI page.
  GURL omnibox(chrome::kChromeUIOmniboxURL);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), omnibox));

  // The host objects from the page before the WebUI navigation stick around
  // until the old renderer cleans up and ACKs, which may happen later than the
  // navigation in the WebUI's renderer. So wait for that.
  before_webui_obs.WaitUntilDeleted();

  // Expect just the WebUI tab's process to be around.
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create a new normal tab with a data URL.  It should be in its own process.
  GURL page1("data:text/html,hello world1");
  content::TestNavigationObserver navigation_observer1(page1);
  navigation_observer1.StartWatchingNewWebContents();
  ui_test_utils::TabAddedWaiter add_tab1(browser());
  ::ShowSingletonTab(browser(), page1);
  add_tab1.Wait();
  navigation_observer1.Wait();
  tab_count++;
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create another data URL tab.  With Site Isolation, this will require its
  // own process, but without Site Isolation, it can share the previous process.
  GURL page2("data:text/html,hello world2");
  content::TestNavigationObserver navigation_observer2(page2);
  navigation_observer2.StartWatchingNewWebContents();
  ui_test_utils::TabAddedWaiter add_tab2(browser());
  ::ShowSingletonTab(browser(), page2);
  add_tab2.Wait();
  navigation_observer2.Wait();
  tab_count++;
  if (content::AreAllSitesIsolatedForTesting())
    host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create another omnibox tab.  It should share the process with the other
  // WebUI.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), omnibox, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  tab_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create another omnibox tab.  It should share the process with the other
  // WebUI.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), omnibox, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  tab_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());
}

class ChromeRenderProcessHostBackgroundingTest
    : public ChromeRenderProcessHostTest {
 public:
  ChromeRenderProcessHostBackgroundingTest() = default;

  ChromeRenderProcessHostBackgroundingTest(
      const ChromeRenderProcessHostBackgroundingTest&) = delete;
  ChromeRenderProcessHostBackgroundingTest& operator=(
      const ChromeRenderProcessHostBackgroundingTest&) = delete;

  ~ChromeRenderProcessHostBackgroundingTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeRenderProcessHostTest::SetUpCommandLine(command_line);

    command_line->AppendSwitch(switches::kProcessPerTab);
  }

  void VerifyProcessIsBackgrounded(WebContents* web_contents) {
    constexpr bool kExpectedIsBackground = true;
    VerifyProcessPriority(web_contents->GetPrimaryMainFrame()->GetProcess(),
                          kExpectedIsBackground);
  }

  void VerifyProcessIsForegrounded(WebContents* web_contents) {
    constexpr bool kExpectedIsBackground = false;
    VerifyProcessPriority(web_contents->GetPrimaryMainFrame()->GetProcess(),
                          kExpectedIsBackground);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  void VerifyProcessPriority(content::RenderProcessHost* process,
                             bool expected_is_backgrounded) {
    EXPECT_TRUE(process->IsInitializedAndNotDead());
    EXPECT_EQ(expected_is_backgrounded,
              process->GetPriority() == base::Process::Priority::kBestEffort);

    if (base::Process::CanSetPriority()) {
      base::Process p = ProcessFromHandle(process->GetProcess().Handle());
      ASSERT_TRUE(p.IsValid());
      EXPECT_EQ(expected_is_backgrounded, IsProcessBackgrounded(p));
    }
  }
};

#define EXPECT_PROCESS_IS_BACKGROUNDED(process_or_tab)                       \
  do {                                                                       \
    SCOPED_TRACE("Verifying that |" #process_or_tab "| is backgrounded..."); \
    VerifyProcessIsBackgrounded(process_or_tab);                             \
  } while (0);

#define EXPECT_PROCESS_IS_FOREGROUNDED(process_or_tab)                       \
  do {                                                                       \
    SCOPED_TRACE("Verifying that |" #process_or_tab "| is foregrounded..."); \
    VerifyProcessIsForegrounded(process_or_tab);                             \
  } while (0);

// Flaky on Mac: https://crbug.com/888308
#if BUILDFLAG(IS_MAC)
#define MAYBE_MultipleTabs DISABLED_MultipleTabs
#else
#define MAYBE_MultipleTabs MultipleTabs
#endif
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostBackgroundingTest,
                       MAYBE_MultipleTabs) {
  // Change the first tab to be the omnibox page (TYPE_WEBUI).
  GURL omnibox(chrome::kChromeUIOmniboxURL);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), omnibox));

  // Create a new tab. It should be foreground.
  GURL page1("data:text/html,hello world1");
  WebContents* tab1 = ShowSingletonTab(page1);
  {
    SCOPED_TRACE("TEST STEP: Single tab");
    EXPECT_PROCESS_IS_FOREGROUNDED(tab1);
  }

  // Create another tab. It should be foreground, and the first tab should
  // now be background.
  GURL page2("data:text/html,hello world2");
  WebContents* tab2 = ShowSingletonTab(page2);
  {
    SCOPED_TRACE("TEST STEP: 2nd tab opened in foreground");
    EXPECT_NE(tab1->GetPrimaryMainFrame()->GetProcess(),
              tab2->GetPrimaryMainFrame()->GetProcess());
    EXPECT_PROCESS_IS_BACKGROUNDED(tab1);
    EXPECT_PROCESS_IS_FOREGROUNDED(tab2);
  }

  // Load another tab in background. The renderer of the new tab should be
  // backgrounded, while visibility of the other renderers should not change.
  GURL page3("data:text/html,hello world3");
  WebContents* tab3 = OpenBackgroundTab(page3);
  {
    SCOPED_TRACE("TEST STEP: 3rd tab opened in background");
    EXPECT_NE(tab1->GetPrimaryMainFrame()->GetProcess(),
              tab3->GetPrimaryMainFrame()->GetProcess());
    EXPECT_NE(tab2->GetPrimaryMainFrame()->GetProcess(),
              tab3->GetPrimaryMainFrame()->GetProcess());
    EXPECT_PROCESS_IS_BACKGROUNDED(tab1);
    EXPECT_PROCESS_IS_FOREGROUNDED(tab2);
    EXPECT_PROCESS_IS_BACKGROUNDED(tab3);
  }

  // Navigate back to the first page. Its renderer should be in foreground
  // again while the other renderers should be backgrounded.
  ShowSingletonTab(page1);
  {
    SCOPED_TRACE("TEST STEP: First tab activated again");
    EXPECT_PROCESS_IS_FOREGROUNDED(tab1);
    EXPECT_PROCESS_IS_BACKGROUNDED(tab2);
    EXPECT_PROCESS_IS_BACKGROUNDED(tab3);
  }

  // Confirm that |tab3| remains backgrounded after being shown/hidden.
  ShowSingletonTab(page3);
  ShowSingletonTab(page1);
  {
    SCOPED_TRACE("TEST STEP: Third tab activated and deactivated");
    EXPECT_PROCESS_IS_FOREGROUNDED(tab1);
    EXPECT_PROCESS_IS_BACKGROUNDED(tab2);
    EXPECT_PROCESS_IS_BACKGROUNDED(tab3);
  }
}

IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTest, ProcessOverflow) {
  // Set max renderers to 1 to force running out of processes.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);
  TestProcessOverflow();
}

// Variation of the ProcessOverflow test, which is driven through command line
// parameter instead of direct function call into the class.
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTestWithCommandLine,
                       ProcessOverflowCommandLine) {
  TestProcessOverflow();
}

// Ensure that DevTools opened to debug DevTools is launched in a separate
// process when --process-per-tab is set. See crbug.com/69873.
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTest,
                       DevToolsOnSelfInOwnProcessPPT) {
  base::CommandLine& parsed_command_line =
      *base::CommandLine::ForCurrentProcess();
  parsed_command_line.AppendSwitch(switches::kProcessPerTab);

  int tab_count = 1;
  int host_count = 1;

  GURL page1("data:text/html,hello world1");
  ui_test_utils::TabAddedWaiter add_tab(browser());
  ::ShowSingletonTab(browser(), page1);
  add_tab.Wait();
  tab_count++;
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // DevTools start in docked mode (no new tab), in a separate process.
  chrome::ToggleDevToolsWindow(browser(), DevToolsToggleAction::Inspect(),
                               DevToolsOpenedByAction::kUnknown);
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  WebContents* devtools = FindFirstDevToolsContents();
  DCHECK(devtools);

  // DevTools start in a separate process.
  DevToolsWindow::OpenDevToolsWindow(devtools, DevToolsToggleAction::Inspect(),
                                     DevToolsOpenedByAction::kUnknown);
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Close docked devtools.
  content::WebContentsDestroyedWatcher close_observer(devtools);

  chrome::ToggleDevToolsWindow(browser(), DevToolsToggleAction::Toggle(),
                               DevToolsOpenedByAction::kUnknown);
  close_observer.Wait();
}

// Ensure that DevTools opened to debug DevTools is launched in a separate
// process. See crbug.com/69873.
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTest,
                       DevToolsOnSelfInOwnProcess) {
  int tab_count = 1;
  int host_count = 1;

  GURL page1("data:text/html,hello world1");
  ui_test_utils::TabAddedWaiter add_tab1(browser());
  ::ShowSingletonTab(browser(), page1);
  add_tab1.Wait();
  tab_count++;
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // DevTools start in docked mode (no new tab), in a separate process.
  chrome::ToggleDevToolsWindow(browser(), DevToolsToggleAction::Inspect(),
                               DevToolsOpenedByAction::kUnknown);
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  WebContents* devtools = FindFirstDevToolsContents();
  DCHECK(devtools);

  // DevTools start in a separate process.
  DevToolsWindow::OpenDevToolsWindow(devtools, DevToolsToggleAction::Inspect(),
                                     DevToolsOpenedByAction::kUnknown);
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Close docked devtools.
  content::WebContentsDestroyedWatcher close_observer(devtools);
  chrome::ToggleDevToolsWindow(browser(), DevToolsToggleAction::Toggle(),
                               DevToolsOpenedByAction::kUnknown);
  close_observer.Wait();
}

// This class's goal is to close the browser window when a renderer process has
// crashed. It does so by monitoring WebContents for RenderProcessGone event and
// closing the passed in TabStripModel. This is used in the following test case.
class WindowDestroyer : public content::WebContentsObserver {
 public:
  WindowDestroyer(content::WebContents* web_contents, TabStripModel* model)
      : content::WebContentsObserver(web_contents), tab_strip_model_(model) {}

  WindowDestroyer(const WindowDestroyer&) = delete;
  WindowDestroyer& operator=(const WindowDestroyer&) = delete;

  // Wait for the browser window to be destroyed.
  void Wait() { ui_test_utils::WaitForBrowserToClose(); }

  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {
    tab_strip_model_->CloseAllTabs();
  }

 private:
  raw_ptr<TabStripModel> tab_strip_model_;
};

// Test to ensure that while iterating through all listeners in
// RenderProcessHost and invalidating them, we remove them properly and don't
// access already freed objects. See http://crbug.com/255524.
// Disabled due to flakiness, see  http://crbug.com/606485.
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTest,
                       DISABLED_CloseAllTabsDuringProcessDied) {
  GURL url(chrome::kChromeUIOmniboxURL);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* wc1 = browser()->tab_strip_model()->GetWebContentsAt(0);

  content::WebContentsAddedObserver wc2_observer;
  content::ExecuteScriptAsync(
      wc1, content::JsReplace("window.open($1, '_blank')", url));
  WebContents* wc2 = wc2_observer.GetWebContents();
  content::TestNavigationObserver nav_observer(wc2, 1);
  nav_observer.Wait();

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(wc1->GetPrimaryMainFrame()->GetLastCommittedURL(),
            wc2->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(wc1->GetPrimaryMainFrame()->GetProcess(),
            wc2->GetPrimaryMainFrame()->GetProcess());

  // Create an object that will close the window on a process crash.
  WindowDestroyer destroyer(wc1, browser()->tab_strip_model());

  // Kill the renderer process, simulating a crash. This should the ProcessDied
  // method to be called. Alternatively, RenderProcessHost::OnChannelError can
  // be called to directly force a call to ProcessDied.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(wc1);
  wc1->GetPrimaryMainFrame()->GetProcess()->Shutdown(-1);

  destroyer.Wait();
}

// Sets up the browser in order to start the tests with two tabs open: one
// called "no audio" in foreground and another called "audio" in background with
// audio in playing state. Also sets up the variables containing the process
// associated with each tab, the urls of the two pages and the WebContents of
// the "audio" page.
class ChromeRenderProcessHostBackgroundingTestWithAudio
    : public ChromeRenderProcessHostTest {
 public:
  ChromeRenderProcessHostBackgroundingTestWithAudio() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
          // Tests require that each tab has a different process.
          features::kDisableProcessReuse,
#if BUILDFLAG(IS_MAC)
          // Tests require that backgrounding processes is possible.
          features::kMacAllowBackgroundingRenderProcesses,
#endif
        },
        /*disabled_features=*/{});
  }

  ChromeRenderProcessHostBackgroundingTestWithAudio(
      const ChromeRenderProcessHostBackgroundingTestWithAudio&) = delete;
  ChromeRenderProcessHostBackgroundingTestWithAudio& operator=(
      const ChromeRenderProcessHostBackgroundingTestWithAudio&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeRenderProcessHostTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
  }

  void SetUpOnMainThread() override {
    ChromeRenderProcessHostTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    // Set up the server and get the test pages.
    base::FilePath test_data_dir;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir.AppendASCII("chrome/test/data/"));
    audio_url_ = embedded_test_server()->GetURL("/extensions/loop_audio.html");
    no_audio_url_ = embedded_test_server()->GetURL("/title1.html");

    embedded_test_server()->StartAcceptingConnections();

    // Open a browser, navigate to the audio page and get its WebContents.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), audio_url_));
    audio_tab_web_contents_ =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Create a new tab for the no audio page and confirm that the process of
    // each tab is different and that both are valid.
    audio_process_ =
        ProcessFromHandle(audio_tab_web_contents_->GetPrimaryMainFrame()
                              ->GetProcess()
                              ->GetProcess()
                              .Handle());
    WebContents* wc = ShowSingletonTab(no_audio_url_);
    no_audio_process_ = ProcessFromHandle(
        wc->GetPrimaryMainFrame()->GetProcess()->GetProcess().Handle());
    ASSERT_NE(audio_process_.Pid(), no_audio_process_.Pid());
    ASSERT_TRUE(no_audio_process_.IsValid());
    ASSERT_TRUE(audio_process_.IsValid());
  }

 protected:
  void WaitUntilBackgrounded(const base::Process& lhs,
                             bool lhs_backgrounded,
                             const base::Process& rhs,
                             bool rhs_backgrounded) {
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return IsProcessBackgrounded(lhs) == lhs_backgrounded &&
             IsProcessBackgrounded(rhs) == rhs_backgrounded;
    }));
  }

  GURL audio_url_;
  GURL no_audio_url_;

  base::Process audio_process_;
  base::Process no_audio_process_;

  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged>
      audio_tab_web_contents_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test to make sure that a process is backgrounded when the audio stops playing
// from the active tab and there is an immediate tab switch.
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostBackgroundingTestWithAudio,
                       ProcessPriorityAfterStoppedAudio) {
  // This test is invalid on platforms that can't set priority.
  if (!base::Process::CanSetPriority()) {
    return;
  }

  ShowSingletonTab(audio_url_);

  // Wait until the no audio page is backgrounded and the audio page is not
  // backgrounded.
  WaitUntilBackgrounded(no_audio_process_, true, audio_process_, false);
  // Pause the audio and immediately switch to the no audio tab.
  ASSERT_TRUE(
      content::ExecJs(audio_tab_web_contents_.get(),
                      "document.getElementById('audioPlayer').pause();"));
  ShowSingletonTab(no_audio_url_);

  // Wait until the no audio page is not backgrounded and the audio page is
  // backgrounded.
  WaitUntilBackgrounded(no_audio_process_, false, audio_process_, true);
}

// Test to make sure that a process is backgrounded automatically when audio
// stops playing from a hidden tab.
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostBackgroundingTestWithAudio,
                       ProcessPriorityAfterAudioStopsOnNotVisibleTab) {
  // This test is invalid on platforms that can't set priority.
  if (!base::Process::CanSetPriority()) {
    return;
  }

  // Wait until the two pages are not backgrounded.
  WaitUntilBackgrounded(audio_process_, false, no_audio_process_, false);
  // Stop the audio.
  ASSERT_TRUE(
      content::ExecJs(audio_tab_web_contents_.get(),
                      "document.getElementById('audioPlayer').pause();"));

  // Wait until the no audio page is not backgrounded and the audio page is
  // backgrounded.
  WaitUntilBackgrounded(no_audio_process_, false, audio_process_, true);
}

// Test to make sure that a process is un-backgrounded automatically when
// audio
// starts playing from a backgrounded tab.
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostBackgroundingTestWithAudio,
                       ProcessPriorityAfterAudioStartsFromBackgroundTab) {
  // This test is invalid on platforms that can't background.
  if (!base::Process::CanSetPriority()) {
    return;
  }

  // Stop the audio.
  ASSERT_TRUE(
      content::ExecJs(audio_tab_web_contents_.get(),
                      "document.getElementById('audioPlayer').pause();"));

  WaitUntilBackgrounded(no_audio_process_, false, audio_process_, true);

  // Start the audio from the backgrounded tab.
  ASSERT_TRUE(
      content::ExecJs(audio_tab_web_contents_.get(),
                      "document.getElementById('audioPlayer').play();"));

  // Wait until the two pages are not backgrounded.
  WaitUntilBackgrounded(no_audio_process_, false, audio_process_, false);
}
