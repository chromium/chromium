// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/browser_shutdown.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/aura/window.h"
#endif

using testing::_;
using testing::AtLeast;

class BrowserShutdownBrowserTest : public InProcessBrowserTest {
 public:
  BrowserShutdownBrowserTest() {}

  BrowserShutdownBrowserTest(const BrowserShutdownBrowserTest&) = delete;
  BrowserShutdownBrowserTest& operator=(const BrowserShutdownBrowserTest&) =
      delete;

  ~BrowserShutdownBrowserTest() override {}

 protected:
  base::HistogramTester histogram_tester_;
};

class BrowserClosingObserver : public BrowserListObserver {
 public:
  BrowserClosingObserver() {}

  BrowserClosingObserver(const BrowserClosingObserver&) = delete;
  BrowserClosingObserver& operator=(const BrowserClosingObserver&) = delete;

  MOCK_METHOD1(OnBrowserClosing, void(Browser* browser));
};

// ChromeOS has the different shutdown flow on user initiated exit process.
// See the comment for chrome::AttemptUserExit() function declaration.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Mac browser shutdown is flaky: https://crbug.com/1259913
#if BUILDFLAG(IS_MAC)
#define MAYBE_ClosingShutdownHistograms DISABLED_ClosingShutdownHistograms
#else
#define MAYBE_ClosingShutdownHistograms ClosingShutdownHistograms
#endif
IN_PROC_BROWSER_TEST_F(BrowserShutdownBrowserTest,
                       MAYBE_ClosingShutdownHistograms) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("browser://version")));
  CloseBrowserSynchronously(browser());
  // RecordShutdownMetrics() is called in the ChromeMainDelegate destructor.
  browser_shutdown::RecordShutdownMetrics();

  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
  EXPECT_EQ(browser_shutdown::GetShutdownType(),
            browser_shutdown::ShutdownType::kWindowClose);

  histogram_tester_.ExpectUniqueSample(
      "Shutdown.ShutdownType2",
      static_cast<int>(browser_shutdown::ShutdownType::kWindowClose), 1);
  histogram_tester_.ExpectTotalCount("Shutdown.WindowClose.Time2", 1);
  histogram_tester_.ExpectTotalCount("Shutdown.Renderers.Total2", 1);
}

// Flakes on Mac12.0: https://crbug.com/1259913
#if BUILDFLAG(IS_MAC)
#define MAYBE_TwoBrowsersClosingShutdownHistograms \
  DISABLED_TwoBrowsersClosingShutdownHistograms
#else
#define MAYBE_TwoBrowsersClosingShutdownHistograms \
  TwoBrowsersClosingShutdownHistograms
#endif
IN_PROC_BROWSER_TEST_F(BrowserShutdownBrowserTest,
                       MAYBE_TwoBrowsersClosingShutdownHistograms) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("browser://version")));
  Browser* browser2 = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser2, GURL("browser://help")));

  BrowserClosingObserver closing_observer;
  BrowserList::AddObserver(&closing_observer);
  EXPECT_CALL(closing_observer, OnBrowserClosing(_)).Times(AtLeast(1));

  base::RunLoop exit_waiter;
  auto subscription =
      browser_shutdown::AddAppTerminatingCallback(exit_waiter.QuitClosure());
  chrome::ExecuteCommand(browser(), IDC_EXIT);
  exit_waiter.Run();

  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
  EXPECT_EQ(browser_shutdown::GetShutdownType(),
            browser_shutdown::ShutdownType::kWindowClose);
  BrowserList::RemoveObserver(&closing_observer);

  // RecordShutdownMetrics() is called in the ChromeMainDelegate destructor.
  browser_shutdown::RecordShutdownMetrics();

  histogram_tester_.ExpectUniqueSample(
      "Shutdown.ShutdownType2",
      static_cast<int>(browser_shutdown::ShutdownType::kWindowClose), 1);
  histogram_tester_.ExpectTotalCount("Shutdown.WindowClose.Time2", 1);
  histogram_tester_.ExpectTotalCount("Shutdown.Renderers.Total2", 1);
}
#else
// On Chrome OS, the shutdown accelerator is handled by Ash and requires
// confirmation, so Chrome shouldn't try to shut down after it's been hit one
// time. Regression test for crbug.com/834092
IN_PROC_BROWSER_TEST_F(BrowserShutdownBrowserTest, ShutdownConfirmation) {
  const int modifiers = ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN;

  ui::test::EventGenerator generator(
      browser()->window()->GetNativeWindow()->GetRootWindow());

  // Press the accelerator for quitting.
  generator.PressKey(ui::VKEY_Q, modifiers);
  generator.ReleaseKey(ui::VKEY_Q, modifiers);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
