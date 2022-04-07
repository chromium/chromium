// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/base/assistant_button.h"
#include <memory>

#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
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

TEST_F(AssistantButtonTest, IconColorTypeDefaultLight) {
  ASSERT_FALSE(features::IsDarkLightModeEnabled());

  AssistantButton::InitParams params;
  params.size_in_dip = kSizeInDip;
  params.icon_size_in_dip = kIconSizeInDip;
  params.accessible_name_id = IDS_ASH_ASSISTANT_DIALOG_PLATE_KEYBOARD_ACCNAME;
  params.icon_color_type = ColorProvider::ContentLayerType::kIconColorPrimary;

  std::unique_ptr<AssistantButton> button = AssistantButton::Create(
      nullptr, vector_icons::kKeyboardIcon,
      AssistantButtonId::kKeyboardInputToggle, std::move(params));

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      *gfx::CreateVectorIcon(vector_icons::kKeyboardIcon, kIconSizeInDip,
                             ash::features::IsProductivityLauncherEnabled()
                                 ? gfx::kGoogleGrey200
                                 : gfx::kGoogleGrey900)
           .bitmap(),
      *button->GetImage(views::Button::STATE_NORMAL).bitmap()));
}

TEST_F(AssistantButtonTest, IconColorType) {
  base::test::ScopedFeatureList scoped_feature_list_enable_dark_light_mode(
      chromeos::features::kDarkLightMode);
  AshColorProvider::Get()->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());

  AssistantButton::InitParams params;
  // Set icon_color to confirm that icon_color is ignored if icon_color_type is
  // set at the same time.
  params.icon_color = gfx::kGoogleBlue900;
  params.size_in_dip = kSizeInDip;
  params.icon_size_in_dip = kIconSizeInDip;
  params.accessible_name_id = IDS_ASH_ASSISTANT_DIALOG_PLATE_KEYBOARD_ACCNAME;
  params.icon_color_type = ColorProvider::ContentLayerType::kIconColorPrimary;

  std::unique_ptr<AssistantButton> button = AssistantButton::Create(
      nullptr, vector_icons::kKeyboardIcon,
      AssistantButtonId::kKeyboardInputToggle, std::move(params));

  ASSERT_FALSE(ColorProvider::Get()->IsDarkModeEnabled());
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      *gfx::CreateVectorIcon(vector_icons::kKeyboardIcon, kIconSizeInDip,
          gfx::kGoogleGrey900).bitmap(),
      *button->GetImage(views::Button::STATE_NORMAL).bitmap()));

  // Switch to dark mode
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kDarkModeEnabled, true);
  ASSERT_TRUE(ColorProvider::Get()->IsDarkModeEnabled());

  // Manually triggers OnThemeChanged as the button is not attached to an UI
  // tree.
  button->OnThemeChanged();

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      *gfx::CreateVectorIcon(vector_icons::kKeyboardIcon, kIconSizeInDip,
          gfx::kGoogleGrey200).bitmap(),
      *button->GetImage(views::Button::STATE_NORMAL).bitmap()));
}

TEST_F(AssistantButtonTest, FocusAndHoverColor) {
  ASSERT_FALSE(features::IsDarkLightModeEnabled());

  AssistantButton::InitParams params;
  params.size_in_dip = kSizeInDip;
  params.icon_size_in_dip = kIconSizeInDip;
  params.accessible_name_id = IDS_ASH_ASSISTANT_DIALOG_PLATE_KEYBOARD_ACCNAME;
  params.icon_color_type = ColorProvider::ContentLayerType::kIconColorPrimary;

  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  AssistantButton* button =
      widget->GetContentsView()->AddChildView(AssistantButton::Create(
          nullptr, vector_icons::kKeyboardIcon,
          AssistantButtonId::kKeyboardInputToggle, std::move(params)));
  button->SizeToPreferredSize();

  button->RequestFocus();
  ASSERT_TRUE(button->HasFocus());

  gfx::Canvas canvas(gfx::Size(kSizeInDip, kSizeInDip), /*image_scale=*/1.0f,
                     /*is_opaque=*/true);
  button->OnPaint(&canvas);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      CreateExpectedImageWithFocus(
          /*icon_color=*/ash::features::IsProductivityLauncherEnabled()
              ? gfx::kGoogleGrey200
              : gfx::kGoogleGrey900,
          /*focus_color=*/ColorProvider::Get()->GetControlsLayerColor(
              ColorProvider::ControlsLayerType::kFocusRingColor)),
      canvas.GetBitmap()));
}

TEST_F(AssistantButtonTest, FocusAndHoverColorDarkLightMode) {
  base::test::ScopedFeatureList scoped_feature_list_enable_dark_light_mode(
      chromeos::features::kDarkLightMode);
  AshColorProvider::Get()->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());
  ASSERT_FALSE(ColorProvider::Get()->IsDarkModeEnabled());

  AssistantButton::InitParams params;
  params.size_in_dip = kSizeInDip;
  params.icon_size_in_dip = kIconSizeInDip;
  params.accessible_name_id = IDS_ASH_ASSISTANT_DIALOG_PLATE_KEYBOARD_ACCNAME;
  params.icon_color_type = ColorProvider::ContentLayerType::kIconColorPrimary;

  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  AssistantButton* button =
      widget->GetContentsView()->AddChildView(AssistantButton::Create(
          nullptr, vector_icons::kKeyboardIcon,
          AssistantButtonId::kKeyboardInputToggle, std::move(params)));
  button->SizeToPreferredSize();

  button->RequestFocus();
  ASSERT_TRUE(button->HasFocus());

  gfx::Canvas canvas(gfx::Size(kSizeInDip, kSizeInDip), /*image_scale=*/1.0f,
                     /*is_opaque=*/true);
  button->OnPaint(&canvas);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      CreateExpectedImageWithFocus(
          /*icon_color=*/gfx::kGoogleGrey900,
          /*focus_color=*/ColorProvider::Get()->GetControlsLayerColor(
              ColorProvider::ControlsLayerType::kFocusRingColor)),
      canvas.GetBitmap()));

  // Switch to dark mode
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kDarkModeEnabled, true);
  ASSERT_TRUE(ColorProvider::Get()->IsDarkModeEnabled());

  canvas.RecreateBackingCanvas(gfx::Size(kSizeInDip, kSizeInDip),
                               /*image_scale=*/1.0f, /*is_opaque=*/true);
  button->OnPaint(&canvas);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      CreateExpectedImageWithFocus(
          /*icon_color=*/gfx::kGoogleGrey200,
          /*focus_color=*/ColorProvider::Get()->GetControlsLayerColor(
              ColorProvider::ControlsLayerType::kFocusRingColor)),
      canvas.GetBitmap()));
}
}  // namespace ash
