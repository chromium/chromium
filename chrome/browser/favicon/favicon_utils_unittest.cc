// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_utils.h"

#include "content/public/browser/navigation_entry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_recipe.h"
#include "ui/resources/grit/ui_resources.h"

namespace favicon {

namespace {

constexpr SkColor kLightColor = SK_ColorWHITE;
constexpr SkColor kDarkColor = SK_ColorBLACK;

}  // namespace

TEST(FaviconUtilsTest, ShouldThemifyFavicon) {
  std::unique_ptr<content::NavigationEntry> entry =
      content::NavigationEntry::Create();
  const GURL unthemeable_url("http://mail.google.com");
  const GURL themeable_virtual_url("chrome://feedback/");
  const GURL themeable_url("chrome://new-tab-page/");

  entry->SetVirtualURL(themeable_virtual_url);
  entry->SetURL(themeable_url);
  // Entry should be themefied if both its virtual and actual URLs are
  // themeable.
  EXPECT_TRUE(ShouldThemifyFaviconForEntry(entry.get()));

  entry->SetVirtualURL(unthemeable_url);
  // Entry should be themefied if only its actual URL is themeable.
  EXPECT_TRUE(ShouldThemifyFaviconForEntry(entry.get()));

  entry->SetURL(unthemeable_url);
  // Entry should not be themefied if both its virtual and actual URLs are
  // not themeable.
  EXPECT_FALSE(ShouldThemifyFaviconForEntry(entry.get()));

  entry->SetVirtualURL(themeable_virtual_url);
  // Entry should be themefied if only its virtual URL is themeable.
  EXPECT_TRUE(ShouldThemifyFaviconForEntry(entry.get()));
}

class DefaultFaviconModelTest : public ::testing::Test {
 protected:
  using InitializerType =
      ui::ColorProviderManager::ColorProviderInitializerList::CallbackType;

  // testing::Test:
  void SetUp() override {
    Test::SetUp();
    AddDefaultInitializer();
  }
  void TearDown() override {
    ui::ColorProviderManager::ResetForTesting();
    Test::TearDown();
  }

  void AddDefaultInitializer() {
    const auto initializer = [&](ui::ColorProvider* provider,
                                 const ui::ColorProviderKey& key) {
      ui::ColorMixer& mixer = provider->AddMixer();
      mixer[ui::kColorWindowBackground] = {
          key.color_mode == ui::ColorProviderKey::ColorMode::kDark
              ? kDarkColor
              : kLightColor};
    };
    AddInitializer(base::BindRepeating(initializer));
  }

  void AddInitializer(InitializerType initializer) {
    ui::ColorProviderManager& manager =
        ui::ColorProviderManager::GetForTesting();
    manager.AppendColorProviderInitializer(base::BindRepeating(initializer));
  }

  ui::ColorProvider* GetColorProvider(
      ui::ColorProviderKey::ColorMode color_mode) {
    ui::ColorProviderKey key;
    key.color_mode = color_mode;
    return ui::ColorProviderManager::GetForTesting().GetColorProviderFor(key);
  }

  gfx::ImageSkia GetDefaultFaviconForColorScheme(bool is_dark) {
    const int resource_id =
        is_dark ? IDR_DEFAULT_FAVICON_DARK : IDR_DEFAULT_FAVICON;
    return ui::ResourceBundle::GetSharedInstance()
        .GetNativeImageNamed(resource_id)
        .AsImageSkia();
  }
};

TEST_F(DefaultFaviconModelTest, UsesCorrectIcon_LightBackground) {
  auto* color_provider =
      GetColorProvider(ui::ColorProviderKey::ColorMode::kLight);
  const auto favicon_image = GetDefaultFaviconModel().Rasterize(color_provider);
  EXPECT_TRUE(GetDefaultFaviconForColorScheme(/*is_dark=*/false)
                  .BackedBySameObjectAs(favicon_image));
}

TEST_F(DefaultFaviconModelTest, UsesCorrectIcon_DarkBackground) {
  auto* color_provider =
      GetColorProvider(ui::ColorProviderKey::ColorMode::kDark);
  const auto favicon_image = GetDefaultFaviconModel().Rasterize(color_provider);
  EXPECT_TRUE(GetDefaultFaviconForColorScheme(/*is_dark=*/true)
                  .BackedBySameObjectAs(favicon_image));
}

TEST_F(DefaultFaviconModelTest, UsesCorrectIcon_LightBackground_Custom) {
  // Flip the background for a new color id such that it is light in dark mode
  // and vice versa.
  const auto initializer = [&](ui::ColorProvider* provider,
                               const ui::ColorProviderKey& key) {
    ui::ColorMixer& mixer = provider->AddMixer();
    mixer[ui::kColorBubbleBackground] = {
        key.color_mode == ui::ColorProviderKey::ColorMode::kDark ? kLightColor
                                                                 : kDarkColor};
  };
  AddInitializer(base::BindRepeating(initializer));

  // Even if the color provider is configured for light mode the default favicon
  // model should match the dark variant when rasterized.
  auto* color_provider =
      GetColorProvider(ui::ColorProviderKey::ColorMode::kLight);
  const auto favicon_image = GetDefaultFaviconModel(ui::kColorBubbleBackground)
                                 .Rasterize(color_provider);
  EXPECT_TRUE(GetDefaultFaviconForColorScheme(/*is_dark=*/true)
                  .BackedBySameObjectAs(favicon_image));
}

TEST_F(DefaultFaviconModelTest, UsesCorrectIcon_DarkBackground_Custom) {
  // Flip the background for a new color id such that it is light in dark mode
  // and vice versa.
  const auto initializer = [&](ui::ColorProvider* provider,
                               const ui::ColorProviderKey& key) {
    ui::ColorMixer& mixer = provider->AddMixer();
    mixer[ui::kColorBubbleBackground] = {
        key.color_mode == ui::ColorProviderKey::ColorMode::kDark ? kLightColor
                                                                 : kDarkColor};
  };
  AddInitializer(base::BindRepeating(initializer));

  // Even if the color provider is configured for dark mode the default favicon
  // model should match the light variant when rasterized.
  auto* color_provider =
      GetColorProvider(ui::ColorProviderKey::ColorMode::kDark);
  const auto favicon_image = GetDefaultFaviconModel(ui::kColorBubbleBackground)
                                 .Rasterize(color_provider);
  EXPECT_TRUE(GetDefaultFaviconForColorScheme(/*is_dark=*/false)
                  .BackedBySameObjectAs(favicon_image));
}

}  // namespace favicon
