// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/system_toast_view.h"

#include <string>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/icon_button.h"
#include "ash/test/ash_test_base.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Test constants
const std::u16string kTestText = u"text";
const std::u16string kTestButtonText = u"dismiss";
const gfx::VectorIcon* kTestIcon = &kSystemMenuBusinessIcon;

views::ImageView* GetToastImageView(SystemToastView* system_toast_view) {
  return static_cast<views::ImageView*>(
      system_toast_view->GetViewByID(VIEW_ID_TOAST_IMAGE_VIEW));
}

views::Label* GetToastLabel(SystemToastView* system_toast_view) {
  return static_cast<views::Label*>(
      system_toast_view->GetViewByID(VIEW_ID_TOAST_LABEL));
}

views::LabelButton* GetToastTextButton(SystemToastView* system_toast_view) {
  return views::AsViewClass<views::LabelButton>(
      system_toast_view->GetViewByID(VIEW_ID_TOAST_BUTTON));
}

IconButton* GetToastIconButton(SystemToastView* system_toast_view) {
  return views::AsViewClass<IconButton>(
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
  EXPECT_FALSE(GetToastTextButton(system_toast_view));
  EXPECT_FALSE(GetToastIconButton(system_toast_view));
}

TEST_F(SystemToastViewTest, WithLeadingIcon) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  auto* system_toast_view =
      widget->SetContentsView(std::make_unique<SystemToastView>(
          /*text=*/kTestText, SystemToastView::ButtonType::kNone,
          /*button_text=*/std::u16string(),
          /*button_icon=*/&gfx::VectorIcon::EmptyIcon(),
          /*button_callback=*/base::DoNothing(), /*leading_icon=*/kTestIcon));

  // Test that the appropriate toast elements were created.
  EXPECT_TRUE(GetToastLabel(system_toast_view));
  EXPECT_TRUE(GetToastImageView(system_toast_view));
  EXPECT_EQ(
      GetToastImageView(system_toast_view)->GetImageModel(),
      ui::ImageModel::FromVectorIcon(*kTestIcon, cros_tokens::kCrosSysOnSurface,
                                     /*icon_size=*/20));
  EXPECT_FALSE(GetToastTextButton(system_toast_view));
  EXPECT_FALSE(GetToastIconButton(system_toast_view));
}

TEST_F(SystemToastViewTest, WithTextButton) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  auto* system_toast_view =
      widget->SetContentsView(std::make_unique<SystemToastView>(
          /*text=*/kTestText, SystemToastView::ButtonType::kTextButton,
          /*button_text=*/kTestButtonText));

  // Test that the appropriate toast elements were created.
  EXPECT_TRUE(GetToastLabel(system_toast_view));
  EXPECT_FALSE(GetToastImageView(system_toast_view));
  EXPECT_TRUE(GetToastTextButton(system_toast_view));
  EXPECT_EQ(GetToastTextButton(system_toast_view)->GetText(), kTestButtonText);
  EXPECT_FALSE(GetToastIconButton(system_toast_view));
}

TEST_F(SystemToastViewTest, WithIconButton) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  auto* system_toast_view =
      widget->SetContentsView(std::make_unique<SystemToastView>(
          /*text=*/kTestText, SystemToastView::ButtonType::kIconButton,
          /*button_text=*/kTestButtonText, /*button_icon=*/kTestIcon));

  // Test that the appropriate toast elements were created.
  EXPECT_TRUE(GetToastLabel(system_toast_view));
  EXPECT_FALSE(GetToastImageView(system_toast_view));
  EXPECT_FALSE(GetToastTextButton(system_toast_view));
  EXPECT_TRUE(GetToastIconButton(system_toast_view));
  EXPECT_EQ(GetToastIconButton(system_toast_view)->GetAccessibleName(),
            kTestButtonText);
}

}  // namespace ash
