// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/test/browser_test.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class AccessibilityLabelsBrowserTest : public InProcessBrowserTest {
 public:
  AccessibilityLabelsBrowserTest() {}

  AccessibilityLabelsBrowserTest(const AccessibilityLabelsBrowserTest&) =
      delete;
  AccessibilityLabelsBrowserTest& operator=(
      const AccessibilityLabelsBrowserTest&) = delete;

  // InProcessBrowserTest overrides:
  void TearDownOnMainThread() override { EnableScreenReader(false); }

  void EnableScreenReader(bool enabled) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Enable Chromevox.
    ash::AccessibilityManager::Get()->EnableSpokenFeedback(enabled);
#else
    // Spoof a screen reader.
    if (enabled) {
      content::BrowserAccessibilityState::GetInstance()
          ->AddAccessibilityModeFlags(ui::AXMode::kScreenReader);
    } else {
      content::BrowserAccessibilityState::GetInstance()
          ->RemoveAccessibilityModeFlags(ui::AXMode::kScreenReader);
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }
};

// Changing the kAccessibilityImageLabelsEnabled pref should affect the
// accessibility mode of a new WebContents for this profile.
IN_PROC_BROWSER_TEST_F(AccessibilityLabelsBrowserTest, NewWebContents) {
  EnableScreenReader(true);
  ui::AXMode ax_mode =
      content::BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kLabelImages));

  chrome::NewTab(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ax_mode = web_contents->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kLabelImages));

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityImageLabelsEnabled, true);

  chrome::NewTab(browser());
  web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ax_mode = web_contents->GetAccessibilityMode();
  EXPECT_TRUE(ax_mode.has_mode(ui::AXMode::kLabelImages));

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityImageLabelsEnabled, false);

  chrome::NewTab(browser());
  web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ax_mode = web_contents->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kLabelImages));
}

// Changing the kAccessibilityImageLabelsEnabled pref should affect the
// accessibility mode of existing WebContents in this profile.
IN_PROC_BROWSER_TEST_F(AccessibilityLabelsBrowserTest, ExistingWebContents) {
  EnableScreenReader(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui::AXMode ax_mode = web_contents->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kLabelImages));

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityImageLabelsEnabled, true);

  ax_mode = web_contents->GetAccessibilityMode();
  EXPECT_TRUE(ax_mode.has_mode(ui::AXMode::kLabelImages));

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityImageLabelsEnabled, false);

  ax_mode = web_contents->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kLabelImages));
}

IN_PROC_BROWSER_TEST_F(AccessibilityLabelsBrowserTest,
                       NotEnabledWithoutScreenReader) {
  EnableScreenReader(false);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui::AXMode ax_mode = web_contents->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kLabelImages));

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityImageLabelsEnabled, true);

  ax_mode = web_contents->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kLabelImages));

  // Reset state.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityImageLabelsEnabled, false);
}
