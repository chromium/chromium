// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"

namespace {

using BrowserActivationTest = InProcessBrowserTest;

// Verifies that the Lacros browser can activate one of its own windows.
// Regression test for https://crbug.com/1172448
IN_PROC_BROWSER_TEST_F(BrowserActivationTest, LacrosWindowActivation) {
  Browser* first_browser = browser();

  // Ensure the initial window is active.
  ui_test_utils::BrowserActivationWaiter waiter1(first_browser);
  waiter1.WaitForActivation();
  EXPECT_TRUE(first_browser->window()->IsActive());

  // Create a second browser.
  Browser* second_browser = CreateBrowser(first_browser->profile());
  ui_test_utils::BrowserActivationWaiter waiter2(second_browser);
  waiter2.WaitForActivation();
  EXPECT_TRUE(second_browser->window()->IsActive());

  // Activate the first browser.
  ui_test_utils::BrowserActivationWaiter waiter3(first_browser);
  first_browser->window()->Activate();
  waiter3.WaitForActivation();

  // First browser is now active again.
  EXPECT_TRUE(first_browser->window()->IsActive());
}

}  // namespace
