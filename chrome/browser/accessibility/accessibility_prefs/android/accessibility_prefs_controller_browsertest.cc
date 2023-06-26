// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_prefs/android/accessibility_prefs_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_prefs.h"

class AccessibilityPrefsControllerTest : public AndroidBrowserTest {
 public:
  AccessibilityPrefsControllerTest() = default;
  ~AccessibilityPrefsControllerTest() override = default;
  AccessibilityPrefsControllerTest(const AccessibilityPrefsControllerTest&) =
      delete;
  AccessibilityPrefsControllerTest& operator=(
      const AccessibilityPrefsControllerTest&) = delete;

  bool GetAccessibilityPerformanceFilteringAllowed() {
    return g_browser_process->local_state()->GetBoolean(
        prefs::kAccessibilityPerformanceFilteringAllowed);
  }

  void SetAccessibilityPerformanceFilteringAllowed(bool allowed) {
    g_browser_process->local_state()->SetBoolean(
        prefs::kAccessibilityPerformanceFilteringAllowed, allowed);
  }
};

IN_PROC_BROWSER_TEST_F(AccessibilityPrefsControllerTest,
                       LocalStatePrefsNotRegistered) {
  EXPECT_TRUE(GetAccessibilityPerformanceFilteringAllowed());
}

IN_PROC_BROWSER_TEST_F(AccessibilityPrefsControllerTest,
                       LocalStatePrefsRegistered) {
  EXPECT_TRUE(GetAccessibilityPerformanceFilteringAllowed());
  SetAccessibilityPerformanceFilteringAllowed(false);
  EXPECT_FALSE(GetAccessibilityPerformanceFilteringAllowed());
  SetAccessibilityPerformanceFilteringAllowed(true);
  EXPECT_TRUE(GetAccessibilityPerformanceFilteringAllowed());
}
