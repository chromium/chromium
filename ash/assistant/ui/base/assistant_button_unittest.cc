// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/base/assistant_button.h"

#include <memory>

#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kSizeInDip = 32;
constexpr int kIconSizeInDip = 24;
constexpr int kIconOffset = 4;
constexpr int kFocusRingStrokeWidth = 2;

SkBitmap CreateExpectedImageWithFocus(SkColor icon_color, SkColor focus_color) {
  gfx::Canvas expected(gfx::Size(kSizeInDip, kSizeInDip), /*image_scale=*/1.0f,
                       /*is_opaque=*/true);
  expected.DrawImageInt(
      gfx::CreateVectorIcon(
        vector_icons::kKeyboardIcon, kIconSizeInDip, icon_color),
      kIconOffset, kIconOffset);

  cc::PaintFlags circle_flags;
  circle_flags.setAntiAlias(true);
  circle_flags.setColor(focus_color);
  circle_flags.setStyle(cc::PaintFlags::kStroke_Style);
  circle_flags.setStrokeWidth(kFocusRingStrokeWidth);
  expected.DrawCircle(gfx::Point(kSizeInDip / 2, kSizeInDip / 2),
                      kSizeInDip / 2 - kFocusRingStrokeWidth, circle_flags);

  return expected.GetBitmap();
}

SkBitmap CreateExpectedImageWithoutFocus(SkColor icon_color) {
  gfx::Canvas expected(gfx::Size(kSizeInDip, kSizeInDip), /*image_scale=*/1.0f,
                       /*is_opaque=*/true);
  expected.DrawImageInt(gfx::CreateVectorIcon(vector_icons::kKeyboardIcon,
                                              kIconSizeInDip, icon_color),
                        kIconOffset, kIconOffset);

  return expected.GetBitmap();
}

}  // namespace

using AssistantButtonTest = AshTestBase;

TEST_F(AssistantButtonTest, IconColor) {
  AssistantButton::InitParams params;
  params.icon_color = gfx::kGoogleBlue900;
  params.size_in_dip = kSizeInDip;
  params.icon_size_in_dip = kIconSizeInDip;
  params.accessible_name_id = IDS_ASH_ASSISTANT_DIALOG_PLATE_KEYBOARD_ACCNAME;

  std::unique_ptr<AssistantButton> button = AssistantButton::Create(
      nullptr, vector_icons::kKeyboardIcon,
      AssistantButtonId::kKeyboardInputToggle, std::move(params));
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      *gfx::CreateVectorIcon(vector_icons::kKeyboardIcon, kIconSizeInDip,
          gfx::kGoogleBlue900).bitmap(),
      *button->GetImage(views::Button::STATE_NORMAL).bitmap()));
}

TEST_F(AssistantButtonTest, IconColorType) {
  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  dark_light_mode_controller->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());

  AssistantButton::InitParams params;
  // Set icon_color to confirm that icon_color is ignored if icon_color_type is
  // set at the same time.
  params.icon_color = gfx::kGoogleBlue900;
  params.size_in_dip = kSizeInDip;
  params.icon_size_in_dip = kIconSizeInDip;
  params.accessible_name_id = IDS_ASH_ASSISTANT_DIALOG_PLATE_KEYBOARD_ACCNAME;
  params.icon_color_type = cros_tokens::kColorPrimary;

  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  AssistantButton* button =
      widget->GetContentsView()->AddChildView(AssistantButton::Create(
          nullptr, vector_icons::kKeyboardIcon,
          AssistantButtonId::kKeyboardInputToggle, std::move(params)));

  const SkBitmap light_mode_expected_image =
      *gfx::CreateVectorIcon(vector_icons::kKeyboardIcon, kIconSizeInDip,
                             gfx::kGoogleGrey900)
           .bitmap();
  const SkBitmap dark_mode_expected_image =
      *gfx::CreateVectorIcon(vector_icons::kKeyboardIcon, kIconSizeInDip,
                             gfx::kGoogleGrey200)
           .bitmap();
  const bool initial_dark_mode_status =
      dark_light_mode_controller->IsDarkModeEnabled();

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      initial_dark_mode_status ? dark_mode_expected_image
                               : light_mode_expected_image,
      *button->GetImage(views::Button::STATE_NORMAL).bitmap()));

  // Switch the color mode.
  dark_light_mode_controller->ToggleColorMode();
  const bool dark_mode_status = dark_light_mode_controller->IsDarkModeEnabled();
  ASSERT_NE(initial_dark_mode_status, dark_mode_status);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      dark_mode_status ? dark_mode_expected_image : light_mode_expected_image,
      *button->GetImage(views::Button::STATE_NORMAL).bitmap()));
}

TEST_F(AssistantButtonTest, FocusAndHoverColorDarkLightMode) {
  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  dark_light_mode_controller->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());

  AssistantButton::InitParams params;
  params.size_in_dip = kSizeInDip;
  params.icon_size_in_dip = kIconSizeInDip;
  params.accessible_name_id = IDS_ASH_ASSISTANT_DIALOG_PLATE_KEYBOARD_ACCNAME;
  params.icon_color_type = cros_tokens::kColorPrimary;

  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  AssistantButton* button =
      widget->GetContentsView()->AddChildView(AssistantButton::Create(
          nullptr, vector_icons::kKeyboardIcon,
          AssistantButtonId::kKeyboardInputToggle, std::move(params)));
  button->SizeToPreferredSize();

  gfx::Canvas canvas(gfx::Size(kSizeInDip, kSizeInDip), /*image_scale=*/1.0f,
                     /*is_opaque=*/true);
  const SkColor light_icon_color = gfx::kGoogleGrey900;
  const SkColor dark_icon_color = gfx::kGoogleGrey200;

  const bool initial_dark_mode_status =
      dark_light_mode_controller->IsDarkModeEnabled();

  SkBitmap dark_light_mode_button_image_with_focus =
      CreateExpectedImageWithFocus(
          /*icon_color=*/initial_dark_mode_status ? dark_icon_color
                                                  : light_icon_color,
          /*focus_color=*/ColorProvider::Get()->GetControlsLayerColor(
              ColorProvider::ControlsLayerType::kFocusRingColor));

  SkBitmap dark_light_mode_button_image_without_focus =
      CreateExpectedImageWithoutFocus(
          /*icon_color=*/initial_dark_mode_status ? dark_icon_color
                                                  : light_icon_color);

  button->RequestFocus();
  ASSERT_TRUE(button->HasFocus());

  // Expect focus ring to be shown when keyboard traversal is enabled.
  AssistantUiController::Get()->SetKeyboardTraversalMode(true);
  button->OnPaint(&canvas);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      dark_light_mode_button_image_with_focus, canvas.GetBitmap()));

  canvas.RecreateBackingCanvas(gfx::Size(kSizeInDip, kSizeInDip),
                               /*image_scale=*/1.0f, /*is_opaque=*/true);

  button->RequestFocus();
  ASSERT_TRUE(button->HasFocus());

  // Expect focus ring to be hidden when keyboard traversal mode is disabled.
  AssistantUiController::Get()->SetKeyboardTraversalMode(false);
  button->OnPaint(&canvas);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      dark_light_mode_button_image_without_focus, canvas.GetBitmap()));

  // Switch the color mode.
  dark_light_mode_controller->ToggleColorMode();
  const bool dark_mode_status = dark_light_mode_controller->IsDarkModeEnabled();
  ASSERT_NE(initial_dark_mode_status, dark_mode_status);

  canvas.RecreateBackingCanvas(gfx::Size(kSizeInDip, kSizeInDip),
                               /*image_scale=*/1.0f, /*is_opaque=*/true);

  dark_light_mode_button_image_with_focus = CreateExpectedImageWithFocus(
      /*icon_color=*/dark_mode_status ? dark_icon_color : light_icon_color,
      /*focus_color=*/ColorProvider::Get()->GetControlsLayerColor(
          ColorProvider::ControlsLayerType::kFocusRingColor));

  dark_light_mode_button_image_without_focus = CreateExpectedImageWithoutFocus(
      /*icon_color=*/dark_mode_status ? dark_icon_color : light_icon_color);

  button->RequestFocus();
  ASSERT_TRUE(button->HasFocus());

  // Expect focus ring to be shown when keyboard traversal is enabled.
  AssistantUiController::Get()->SetKeyboardTraversalMode(true);
  button->OnPaint(&canvas);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      dark_light_mode_button_image_with_focus, canvas.GetBitmap()));

  canvas.RecreateBackingCanvas(gfx::Size(kSizeInDip, kSizeInDip),
                               /*image_scale=*/1.0f, /*is_opaque=*/true);

  button->RequestFocus();
  ASSERT_TRUE(button->HasFocus());

  // Expect focus ring to be hidden when keyboard traversal mode is disabled.
  AssistantUiController::Get()->SetKeyboardTraversalMode(false);
  button->OnPaint(&canvas);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      dark_light_mode_button_image_without_focus, canvas.GetBitmap()));
}
}  // namespace ash
