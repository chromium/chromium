// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_features.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace default_browser {

TEST(DefaultBrowserFeaturesTest, IsDefaultBrowserPromptSurfacesEnabled) {
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
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kDefaultBrowserStickyModal);
#if BUILDFLAG(IS_WIN)
    EXPECT_TRUE(IsDefaultBrowserPromptSurfacesEnabled());
#else
    EXPECT_FALSE(IsDefaultBrowserPromptSurfacesEnabled());
#endif
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {}, {kDefaultBrowserPromptSurfaces, kDefaultBrowserStickyModal});
    EXPECT_FALSE(IsDefaultBrowserPromptSurfacesEnabled());
  }
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
}

// Tests for GetDefaultBrowserPromptSurface behavior with prompt surfaces and
// setter selection.
TEST(DefaultBrowserFeaturesTest, GetDefaultBrowserPromptSurface) {
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
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{kDefaultBrowserStickyModal,
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
        {{kDefaultBrowserStickyModal,
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

}  // namespace default_browser
