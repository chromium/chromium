// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/action_button_view.h"

#include <memory>
#include <string>

#include "ash/capture_mode/capture_mode_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr auto kSystemOnBaseColor = SK_ColorRED;
constexpr auto kDisabledContainerColor = SK_ColorYELLOW;
constexpr auto kOnSurfaceColor = SK_ColorGREEN;
constexpr auto kDisabledColor = SK_ColorBLUE;

// Sets colors used by `ActionButtonView` in the provided widget's color
// provider.
void SetTestColors(views::Widget* widget) {
  ui::ColorProvider* color_provider =
      ui::ColorProviderManager::Get().GetColorProviderFor(
          widget->GetColorProviderKeyForTesting());
  color_provider->SetColorForTesting(cros_tokens::kCrosSysSystemOnBase,
                                     kSystemOnBaseColor);
  color_provider->SetColorForTesting(cros_tokens::kCrosSysDisabledContainer,
                                     kDisabledContainerColor);
  color_provider->SetColorForTesting(cros_tokens::kCrosSysOnSurface,
                                     kOnSurfaceColor);
  color_provider->SetColorForTesting(cros_tokens::kCrosSysDisabled,
                                     kDisabledColor);
}

using ActionButtonViewTest = views::ViewsTestBase;

TEST_F(ActionButtonViewTest, ShowsIconAndLabelByDefault) {
  ActionButtonView action_button(
      views::Button::PressedCallback(), u"text", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kOther, /*weight=*/0));

  EXPECT_TRUE(action_button.image_view_for_testing()->GetVisible());
  EXPECT_TRUE(action_button.label_for_testing()->GetVisible());
}

TEST_F(ActionButtonViewTest, ShowsIconOnlyAfterCollapsed) {
  ActionButtonView action_button(
      views::Button::PressedCallback(), u"text", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kOther, /*weight=*/0));

  action_button.CollapseToIconButton();

  EXPECT_TRUE(action_button.image_view_for_testing()->GetVisible());
  EXPECT_FALSE(action_button.label_for_testing()->GetVisible());
}

TEST_F(ActionButtonViewTest, CorrectBackgroundColor) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  SetTestColors(widget.get());
  ActionButtonView action_button(
      views::Button::PressedCallback(), u"text", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kOther, /*weight=*/0));
  widget->SetContentsView(&action_button);

  SkColor background_color = action_button.GetBackground()->get_color();
  EXPECT_EQ(background_color, kSystemOnBaseColor);
}

TEST_F(ActionButtonViewTest, CorrectBackgroundColorWhenDisabled) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  SetTestColors(widget.get());
  ActionButtonView action_button(
      views::Button::PressedCallback(), u"text", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kOther, /*weight=*/0));
  widget->SetContentsView(&action_button);

  action_button.SetEnabled(false);

  SkColor background_color = action_button.GetBackground()->get_color();
  EXPECT_EQ(background_color, kDisabledContainerColor);
}

TEST_F(ActionButtonViewTest, CorrectBackgroundColorWhenReenabled) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  SetTestColors(widget.get());
  ActionButtonView action_button(
      views::Button::PressedCallback(), u"text", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kOther, /*weight=*/0));
  widget->SetContentsView(&action_button);

  action_button.SetEnabled(false);
  action_button.SetEnabled(true);

  SkColor background_color = action_button.GetBackground()->get_color();
  EXPECT_EQ(background_color, kSystemOnBaseColor);
}

TEST_F(ActionButtonViewTest, CorrectLabelColor) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  SetTestColors(widget.get());
  ActionButtonView action_button(
      views::Button::PressedCallback(), u"text", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kOther, /*weight=*/0));
  widget->SetContentsView(&action_button);

  SkColor label_color = action_button.label_for_testing()->GetEnabledColor();
  EXPECT_EQ(label_color, kOnSurfaceColor);
}

TEST_F(ActionButtonViewTest, CorrectLabelColorWhenDisabled) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  SetTestColors(widget.get());
  ActionButtonView action_button(
      views::Button::PressedCallback(), u"text", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kOther, /*weight=*/0));
  widget->SetContentsView(&action_button);

  action_button.SetEnabled(false);

  SkColor label_color = action_button.label_for_testing()->GetEnabledColor();
  EXPECT_EQ(label_color, kDisabledColor);
}

TEST_F(ActionButtonViewTest, CorrectLabelColorWhenReenabled) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  SetTestColors(widget.get());
  ActionButtonView action_button(
      views::Button::PressedCallback(), u"text", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kOther, /*weight=*/0));
  widget->SetContentsView(&action_button);

  action_button.SetEnabled(false);
  action_button.SetEnabled(true);

  SkColor label_color = action_button.label_for_testing()->GetEnabledColor();
  EXPECT_EQ(label_color, kOnSurfaceColor);
}

TEST_F(ActionButtonViewTest, CorrectIconColorId) {
  ActionButtonView action_button(
      views::Button::PressedCallback(), u"text", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kOther, /*weight=*/0));

  ui::ColorId icon_color_id = action_button.image_view_for_testing()
                                  ->GetImageModel()
                                  .GetVectorIcon()
                                  .color_id();
  EXPECT_EQ(icon_color_id, cros_tokens::kCrosSysOnSurface);
}

TEST_F(ActionButtonViewTest, CorrectIconColorIdWhenDisabled) {
  ActionButtonView action_button(
      views::Button::PressedCallback(), u"text", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kOther, /*weight=*/0));

  action_button.SetEnabled(false);

  ui::ColorId icon_color_id = action_button.image_view_for_testing()
                                  ->GetImageModel()
                                  .GetVectorIcon()
                                  .color_id();
  EXPECT_EQ(icon_color_id, cros_tokens::kCrosSysDisabled);
}

TEST_F(ActionButtonViewTest, CorrectIconColorIdWhenReenabled) {
  ActionButtonView action_button(
      views::Button::PressedCallback(), u"text", &kCaptureModeImageIcon,
      ActionButtonRank(ActionButtonType::kOther, /*weight=*/0));

  action_button.SetEnabled(false);
  action_button.SetEnabled(true);

  ui::ColorId icon_color_id = action_button.image_view_for_testing()
                                  ->GetImageModel()
                                  .GetVectorIcon()
                                  .color_id();
  EXPECT_EQ(icon_color_id, cros_tokens::kCrosSysOnSurface);
}

}  // namespace
}  // namespace ash
