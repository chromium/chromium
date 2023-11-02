// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/icon_switch.h"
#include "ash/test/ash_test_base.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

const gfx::VectorIcon& kTestIcon = kFolderIcon;

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

}  // namespace ash
