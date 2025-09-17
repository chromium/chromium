// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/page_colors_controller.h"

#include "build/build_config.h"
#include "chrome/browser/accessibility/page_colors_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_factory.h"
#endif

class PageColorsControllerBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ui_native_theme().set_preferred_color_scheme(
        ui::NativeTheme::PreferredColorScheme::kLight);
    ui_native_theme().NotifyOnNativeThemeUpdated();
  }

  ui::NativeTheme& ui_native_theme() {
#if BUILDFLAG(IS_LINUX)
    // Match PageColorsController::OnPageColorsChanged().
    if (auto* const linux_ui_theme = ui::GetDefaultLinuxUiTheme()) {
      if (auto* const linux_native_theme = linux_ui_theme->GetNativeTheme()) {
        return *linux_native_theme;
      }
    }
#endif
    return *ui::NativeTheme::GetInstanceForNativeUi();
  }
};

// Changing the requested page colors should affect the web theme.
IN_PROC_BROWSER_TEST_F(PageColorsControllerBrowserTest, PageColorsChange) {
  PageColorsControllerFactory::GetForProfile(browser()->profile())
      ->SetRequestedPageColors(PageColors::kDusk);
  EXPECT_EQ(ui::NativeTheme::GetInstanceForWeb()->page_colors(),
            ui::NativeTheme::PageColors::kDusk);
}

IN_PROC_BROWSER_TEST_F(PageColorsControllerBrowserTest,
                       ApplyPageColorsOnIncreasedContrast) {
  auto* const native_theme = ui::NativeTheme::GetInstanceForWeb();

  // When `kApplyPageColorsOnlyOnIncreasedContrast` is true but the OS is not in
  // increased contrast mode, there should be no forced colors.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kApplyPageColorsOnlyOnIncreasedContrast, true);
  PageColorsControllerFactory::GetForProfile(browser()->profile())
      ->SetRequestedPageColors(PageColors::kDusk);
  EXPECT_EQ(native_theme->page_colors(), ui::NativeTheme::PageColors::kOff);

  // Once the OS is in increased contrast mode, the requested page colors should
  // be honored.
  ui_native_theme().SetPreferredContrast(
      ui::NativeTheme::PreferredContrast::kMore);
  EXPECT_EQ(native_theme->page_colors(), ui::NativeTheme::PageColors::kDusk);

  // Switching increased contrast back off should turn forced colors back off
  // since `kApplyPageColorsOnlyOnIncreasedContrast` is still true.
  ui_native_theme().SetPreferredContrast(
      ui::NativeTheme::PreferredContrast::kNoPreference);
  EXPECT_EQ(native_theme->page_colors(), ui::NativeTheme::PageColors::kOff);

  // Setting `kApplyPageColorsOnlyOnIncreasedContrast` to false should lead to
  // honoring the requested page colors.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kApplyPageColorsOnlyOnIncreasedContrast, false);
  EXPECT_EQ(native_theme->page_colors(), ui::NativeTheme::PageColors::kDusk);
}

// When page colors change, not only the web theme's forced colors, but also its
// preferred color scheme and preferred contrast should change.
IN_PROC_BROWSER_TEST_F(PageColorsControllerBrowserTest,
                       OtherNativeThemePropertiesSet) {
  auto* const native_theme = ui::NativeTheme::GetInstanceForWeb();
  auto* const page_colors =
      PageColorsControllerFactory::GetForProfile(browser()->profile());

  // The web theme should be in a default state.
  EXPECT_EQ(native_theme->page_colors(), ui::NativeTheme::PageColors::kOff);
  EXPECT_EQ(native_theme->preferred_color_scheme(),
            ui::NativeTheme::PreferredColorScheme::kLight);
  EXPECT_EQ(native_theme->preferred_contrast(),
            ui::NativeTheme::PreferredContrast::kNoPreference);

  // Changing page colors to White should be reflected in the web theme's
  // contrast and color scheme.
  page_colors->SetRequestedPageColors(PageColors::kWhite);
  EXPECT_EQ(native_theme->page_colors(), ui::NativeTheme::PageColors::kWhite);
  EXPECT_EQ(native_theme->preferred_color_scheme(),
            ui::NativeTheme::PreferredColorScheme::kLight);
  EXPECT_EQ(native_theme->preferred_contrast(),
            ui::NativeTheme::PreferredContrast::kMore);

  // Changing page colors to Dusk should similarly be reflected.
  page_colors->SetRequestedPageColors(PageColors::kDusk);
  EXPECT_EQ(native_theme->page_colors(), ui::NativeTheme::PageColors::kDusk);
  EXPECT_EQ(native_theme->preferred_color_scheme(),
            ui::NativeTheme::PreferredColorScheme::kDark);
  EXPECT_EQ(native_theme->preferred_contrast(),
            ui::NativeTheme::PreferredContrast::kMore);

  // Changing the UI theme to high contrast should not overwrite the web theme
  // page colors.
  ui_native_theme().SetPreferredContrast(
      ui::NativeTheme::PreferredContrast::kMore);
  EXPECT_EQ(native_theme->page_colors(), ui::NativeTheme::PageColors::kDusk);
  EXPECT_EQ(native_theme->preferred_color_scheme(),
            ui::NativeTheme::PreferredColorScheme::kDark);
  EXPECT_EQ(native_theme->preferred_contrast(),
            ui::NativeTheme::PreferredContrast::kMore);

  // Changing the page colors to Off should reduce the web theme's preferred
  // contrast.
  page_colors->SetRequestedPageColors(PageColors::kOff);
  EXPECT_EQ(native_theme->page_colors(), ui::NativeTheme::PageColors::kOff);
  EXPECT_EQ(native_theme->preferred_color_scheme(),
            ui::NativeTheme::PreferredColorScheme::kLight);
  EXPECT_EQ(native_theme->preferred_contrast(),
            ui::NativeTheme::PreferredContrast::kNoPreference);

  // Changing the UI color scheme should be reflected in the web theme while
  // page colors are Off.
  ui_native_theme().set_preferred_color_scheme(
      ui::NativeTheme::PreferredColorScheme::kDark);
  ui_native_theme().NotifyOnNativeThemeUpdated();
  EXPECT_EQ(native_theme->page_colors(), ui::NativeTheme::PageColors::kOff);
  EXPECT_EQ(native_theme->preferred_color_scheme(),
            ui::NativeTheme::PreferredColorScheme::kDark);
  EXPECT_EQ(native_theme->preferred_contrast(),
            ui::NativeTheme::PreferredContrast::kNoPreference);

  // Unsetting the preferred page colors should cause the web theme to reflect
  // the high contrast state of the native theme.
  page_colors->SetRequestedPageColors(PageColors::kNoPreference);
  EXPECT_EQ(native_theme->page_colors(), ui::NativeTheme::PageColors::kOff);
  EXPECT_EQ(native_theme->preferred_color_scheme(),
            ui::NativeTheme::PreferredColorScheme::kDark);
  EXPECT_EQ(native_theme->preferred_contrast(),
            ui::NativeTheme::PreferredContrast::kMore);
}
