// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/system_nudge_view.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/toast/nudge_constants.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_utils.h"
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
  return static_cast<views::ImageButton*>(
      nudge_view->GetViewByID(VIEW_ID_SYSTEM_NUDGE_CLOSE_BUTTON));
}

}  // namespace

class SystemNudgeViewTest : public AshTestBase {
 public:
  SystemNudgeViewTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kSystemNudgeV2);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SystemNudgeViewTest, TextOnly) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up base nudge data which will create a text-only nudge.
  auto nudge_data = CreateBaseNudgeData();

  SystemNudgeView* system_nudge_view =
      widget->SetContentsView(std::make_unique<SystemNudgeView>(nudge_data));

  // Test that appropriate nudge elements were created.
  EXPECT_FALSE(system_nudge_view->image_view());
  EXPECT_FALSE(system_nudge_view->title_label());
  ASSERT_TRUE(system_nudge_view->body_label());
  EXPECT_FALSE(system_nudge_view->first_button());
  EXPECT_FALSE(system_nudge_view->second_button());

  // Test that text labels max width is set correctly.
  EXPECT_EQ(kNudgeLabelWidth_TextOnlyNudge,
            system_nudge_view->body_label()->GetMaximumWidth());
}

TEST_F(SystemNudgeViewTest, WithButtons) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up base nudge data and add two buttons.
  auto nudge_data = CreateBaseNudgeData();
  nudge_data.first_button_text = u"Button";
  nudge_data.second_button_text = u"Button";

  SystemNudgeView* system_nudge_view =
      widget->SetContentsView(std::make_unique<SystemNudgeView>(nudge_data));

  // Test that appropriate nudge elements were created.
  EXPECT_FALSE(system_nudge_view->image_view());
  EXPECT_FALSE(system_nudge_view->title_label());
  ASSERT_TRUE(system_nudge_view->body_label());
  EXPECT_TRUE(system_nudge_view->first_button());
  EXPECT_TRUE(system_nudge_view->second_button());

  // Test that text labels max width is set correctly.
  EXPECT_EQ(kNudgeLabelWidth_NudgeWithoutLeadingImage,
            system_nudge_view->body_label()->GetFixedWidth());
}

TEST_F(SystemNudgeViewTest, TitleAndLeadingImage) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set up base nudge data and add a title and an image model.
  auto nudge_data = CreateBaseNudgeData();
  nudge_data.title_text = u"Title";
  nudge_data.image_model = ui::ImageModel::FromVectorIcon(
      vector_icons::kDogfoodIcon, kColorAshIconColorPrimary, /*icon_size=*/60);

  SystemNudgeView* system_nudge_view =
      widget->SetContentsView(std::make_unique<SystemNudgeView>(nudge_data));

  // Test that appropriate nudge elements were created.
  EXPECT_TRUE(system_nudge_view->image_view());
  EXPECT_TRUE(system_nudge_view->title_label());
  ASSERT_TRUE(system_nudge_view->body_label());
  EXPECT_FALSE(system_nudge_view->first_button());
  EXPECT_FALSE(system_nudge_view->second_button());

  // Test that text labels max width is set correctly.
  EXPECT_EQ(kNudgeLabelWidth_NudgeWithLeadingImage,
            system_nudge_view->body_label()->GetFixedWidth());
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
  nudge_data.first_button_text = u"Button";
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

}  // namespace ash
