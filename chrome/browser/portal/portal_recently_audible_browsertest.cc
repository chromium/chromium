// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

using content::WebContents;

class PortalRecentlyAudibleBrowserTest : public InProcessBrowserTest {
 public:
  PortalRecentlyAudibleBrowserTest() = default;

  void SetUp() override {
    EXPECT_GT(TestTimeouts::action_timeout(), base::Seconds(2))
        << "action timeout must be long enough for recently audible indicator "
           "to update";

    scoped_feature_list_.InitAndEnableFeature(blink::features::kPortals);
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
  }

  Tab* GetActiveTab() {
    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    TabStrip* tab_strip =
        BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
    return tab_strip->tab_at(tab_strip_model->active_index());
  }

  bool ActiveTabHasAlertState(TabAlertState alert_state) {
    // This doesn't merely use GetTabAlertStatesForContents, since we want to
    // verify that the browser was actually notified that it should update this.
    return base::Contains(GetActiveTab()->data().alert_state, alert_state);
  }

  ::testing::AssertionResult ActiveTabChangesTo(TabAlertState alert_state,
                                                bool expected_present) {
    base::ElapsedTimer timer;
    do {
      if (ActiveTabHasAlertState(alert_state) == expected_present)
        return ::testing::AssertionSuccess();
      base::RunLoop run_loop;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
      run_loop.Run();
    } while (timer.Elapsed() < TestTimeouts::action_timeout());

    return ::testing::AssertionFailure()
           << "tab alert state did not "
           << (expected_present ? "appear" : "disappear") << " within "
           << TestTimeouts::action_timeout();
  }

  ::testing::AssertionResult PlayTone(
      const content::ToRenderFrameHost& target) {
    return content::ExecJs(
        target,
        "if (!window.testTone) {"
        "  window.testAudioContext = new AudioContext();"
        "  window.testTone = testAudioContext.createOscillator();"
        "  testTone.type = 'square';"
        "  testTone.frequency.setValueAtTime("
        "      440, testAudioContext.currentTime);"
        "  testTone.connect(testAudioContext.destination);"
        "}"
        "testTone.start();");
  }

  ::testing::AssertionResult StopTone(
      const content::ToRenderFrameHost& target) {
    return content::ExecJs(target, "window.testTone?.stop();");
  }

  ::testing::AssertionResult InsertPortalTo(
      const content::ToRenderFrameHost& target,
      const GURL& url) {
    auto result = content::EvalJs(
        target,
        content::JsReplace("new Promise((resolve, reject) => {"
                           "  let portal = document.createElement('portal');"
                           "  portal.src = $1;"
                           "  portal.onload = () => resolve(true);"
                           "  document.body.appendChild(portal);"
                           "})",
                           url));
    if (!result.error.empty())
      return ::testing::AssertionFailure() << result.error;
    return ::testing::AssertionSuccess();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PortalRecentlyAudibleBrowserTest, PlayToneAtTopLevel) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_FALSE(ActiveTabHasAlertState(TabAlertState::AUDIO_PLAYING));

  ASSERT_TRUE(PlayTone(browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_TRUE(ActiveTabChangesTo(TabAlertState::AUDIO_PLAYING, true));

  ASSERT_TRUE(StopTone(browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_TRUE(ActiveTabChangesTo(TabAlertState::AUDIO_PLAYING, false));
}

IN_PROC_BROWSER_TEST_F(PortalRecentlyAudibleBrowserTest, PlayToneInPortal) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* title1 = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      InsertPortalTo(title1, embedded_test_server()->GetURL("/title2.html")));
  WebContents* title2 = title1->GetInnerWebContents()[0];

  EXPECT_FALSE(ActiveTabHasAlertState(TabAlertState::AUDIO_PLAYING));

  ASSERT_TRUE(PlayTone(title2));
  EXPECT_TRUE(ActiveTabChangesTo(TabAlertState::AUDIO_PLAYING, true));

  ASSERT_TRUE(StopTone(title2));
  EXPECT_TRUE(ActiveTabChangesTo(TabAlertState::AUDIO_PLAYING, false));
}

IN_PROC_BROWSER_TEST_F(PortalRecentlyAudibleBrowserTest,
                       PlayToneInNestedPortal) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* title1 = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      InsertPortalTo(title1, embedded_test_server()->GetURL("/title2.html")));
  WebContents* title2 = title1->GetInnerWebContents()[0];
  ASSERT_TRUE(
      InsertPortalTo(title2, embedded_test_server()->GetURL("/title3.html")));
  WebContents* title3 = title2->GetInnerWebContents()[0];

  EXPECT_FALSE(ActiveTabHasAlertState(TabAlertState::AUDIO_PLAYING));

  ASSERT_TRUE(PlayTone(title3));
  EXPECT_TRUE(ActiveTabChangesTo(TabAlertState::AUDIO_PLAYING, true));

  ASSERT_TRUE(StopTone(title3));
  EXPECT_TRUE(ActiveTabChangesTo(TabAlertState::AUDIO_PLAYING, false));
}

IN_PROC_BROWSER_TEST_F(PortalRecentlyAudibleBrowserTest,
                       ActivateWithTonePlayingInHost) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* title1 = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      InsertPortalTo(title1, embedded_test_server()->GetURL("/title2.html")));

  EXPECT_FALSE(ActiveTabHasAlertState(TabAlertState::AUDIO_PLAYING));

  ASSERT_TRUE(PlayTone(title1));
  EXPECT_TRUE(ActiveTabChangesTo(TabAlertState::AUDIO_PLAYING, true));

  ASSERT_TRUE(
      content::ExecJs(title1, "document.querySelector('portal').activate()"));
  EXPECT_TRUE(ActiveTabChangesTo(TabAlertState::AUDIO_PLAYING, false));
}

// TODO(crbug.com/1155813): Test is flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ActivateWithTonePlayingInPortal \
  DISABLED_ActivateWithTonePlayingInPortal
#else
#define MAYBE_ActivateWithTonePlayingInPortal ActivateWithTonePlayingInPortal
#endif
IN_PROC_BROWSER_TEST_F(PortalRecentlyAudibleBrowserTest,
                       MAYBE_ActivateWithTonePlayingInPortal) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* title1 = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      InsertPortalTo(title1, embedded_test_server()->GetURL("/title2.html")));
  WebContents* title2 = title1->GetInnerWebContents()[0];

  EXPECT_FALSE(ActiveTabHasAlertState(TabAlertState::AUDIO_PLAYING));

  ASSERT_TRUE(PlayTone(title2));
  EXPECT_TRUE(ActiveTabChangesTo(TabAlertState::AUDIO_PLAYING, true));

  ASSERT_TRUE(
      content::ExecJs(title1, "document.querySelector('portal').activate()"));
  EXPECT_FALSE(ActiveTabChangesTo(TabAlertState::AUDIO_PLAYING, false));

  ASSERT_TRUE(StopTone(title2));
  EXPECT_TRUE(ActiveTabChangesTo(TabAlertState::AUDIO_PLAYING, false));
}

IN_PROC_BROWSER_TEST_F(PortalRecentlyAudibleBrowserTest,
                       ActivateAndAdoptWithTonePlayingInHost) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* title1 = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      InsertPortalTo(title1, embedded_test_server()->GetURL("/title2.html")));
  WebContents* title2 = title1->GetInnerWebContents()[0];

  EXPECT_FALSE(ActiveTabHasAlertState(TabAlertState::AUDIO_PLAYING));

  ASSERT_TRUE(PlayTone(title1));
  EXPECT_TRUE(ActiveTabChangesTo(TabAlertState::AUDIO_PLAYING, true));

  ASSERT_TRUE(
      content::ExecJs(title2,
                      "onportalactivate = e => "
                      "document.body.appendChild(e.adoptPredecessor())"));
  ASSERT_TRUE(
      content::ExecJs(title1, "document.querySelector('portal').activate()"));

  // Ideally this would never briefly flicker to false, but it can because the
  // hystersis here applies at the WebContents level, not the tab level, and
  // portals swaps WebContents. So if it does change to false, ignore that...
  std::ignore = ActiveTabChangesTo(TabAlertState::AUDIO_PLAYING, false);

  // ...for it will shortly become true again.
  EXPECT_TRUE(ActiveTabChangesTo(TabAlertState::AUDIO_PLAYING, true));

  ASSERT_TRUE(StopTone(title1));
  EXPECT_TRUE(ActiveTabChangesTo(TabAlertState::AUDIO_PLAYING, false));
}

IN_PROC_BROWSER_TEST_F(PortalRecentlyAudibleBrowserTest,
                       NavigateTabWithTonePlayingInPortal) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* title1 = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      InsertPortalTo(title1, embedded_test_server()->GetURL("/title2.html")));
  WebContents* title2 = title1->GetInnerWebContents()[0];

  EXPECT_FALSE(ActiveTabHasAlertState(TabAlertState::AUDIO_PLAYING));

  ASSERT_TRUE(PlayTone(title2));
  EXPECT_TRUE(ActiveTabChangesTo(TabAlertState::AUDIO_PLAYING, true));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title3.html")));
  EXPECT_TRUE(ActiveTabChangesTo(TabAlertState::AUDIO_PLAYING, false));
}

IN_PROC_BROWSER_TEST_F(PortalRecentlyAudibleBrowserTest,
                       NavigatePortalWithTonePlayingInPortal) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* title1 = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      InsertPortalTo(title1, embedded_test_server()->GetURL("/title2.html")));
  WebContents* title2 = title1->GetInnerWebContents()[0];

  EXPECT_FALSE(ActiveTabHasAlertState(TabAlertState::AUDIO_PLAYING));

  ASSERT_TRUE(PlayTone(title2));
  EXPECT_TRUE(ActiveTabChangesTo(TabAlertState::AUDIO_PLAYING, true));

  ASSERT_TRUE(content::ExecJs(title2, "location.href = '/title3.html';"));
  EXPECT_TRUE(ActiveTabChangesTo(TabAlertState::AUDIO_PLAYING, false));
}

IN_PROC_BROWSER_TEST_F(PortalRecentlyAudibleBrowserTest,
                       RemovePortalWithTonePlayingInPortal) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* title1 = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      InsertPortalTo(title1, embedded_test_server()->GetURL("/title2.html")));
  WebContents* title2 = title1->GetInnerWebContents()[0];

  EXPECT_FALSE(ActiveTabHasAlertState(TabAlertState::AUDIO_PLAYING));

  ASSERT_TRUE(PlayTone(title2));
  EXPECT_TRUE(ActiveTabChangesTo(TabAlertState::AUDIO_PLAYING, true));

  ASSERT_TRUE(
      content::ExecJs(title1, "document.querySelector('portal').remove()"));
  EXPECT_TRUE(ActiveTabChangesTo(TabAlertState::AUDIO_PLAYING, false));
}
