// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/system_toast_view.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/test/ash_test_base.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Creates a `ToastData` object with only the required elements.
ToastData CreateBaseToastData() {
  const std::string id = "id";
  const std::u16string text = u"text";
  auto catalog_name = ToastCatalogName::kTestCatalogName;
  return ToastData(id, catalog_name, text);
}

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
  auto* contents_view =
      widget->SetContentsView(std::make_unique<views::View>());

  // Set up base toast data, which has an id, a catalog name and a body text.
  auto toast_data = CreateBaseToastData();

  auto* system_toast_view = contents_view->AddChildView(
      std::make_unique<SystemToastView>(toast_data));

  // Test that the appropriate toast elements were created.
  ASSERT_TRUE(GetToastLabel(system_toast_view));
  EXPECT_EQ(toast_data.text, GetToastLabel(system_toast_view)->GetText());
  EXPECT_FALSE(GetToastImageView(system_toast_view));
  EXPECT_FALSE(GetToastButton(system_toast_view));
}

TEST_F(SystemToastViewTest, WithLeadingIcon) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  auto* contents_view =
      widget->SetContentsView(std::make_unique<views::View>());

  // Set up base toast data and add a leading icon.
  auto toast_data = CreateBaseToastData();
  toast_data.leading_icon = &kSystemMenuBusinessIcon;

  auto* system_toast_view = contents_view->AddChildView(
      std::make_unique<SystemToastView>(toast_data));

  // Test that the appropriate toast elements were created.
  EXPECT_TRUE(GetToastLabel(system_toast_view));
  EXPECT_TRUE(GetToastImageView(system_toast_view));
  EXPECT_EQ(ui::ImageModel::FromVectorIcon(kSystemMenuBusinessIcon,
                                           cros_tokens::kCrosSysOnSurface,
                                           /*icon_size=*/20),
            GetToastImageView(system_toast_view)->GetImageModel());
  EXPECT_FALSE(GetToastButton(system_toast_view));
}

TEST_F(SystemToastViewTest, WithButton) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  auto* contents_view =
      widget->SetContentsView(std::make_unique<views::View>());

  // Set up base toast data and add a button.
  auto toast_data = CreateBaseToastData();
  toast_data.dismiss_text = u"Button";

  auto* system_toast_view = contents_view->AddChildView(
      std::make_unique<SystemToastView>(toast_data));

  // Test that the appropriate toast elements were created.
  EXPECT_TRUE(GetToastLabel(system_toast_view));
  EXPECT_FALSE(GetToastImageView(system_toast_view));
  EXPECT_TRUE(GetToastButton(system_toast_view));
  EXPECT_EQ(toast_data.dismiss_text,
            GetToastButton(system_toast_view)->GetText());
}

}  // namespace ash
