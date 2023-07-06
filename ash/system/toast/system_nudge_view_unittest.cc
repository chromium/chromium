// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/system_nudge_view.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/style/ash_color_id.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kLabelMaxWidth_TextOnlyNudge = 300;
constexpr int kLabelMaxWidth_NudgeWithoutLeadingImage = 292;
constexpr int kLabelMaxWidth_NudgeWithLeadingImage = 276;
constexpr int kLabelMaxWidth_ToastStyleNudge = 512;
constexpr int kImageViewSize = 64;

// Creates an `AnchoredNudgeData` object with only the required elements.
AnchoredNudgeData CreateBaseNudgeData(views::View* contents_view) {
  // Set up nudge data contents.
  const std::string id = "id";
  const std::u16string body_text = u"text";
  auto catalog_name = NudgeCatalogName::kTestCatalogName;
  auto* anchor_view =
      contents_view->AddChildView(std::make_unique<views::View>());

  return AnchoredNudgeData(id, catalog_name, body_text, anchor_view);
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
  auto* contents_view =
      widget->SetContentsView(std::make_unique<views::View>());

  // Set up base nudge data which will create a text-only nudge.
  auto nudge_data = CreateBaseNudgeData(contents_view);

  auto* system_nudge_view = contents_view->AddChildView(
      std::make_unique<SystemNudgeView>(nudge_data));

  // Test that appropriate nudge elements were created.
  EXPECT_FALSE(system_nudge_view->image_view());
  EXPECT_FALSE(system_nudge_view->title_label());
  ASSERT_TRUE(system_nudge_view->body_label());
  EXPECT_FALSE(system_nudge_view->first_button());
  EXPECT_FALSE(system_nudge_view->second_button());

  // Test that text labels max width is set correctly.
  EXPECT_EQ(kLabelMaxWidth_TextOnlyNudge,
            system_nudge_view->body_label()->GetMaximumWidth());
}

TEST_F(SystemNudgeViewTest, WithButtons) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  auto* contents_view =
      widget->SetContentsView(std::make_unique<views::View>());

  // Set up base nudge data and add two buttons.
  auto nudge_data = CreateBaseNudgeData(contents_view);
  nudge_data.first_button_text = u"Button";
  nudge_data.second_button_text = u"Button";

  auto* system_nudge_view = contents_view->AddChildView(
      std::make_unique<SystemNudgeView>(nudge_data));

  // Test that appropriate nudge elements were created.
  EXPECT_FALSE(system_nudge_view->image_view());
  EXPECT_FALSE(system_nudge_view->title_label());
  ASSERT_TRUE(system_nudge_view->body_label());
  EXPECT_TRUE(system_nudge_view->first_button());
  EXPECT_TRUE(system_nudge_view->second_button());

  // Test that text labels max width is set correctly.
  EXPECT_EQ(kLabelMaxWidth_NudgeWithoutLeadingImage,
            system_nudge_view->body_label()->GetMaximumWidth());
}

TEST_F(SystemNudgeViewTest, TitleAndLeadingImage) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  auto* contents_view =
      widget->SetContentsView(std::make_unique<views::View>());

  // Set up base nudge data and add a title and an image model.
  auto nudge_data = CreateBaseNudgeData(contents_view);
  nudge_data.title_text = u"Title";
  nudge_data.image_model = ui::ImageModel::FromVectorIcon(
      vector_icons::kDogfoodIcon, kColorAshIconColorPrimary, kImageViewSize);

  auto* system_nudge_view = contents_view->AddChildView(
      std::make_unique<SystemNudgeView>(nudge_data));

  // Test that appropriate nudge elements were created.
  EXPECT_TRUE(system_nudge_view->image_view());
  EXPECT_TRUE(system_nudge_view->title_label());
  ASSERT_TRUE(system_nudge_view->body_label());
  EXPECT_FALSE(system_nudge_view->first_button());
  EXPECT_FALSE(system_nudge_view->second_button());

  // Test that text labels max width is set correctly.
  EXPECT_EQ(kLabelMaxWidth_NudgeWithLeadingImage,
            system_nudge_view->body_label()->GetMaximumWidth());
}

TEST_F(SystemNudgeViewTest, ToastStyleNudge) {
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  auto* contents_view =
      widget->SetContentsView(std::make_unique<views::View>());

  // Set up base nudge data, have it use "toast style" and add a button.
  auto nudge_data = CreateBaseNudgeData(contents_view);
  nudge_data.first_button_text = u"Button";
  nudge_data.use_toast_style = true;

  auto* system_nudge_view = contents_view->AddChildView(
      std::make_unique<SystemNudgeView>(nudge_data));

  // Test that appropriate nudge elements were created.
  EXPECT_FALSE(system_nudge_view->image_view());
  EXPECT_FALSE(system_nudge_view->title_label());
  ASSERT_TRUE(system_nudge_view->body_label());
  EXPECT_TRUE(system_nudge_view->first_button());
  EXPECT_FALSE(system_nudge_view->second_button());

  // Test that text labels max width is set correctly.
  EXPECT_EQ(kLabelMaxWidth_ToastStyleNudge,
            system_nudge_view->body_label()->GetMaximumWidth());
}

}  // namespace ash
