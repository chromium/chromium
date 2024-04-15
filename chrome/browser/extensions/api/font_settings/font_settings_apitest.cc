// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Font Settings Extension API browser tests.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace extensions {

// Test of extension API on a standard profile.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, FontSettings) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetString(prefs::kWebKitStandardFontFamilyKorean, "Tahoma");
  prefs->SetString(prefs::kWebKitSansSerifFontFamily, "Arial");
  prefs->SetInteger(prefs::kWebKitDefaultFontSize, 16);
  prefs->SetInteger(prefs::kWebKitDefaultFixedFontSize, 14);
  prefs->SetInteger(prefs::kWebKitMinimumFontSize, 8);

  EXPECT_TRUE(RunExtensionTest("font_settings/standard")) << message_;
}

// Test of extension API in incognito split mode.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, FontSettingsIncognito) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetString(prefs::kWebKitStandardFontFamilyKorean, "Tahoma");
  prefs->SetString(prefs::kWebKitSansSerifFontFamily, "Arial");
  prefs->SetInteger(prefs::kWebKitDefaultFontSize, 16);

  EXPECT_TRUE(RunExtensionTest(
      "font_settings/incognito",
      {.extension_url = "launch.html", .open_in_incognito = true},
      {.allow_in_incognito = true}));
}

// Test the list of generic font families.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, FontSettingsGenericFamilies) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  // TODO(crbug.com/40187445): Test generic font families added to CSS Fonts
  // Module Level 4.
  prefs->SetString(prefs::kWebKitStandardFontFamily, "default_standard");
  prefs->SetString(prefs::kWebKitSansSerifFontFamily, "default_sansserif");
  prefs->SetString(prefs::kWebKitSerifFontFamily, "default_serif");
  prefs->SetString(prefs::kWebKitCursiveFontFamily, "default_cursive");
  prefs->SetString(prefs::kWebKitFantasyFontFamily, "default_fantasy");
  prefs->SetString(prefs::kWebKitFixedFontFamily, "default_fixed");
  prefs->SetString(prefs::kWebKitMathFontFamily, "default_math");
  EXPECT_TRUE(RunExtensionTest("font_settings/generic_families")) << message_;
}

}  // namespace extensions
