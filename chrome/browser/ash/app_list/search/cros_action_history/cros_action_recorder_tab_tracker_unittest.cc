// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/app_list/search/cros_action_history/cros_action_recorder.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

#include "base/strings/strcat.h"
#include "chrome/browser/ui/tabs/tab_activity_simulator.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window.h"

namespace app_list::test {

const std::vector<GURL>& TestUrls() {
  static base::NoDestructor<std::vector<GURL>> test_urls{
      {GURL("https://test0.example.com"), GURL("https://test1.example.com")}};
  return *test_urls;
}

// Test functions of CrOSActionRecorder.
class CrOSActionRecorderTabTrackerTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // Set the flag.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        ash::switches::kEnableCrOSActionRecorder,
        ash::switches::kCrOSActionRecorderWithoutHash);

    // Initialize CrOSActionRecorder.
    CrOSActionRecorder::GetCrosActionRecorder()->Init(profile());

    // Initialize browser.
    browser_ = CreateBrowserWithTestWindowForParams(
        Browser::CreateParams(profile(), true));
    tab_strip_model_ = browser_->tab_strip_model();
  }

  void TearDown() override {
    tab_strip_model_->CloseAllTabs();
    browser_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  CrOSActionHistoryProto GetCrOSActionHistory() {
    return CrOSActionRecorder::GetCrosActionRecorder()->actions_;
  }

  // Add a tab with url as TestUrls[i] and navigate to it immediately.
  void AddNewTab(const int i) {
    tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model_,
                                                      TestUrls()[i]);
    if (i == 0)
      tab_strip_model_->ActivateTabAt(i);
    else
      tab_activity_simulator_.SwitchToTabAt(tab_strip_model_, i);
  }

  void ExpectCrOSAction(const CrOSActionProto& action,
                        const std::string& action_name,
                        const GURL& url) {
    // Expected action_name should be action_name-url;
    const std::string expected_action_name =
        base::StrCat({action_name, "-", url.spec()});
    EXPECT_EQ(action.action_name(), expected_action_name);
  }

  TabActivitySimulator tab_activity_simulator_;
  TabStripModel* tab_strip_model_;
  std::unique_ptr<Browser> browser_;
};

// Check navigations and reactivations are logged properly.
TEST_F(CrOSActionRecorderTabTrackerTest, NavigateAndForegroundTest) {
  // Add and activate the first tab will only log a TabNavigated event.
  AddNewTab(0);
  ExpectCrOSAction(GetCrOSActionHistory().actions(0), "TabNavigated",
                   TestUrls()[0]);

  // Add and activate the second tab will log a TabNavigated event, and then a
  // TabReactivated event.
  AddNewTab(1);
  ExpectCrOSAction(GetCrOSActionHistory().actions(1), "TabNavigated",
                   TestUrls()[1]);
  ExpectCrOSAction(GetCrOSActionHistory().actions(2), "TabReactivated",
                   TestUrls()[1]);

  // Switch to tab@0 will log a TabReactivated event.
  tab_activity_simulator_.SwitchToTabAt(tab_strip_model_, 0);
  ExpectCrOSAction(GetCrOSActionHistory().actions(3), "TabReactivated",
                   TestUrls()[0]);

  // Check no extra events are logged.
  EXPECT_EQ(GetCrOSActionHistory().actions_size(), 4);
}

class CrOSActionRecorderTabTrackerPrerenderTest
    : public CrOSActionRecorderTabTrackerTest {
 public:
  CrOSActionRecorderTabTrackerPrerenderTest() = default;

  content::WebContents* GetActiveWebContents() const {
    return tab_strip_model_->GetActiveWebContents();
  }

 private:
  content::test::ScopedPrerenderFeatureList prerender_feature_list_;
};

TEST_F(CrOSActionRecorderTabTrackerPrerenderTest,
       EnsureDoNotRecordActionInPrerendering) {
  // Add and activate the first tab will only log a TabNavigated event.
  AddNewTab(0);
  ExpectCrOSAction(GetCrOSActionHistory().actions(0), "TabNavigated",
                   TestUrls()[0]);

  // Add a prerender page.
  const GURL prerendering_url("https://test0.example.com/?prerendering");
  content::RenderFrameHost* prerender_frame =
      content::WebContentsTester::For(GetActiveWebContents())
          ->AddPrerenderAndCommitNavigation(prerendering_url);
  ASSERT_NE(prerender_frame, nullptr);
  EXPECT_EQ(prerender_frame->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kPrerendering);

  // Ensure not to record the TabNavigated event in prerendering.
  EXPECT_EQ(GetCrOSActionHistory().actions_size(), 1);

  // Activate the prerendered page.
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      prerendering_url, GetActiveWebContents()->GetPrimaryMainFrame());
  EXPECT_EQ(prerender_frame->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kActive);

  // Ensure to record the TabNavigated event after the prerendered page is
  // activated.
  EXPECT_EQ(GetCrOSActionHistory().actions_size(), 2);
  ExpectCrOSAction(GetCrOSActionHistory().actions(1), "TabNavigated",
                   prerendering_url);
}

}  // namespace app_list::test
