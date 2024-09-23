// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/system_toast_view.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Test constants
const std::u16string kTestText = u"text";
const std::u16string kTestDismissText = u"dismiss";
const gfx::VectorIcon* kTestIcon = &kSystemMenuBusinessIcon;

views::ImageView* GetToastImageView(SystemToastView* system_toast_view) {
  return static_cast<views::ImageView*>(
      system_toast_view->GetViewByID(VIEW_ID_TOAST_IMAGE_VIEW));
}

views::Label* GetToastLabel(SystemToastView* system_toast_view) {
  return static_cast<views::Label*>(
      system_toast_view->GetViewByID(VIEW_ID_TOAST_LABEL));
}

views::LabelButton* GetToastButton(SystemToastView* system_toast_view) {
  return static_cast<views::LabelButton*>(
      system_toast_view->GetViewByID(VIEW_ID_TOAST_BUTTON));
}

}  // namespace

class SystemToastViewTest : public AshTestBase {};

TEST_F(SystemToastViewTest, TextOnly) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  auto* system_toast_view =
      widget->SetContentsView(std::make_unique<SystemToastView>(kTestText));

  // Test that the appropriate toast elements were created.
  ASSERT_TRUE(GetToastLabel(system_toast_view));
  EXPECT_EQ(GetToastLabel(system_toast_view)->GetText(), kTestText);
  EXPECT_FALSE(GetToastImageView(system_toast_view));
  EXPECT_FALSE(GetToastButton(system_toast_view));
}

TEST_F(SystemToastViewTest, WithLeadingIcon) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  auto* system_toast_view =
      widget->SetContentsView(std::make_unique<SystemToastView>(
          /*text=*/kTestText, /*dismiss_text=*/std::u16string(),
          /*dismiss_callback=*/base::DoNothing(), /*leading_icon=*/kTestIcon));

  // Test that the appropriate toast elements were created.
  EXPECT_TRUE(GetToastLabel(system_toast_view));
  EXPECT_TRUE(GetToastImageView(system_toast_view));
  EXPECT_EQ(
      GetToastImageView(system_toast_view)->GetImageModel(),
      ui::ImageModel::FromVectorIcon(*kTestIcon, cros_tokens::kCrosSysOnSurface,
                                     /*icon_size=*/20));
  EXPECT_FALSE(GetToastButton(system_toast_view));
}

TEST_F(SystemToastViewTest, WithButton) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  auto* system_toast_view =
      widget->SetContentsView(std::make_unique<SystemToastView>(
          /*text=*/kTestText, /*dismiss_text=*/kTestDismissText));

  // Test that the appropriate toast elements were created.
  EXPECT_TRUE(GetToastLabel(system_toast_view));
  EXPECT_FALSE(GetToastImageView(system_toast_view));
  EXPECT_TRUE(GetToastButton(system_toast_view));
  EXPECT_EQ(GetToastButton(system_toast_view)->GetText(), kTestDismissText);
}

}  // namespace ash
