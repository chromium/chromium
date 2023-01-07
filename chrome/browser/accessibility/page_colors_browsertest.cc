// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/native_theme/native_theme.h"

class PageColorsBrowserTest : public InProcessBrowserTest {
 public:
  PageColorsBrowserTest() = default;

  PageColorsBrowserTest(const PageColorsBrowserTest&) = delete;
  PageColorsBrowserTest& operator=(const PageColorsBrowserTest&) = delete;
};

// Changing the kPageColors pref should affect the state of Page Colors in the
// NativeTheme.
IN_PROC_BROWSER_TEST_F(PageColorsBrowserTest, PageColorsPrefChange) {
  ui::NativeTheme::PageColors page_colors_pref =
      static_cast<ui::NativeTheme::PageColors>(
          browser()->profile()->GetPrefs()->GetInteger(prefs::kPageColors));
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  ui::NativeTheme::PageColors page_colors_state = native_theme->GetPageColors();
  EXPECT_EQ(page_colors_state, page_colors_pref);

  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kPageColors, ui::NativeTheme::PageColors::kDusk);
// Turning on increased contrast for Windows because the initial state of
// kApplyPageColorsOnlyOnIncreasedContrast on Windows is true.
#if BUILDFLAG(IS_WIN)
  native_theme->SetPreferredContrast(ui::NativeTheme::PreferredContrast::kMore);
#endif
  page_colors_state = native_theme->GetPageColors();
  EXPECT_EQ(page_colors_state, ui::NativeTheme::PageColors::kDusk);
}

// TODO(crbug.com/1231644): This test is failing on ChromeOS - appears to be a
// result of MultiDeviceSetupClientHolder leading to multiple Prefs getting
// created. May need to look into TestingProfile.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_PageColorsCalculationWithIncreasedContrastToggle \
  DISABLED_PageColorsCalculationWithIncreasedContrastToggle
#else
#define MAYBE_PageColorsCalculationWithIncreasedContrastToggle \
  PageColorsCalculationWithIncreasedContrastToggle
#endif  // BUILDFLAG(IS_CHROMEOS)
// When kApplyPageColorsOnlyOnIncreasedContrast is true but the OS is not in
// increased contrast, the page colors pref shouldn't be honored.
IN_PROC_BROWSER_TEST_F(PageColorsBrowserTest,
                       MAYBE_PageColorsCalculationWithIncreasedContrastToggle) {
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  ui::NativeTheme::PageColors page_colors_pref =
      static_cast<ui::NativeTheme::PageColors>(
          browser()->profile()->GetPrefs()->GetInteger(prefs::kPageColors));

  // Should not honor the page colors pref if
  // kApplyPageColorsOnlyOnIncreasedContrast is true and we're not in increased
  // contrast.
  EXPECT_FALSE(native_theme->UserHasContrastPreference());
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kApplyPageColorsOnlyOnIncreasedContrast, true);
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kPageColors, ui::NativeTheme::PageColors::kDusk);
  page_colors_pref = static_cast<ui::NativeTheme::PageColors>(
      browser()->profile()->GetPrefs()->GetInteger(prefs::kPageColors));
  ui::NativeTheme::PageColors page_colors_state = native_theme->GetPageColors();
  EXPECT_EQ(page_colors_pref, ui::NativeTheme::PageColors::kDusk);
  EXPECT_EQ(page_colors_state, ui::NativeTheme::PageColors::kOff);

  // Page colors should be honored when kApplyPageColorsOnlyOnIncreasedContrast
  // is true and we're in an increased contrast.
  native_theme->SetPreferredContrast(ui::NativeTheme::PreferredContrast::kMore);
  native_theme->NotifyOnNativeThemeUpdated();
  page_colors_state = native_theme->GetPageColors();
  EXPECT_EQ(page_colors_pref, page_colors_state);

  // Switching increased contrast back off should lead to us not honoring page
  // colors since kApplyPageColorsOnlyOnIncreasedContrast is still true.
  native_theme->SetPreferredContrast(
      ui::NativeTheme::PreferredContrast::kNoPreference);
  native_theme->NotifyOnNativeThemeUpdated();
  page_colors_state = native_theme->GetPageColors();
  EXPECT_EQ(page_colors_pref, ui::NativeTheme::PageColors::kDusk);
  EXPECT_EQ(page_colors_state, ui::NativeTheme::PageColors::kOff);

  // Setting kApplyPageColorsOnlyOnIncreasedContrast to false should lead to us
  // honoring page colors.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kApplyPageColorsOnlyOnIncreasedContrast, false);
  page_colors_state = native_theme->GetPageColors();
  EXPECT_EQ(page_colors_pref, page_colors_state);
}

#if BUILDFLAG(IS_WIN)
// Test default behaviour only for Windows when High Contrast gets switched
// on and page colors 'Off'. Default should be High contrast initially, unless
// the user changes it to 'Off' while in a HC state.
IN_PROC_BROWSER_TEST_F(PageColorsBrowserTest, PageColorsWithHighContrast) {
  // Initially, expect kIsDefaultPageColorsOnHighContrast to be true.
  ui::NativeTheme::PageColors page_colors_pref =
      static_cast<ui::NativeTheme::PageColors>(
          browser()->profile()->GetPrefs()->GetInteger(prefs::kPageColors));
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  ui::NativeTheme::PageColors page_colors_state = native_theme->GetPageColors();
  bool is_default_page_colors_on_high_contrast =
      browser()->profile()->GetPrefs()->GetBoolean(
          prefs::kIsDefaultPageColorsOnHighContrast);
  EXPECT_EQ(page_colors_state, page_colors_pref);
  EXPECT_TRUE(is_default_page_colors_on_high_contrast);

  // When the OS High Contrast is turned on and Page Colors is 'Off', the used
  // Page Colors should be 'High Contrast'.
  native_theme->SetPreferredContrast(ui::NativeTheme::PreferredContrast::kMore);
  page_colors_pref = static_cast<ui::NativeTheme::PageColors>(
      browser()->profile()->GetPrefs()->GetInteger(prefs::kPageColors));
  page_colors_state = native_theme->GetPageColors();
  is_default_page_colors_on_high_contrast =
      browser()->profile()->GetPrefs()->GetBoolean(
          prefs::kIsDefaultPageColorsOnHighContrast);
  EXPECT_TRUE(is_default_page_colors_on_high_contrast);
  EXPECT_EQ(page_colors_pref, ui::NativeTheme::PageColors::kHighContrast);
  EXPECT_EQ(page_colors_state, page_colors_pref);

  // When the OS High Contrast is turned off and Page Colors is 'High Contrast',
  // the used Page Colors should be 'Off'.
  native_theme->SetPreferredContrast(
      ui::NativeTheme::PreferredContrast::kNoPreference);
  page_colors_pref = static_cast<ui::NativeTheme::PageColors>(
      browser()->profile()->GetPrefs()->GetInteger(prefs::kPageColors));
  page_colors_state = native_theme->GetPageColors();
  is_default_page_colors_on_high_contrast =
      browser()->profile()->GetPrefs()->GetBoolean(
          prefs::kIsDefaultPageColorsOnHighContrast);
  EXPECT_TRUE(is_default_page_colors_on_high_contrast);
  EXPECT_EQ(page_colors_pref, ui::NativeTheme::PageColors::kOff);
  EXPECT_EQ(page_colors_state, page_colors_pref);

  // When kPageColors is 'Off' while High Contrast is on,
  // kIsDefaultPageColorsOnHighContrast should be false.
  native_theme->SetPreferredContrast(ui::NativeTheme::PreferredContrast::kMore);
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kPageColors, ui::NativeTheme::PageColors::kOff);
  is_default_page_colors_on_high_contrast =
      browser()->profile()->GetPrefs()->GetBoolean(
          prefs::kIsDefaultPageColorsOnHighContrast);
  EXPECT_FALSE(is_default_page_colors_on_high_contrast);
  native_theme->SetPreferredContrast(
      ui::NativeTheme::PreferredContrast::kNoPreference);

  // When the OS High Contrast is turned on next, and Page Colors is 'Off', the
  // used Page Colors should be 'Off'.
  native_theme->SetPreferredContrast(ui::NativeTheme::PreferredContrast::kMore);
  page_colors_pref = static_cast<ui::NativeTheme::PageColors>(
      browser()->profile()->GetPrefs()->GetInteger(prefs::kPageColors));
  page_colors_state = native_theme->GetPageColors();
  EXPECT_EQ(page_colors_pref, ui::NativeTheme::PageColors::kOff);
  EXPECT_EQ(page_colors_state, page_colors_pref);
}
#endif  // BUILDFLAG(IS_WIN)
