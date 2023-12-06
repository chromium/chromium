// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/system_nudge_view.h"

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/style/keyboard_shortcut_view.h"
#include "ash/system/toast/nudge_constants.h"
#include "ash/test/ash_test_base.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Creates an `AnchoredNudgeData` object with only the required elements.
// This will create a nudge shown on its default location.
AnchoredNudgeData CreateBaseNudgeData() {
  // Set up nudge data contents.
  const std::string id = "id";
  const std::u16string body_text = u"text";
  auto catalog_name = NudgeCatalogName::kTestCatalogName;

  return AnchoredNudgeData(id, catalog_name, body_text);
}

views::ImageButton* GetCloseButton(views::View* nudge_view) {
  return views::AsViewClass<views::ImageButton>(
      nudge_view->GetViewByID(VIEW_ID_SYSTEM_NUDGE_CLOSE_BUTTON));
}

views::ImageView* GetImageView(views::View* nudge_view) {
  return views::AsViewClass<views::ImageView>(
      nudge_view->GetViewByID(VIEW_ID_SYSTEM_NUDGE_IMAGE_VIEW));
}

views::Label* GetTitleLabel(views::View* nudge_view) {
  return views::AsViewClass<views::Label>(
      nudge_view->GetViewByID(VIEW_ID_SYSTEM_NUDGE_TITLE_LABEL));
}

views::Label* GetBodyLabel(views::View* nudge_view) {
  return views::AsViewClass<views::Label>(
      nudge_view->GetViewByID(VIEW_ID_SYSTEM_NUDGE_BODY_LABEL));
}

KeyboardShortcutView* GetShortcutView(views::View* nudge_view) {
  return views::AsViewClass<KeyboardShortcutView>(
      nudge_view->GetViewByID(VIEW_ID_SYSTEM_NUDGE_SHORTCUT_VIEW));
}

views::LabelButton* GetPrimaryButton(views::View* nudge_view) {
  return views::AsViewClass<views::LabelButton>(
      nudge_view->GetViewByID(VIEW_ID_SYSTEM_NUDGE_PRIMARY_BUTTON));
}

views::LabelButton* GetSecondaryButton(views::View* nudge_view) {
  return views::AsViewClass<views::LabelButton>(
      nudge_view->GetViewByID(VIEW_ID_SYSTEM_NUDGE_SECONDARY_BUTTON));
}

}  // namespace

using SystemNudgeViewTest = AshTestBase;

TEST_F(SystemNudgeViewTest, TextOnly) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  const std::u16string body_text = u"Body text";

  // Set up base nudge data which will create a text-only nudge.
  auto nudge_data = CreateBaseNudgeData();
  nudge_data.body_text = body_text;

  SystemNudgeView* nudge_view =
      widget->SetContentsView(std::make_unique<SystemNudgeView>(nudge_data));

  // Test that appropriate nudge elements were created.
  EXPECT_FALSE(GetImageView(nudge_view));
  EXPECT_FALSE(GetTitleLabel(nudge_view));
  ASSERT_TRUE(GetBodyLabel(nudge_view));
  EXPECT_FALSE(GetPrimaryButton(nudge_view));
  EXPECT_FALSE(GetSecondaryButton(nudge_view));

  // Test that view contents are properly set.
  EXPECT_EQ(body_text, GetBodyLabel(nudge_view)->GetText());

  // Test that text labels max width is set correctly.
  EXPECT_EQ(kNudgeLabelWidth_TextOnlyNudge,
            GetBodyLabel(nudge_view)->GetMaximumWidth());
}

TEST_F(SystemNudgeViewTest, WithButtons) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  const std::u16string primary_button_text = u"Primary";
  const std::u16string secondary_button_text = u"Secondary";

  // Set up base nudge data and add two buttons.
  auto nudge_data = CreateBaseNudgeData();
  nudge_data.primary_button_text = primary_button_text;
  nudge_data.secondary_button_text = secondary_button_text;

  SystemNudgeView* nudge_view =
      widget->SetContentsView(std::make_unique<SystemNudgeView>(nudge_data));

  // Test that appropriate nudge elements were created.
  EXPECT_FALSE(GetImageView(nudge_view));
  EXPECT_FALSE(GetTitleLabel(nudge_view));
  ASSERT_TRUE(GetBodyLabel(nudge_view));
  ASSERT_TRUE(GetPrimaryButton(nudge_view));
  ASSERT_TRUE(GetSecondaryButton(nudge_view));

  // Test that view contents are properly set.
  EXPECT_EQ(primary_button_text, GetPrimaryButton(nudge_view)->GetText());
  EXPECT_EQ(secondary_button_text, GetSecondaryButton(nudge_view)->GetText());

  // Test that text labels max width is set correctly.
  EXPECT_EQ(kNudgeLabelWidth_NudgeWithoutLeadingImage,
            GetBodyLabel(nudge_view)->GetFixedWidth());
}

TEST_F(SystemNudgeViewTest, TitleAndLeadingImage) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  const std::u16string title_text = u"Title text";
  const ui::ImageModel image_model = ui::ImageModel::FromVectorIcon(
      vector_icons::kDogfoodIcon, cros_tokens::kCrosSysOnSurface,
      /*icon_size=*/60);

  // Set up base nudge data and add a title and an image model.
  auto nudge_data = CreateBaseNudgeData();
  nudge_data.title_text = title_text;
  nudge_data.image_model = image_model;

  SystemNudgeView* nudge_view =
      widget->SetContentsView(std::make_unique<SystemNudgeView>(nudge_data));

  // Test that appropriate nudge elements were created.
  ASSERT_TRUE(GetImageView(nudge_view));
  ASSERT_TRUE(GetTitleLabel(nudge_view));
  ASSERT_TRUE(GetBodyLabel(nudge_view));
  EXPECT_FALSE(GetPrimaryButton(nudge_view));
  EXPECT_FALSE(GetSecondaryButton(nudge_view));

  // Test that view contents are properly set.
  EXPECT_EQ(title_text, GetTitleLabel(nudge_view)->GetText());
  EXPECT_EQ(image_model, GetImageView(nudge_view)->GetImageModel());

  // Test that text labels max width is set correctly.
  EXPECT_EQ(kNudgeLabelWidth_NudgeWithLeadingImage,
            GetBodyLabel(nudge_view)->GetFixedWidth());
}

// Test that the nudge close button is properly created / made visible in
// different circumstances.
TEST_F(SystemNudgeViewTest, CloseButton) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  widget->SetFullscreen(true);

  // Test that text-only nudges will not have a close button.
  auto nudge_data = CreateBaseNudgeData();
  widget->SetContentsView(std::make_unique<SystemNudgeView>(nudge_data));
  EXPECT_FALSE(GetCloseButton(widget->GetContentsView()));

  // Test that a non-text-only nudge will have a close button.
  nudge_data.primary_button_text = u"Button";
  widget->SetContentsView(std::make_unique<SystemNudgeView>(nudge_data));
  ASSERT_TRUE(GetCloseButton(widget->GetContentsView()));
  EXPECT_FALSE(GetCloseButton(widget->GetContentsView())->GetVisible());

  // Simulate mouse hover events to toggle the close button visibility.
  GetEventGenerator()->MoveMouseTo(
      widget->GetContentsView()->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(GetCloseButton(widget->GetContentsView())->GetVisible());
  GetEventGenerator()->MoveMouseTo(-100, -100);
  EXPECT_FALSE(GetCloseButton(widget->GetContentsView())->GetVisible());

  // Test that nudges with an anchor view will not have a close button.
  auto anchor_view = std::make_unique<views::View>();
  nudge_data.SetAnchorView(anchor_view.get());
  widget->SetContentsView(std::make_unique<SystemNudgeView>(nudge_data));
  EXPECT_FALSE(GetCloseButton(widget->GetContentsView()));
}

// Test that the keyboard shortcut view is properly created in different
// circumstances.
TEST_F(SystemNudgeViewTest, ShortcutView) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Test that passing an empty vector of keyboard codes does not create a
  // shortcut view, and should not have a close button.
  auto nudge_data = CreateBaseNudgeData();
  nudge_data.keyboard_codes = {};
  widget->SetContentsView(std::make_unique<SystemNudgeView>(nudge_data));
  EXPECT_FALSE(GetShortcutView(widget->GetContentsView()));
  EXPECT_FALSE(GetCloseButton(widget->GetContentsView()));

  // Test that passing a non-empty vector of keyboard codes will create a
  // shortcut view, and will have a close button.
  nudge_data = CreateBaseNudgeData();
  nudge_data.keyboard_codes = {ui::VKEY_CONTROL, ui::VKEY_SHIFT,
                               ui::VKEY_MEDIA_LAUNCH_APP1};
  widget->SetContentsView(std::make_unique<SystemNudgeView>(nudge_data));
  EXPECT_TRUE(GetShortcutView(widget->GetContentsView()));
  ASSERT_TRUE(GetCloseButton(widget->GetContentsView()));
  EXPECT_FALSE(GetCloseButton(widget->GetContentsView())->GetVisible());
}

}  // namespace ash
