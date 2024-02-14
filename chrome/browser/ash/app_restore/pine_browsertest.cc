// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/full_restore_service.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/window_restore/pine_contents_view.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_restore/app_restore_test_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/ash_test_util.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace ash::full_restore {

// Class used to wait for multiple browser windows to be created.
class BrowsersWaiter : public BrowserListObserver {
 public:
  explicit BrowsersWaiter(int expected_count)
      : expected_count_(expected_count) {
    BrowserList::AddObserver(this);
  }
  BrowsersWaiter(const BrowsersWaiter&) = delete;
  BrowsersWaiter& operator=(const BrowsersWaiter&) = delete;
  ~BrowsersWaiter() override { BrowserList::RemoveObserver(this); }

  void Wait() { run_loop_.Run(); }

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    ++current_count_;
    if (current_count_ == expected_count_) {
      run_loop_.Quit();
    }
  }

 private:
  int current_count_ = 0;
  const int expected_count_;
  base::RunLoop run_loop_;
};

class PineBrowserTest : public InProcessBrowserTest {
 public:
  PineBrowserTest() {
    switches::SetIgnoreForestSecretKeyForTest(true);
    set_launch_browser_for_testing(nullptr);
  }
  PineBrowserTest(const PineBrowserTest&) = delete;
  PineBrowserTest& operator=(const PineBrowserTest&) = delete;
  ~PineBrowserTest() override {
    switches::SetIgnoreForestSecretKeyForTest(false);
  }

 private:
  base::test::ScopedFeatureList feature_list_{features::kForestFeature};
};

// Creates 2 browser windows that will be restored in the main test.
IN_PROC_BROWSER_TEST_F(PineBrowserTest, PRE_LaunchBrowsers) {
  Profile* profile = ProfileManager::GetActiveUserProfile();

  // Set the restore pref setting as "Ask every time". This will ensure the pine
  // dialog comes up on the next session.
  profile->GetPrefs()->SetInteger(
      prefs::kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));

  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  CreateBrowser(profile);
  CreateBrowser(profile);
  EXPECT_EQ(2u, BrowserList::GetInstance()->size());

  // Immediate save to full restore file to bypass the 2.5 second throttle.
  AppLaunchInfoSaveWaiter::Wait();
}

// Verify that with two elements in the full restore file, we enter overview on
// login. Then when we click the restore button, we restore two browsers.
IN_PROC_BROWSER_TEST_F(PineBrowserTest, LaunchBrowsers) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  // Verify we have entered overview. The grid object will be null if we failed
  // to enter overview.
  WaitForOverviewEnterAnimation();
  OverviewGrid* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);

  // Retrieve the "Restore" button from the pine dialog.
  views::Widget* pine_widget = overview_grid->pine_widget_for_testing();
  PineContentsView* pine_contents_view =
      views::AsViewClass<PineContentsView>(pine_widget->GetContentsView());
  const PillButton* restore_button =
      pine_contents_view->restore_button_for_testing();
  ASSERT_TRUE(restore_button);

  // Click the "Restore" button and verify we have launched 2 browsers.
  BrowsersWaiter waiter(/*expected_count=*/2);
  test::Click(restore_button, /*flag=*/0);
  // TODO(sammiequon): Change `BrowsersWaiter::Wait()` to return a vector of
  // browsers and verify their tabs and pages as well.
  waiter.Wait();
}

}  // namespace ash::full_restore
