// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/suggestion_chip_view.h"

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/test_support/mock_assistant_view_delegate.h"
#include "ash/assistant/util/test_support/macros.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

using chromeos::assistant::AssistantSuggestion;

// Helpers ---------------------------------------------------------------------

AssistantSuggestion CreateSuggestionWithIconUrl(const std::string& icon_url) {
  AssistantSuggestion suggestion;
  suggestion.icon_url = GURL(icon_url);
  return suggestion;
}

}  // namespace

// Tests -----------------------------------------------------------------------

using SuggestionChipViewTest = AshTestBase;

TEST_F(SuggestionChipViewTest, ShouldHandleLocalIcons) {
  auto widget = CreateFramelessTestWidget();
  auto* suggestion_chip_view =
      widget->SetContentsView(std::make_unique<SuggestionChipView>(
          /*delegate=*/nullptr,
          CreateSuggestionWithIconUrl(
              "googleassistant://resource?type=icon&name=assistant")));

  const auto& actual = suggestion_chip_view->GetIcon();
  gfx::ImageSkia expected = gfx::CreateVectorIcon(
      gfx::IconDescription(chromeos::kAssistantIcon, /*size=*/16));

  ASSERT_PIXELS_EQ(actual, expected);
}

TEST_F(SuggestionChipViewTest, ShouldHandleRemoteIcons) {
  const gfx::ImageSkia expected =
      gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);

  MockAssistantViewDelegate delegate;
  EXPECT_CALL(delegate, DownloadImage)
      .WillOnce(testing::Invoke(
          [&](const GURL& url, ImageDownloader::DownloadCallback callback) {
            std::move(callback).Run(expected);
          }));

  auto widget = CreateFramelessTestWidget();
  auto* suggestion_chip_view =
      widget->SetContentsView(std::make_unique<SuggestionChipView>(
          &delegate,
          CreateSuggestionWithIconUrl("https://www.gstatic.com/images/branding/"
                                      "product/2x/googleg_48dp.png")));

  const auto& actual = suggestion_chip_view->GetIcon();
  EXPECT_TRUE(actual.BackedBySameObjectAs(expected));
}

TEST_F(SuggestionChipViewTest, DarkAndLightTheme) {
  base::test::ScopedFeatureList scoped_feature_list(features::kDarkLightMode);
  AshColorProvider::Get()->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());
  ASSERT_TRUE(features::IsDarkLightModeEnabled());
  ASSERT_FALSE(ColorProvider::Get()->IsDarkModeEnabled());

  auto widget = CreateFramelessTestWidget();
  auto* suggestion_chip_view =
      widget->SetContentsView(std::make_unique<SuggestionChipView>(
          /*delegate=*/nullptr,
          CreateSuggestionWithIconUrl(
              "googleassistant://resource?type=icon&name=assistant")));

  views::Label* label =
      static_cast<views::Label*>(suggestion_chip_view->children().at(1));

  EXPECT_EQ(label->GetEnabledColor(),
            ColorProvider::Get()->GetContentLayerColor(
                ColorProvider::ContentLayerType::kTextColorSecondary));

  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kDarkModeEnabled, true);
  ASSERT_TRUE(ColorProvider::Get()->IsDarkModeEnabled());

  EXPECT_EQ(label->GetEnabledColor(),
            ColorProvider::Get()->GetContentLayerColor(
                ColorProvider::ContentLayerType::kTextColorSecondary));
}

TEST_F(SuggestionChipViewTest, DarkAndLightModeFlagOff) {
  ASSERT_FALSE(features::IsDarkLightModeEnabled());

  auto widget = CreateFramelessTestWidget();
  auto* suggestion_chip_view =
      widget->SetContentsView(std::make_unique<SuggestionChipView>(
          /*delegate=*/nullptr,
          CreateSuggestionWithIconUrl(
              "googleassistant://resource?type=icon&name=assistant")));

  views::Label* label =
      static_cast<views::Label*>(suggestion_chip_view->children().at(1));
  EXPECT_EQ(label->GetEnabledColor(), kTextColorSecondary);
}

}  // namespace ash
