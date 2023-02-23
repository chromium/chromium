// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/checkbox_group.h"
#include "ash/style/icon_button.h"
#include "ash/style/icon_switch.h"
#include "ash/style/radio_button_group.h"
#include "ash/style/tab_slider.h"
#include "ash/style/tab_slider_button.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_test_api.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

const gfx::VectorIcon& kTestIcon = kFolderIcon;

enum class TabSliderType {
  kIconSlider,
  kLabelSlider,
  kIconLabelSlider,
};

// A WidgetDelegateView with given component as the only content.
class WidgetWithSystemUIComponentView : public views::WidgetDelegateView {
 public:
  explicit WidgetWithSystemUIComponentView(
      std::unique_ptr<views::View> component) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(std::move(component));
  }
  WidgetWithSystemUIComponentView(const WidgetWithSystemUIComponentView&) =
      delete;
  WidgetWithSystemUIComponentView& operator=(
      const WidgetWithSystemUIComponentView&) = delete;
  ~WidgetWithSystemUIComponentView() override = default;
};

// Creates a test widget with given component as the only content.
std::unique_ptr<views::Widget> CreateWidgetWithComponent(
    std::unique_ptr<views::View> component) {
  return AshTestBase::CreateTestWidget(
      new WidgetWithSystemUIComponentView(std::move(component)));
}

}  // namespace

using SystemComponentsTest = AshTestBase;

// TODO(crbug/1384370): Disable for constant failure.
TEST_F(SystemComponentsTest,
       DISABLED_IconButtonWithBackgroundColorIdDoesNotCrash) {
  // Create an IconButton with an explicit background color ID.
  auto icon_button = std::make_unique<IconButton>(
      IconButton::PressedCallback(), IconButton::Type::kSmall, &kTestIcon,
      u"button 1",
      /*is_toggleable=*/false, /*has_border=*/false);
  auto* icon_button_ptr = icon_button.get();
  icon_button->SetBackgroundColorId(cros_tokens::kCrosSysSystemOnBase);
  auto widget = CreateWidgetWithComponent(std::move(icon_button));

  // Schedule a paint for the button.
  icon_button_ptr->SchedulePaint();
  EXPECT_TRUE(views::ViewTestApi(icon_button_ptr).needs_paint());

  // Spin the message loop so the button paints.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(views::ViewTestApi(icon_button_ptr).needs_paint());

  // No crash.
}

TEST_F(SystemComponentsTest, IconSwitch) {
  auto icon_switch = std::make_unique<IconSwitch>();

  // Add three toggle buttons.
  auto* button_1 = icon_switch->AddButton(IconButton::PressedCallback(),
                                          &kTestIcon, u"button 1");
  auto* button_2 = icon_switch->AddButton(IconButton::PressedCallback(),
                                          &kTestIcon, u"button 2");
  auto* button_3 = icon_switch->AddButton(IconButton::PressedCallback(),
                                          &kTestIcon, u"button 3");

  auto* switch_raw_ptr = icon_switch.get();
  auto widget = CreateWidgetWithComponent(std::move(icon_switch));

  // All the buttons should be in untoggled state.
  EXPECT_FALSE(button_1->toggled());
  EXPECT_FALSE(button_2->toggled());
  EXPECT_FALSE(button_3->toggled());

  // Toggle the first button by using `IconButton::SetToggled`.
  button_1->SetToggled(true);
  // Only the first button is toggled.
  EXPECT_TRUE(button_1->toggled());
  EXPECT_FALSE(button_2->toggled());
  EXPECT_FALSE(button_3->toggled());

  // Toggle the second button by mouse clicking.
  LeftClickOn(button_2);
  // Only the second button is toggled.
  EXPECT_FALSE(button_1->toggled());
  EXPECT_TRUE(button_2->toggled());
  EXPECT_FALSE(button_3->toggled());

  // Toggle the third button by using `IconSwitch::Toggle`.
  switch_raw_ptr->ToggleButtonOnAtIndex(2);
  // Only the third button is toggled.
  EXPECT_FALSE(button_1->toggled());
  EXPECT_FALSE(button_2->toggled());
  EXPECT_TRUE(button_3->toggled());

  // Using `SetToggled` again on the first button will untoggle the other
  // buttons.
  button_1->SetToggled(true);
  // Only the first button is toggled.
  EXPECT_TRUE(button_1->toggled());
  EXPECT_FALSE(button_2->toggled());
  EXPECT_FALSE(button_3->toggled());

  // Disabling icon switch makes all toggle buttons disabled.
  switch_raw_ptr->SetEnabled(false);
  EXPECT_FALSE(button_1->GetEnabled());
  EXPECT_FALSE(button_2->GetEnabled());
  EXPECT_FALSE(button_3->GetEnabled());
}

// Tests that when one button is selected in the radio button group, the others
// will be unselected automatically.
TEST_F(SystemComponentsTest, RadioButtonGroup) {
  std::unique_ptr<RadioButtonGroup> radio_button_group =
      std::make_unique<RadioButtonGroup>(198);

  // Add three buttons to the group.
  auto* button_1 = radio_button_group->AddButton(RadioButton::PressedCallback(),
                                                 u"Test Button1");
  auto* button_2 = radio_button_group->AddButton(RadioButton::PressedCallback(),
                                                 u"Test Button2");
  auto* button_3 = radio_button_group->AddButton(RadioButton::PressedCallback(),
                                                 u"Test Button3");

  auto* switch_raw_ptr = radio_button_group.get();
  auto widget = CreateWidgetWithComponent(std::move(radio_button_group));

  // All the buttons should be in unselected state.
  EXPECT_FALSE(button_1->selected());
  EXPECT_FALSE(button_2->selected());
  EXPECT_FALSE(button_3->selected());

  // select the first button by using `RadioButton::SetSelected`.
  button_1->SetSelected(true);
  // Only the first button is selected.
  EXPECT_TRUE(button_1->selected());
  EXPECT_FALSE(button_2->selected());
  EXPECT_FALSE(button_3->selected());

  // Select the second button by mouse clicking.
  LeftClickOn(button_2);
  // Only the second button is selected.
  EXPECT_FALSE(button_1->selected());
  EXPECT_TRUE(button_2->selected());
  EXPECT_FALSE(button_3->selected());

  // Select the third button by using `RadioButtonGroup::SelectButtonAtIndex`.
  switch_raw_ptr->SelectButtonAtIndex(2);
  // Only the third button is selected.
  EXPECT_FALSE(button_1->selected());
  EXPECT_FALSE(button_2->selected());
  EXPECT_TRUE(button_3->selected());

  // Using `SetSelected` again on the first button will unselect the other
  // buttons.
  button_1->SetSelected(true);
  // Only the first button is selected.
  EXPECT_TRUE(button_1->selected());
  EXPECT_FALSE(button_2->selected());
  EXPECT_FALSE(button_3->selected());

  // Disabling radio button group makes all radio buttons disabled.
  switch_raw_ptr->SetEnabled(false);
  EXPECT_FALSE(button_1->GetEnabled());
  EXPECT_FALSE(button_2->GetEnabled());
  EXPECT_FALSE(button_3->GetEnabled());
}

// Tests that all buttons in the checkbox group can be selected / unselected.
// Click on a selected button will unselect, and vice versa。
TEST_F(SystemComponentsTest, CheckboxGroup) {
  std::unique_ptr<CheckboxGroup> checkbox_group =
      std::make_unique<CheckboxGroup>(198);

  // Add four buttons to the group.
  auto* button_1 =
      checkbox_group->AddButton(Checkbox::PressedCallback(), u"Test Button1");
  auto* button_2 =
      checkbox_group->AddButton(Checkbox::PressedCallback(), u"Test Button2");
  auto* button_3 =
      checkbox_group->AddButton(Checkbox::PressedCallback(), u"Test Button3");
  auto* button_4 =
      checkbox_group->AddButton(Checkbox::PressedCallback(), u"Test Button4");

  auto* switch_raw_ptr = checkbox_group.get();
  auto widget = CreateWidgetWithComponent(std::move(checkbox_group));

  // All the buttons should be in unselected state.
  EXPECT_FALSE(button_1->selected());
  EXPECT_FALSE(button_2->selected());
  EXPECT_FALSE(button_3->selected());
  EXPECT_FALSE(button_4->selected());

  // select the first button by using `Checkbox::SetSelected`.
  button_1->SetSelected(true);
  // The first button is selected.
  EXPECT_TRUE(button_1->selected());
  EXPECT_FALSE(button_2->selected());
  EXPECT_FALSE(button_3->selected());
  EXPECT_FALSE(button_4->selected());

  // Select the second button by mouse clicking.
  LeftClickOn(button_2);
  // The first and second buttons are selected.
  EXPECT_TRUE(button_1->selected());
  EXPECT_TRUE(button_2->selected());
  EXPECT_FALSE(button_3->selected());
  EXPECT_FALSE(button_4->selected());

  // Click on the second button again, it should be unselected.
  LeftClickOn(button_2);
  EXPECT_FALSE(button_2->selected());

  // Select the third button by using `CheckboxGroup::SelectButtonAtIndex`.
  switch_raw_ptr->SelectButtonAtIndex(2);
  // The third button should be selected.
  EXPECT_TRUE(button_3->selected());

  // Using `SetSelected` on the fourth button.
  button_4->SetSelected(true);
  // All buttons should be selected except the second one.
  EXPECT_TRUE(button_1->selected());
  EXPECT_FALSE(button_2->selected());
  EXPECT_TRUE(button_3->selected());
  EXPECT_TRUE(button_4->selected());

  // Disabling radio button group makes all radio buttons disabled.
  switch_raw_ptr->SetEnabled(false);
  EXPECT_FALSE(button_1->GetEnabled());
  EXPECT_FALSE(button_2->GetEnabled());
  EXPECT_FALSE(button_3->GetEnabled());
  EXPECT_FALSE(button_4->GetEnabled());
}

struct TabSliderTestParams {
  TabSliderType type;
  bool distribute_space_evenly;
  absl::optional<TabSlider::LayoutParams> custom_layout;
  int button_num;
  std::vector<std::u16string> labels_text;
};

class TabSliderTest : public SystemComponentsTest,
                      public testing::WithParamInterface<TabSliderTestParams> {
 public:
  TabSliderTest() = default;
  TabSliderTest(const TabSliderTest&) = delete;
  TabSliderTest& operator=(const TabSliderTest&) = delete;
  ~TabSliderTest() override = default;
};

const TabSliderTestParams kTabSliderLayoutTestParams[] = {
    // IconSliderButton
    // Two buttons with evenly distributed space and recommended layout.
    {TabSliderType::kIconSlider,
     /*distribute_space_evenly=*/true,
     absl::nullopt,
     /*button_num=*/2,
     {u"", u""}},
    // Two buttons with unevenly distributed space and recommended layout.
    {TabSliderType::kIconSlider,
     /*distribute_space_evenly=*/false,
     absl::nullopt,
     /*button_num=*/2,
     {u"", u""}},
    // Three buttons with evenly distributed space and recommended layout.
    {TabSliderType::kIconSlider,
     /*distribute_space_evenly=*/true,
     absl::nullopt,
     /*button_num=*/3,
     {u"", u"", u""}},
    // Three buttons with unevenly distributed space and recommended layout.
    {TabSliderType::kIconSlider,
     /*distribute_space_evenly=*/false,
     absl::nullopt,
     /*button_num=*/3,
     {u"", u"", u""}},
    // Two buttons with evenly distributed space and customized layout.
    {TabSliderType::kIconSlider,
     /*distribute_space_evenly=*/true,
     TabSlider::LayoutParams{3, 5},
     /*button_num=*/2,
     {u"", u""}},
    // Two buttons with unevenly distributed space and customized layout.
    {TabSliderType::kIconSlider,
     /*distribute_space_evenly=*/false,
     TabSlider::LayoutParams{3, 5},
     /*button_num=*/2,
     {u"", u""}},
    // Three buttons with evenly distributed space and customized layout.
    {TabSliderType::kIconSlider,
     /*distribute_space_evenly=*/true,
     TabSlider::LayoutParams{3, 5},
     /*button_num=*/3,
     {u"", u"", u""}},
    // Three buttons with unevenly distributed space and customized layout.
    {TabSliderType::kIconSlider,
     /*distribute_space_evenly=*/false,
     TabSlider::LayoutParams{3, 5},
     /*button_num=*/3,
     {u"", u"", u""}},

    // LabelSliderButton
    // Two buttons with evenly distributed space and recommended layout.
    {TabSliderType::kLabelSlider,
     /*distribute_space_evenly=*/true,
     absl::nullopt,
     /*button_num=*/2,
     {u"one", u"one two three"}},
    // Two buttons with unevenly distributed space and recommended layout.
    {TabSliderType::kLabelSlider,
     /*distribute_space_evenly=*/false,
     absl::nullopt,
     /*button_num=*/2,
     {u"one", u"one two three"}},
    // Three buttons with evenly distributed space and recommended layout.
    {TabSliderType::kLabelSlider,
     /*distribute_space_evenly=*/true,
     absl::nullopt,
     /*button_num=*/3,
     {u"one", u"one two three", u"one two three four five"}},
    // Three buttons with unevenly distributed space and recommended layout.
    {TabSliderType::kLabelSlider,
     /*distribute_space_evenly=*/false,
     absl::nullopt,
     /*button_num=*/3,
     {u"one", u"one two three", u"one two three four five"}},
    // Two buttons with evenly distributed space and customized layout.
    {TabSliderType::kLabelSlider,
     /*distribute_space_evenly=*/true,
     TabSlider::LayoutParams{3, 5},
     /*button_num=*/2,
     {u"one", u"one two three"}},
    // Two buttons with unevenly distributed space and customized layout.
    {TabSliderType::kLabelSlider,
     /*distribute_space_evenly=*/false,
     TabSlider::LayoutParams{3, 5},
     /*button_num=*/2,
     {u"one", u"one two three"}},
    // Three buttons with evenly distributed space and customized layout.
    {TabSliderType::kLabelSlider,
     /*distribute_space_evenly=*/true,
     TabSlider::LayoutParams{3, 5},
     /*button_num=*/3,
     {u"one", u"one two three", u"one two three four five"}},
    // Three buttons with unevenly distributed space and customized layout.
    {TabSliderType::kLabelSlider,
     /*distribute_space_evenly=*/false,
     TabSlider::LayoutParams{3, 5},
     /*button_num=*/3,
     {u"one", u"one two three", u"one two three four five"}},

    // IconLabelSliderButton
    // Two buttons with evenly distributed space and recommended layout.
    {TabSliderType::kIconLabelSlider,
     /*distribute_space_evenly=*/true,
     absl::nullopt,
     /*button_num=*/2,
     {u"one", u"one two three"}},
    // Two buttons with unevenly distributed space and recommended layout.
    {TabSliderType::kIconLabelSlider,
     /*distribute_space_evenly=*/false,
     absl::nullopt,
     /*button_num=*/2,
     {u"one", u"one two three"}},
    // Three buttons with evenly distributed space and recommended layout.
    {TabSliderType::kIconLabelSlider,
     /*distribute_space_evenly=*/true,
     absl::nullopt,
     /*button_num=*/3,
     {u"one", u"one two three", u"one two three four five"}},
    // Three buttons with unevenly distributed space and recommended layout.
    {TabSliderType::kIconLabelSlider,
     /*distribute_space_evenly=*/false,
     absl::nullopt,
     /*button_num=*/3,
     {u"one", u"one two three", u"one two three four five"}},
    // Two buttons with evenly distributed space and customized layout.
    {TabSliderType::kIconLabelSlider,
     /*distribute_space_evenly=*/true,
     TabSlider::LayoutParams{3, 5},
     /*button_num=*/2,
     {u"one", u"one two three"}},
    // Two buttons with unevenly distributed space and customized layout.
    {TabSliderType::kIconLabelSlider,
     /*distribute_space_evenly=*/false,
     TabSlider::LayoutParams{3, 5},
     /*button_num=*/2,
     {u"one", u"one two three"}},
    // Three buttons with evenly distributed space and customized layout.
    {TabSliderType::kIconLabelSlider,
     /*distribute_space_evenly=*/true,
     TabSlider::LayoutParams{3, 5},
     /*button_num=*/3,
     {u"one", u"one two three", u"one two three four five"}},
    // Three buttons with unevenly distributed space and customized layout.
    {TabSliderType::kIconLabelSlider,
     /*distribute_space_evenly=*/false,
     TabSlider::LayoutParams{3, 5},
     /*button_num=*/3,
     {u"one", u"one two three", u"one two three four five"}},
};

INSTANTIATE_TEST_SUITE_P(TabSliderStyle,
                         TabSliderTest,
                         testing::ValuesIn(kTabSliderLayoutTestParams));

// Tests tab slider layout works properly with different layout settings.
TEST_P(TabSliderTest, TabSliderLayout) {
  TabSliderTestParams params = GetParam();

  // Create a tab slider.
  auto tab_slider =
      std::make_unique<TabSlider>(true, true, params.distribute_space_evenly);

  // Add slider buttons according to the testing parameters.
  TabSlider::LayoutParams layout_params;
  int max_button_width = 0;
  int max_button_height = 0;
  std::vector<TabSliderButton*> buttons(params.button_num, nullptr);
  for (int i = 0; i < params.button_num; i++) {
    switch (params.type) {
      case TabSliderType::kIconSlider:
        buttons[i] = tab_slider->AddButton<IconSliderButton>(
            IconSliderButton::PressedCallback(), &kTestIcon,
            u"icon slider button");
        break;
      case TabSliderType::kLabelSlider:
        buttons[i] = tab_slider->AddButton<LabelSliderButton>(
            LabelSliderButton::PressedCallback(), params.labels_text[i],
            u"label slider button");
        break;
      case TabSliderType::kIconLabelSlider:
        buttons[i] = tab_slider->AddButton<IconLabelSliderButton>(
            IconLabelSliderButton::PressedCallback(), &kTestIcon,
            params.labels_text[i], u"icon label slider button");
        break;
    }

    // Cache the recommended layout provided by the slider button.
    if (auto recommended_layout = buttons[i]->GetRecommendedSliderLayout()) {
      layout_params = *recommended_layout;
    }

    // Cache the maximum size of the button.
    gfx::Size pref_size = buttons[i]->GetPreferredSize();
    max_button_width = std::max(max_button_width, pref_size.width());
    max_button_height = std::max(max_button_height, pref_size.height());
  }

  // If using customized layout, overwrite the current layout.
  if (params.custom_layout) {
    tab_slider->SetCustomLayout(*params.custom_layout);
    layout_params = *params.custom_layout;
  }

  // Attach the tab slider to a widget.
  auto widget = CreateWidgetWithComponent(std::move(tab_slider));
  int x = layout_params.internal_border_padding;
  const int y = layout_params.internal_border_padding;
  // Check if the layout works properly.
  for (int i = 0; i < params.button_num; i++) {
    const gfx::Size pref_size = buttons[i]->GetPreferredSize();
    const int expect_width =
        params.distribute_space_evenly ? max_button_width : pref_size.width();
    const int expect_height =
        params.distribute_space_evenly ? max_button_height : pref_size.height();
    EXPECT_EQ(buttons[i]->bounds(),
              gfx::Rect(x, y, expect_width, expect_height));
    x += expect_width + layout_params.between_buttons_spacing;
  }
}

}  // namespace ash
