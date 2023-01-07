// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/notreached.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/tab_manager_resource_coordinator_signal_observer.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/sessions/tab_loader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::MockNavigationHandle;
using content::NavigationThrottle;
using content::WebContents;
using content::WebContentsTester;

namespace resource_coordinator {

class TabManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  TabManagerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    // Start with a non-zero time.
    task_environment()->FastForwardBy(base::Seconds(42));
  }

  std::unique_ptr<WebContents> CreateWebContents() {
    std::unique_ptr<WebContents> web_contents = CreateTestWebContents();
    ResourceCoordinatorTabHelper::CreateForWebContents(web_contents.get());
    // Commit an URL to allow discarding.
    content::WebContentsTester::For(web_contents.get())
        ->NavigateAndCommit(GURL("https://www.example.com"));

    return web_contents;
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    tab_manager_ = g_browser_process->GetTabManager();
  }

  bool IsTabDiscarded(content::WebContents* content) {
    return TabLifecycleUnitExternal::FromWebContents(content)->IsDiscarded();
  }

 protected:
  raw_ptr<TabManager> tab_manager_ = nullptr;
};

// TODO(georgesak): Add tests for protection to tabs with form input and
// playing audio;

TEST_F(TabManagerTest, IsInternalPage) {
  EXPECT_TRUE(TabManager::IsInternalPage(GURL(chrome::kChromeUIDownloadsURL)));
  EXPECT_TRUE(TabManager::IsInternalPage(GURL(chrome::kChromeUIHistoryURL)));
  EXPECT_TRUE(TabManager::IsInternalPage(GURL(chrome::kChromeUINewTabURL)));
  EXPECT_TRUE(TabManager::IsInternalPage(GURL(chrome::kChromeUISettingsURL)));

// Debugging URLs are not included.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_FALSE(TabManager::IsInternalPage(GURL(chrome::kChromeUIDiscardsURL)));
#endif
  EXPECT_FALSE(
      TabManager::IsInternalPage(GURL(chrome::kChromeUINetInternalsURL)));

  // Prefix matches are included.
  GURL::Replacements replace_fake_path;
  replace_fake_path.SetPathStr("fakeSetting");
  EXPECT_TRUE(TabManager::IsInternalPage(
      GURL(chrome::kChromeUISettingsURL).ReplaceComponents(replace_fake_path)));
}

// Data race on Linux. http://crbug.com/787842
// Flaky on Mac and Windows: https://crbug.com/995682
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
#define MAYBE_DiscardTabWithNonVisibleTabs DISABLED_DiscardTabWithNonVisibleTabs
#else
#define MAYBE_DiscardTabWithNonVisibleTabs DiscardTabWithNonVisibleTabs
#endif

// Verify that:
// - On ChromeOS, DiscardTab can discard every non-visible tab, but cannot
//   discard a visible tab.
// - On other platforms, DiscardTab can discard every tab that is not active in
//   its tab strip.
TEST_F(TabManagerTest, MAYBE_DiscardTabWithNonVisibleTabs) {
  // Create 2 tab strips. Simulate the second tab strip being hidden by hiding
  // its active tab.
  auto window1 = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params1(profile(), true);
  params1.type = Browser::TYPE_NORMAL;
  params1.window = window1.get();
  auto browser1 = std::unique_ptr<Browser>(Browser::Create(params1));
  TabStripModel* tab_strip1 = browser1->tab_strip_model();
  tab_strip1->AppendWebContents(CreateWebContents(), true);
  tab_strip1->AppendWebContents(CreateWebContents(), false);
  tab_strip1->GetWebContentsAt(0)->WasShown();
  tab_strip1->GetWebContentsAt(1)->WasHidden();

  auto window2 = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params2(profile(), true);
  params2.type = Browser::TYPE_NORMAL;
  params2.window = window2.get();
  auto browser2 = std::unique_ptr<Browser>(Browser::Create(params1));
  TabStripModel* tab_strip2 = browser2->tab_strip_model();
  tab_strip2->AppendWebContents(CreateWebContents(), true);
  tab_strip2->AppendWebContents(CreateWebContents(), false);
  tab_strip2->GetWebContentsAt(0)->WasHidden();
  tab_strip2->GetWebContentsAt(1)->WasHidden();

  // Advance time enough that the tabs are urgent discardable.
  task_environment()->AdvanceClock(kBackgroundUrgentProtectionTime);

  for (int i = 0; i < 4; ++i)
    tab_manager_->DiscardTab(LifecycleUnitDiscardReason::URGENT);

  // Active tab in a visible window should not be discarded.
  EXPECT_FALSE(IsTabDiscarded(tab_strip1->GetWebContentsAt(0)));

  // Non-active tabs should be discarded.
  EXPECT_TRUE(IsTabDiscarded(tab_strip1->GetWebContentsAt(1)));
  EXPECT_TRUE(IsTabDiscarded(tab_strip2->GetWebContentsAt(1)));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On ChromeOS, a non-visible tab should be discarded even if it's active in
  // its tab strip.
  EXPECT_TRUE(IsTabDiscarded(tab_strip2->GetWebContentsAt(0)));
#else
  // On other platforms, an active tab is never discarded, even if it's not
  // visible.
  EXPECT_FALSE(IsTabDiscarded(tab_strip2->GetWebContentsAt(0)));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Tabs with a committed URL must be closed explicitly to avoid DCHECK errors.
  tab_strip1->CloseAllTabs();
  tab_strip2->CloseAllTabs();
}

TEST_F(TabManagerTest, GetSortedLifecycleUnits) {
  auto window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window.get();
  auto browser = std::unique_ptr<Browser>(Browser::Create(params));
  TabStripModel* tab_strip = browser->tab_strip_model();

  const int num_of_tabs_to_test = 20;
  for (int i = 0; i < num_of_tabs_to_test; ++i) {
    task_environment()->FastForwardBy(base::Seconds(10));
    tab_strip->AppendWebContents(CreateWebContents(), /*foreground=*/true);
  }

  LifecycleUnitVector lifecycle_units = tab_manager_->GetSortedLifecycleUnits();
  EXPECT_EQ(lifecycle_units.size(), static_cast<size_t>(num_of_tabs_to_test));

  // Check that the lifecycle_units are sorted with ascending importance.
  for (int i = 0; i < num_of_tabs_to_test - 1; ++i) {
    EXPECT_TRUE(lifecycle_units[i]->GetSortKey() <
                lifecycle_units[i + 1]->GetSortKey());
  }

  tab_strip->CloseAllTabs();
}

}  // namespace resource_coordinator
