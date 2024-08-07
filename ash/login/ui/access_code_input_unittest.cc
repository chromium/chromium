// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/access_code_input.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/test/ash_test_base.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

namespace {
const int fixed_pin_length = 6;
}

class FixedLengthCodeInputTest : public AshTestBase {
 public:
  FixedLengthCodeInputTest() = default;
  FixedLengthCodeInputTest(const FixedLengthCodeInputTest&) = delete;
  FixedLengthCodeInputTest& operator=(const FixedLengthCodeInputTest&) = delete;
  ~FixedLengthCodeInputTest() override = default;

 protected:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    input_view_ = std::make_unique<FixedLengthCodeInput>(
        fixed_pin_length,
        base::BindRepeating(&FixedLengthCodeInputTest::OnInputChange,
                            base::Unretained(this)),
        base::BindRepeating(&FixedLengthCodeInputTest::OnEnter,
                            base::Unretained(this)),
        base::BindRepeating(&FixedLengthCodeInputTest::OnEscape,
                            base::Unretained(this)),
        /*obscure_pin=*/false);
    obscure_input_view_ = std::make_unique<FixedLengthCodeInput>(
        fixed_pin_length,
        base::BindRepeating(&FixedLengthCodeInputTest::OnInputChange,
                            base::Unretained(this)),
        base::BindRepeating(&FixedLengthCodeInputTest::OnEnter,
                            base::Unretained(this)),
        base::BindRepeating(&FixedLengthCodeInputTest::OnEscape,
                            base::Unretained(this)),
        /*obscure_pin=*/true);
  }

  void TearDown() override { AshTestBase::TearDown(); }

  void OnInputChange(bool last_field_active, bool complete) {
    ++on_input_change_count;
    if (complete) {
      ++on_input_change_complete_count;
    }
  }

  void OnEnter() { ++on_enter_count; }

  void OnEscape() { ++on_escape_count; }

  std::unique_ptr<FixedLengthCodeInput> input_view_;
  std::unique_ptr<FixedLengthCodeInput> obscure_input_view_;

  int on_input_change_count = 0;
  int on_input_change_complete_count = 0;
  int on_enter_count = 0;
  int on_escape_count = 0;
};

// Validates that the FixedLengthCodeInput::ContentsChanged() method handles
// correctly when the Textfield::InsertText() method is called with a digit.
TEST_F(FixedLengthCodeInputTest, ContentsChangedWithDigits) {
  FixedLengthCodeInput::TestApi test_api(input_view_.get());
  for (int index = 1; index <= fixed_pin_length; ++index) {
    int active_index = test_api.GetActiveIndex();
    EXPECT_EQ(active_index + 1, index);
    views::Textfield* textfield = test_api.GetInputTextField(active_index);
    textfield->InsertText(
        base::NumberToString16(index),
        ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
    EXPECT_EQ(on_input_change_count, index);
    EXPECT_EQ(on_input_change_complete_count,
              (index == fixed_pin_length ? 1 : 0));
  }
  std::optional<std::string> code = test_api.GetCode();
  EXPECT_TRUE(code.has_value());
  EXPECT_EQ(code.value(), "123456");
  EXPECT_EQ(on_enter_count, 0);
  EXPECT_EQ(on_escape_count, 0);
}

TEST_F(FixedLengthCodeInputTest, AccessibleProperties) {
  ui::AXNodeData data;

  input_view_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_FALSE(data.HasState(ax::mojom::State::kProtected));
  EXPECT_EQ(data.role, ax::mojom::Role::kTextField);

  data = ui::AXNodeData();
  obscure_input_view_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_TRUE(data.HasState(ax::mojom::State::kProtected));
}

TEST_F(FixedLengthCodeInputTest, AccessibilityTextSelectionBound) {
  ui::AXNodeData data;

  input_view_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kTextSelStart), 0);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd), 0);

  input_view_->InsertDigit(4);
  input_view_->InsertDigit(4);
  input_view_->InsertDigit(4);
  data = ui::AXNodeData();
  input_view_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kTextSelStart), 3);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd), 3);

  input_view_->Backspace();
  data = ui::AXNodeData();
  input_view_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kTextSelStart), 2);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd), 2);

  input_view_->ClearInput();
  data = ui::AXNodeData();
  input_view_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kTextSelStart), 0);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd), 1);

  input_view_->InsertDigit(4);
  data = ui::AXNodeData();
  input_view_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kTextSelStart), 1);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd), 1);
}

// Validates that the FixedLengthCodeInput::ContentsChanged() method handles
// and ignores correctly when the Textfield::InsertText() method is called
// with multipledigit.
TEST_F(FixedLengthCodeInputTest, ContentsChangedWithMultipleDigits) {
  FixedLengthCodeInput::TestApi test_api(input_view_.get());
  int active_index = test_api.GetActiveIndex();
  EXPECT_EQ(active_index, 0);
  EXPECT_EQ(on_input_change_count, 0);

  auto CheckInsertIgnored = [&]() {
    std::optional<std::string> code = test_api.GetCode();
    EXPECT_FALSE(code.has_value());
    EXPECT_EQ(test_api.GetActiveIndex(), 0);
    EXPECT_EQ(on_input_change_count, 0);
    EXPECT_EQ(on_enter_count, 0);
    EXPECT_EQ(on_escape_count, 0);
  };

  views::Textfield* textfield = test_api.GetInputTextField(active_index);
  textfield->InsertText(
      u"01",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  CheckInsertIgnored();

  textfield->InsertText(
      u"12",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  CheckInsertIgnored();

  textfield->InsertText(
      u"004",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  CheckInsertIgnored();

  textfield->InsertText(
      u"987",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  CheckInsertIgnored();
}

// Validates that the FixedLengthCodeInput::ContentsChanged() method handles
// and ignores correctly when the Textfield::InsertText() method is called
// with non numerical strings.
TEST_F(FixedLengthCodeInputTest, ContentsChangedWithNonNumericalStrings) {
  FixedLengthCodeInput::TestApi test_api(input_view_.get());

  int active_index = test_api.GetActiveIndex();
  EXPECT_EQ(active_index, 0);
  EXPECT_EQ(on_input_change_count, 0);

  auto CheckInsertIgnored = [&]() {
    std::optional<std::string> code = test_api.GetCode();
    EXPECT_FALSE(code.has_value());
    EXPECT_EQ(test_api.GetActiveIndex(), 0);
    EXPECT_EQ(on_input_change_count, 0);
    EXPECT_EQ(on_enter_count, 0);
    EXPECT_EQ(on_escape_count, 0);
  };

  views::Textfield* textfield = test_api.GetInputTextField(active_index);
  textfield->InsertText(
      u"a",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  CheckInsertIgnored();

  textfield->InsertText(
      u"xz",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  CheckInsertIgnored();

  textfield->InsertText(
      u"/",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  CheckInsertIgnored();

  textfield->InsertText(
      u"", ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  CheckInsertIgnored();
}

}  // namespace ash
