// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_features.h"

#include <string>

#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/test/scoped_os_info_override_win.h"
#endif

namespace default_browser {

TEST(DefaultBrowserFeaturesTest, IsDefaultBrowserPromptSurfacesEnabled) {
#if BUILDFLAG(IS_WIN)
  base::test::ScopedOSInfoOverride win11_os(
      base::test::ScopedOSInfoOverride::Type::kWin11Pro);
#endif
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kDefaultBrowserPromptSurfaces);
#if BUILDFLAG(IS_WIN)
    EXPECT_TRUE(IsDefaultBrowserPromptSurfacesEnabled());
#else
    EXPECT_FALSE(IsDefaultBrowserPromptSurfacesEnabled());
#endif
  }
  {
    // The sticky modal experiment alone does not enable prompt surfaces.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        /*enabled_features=*/{kDefaultBrowserStickyModal},
        /*disabled_features=*/{kDefaultBrowserPromptSurfaces});
    EXPECT_FALSE(IsDefaultBrowserPromptSurfacesEnabled());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {}, {kDefaultBrowserPromptSurfaces, kDefaultBrowserStickyModal});
    EXPECT_FALSE(IsDefaultBrowserPromptSurfacesEnabled());
  }
#if BUILDFLAG(IS_WIN)
  {
    base::test::ScopedOSInfoOverride win10_os(
        base::test::ScopedOSInfoOverride::Type::kWin10Pro);
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kDefaultBrowserPromptSurfaces);
    EXPECT_FALSE(IsDefaultBrowserPromptSurfacesEnabled());
  }
#endif
}

TEST(DefaultBrowserFeaturesTest, IsDefaultBrowserModalSticky) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{kDefaultBrowserStickyModal, {{"IsSticky", "true"}}}}, {});
#if BUILDFLAG(IS_WIN)
    EXPECT_TRUE(IsDefaultBrowserModalSticky());
#else
    EXPECT_FALSE(IsDefaultBrowserModalSticky());
#endif
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{kDefaultBrowserStickyModal, {{"IsSticky", "true"}}},
         {kDefaultBrowserSetterSelection, {{"setter_option", "visual_guide"}}}},
        {});
    EXPECT_FALSE(IsDefaultBrowserModalSticky());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{kDefaultBrowserStickyModal, {{"IsSticky", "false"}}}}, {});
    EXPECT_FALSE(IsDefaultBrowserModalSticky());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kDefaultBrowserStickyModal);
    EXPECT_FALSE(IsDefaultBrowserModalSticky());
  }
}

TEST(DefaultBrowserFeaturesTest, IsDefaultBrowserChangedOsNotificationEnabled) {
#if BUILDFLAG(IS_WIN)
  base::test::ScopedOSInfoOverride win11_os(
      base::test::ScopedOSInfoOverride::Type::kWin11Pro);
#endif
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kDefaultBrowserChangedOsNotification);
#if BUILDFLAG(IS_WIN)
    EXPECT_TRUE(IsDefaultBrowserChangedOsNotificationEnabled());
#else
    EXPECT_FALSE(IsDefaultBrowserChangedOsNotificationEnabled());
#endif
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kDefaultBrowserChangedOsNotification);
    EXPECT_FALSE(IsDefaultBrowserChangedOsNotificationEnabled());
  }
#if BUILDFLAG(IS_WIN)
  {
    base::test::ScopedOSInfoOverride win10_os(
        base::test::ScopedOSInfoOverride::Type::kWin10Pro);
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kDefaultBrowserChangedOsNotification);
    EXPECT_FALSE(IsDefaultBrowserChangedOsNotificationEnabled());
  }
#endif
}

TEST(DefaultBrowserFeaturesTest,
     IsPerformDefaultBrowserCheckValidationsEnabled) {
#if BUILDFLAG(IS_WIN)
  base::test::ScopedOSInfoOverride win11_os(
      base::test::ScopedOSInfoOverride::Type::kWin11Pro);
#endif
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kPerformDefaultBrowserCheckValidations);
#if BUILDFLAG(IS_WIN)
    EXPECT_TRUE(IsPerformDefaultBrowserCheckValidationsEnabled());
#else
    EXPECT_FALSE(IsPerformDefaultBrowserCheckValidationsEnabled());
#endif
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kPerformDefaultBrowserCheckValidations);
    EXPECT_FALSE(IsPerformDefaultBrowserCheckValidationsEnabled());
  }
#if BUILDFLAG(IS_WIN)
  {
    base::test::ScopedOSInfoOverride win10_os(
        base::test::ScopedOSInfoOverride::Type::kWin10Pro);
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kPerformDefaultBrowserCheckValidations);
    EXPECT_FALSE(IsPerformDefaultBrowserCheckValidationsEnabled());
  }
#endif
}

// Tests for GetDefaultBrowserPromptSurface behavior with prompt surfaces and
// setter selection.
TEST(DefaultBrowserFeaturesTest, GetDefaultBrowserPromptSurface) {
#if BUILDFLAG(IS_WIN)
  base::test::ScopedOSInfoOverride win11_os(
      base::test::ScopedOSInfoOverride::Type::kWin11Pro);
#endif
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{kDefaultBrowserPromptSurfaces,
          {{"prompt_surface", "modal_dialog_with_settings_illustration"}}},
         {kDefaultBrowserSetterSelection, {{"setter_option", "visual_guide"}}}},
        {});
#if BUILDFLAG(IS_WIN)
    EXPECT_EQ(
        GetDefaultBrowserPromptSurface(),
        DefaultBrowserPromptSurface::kModalDialogWithoutSettingsIllustration);
#else
    EXPECT_EQ(GetDefaultBrowserPromptSurface(),
              DefaultBrowserPromptSurface::kInfobar);
#endif
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{kDefaultBrowserPromptSurfaces,
          {{"prompt_surface", "modal_dialog_with_settings_illustration"}}},
         {kDefaultBrowserSetterSelection,
          {{"setter_option", "shell_integration"}}}},
        {});
#if BUILDFLAG(IS_WIN)
    DefaultBrowserPromptSurface surface = GetDefaultBrowserPromptSurface();
    EXPECT_TRUE(
        surface ==
            DefaultBrowserPromptSurface::kModalDialogWithSettingsIllustration ||
        surface == DefaultBrowserPromptSurface::
                       kModalDialogWithoutSettingsIllustration);
#else
    EXPECT_EQ(GetDefaultBrowserPromptSurface(),
              DefaultBrowserPromptSurface::kInfobar);
#endif
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{kDefaultBrowserPromptSurfaces,
          {{"prompt_surface", "modal_dialog_without_settings_illustration"}}}},
        {});
#if BUILDFLAG(IS_WIN)
    EXPECT_EQ(
        GetDefaultBrowserPromptSurface(),
        DefaultBrowserPromptSurface::kModalDialogWithoutSettingsIllustration);
#else
    EXPECT_EQ(GetDefaultBrowserPromptSurface(),
              DefaultBrowserPromptSurface::kInfobar);
#endif
  }

  {
    // The sticky modal experiment alone does not select a modal surface.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{kDefaultBrowserStickyModal,
          {{"IsSticky", "true"}, {"WithSettingsIllustration", "false"}}}},
        {kDefaultBrowserPromptSurfaces});
    EXPECT_EQ(GetDefaultBrowserPromptSurface(),
              DefaultBrowserPromptSurface::kInfobar);
  }

  {
    // When a modal surface is selected, the sticky modal experiment determines
    // whether the settings illustration is used.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{kDefaultBrowserPromptSurfaces,
          {{"prompt_surface", "modal_dialog_with_settings_illustration"}}},
         {kDefaultBrowserStickyModal,
          {{"IsSticky", "true"}, {"WithSettingsIllustration", "false"}}}},
        {});
#if BUILDFLAG(IS_WIN)
    EXPECT_EQ(
        GetDefaultBrowserPromptSurface(),
        DefaultBrowserPromptSurface::kModalDialogWithoutSettingsIllustration);
#else
    EXPECT_EQ(GetDefaultBrowserPromptSurface(),
              DefaultBrowserPromptSurface::kInfobar);
#endif
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{kDefaultBrowserPromptSurfaces,
          {{"prompt_surface", "modal_dialog_without_settings_illustration"}}},
         {kDefaultBrowserStickyModal,
          {{"IsSticky", "true"}, {"WithSettingsIllustration", "true"}}},
         {kDefaultBrowserSetterSelection, {{"setter_option", "visual_guide"}}}},
        {});
#if BUILDFLAG(IS_WIN)
    EXPECT_EQ(
        GetDefaultBrowserPromptSurface(),
        DefaultBrowserPromptSurface::kModalDialogWithoutSettingsIllustration);
#else
    EXPECT_EQ(GetDefaultBrowserPromptSurface(),
              DefaultBrowserPromptSurface::kInfobar);
#endif
  }
}

// Tests that the sticky modal experiment is only activated once a modal dialog
// prompt surface has been selected.
TEST(DefaultBrowserFeaturesTest,
     GetDefaultBrowserPromptSurfaceActivatesStickyModalOnlyForModal) {
#if BUILDFLAG(IS_WIN)
  base::test::ScopedOSInfoOverride win11_os(
      base::test::ScopedOSInfoOverride::Type::kWin11Pro);
#endif
  const std::string sticky_trial_name =
      std::string("scoped_feature_list_trial_for_") +
      kDefaultBrowserStickyModal.name;
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{kDefaultBrowserPromptSurfaces, {{"prompt_surface", "bubble_dialog"}}},
         {kDefaultBrowserStickyModal, {{"IsSticky", "true"}}}},
        {});
    GetDefaultBrowserPromptSurface();
    EXPECT_FALSE(base::FieldTrialList::IsTrialActive(sticky_trial_name));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{kDefaultBrowserPromptSurfaces,
          {{"prompt_surface", "modal_dialog_without_settings_illustration"}}},
         {kDefaultBrowserStickyModal, {{"IsSticky", "true"}}}},
        {});
    GetDefaultBrowserPromptSurface();
#if BUILDFLAG(IS_WIN)
    EXPECT_TRUE(base::FieldTrialList::IsTrialActive(sticky_trial_name));
#else
    EXPECT_FALSE(base::FieldTrialList::IsTrialActive(sticky_trial_name));
#endif
  }
}

}  // namespace default_browser
