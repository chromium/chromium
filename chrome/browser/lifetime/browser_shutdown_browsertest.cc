// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
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
  ~BrowserShutdownBrowserTest() override {}

 protected:
  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserShutdownBrowserTest);
};

class BrowserClosingObserver : public BrowserListObserver {
 public:
  BrowserClosingObserver() {}
  MOCK_METHOD1(OnBrowserClosing, void(Browser* browser));

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserClosingObserver);
};

// ChromeOS has the different shutdown flow on user initiated exit process.
// See the comment for chrome::AttemptUserExit() function declaration.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(BrowserShutdownBrowserTest,
                       PRE_TwoBrowsersClosingShutdownHistograms) {
  ui_test_utils::NavigateToURL(browser(), GURL("browser://version"));
  Browser* browser2 = CreateBrowser(browser()->profile());
  ui_test_utils::NavigateToURL(browser2, GURL("browser://help"));

  BrowserClosingObserver closing_observer;
  BrowserList::AddObserver(&closing_observer);
  EXPECT_CALL(closing_observer, OnBrowserClosing(_)).Times(AtLeast(1));

  content::WindowedNotificationObserver terminate_observer(
      chrome::NOTIFICATION_APP_TERMINATING,
      content::NotificationService::AllSources());
  chrome::ExecuteCommand(browser(), IDC_EXIT);
  terminate_observer.Wait();

  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
  EXPECT_EQ(browser_shutdown::GetShutdownType(),
            browser_shutdown::ShutdownType::kWindowClose);
  BrowserList::RemoveObserver(&closing_observer);
}

IN_PROC_BROWSER_TEST_F(BrowserShutdownBrowserTest,
                       TwoBrowsersClosingShutdownHistograms) {
  histogram_tester_.ExpectUniqueSample(
      "Shutdown.ShutdownType",
      static_cast<int>(browser_shutdown::ShutdownType::kWindowClose), 1);
  histogram_tester_.ExpectTotalCount("Shutdown.renderers.total", 1);
  histogram_tester_.ExpectTotalCount("Shutdown.window_close.time2", 1);
  histogram_tester_.ExpectTotalCount("Shutdown.window_close.time_per_process",
                                     1);
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
