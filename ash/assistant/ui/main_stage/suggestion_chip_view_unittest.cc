// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/suggestion_chip_view.h"

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/test_support/mock_assistant_view_delegate.h"
#include "ash/assistant/util/test_support/macros.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "cc/paint/paint_flags.h"
#include "cc/test/pixel_comparator.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

using assistant::AssistantSuggestion;

constexpr gfx::Size kSuggestionChipViewSize = gfx::Size(120, 32);

// Helpers ---------------------------------------------------------------------

AssistantSuggestion CreateSuggestionWithIconUrl(const std::string& icon_url) {
  AssistantSuggestion suggestion;
  suggestion.icon_url = GURL(icon_url);
  return suggestion;
}

SkBitmap GetBitmapWithInnerRoundedRect(gfx::Size size,
                                       int stroke_width,
                                       SkColor color) {
  gfx::Canvas canvas(size, /*image_scale=*/1.0f, /*is_opaque=*/false);
  gfx::RectF bounds(size.width(), size.height());

  cc::PaintFlags paint_flags;
  paint_flags.setAntiAlias(true);
  paint_flags.setColor(color);
  paint_flags.setStrokeWidth(stroke_width);
  paint_flags.setStyle(cc::PaintFlags::Style::kStroke_Style);
  bounds.Inset(gfx::InsetsF(static_cast<float>(stroke_width) / 2.0f));
  canvas.DrawRoundRect(bounds, /*radius=*/bounds.height() / 2.0f, paint_flags);
  return canvas.GetBitmap();
}

SkBitmap GetSuggestionChipViewBitmap(SuggestionChipView* view) {
  gfx::Canvas canvas(view->size(), /*image_scale=*/1.0f, /*is_opaque=*/false);
  view->OnPaint(&canvas);
  return canvas.GetBitmap();
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
  base::test::ScopedFeatureList scoped_feature_list(
      chromeos::features::kDarkLightMode);
  ASSERT_TRUE(chromeos::features::IsDarkLightModeEnabled());

  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  dark_light_mode_controller->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());
  const bool initial_dark_mode_status =
      dark_light_mode_controller->IsDarkModeEnabled();

  auto widget = CreateFramelessTestWidget();
  auto* suggestion_chip_view =
      widget->SetContentsView(std::make_unique<SuggestionChipView>(
          /*delegate=*/nullptr,
          CreateSuggestionWithIconUrl(
              "googleassistant://resource?type=icon&name=assistant")));

  views::Label* label = static_cast<views::Label*>(
      suggestion_chip_view->GetViewByID(kSuggestionChipViewLabel));

  widget->SetSize(kSuggestionChipViewSize);

  // No background if dark and light theme is on.
  EXPECT_EQ(suggestion_chip_view->GetBackground(), nullptr);

  EXPECT_EQ(label->GetEnabledColor(),
            ColorProvider::Get()->GetContentLayerColor(
                ColorProvider::ContentLayerType::kTextColorSecondary));
  EXPECT_TRUE(cc::ExactPixelComparator().Compare(
      GetSuggestionChipViewBitmap(suggestion_chip_view),
      GetBitmapWithInnerRoundedRect(
          kSuggestionChipViewSize, /*stroke_width=*/1,
          ColorProvider::Get()->GetContentLayerColor(
              ColorProvider::ContentLayerType::kSeparatorColor))));

  // Switch the color mode.
  dark_light_mode_controller->ToggleColorMode();
  ASSERT_NE(initial_dark_mode_status,
            dark_light_mode_controller->IsDarkModeEnabled());

  EXPECT_EQ(label->GetEnabledColor(),
            ColorProvider::Get()->GetContentLayerColor(
                ColorProvider::ContentLayerType::kTextColorSecondary));
  EXPECT_TRUE(cc::ExactPixelComparator().Compare(
      GetSuggestionChipViewBitmap(suggestion_chip_view),
      GetBitmapWithInnerRoundedRect(
          kSuggestionChipViewSize, /*stroke_width=*/1,
          ColorProvider::Get()->GetContentLayerColor(
              ColorProvider::ContentLayerType::kSeparatorColor))));
}

TEST_F(SuggestionChipViewTest, FontWeight) {
  auto widget = CreateFramelessTestWidget();
  auto* suggestion_chip_view =
      widget->SetContentsView(std::make_unique<SuggestionChipView>(
          /*delegate=*/nullptr,
          CreateSuggestionWithIconUrl(
              "googleassistant://resource?type=icon&name=assistant")));

  views::Label* label = static_cast<views::Label*>(
      suggestion_chip_view->GetViewByID(kSuggestionChipViewLabel));

  EXPECT_EQ(label->font_list().GetFontWeight(), gfx::Font::Weight::MEDIUM);
}

}  // namespace ash
