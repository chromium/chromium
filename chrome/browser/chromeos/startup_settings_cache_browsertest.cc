// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/startup_settings_cache.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/test/browser_test_utils.h"

namespace {

const char kSpanishLocale[] = "es";

using StartupSettingsCacheTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(StartupSettingsCacheTest, PRE_RendererLocale) {
  // Simulate the user changing the browser's language setting. The setting
  // takes effect after restart.
  browser()->profile()->GetPrefs()->SetString(
      language::prefs::kApplicationLocale, kSpanishLocale);
}

// Regression test for the "Choose File" button not being localized.
// https://crbug.com/510455
IN_PROC_BROWSER_TEST_F(StartupSettingsCacheTest, RendererLocale) {
  EXPECT_EQ(kSpanishLocale, browser()->profile()->GetPrefs()->GetString(
                                language::prefs::kApplicationLocale));

  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();

  // Load a page with a "Choose File" form button.
  const char kHTML[] =
      "<html><body><form><input type='file'></form></body></html>";
  GURL url(std::string("data:text/html,") + kHTML);
  ui_test_utils::NavigateToURL(browser(), url);

  // The localized button label in the renderer is in Spanish.
  WaitForAccessibilityTreeToContainNodeWithName(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "Seleccionar archivo");
}

}  // namespace
