// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/voice_focused_contents_tracker.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/base_window.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace ttc {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

// Records every active tab change reported by a VoiceFocusedContentsTracker.
// The recorded pointers are only compared, never dereferenced, since the
// WebContents they point to may since have been destroyed.
class RecordingObserver : public VoiceFocusedContentsTracker::Observer {
 public:
  explicit RecordingObserver(VoiceFocusedContentsTracker& tracker) {
    observation_.Observe(&tracker);
  }

  void OnVoiceFocusedContentsChanged(
      content::WebContents* web_contents) override {
    changes_.push_back(web_contents);
  }

  // Returns the changes recorded since the last call, and clears them.
  std::vector<content::WebContents*> TakeChanges() {
    return std::exchange(changes_, {});
  }

 private:
  std::vector<content::WebContents*> changes_;
  base::ScopedObservation<VoiceFocusedContentsTracker,
                          VoiceFocusedContentsTracker::Observer>
      observation_{this};
};

class VoiceFocusedContentsTrackerBrowserTest : public InProcessBrowserTest {
 protected:
  std::unique_ptr<VoiceFocusedContentsTracker> CreateTracker() {
    return VoiceFocusedContentsTracker::Create(*browser()->GetProfile());
  }

  content::WebContents* ActiveContents(BrowserWindowInterface* browser) {
    return browser->GetTabStripModel()->GetActiveWebContents();
  }

  void AddForegroundTab(BrowserWindowInterface* browser) {
    ASSERT_TRUE(AddTabAtIndexToBrowser(
        browser, /*index=*/-1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  }

  // Creates a second browser window for the test profile and waits for it to
  // become the last active browser.
  BrowserWindowInterface* CreateAndActivateSecondBrowser() {
    BrowserWindowInterface* second_browser =
        CreateBrowser(browser()->GetProfile());
    // This returns immediately if `second_browser` is already last-active, and
    // otherwise waits for it to become so.
    ui_test_utils::WaitForBrowserSetLastActive(second_browser);
    return second_browser;
  }

  void ActivateBrowser(BrowserWindowInterface* browser) {
    browser->GetWindow()->Activate();
    ui_test_utils::WaitForBrowserSetLastActive(browser);
  }
};

// The tracker initially reports the active tab of the active browser window.
IN_PROC_BROWSER_TEST_F(VoiceFocusedContentsTrackerBrowserTest,
                       InitialActiveTab) {
  std::unique_ptr<VoiceFocusedContentsTracker> tracker = CreateTracker();
  EXPECT_EQ(tracker->GetActiveWebContents(), ActiveContents(browser()));
}

// Switching tabs within the bound window updates the active tab.
IN_PROC_BROWSER_TEST_F(VoiceFocusedContentsTrackerBrowserTest,
                       TabSwitchInBoundWindow) {
  std::unique_ptr<VoiceFocusedContentsTracker> tracker = CreateTracker();
  RecordingObserver observer(*tracker);
  content::WebContents* first_tab = ActiveContents(browser());

  AddForegroundTab(browser());
  content::WebContents* second_tab = ActiveContents(browser());
  ASSERT_NE(first_tab, second_tab);
  EXPECT_EQ(tracker->GetActiveWebContents(), second_tab);
  EXPECT_THAT(observer.TakeChanges(), ElementsAre(second_tab));

  browser()->GetTabStripModel()->ActivateTabAt(0);
  EXPECT_EQ(tracker->GetActiveWebContents(), first_tab);
  EXPECT_THAT(observer.TakeChanges(), ElementsAre(first_tab));
}

// Closing the active tab of the bound window moves to the newly activated tab.
IN_PROC_BROWSER_TEST_F(VoiceFocusedContentsTrackerBrowserTest, CloseActiveTab) {
  AddForegroundTab(browser());
  std::unique_ptr<VoiceFocusedContentsTracker> tracker = CreateTracker();
  RecordingObserver observer(*tracker);
  TabStripModel* tab_strip = browser()->GetTabStripModel();
  content::WebContents* remaining_tab = tab_strip->GetWebContentsAt(0);

  tab_strip->CloseWebContentsAt(tab_strip->active_index(),
                                TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(tracker->GetActiveWebContents(), remaining_tab);
  EXPECT_THAT(observer.TakeChanges(), ElementsAre(remaining_tab));
}

// Activating a second browser window of the profile binds to it and reports its
// active tab; adding/switching tabs in the second window updates the active
// tab; re-activating the first window binds back to it and reports its active
// tab.
IN_PROC_BROWSER_TEST_F(VoiceFocusedContentsTrackerBrowserTest,
                       SwitchingWindowsBindsToNewWindow) {
#if BUILDFLAG(IS_OZONE)
  if (::ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP() << "Wayland doesn't support programmatic window activation";
  }
#endif
  std::unique_ptr<VoiceFocusedContentsTracker> tracker = CreateTracker();
  RecordingObserver observer(*tracker);
  content::WebContents* first_window_tab = ActiveContents(browser());

  BrowserWindowInterface* second_browser = CreateAndActivateSecondBrowser();
  content::WebContents* second_window_tab1 = ActiveContents(second_browser);
  ASSERT_NE(first_window_tab, second_window_tab1);
  EXPECT_EQ(tracker->GetActiveWebContents(), second_window_tab1);
  EXPECT_THAT(observer.TakeChanges(),
              ElementsAre(first_window_tab, second_window_tab1));

  AddForegroundTab(second_browser);
  content::WebContents* second_window_tab2 = ActiveContents(second_browser);
  ASSERT_NE(second_window_tab1, second_window_tab2);
  EXPECT_EQ(tracker->GetActiveWebContents(), second_window_tab2);
  EXPECT_THAT(observer.TakeChanges(), ElementsAre(second_window_tab2));

  ActivateBrowser(browser());
  EXPECT_EQ(tracker->GetActiveWebContents(), first_window_tab);
  EXPECT_THAT(observer.TakeChanges(), ElementsAre(first_window_tab));
}

// Tab changes in an inactive window of the profile are not reported while
// another window is active, and activating that window afterwards reports its
// new active tab.
IN_PROC_BROWSER_TEST_F(VoiceFocusedContentsTrackerBrowserTest,
                       TabSwitchInInactiveWindow) {
#if BUILDFLAG(IS_OZONE)
  if (::ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP() << "Wayland doesn't support programmatic window activation";
  }
#endif
  std::unique_ptr<VoiceFocusedContentsTracker> tracker = CreateTracker();
  RecordingObserver observer(*tracker);
  content::WebContents* first_window_tab = ActiveContents(browser());

  BrowserWindowInterface* second_browser = CreateAndActivateSecondBrowser();
  content::WebContents* second_window_tab = ActiveContents(second_browser);
  EXPECT_EQ(tracker->GetActiveWebContents(), second_window_tab);
  EXPECT_THAT(observer.TakeChanges(),
              ElementsAre(first_window_tab, second_window_tab));

  AddForegroundTab(browser());
  content::WebContents* first_window_new_tab = ActiveContents(browser());
  EXPECT_EQ(tracker->GetActiveWebContents(), second_window_tab);
  EXPECT_THAT(observer.TakeChanges(), IsEmpty());

  ActivateBrowser(browser());
  EXPECT_EQ(tracker->GetActiveWebContents(), first_window_new_tab);
  EXPECT_THAT(observer.TakeChanges(), ElementsAre(first_window_new_tab));
}

// When two windows of the profile are open and the active one closes, the
// tracker binds to the remaining window; when the last window of the profile
// closes, the active tab is cleared (nullptr) and observers are notified with
// nullptr.
IN_PROC_BROWSER_TEST_F(VoiceFocusedContentsTrackerBrowserTest,
                       ClosingWindowAndAllWindows) {
  BrowserWindowInterface* first_browser = browser();
  BrowserWindowInterface* second_browser = CreateAndActivateSecondBrowser();
  // Keep an incognito browser open so the browser process does not shut down
  // when all windows of the test profile close.
  CreateIncognitoBrowser();

  std::unique_ptr<VoiceFocusedContentsTracker> tracker =
      VoiceFocusedContentsTracker::Create(*first_browser->GetProfile());
  RecordingObserver observer(*tracker);

  content::WebContents* first_tab = ActiveContents(first_browser);
  content::WebContents* second_tab = ActiveContents(second_browser);
  ASSERT_EQ(tracker->GetActiveWebContents(), second_tab);

  CloseBrowserSynchronously(second_browser);
  EXPECT_EQ(tracker->GetActiveWebContents(), first_tab);
  EXPECT_THAT(observer.TakeChanges(), ElementsAre(first_tab));

  CloseBrowserSynchronously(first_browser);
  EXPECT_EQ(tracker->GetActiveWebContents(), nullptr);
  EXPECT_THAT(observer.TakeChanges(), ElementsAre(nullptr));
}

}  // namespace
}  // namespace ttc
