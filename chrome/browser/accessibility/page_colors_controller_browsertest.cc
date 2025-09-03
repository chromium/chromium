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

using PageColorsControllerBrowserTest = InProcessBrowserTest;

// Changing the kPageColors pref should affect the state of Page Colors in the
// NativeTheme.
IN_PROC_BROWSER_TEST_F(PageColorsControllerBrowserTest, PageColorsPrefChange) {
  ui::NativeTheme::PageColors page_colors_pref =
      static_cast<ui::NativeTheme::PageColors>(
          browser()->profile()->GetPrefs()->GetInteger(prefs::kPageColors));
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  EXPECT_EQ(native_theme->GetPageColors(), page_colors_pref);

  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kPageColors, ui::NativeTheme::PageColors::kDusk);
  EXPECT_EQ(native_theme->GetPageColors(), ui::NativeTheme::PageColors::kDusk);
}

// TODO(crbug.com/40779801): This test is failing on ChromeOS - appears to be a
// result of MultiDeviceSetupClientHolder leading to multiple Prefs getting
// created. May need to look into TestingProfile.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_PageColorsCalculationWithIncreasedContrastToggle \
  DISABLED_PageColorsCalculationWithIncreasedContrastToggle
#else
#define MAYBE_PageColorsCalculationWithIncreasedContrastToggle \
  PageColorsCalculationWithIncreasedContrastToggle
#endif  // BUILDFLAG(IS_CHROMEOS)
// When `kApplyPageColorsOnlyOnIncreasedContrast` is true but the OS is not in
// increased contrast, the page colors pref shouldn't be honored.
IN_PROC_BROWSER_TEST_F(PageColorsControllerBrowserTest,
                       MAYBE_PageColorsCalculationWithIncreasedContrastToggle) {
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();

  // Should not honor the page colors pref if
  // `kApplyPageColorsOnlyOnIncreasedContrast` is true and we're not in
  // increased contrast.
  native_theme->SetPreferredContrast(
      ui::NativeTheme::PreferredContrast::kNoPreference);
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kApplyPageColorsOnlyOnIncreasedContrast, true);
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kPageColors, ui::NativeTheme::PageColors::kDusk);
  EXPECT_EQ(native_theme->GetPageColors(), ui::NativeTheme::PageColors::kOff);

  // Page colors should be honored when
  // `kApplyPageColorsOnlyOnIncreasedContrast` is true and we're in an increased
  // contrast.
#if BUILDFLAG(IS_WIN)
  native_theme->set_forced_colors(true);
#endif
  native_theme->SetPreferredContrast(ui::NativeTheme::PreferredContrast::kMore);
  native_theme->NotifyOnNativeThemeUpdated();
  EXPECT_EQ(native_theme->GetPageColors(), ui::NativeTheme::PageColors::kDusk);

  // Switching increased contrast back off should lead to us not honoring page
  // colors since `kApplyPageColorsOnlyOnIncreasedContrast` is still true.
#if BUILDFLAG(IS_WIN)
  native_theme->set_forced_colors(false);
#endif
  native_theme->SetPreferredContrast(
      ui::NativeTheme::PreferredContrast::kNoPreference);
  native_theme->NotifyOnNativeThemeUpdated();
  EXPECT_EQ(native_theme->GetPageColors(), ui::NativeTheme::PageColors::kOff);

  // Setting `kApplyPageColorsOnlyOnIncreasedContrast` to false should lead to
  // us honoring page colors.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kApplyPageColorsOnlyOnIncreasedContrast, false);
  EXPECT_EQ(native_theme->GetPageColors(), ui::NativeTheme::PageColors::kDusk);
}

#if BUILDFLAG(IS_WIN)
// Test default behavior only for Windows when High Contrast gets switched
// on and page colors 'Off'. Default should be High contrast initially, unless
// the user changes it to 'Off' while in a HC state.
IN_PROC_BROWSER_TEST_F(PageColorsControllerBrowserTest,
                       PageColorsWithHighContrast) {
  // Initially, expect kIsDefaultPageColorsOnHighContrast to be true.
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  auto* native_theme_web = ui::NativeTheme::GetInstanceForWeb();
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kIsDefaultPageColorsOnHighContrast));

  // When the OS High Contrast is turned on and Page Colors is 'Off', the used
  // Page Colors should be 'High Contrast'.
  native_theme->set_forced_colors(true);
  native_theme->SetPreferredContrast(ui::NativeTheme::PreferredContrast::kMore);
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kIsDefaultPageColorsOnHighContrast));
  EXPECT_EQ(native_theme->GetPageColors(),
            ui::NativeTheme::PageColors::kHighContrast);

  // When the OS High Contrast is turned off and Page Colors is 'High Contrast',
  // the used Page Colors should be 'Off'.
  native_theme->set_forced_colors(false);
  native_theme->SetPreferredContrast(
      ui::NativeTheme::PreferredContrast::kNoPreference);
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kIsDefaultPageColorsOnHighContrast));
  EXPECT_EQ(native_theme->GetPageColors(), ui::NativeTheme::PageColors::kOff);

  // When `kPageColors` is 'Off' while High Contrast is on,
  // `kIsDefaultPageColorsOnHighContrast` should be false and forced_colors for
  // the NativeTheme web instance should be false (i.e. page colors supersedes
  // OS for web content).
  native_theme->set_forced_colors(true);
  native_theme->SetPreferredContrast(ui::NativeTheme::PreferredContrast::kMore);
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kPageColors, ui::NativeTheme::PageColors::kOff);
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kIsDefaultPageColorsOnHighContrast));
  EXPECT_FALSE(native_theme_web->InForcedColorsMode());
  native_theme->set_forced_colors(false);
  native_theme->SetPreferredContrast(
      ui::NativeTheme::PreferredContrast::kNoPreference);

  // When the OS High Contrast is turned on next, and Page Colors is 'Off', the
  // used Page Colors should remain 'Off' since
  // `kIsDefaultPageColorsOnHighContrast` is false.
  native_theme->set_forced_colors(true);
  native_theme->SetPreferredContrast(ui::NativeTheme::PreferredContrast::kMore);
  EXPECT_FALSE(native_theme_web->InForcedColorsMode());
  EXPECT_EQ(native_theme->GetPageColors(), ui::NativeTheme::PageColors::kOff);
}
#endif  // BUILDFLAG(IS_WIN)

// When Page colors is changed, the states such as `forced_colors`,
// `should_use_dark_colors`, `preferred_color_scheme` and `preferred_contrast`
// for the NativeTheme web instance should be updated appropriately.
IN_PROC_BROWSER_TEST_F(PageColorsControllerBrowserTest,
                       PageColorsWithNativeTheme) {
  // Initially expect Page colors to be 'Off', forced colors to be false, uses
  // dark colors to be false and preferred contrast to be kNoPreference.
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  auto* native_theme_web = ui::NativeTheme::GetInstanceForWeb();
  native_theme_web->set_forced_colors(false);
  native_theme_web->set_use_dark_colors(false);
  native_theme_web->SetPreferredContrast(
      ui::NativeTheme::PreferredContrast::kNoPreference);
  EXPECT_EQ(native_theme->GetPageColors(), ui::NativeTheme::PageColors::kOff);

  // Setting Page colors to 'kHighContrast' while forced colors is false should
  // not affect any state.
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kPageColors, ui::NativeTheme::PageColors::kHighContrast);
  EXPECT_FALSE(native_theme_web->InForcedColorsMode());
  EXPECT_EQ(native_theme_web->GetPreferredContrast(),
            ui::NativeTheme::PreferredContrast::kNoPreference);

  // Changing Page colors to a dark theme (e.g. 'Dusk') should make forced
  // colors to be true, uses dark colors to be true, contrast preference to be
  // 'kMore' for the native theme web instance.
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kPageColors, ui::NativeTheme::PageColors::kDusk);
  EXPECT_EQ(native_theme->GetPageColors(), ui::NativeTheme::PageColors::kDusk);
  EXPECT_TRUE(native_theme_web->InForcedColorsMode());
  EXPECT_TRUE(native_theme_web->ShouldUseDarkColors());
  EXPECT_EQ(native_theme_web->GetPreferredContrast(),
            ui::NativeTheme::PreferredContrast::kMore);

  // Changing Page colors to be a light theme (e.g. 'White') should make forced
  // colors to be true, uses dark colors to be false, contrast preference to be
  // 'kMore' for the native theme web instance.
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kPageColors, ui::NativeTheme::PageColors::kWhite);
  EXPECT_EQ(native_theme->GetPageColors(), ui::NativeTheme::PageColors::kWhite);
  EXPECT_TRUE(native_theme_web->InForcedColorsMode());
  EXPECT_FALSE(native_theme_web->ShouldUseDarkColors());
  EXPECT_EQ(native_theme_web->GetPreferredContrast(),
            ui::NativeTheme::PreferredContrast::kMore);

  // Setting Page colors to 'Off' while in an increased contrast state should
  // make the native theme instance for web's forced colors to be false,
  // contrast preference to be 'kNone', and uses dark colors to be same with the
  // native theme instance for ui.
  native_theme->set_forced_colors(true);
  native_theme->SetPreferredContrast(ui::NativeTheme::PreferredContrast::kMore);
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kPageColors, ui::NativeTheme::PageColors::kOff);
  EXPECT_EQ(native_theme->GetPageColors(), ui::NativeTheme::PageColors::kOff);
  EXPECT_FALSE(native_theme_web->InForcedColorsMode());
  EXPECT_EQ(native_theme_web->GetPreferredContrast(),
            ui::NativeTheme::PreferredContrast::kNoPreference);
  EXPECT_EQ(native_theme_web->ShouldUseDarkColors(),
            native_theme->ShouldUseDarkColors());

  // Changing the color scheme for native theme ui while Page colors is 'Off'
  // and increased contrast is on should be respected and reflected in the
  // native theme instance for web.
  native_theme->set_preferred_color_scheme(
      ui::NativeTheme::PreferredColorScheme::kDark);
  native_theme->NotifyOnNativeThemeUpdated();
  EXPECT_EQ(native_theme_web->GetPreferredColorScheme(),
            ui::NativeTheme::PreferredColorScheme::kDark);
}
