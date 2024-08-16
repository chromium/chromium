// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/combobox.h"

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/glanceables/classroom/glanceables_classroom_student_view.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

class TestComboboxModel : public ui::ComboboxModel {
 public:
  // Overridden from ui::ComboboxModel:
  TestComboboxModel() = default;
  size_t GetItemCount() const override { return 10; }
  std::u16string GetItemAt(size_t index) const override {
    return u"Item " + base::NumberToString16(index);
  }
};

class ComboboxTest : public AshTestBase {
 public:
  ComboboxTest() = default;

  void SetUp() override {
    AshTestBase::SetUp();
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);

    auto combobox_model_ = std::make_unique<TestComboboxModel>();
    combobox_view_ = widget_->SetContentsView(
        std::make_unique<Combobox>(std::move(combobox_model_)));
    widget_->Show();
  }

  void TearDown() override {
    combobox_view_ = nullptr;
    widget_.reset();
    AshTestBase::TearDown();
  }

  Combobox* GetComboboxView() { return combobox_view_; }

  void ShowComboBoxDropDownMenu() { GetComboboxView()->ShowDropDownMenu(); }

  void CloseComboBoxDropDownMenu() { GetComboboxView()->CloseDropDownMenu(); }

 protected:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<Combobox> combobox_view_;
};

TEST_F(ComboboxTest, AccessibleCheckedState) {
  ShowComboBoxDropDownMenu();

  Combobox* combobox_view = GetComboboxView();
  combobox_view->SetSelectedIndex(1);

  ui::AXNodeData data;
  views::View* selected_combobox_menu_option =
      combobox_view->MenuItemAtIndex(1);
  selected_combobox_menu_option->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kTrue);

  data = ui::AXNodeData();
  views::View* nonselected_combobox_menu_option =
      combobox_view->MenuItemAtIndex(0);
  nonselected_combobox_menu_option->GetViewAccessibility()
      .GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kFalse);

  combobox_view->SetSelectedIndex(std::nullopt);

  CloseComboBoxDropDownMenu();
}

TEST_F(ComboboxTest, AccessibleDefaultActionVerb) {
  auto* combobox = GetComboboxView();
  ui::AXNodeData data;

  combobox->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetDefaultActionVerb(), ax::mojom::DefaultActionVerb::kOpen);

  combobox->SetEnabled(false);
  data = ui::AXNodeData();
  combobox->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetDefaultActionVerb(), ax::mojom::DefaultActionVerb::kOpen);

  // ComboboxMenuOption default action verb test.
  combobox->SetEnabled(true);
  ShowComboBoxDropDownMenu();
  combobox->SetSelectedIndex(1);
  auto* menu_option = combobox->MenuItemAtIndex(1);
  data = ui::AXNodeData();
  ASSERT_TRUE(menu_option);
  menu_option->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetDefaultActionVerb(), ax::mojom::DefaultActionVerb::kClick);

  menu_option->SetEnabled(false);
  data = ui::AXNodeData();
  menu_option->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetDefaultActionVerb(), ax::mojom::DefaultActionVerb::kClick);

  CloseComboBoxDropDownMenu();
}

}  // namespace ash
