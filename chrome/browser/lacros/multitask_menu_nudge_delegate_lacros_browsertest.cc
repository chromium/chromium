// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/multitask_menu_nudge_delegate_lacros.h"

#include "base/json/values_util.h"
#include "base/test/test_future.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/prefs.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/views/test/widget_test.h"

using MultitaskMenuNudgeDelegateLacrosBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(MultitaskMenuNudgeDelegateLacrosBrowserTest, Basics) {
  auto* lacros_service = chromeos::LacrosService::Get();
  ASSERT_TRUE(lacros_service);
  ASSERT_TRUE(lacros_service->IsAvailable<crosapi::mojom::Prefs>());

  auto& prefs =
      chromeos::LacrosService::Get()->GetRemote<crosapi::mojom::Prefs>();

  base::test::TestFuture<std::optional<base::Value>> int_value_future;
  prefs->GetPref(
      crosapi::mojom::PrefPath::kMultitaskMenuNudgeClamshellShownCount,
      int_value_future.GetCallback());

  // If the pref cannot be fetched, the ash version may be too old.
  if (!int_value_future.Take().has_value()) {
    GTEST_SKIP() << "Skipping as the nudge prefs are not available in the "
                    "current version of Ash";
  }

  base::test::TestFuture<void> set_future;
  base::Time expected_time = base::Time();

  prefs->SetPref(
      crosapi::mojom::PrefPath::kMultitaskMenuNudgeClamshellShownCount,
      base::Value(2), set_future.GetCallback());
  EXPECT_TRUE(set_future.Wait());
  set_future.Clear();

  prefs->SetPref(
      crosapi::mojom::PrefPath::kMultitaskMenuNudgeClamshellLastShown,
      base::TimeToValue(expected_time), set_future.GetCallback());
  EXPECT_TRUE(set_future.Wait());

  base::test::TestFuture<std::optional<base::Value>> time_value_future;
  prefs->GetPref(
      crosapi::mojom::PrefPath::kMultitaskMenuNudgeClamshellShownCount,
      int_value_future.GetCallback());
  prefs->GetPref(
      crosapi::mojom::PrefPath::kMultitaskMenuNudgeClamshellLastShown,
      time_value_future.GetCallback());

  auto int_value = int_value_future.Take();
  ASSERT_TRUE(int_value.has_value());
  EXPECT_EQ(2, int_value.value());

  auto time_value = time_value_future.Take();
  ASSERT_TRUE(time_value.has_value());
  EXPECT_EQ(expected_time, base::ValueToTime(*time_value).value());
}

IN_PROC_BROWSER_TEST_F(MultitaskMenuNudgeDelegateLacrosBrowserTest,
                       PRE_QuickActivations) {
  // Do nothing. This is to let `Profile::IsNewProfile()` return false,
  // in QuickActivations test.
}

// Tests that if we try to reactivate a window which is fetching preferences
// data asynchronously for the multitask menu nudge, there is no crash.
// Regression test for http://crbug.com/1505602.
IN_PROC_BROWSER_TEST_F(MultitaskMenuNudgeDelegateLacrosBrowserTest,
                       QuickActivations) {
  auto* lacros_service = chromeos::LacrosService::Get();
  ASSERT_TRUE(lacros_service);
  ASSERT_TRUE(lacros_service->IsAvailable<crosapi::mojom::Prefs>());

  chromeos::MultitaskMenuNudgeController::SetSuppressNudgeForTesting(true);

  // Wait for the window to be created.
  Browser* browser1 = browser();
  aura::Window* window1 = browser1->window()->GetNativeWindow();
  ASSERT_TRUE(browser_test_util::WaitForWindowCreation(
      lacros_window_utility::GetRootWindowUniqueId(window1->GetRootWindow())));

  Browser* browser2 = CreateBrowser(browser()->profile());
  aura::Window* window2 = BrowserView::GetBrowserViewForBrowser(browser2)
                              ->frame()
                              ->GetNativeWindow();
  ASSERT_TRUE(browser_test_util::WaitForWindowCreation(
      lacros_window_utility::GetRootWindowUniqueId(window2->GetRootWindow())));
  views::test::WidgetActivationWaiter(
      BrowserView::GetBrowserViewForBrowser(browser2)->frame(), /*active=*/true)
      .Wait();
  ASSERT_TRUE(browser2->window()->IsActive());

  chromeos::MultitaskMenuNudgeController::SetSuppressNudgeForTesting(false);

  // Try activating the window to show once and send another activation request
  // to the same window before the async preferences fetching is complete. Test
  // that there is no crash here.
  browser1->window()->Activate();
  browser2->window()->Activate();
  browser1->window()->Activate();
  views::test::WidgetActivationWaiter(
      BrowserView::GetBrowserViewForBrowser(browser1)->frame(), /*active=*/true)
      .Wait();

  browser2->window()->Activate();
  views::test::WidgetActivationWaiter(
      BrowserView::GetBrowserViewForBrowser(browser2)->frame(), /*active=*/true)
      .Wait();
}
