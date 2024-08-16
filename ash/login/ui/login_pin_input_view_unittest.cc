// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_pin_input_view.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/login/ui/login_test_base.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"

namespace ash {

class LoginPinInputViewTest
    : public LoginTestBase,
      public ::testing::WithParamInterface<int> /* length */ {
 public:
  static std::string ParamInfoToString(
      testing::TestParamInfo<LoginPinInputViewTest::ParamType> info) {
    return base::StrCat({"Length", base::NumberToString(info.param)});
  }

 protected:
  LoginPinInputViewTest() = default;
  ~LoginPinInputViewTest() override = default;

  void SetUp() override {
    LoginTestBase::SetUp();
    view_ = new LoginPinInputView();
    view_->Init(base::BindRepeating(&LoginPinInputViewTest::OnPinSubmit,
                                    base::Unretained(this)),
                base::BindRepeating(&LoginPinInputViewTest::OnPinChanged,
                                    base::Unretained(this)));

    length_ = GetParam();
    view_->UpdateLength(length_);
    SetWidget(CreateWidgetWithContent(view_));
  }

  void OnPinSubmit(const std::u16string& pin) {
    submitted_pin_ = std::make_optional(pin);
  }

  void OnPinChanged(const bool is_empty) {
    is_empty_ = std::make_optional(is_empty);
  }

  void PressKeyHelper(ui::KeyboardCode key) {
    GetEventGenerator()->PressKey(key, ui::EF_NONE);
    // Wait until the keypress is processed.
    base::RunLoop().RunUntilIdle();
  }

  void ExpectAttribute(const std::string& value,
                       ax::mojom::StringAttribute attribute) {
    LoginPinInputView::TestApi test_api(view_);
    ui::AXNodeData node_data;
    test_api.code_input()->GetViewAccessibility().GetAccessibleNodeData(
        &node_data);
    EXPECT_EQ(value, node_data.GetStringAttribute(attribute));
  }

  void ExpectDescription(const std::string& value) {
    LoginPinInputView::TestApi test_api(view_);
    EXPECT_EQ(
        base::UTF8ToUTF16(value),
        test_api.code_input()->GetViewAccessibility().GetCachedDescription());
  }

  void ExpectTextValue(const std::string& value) {
    ExpectAttribute(value, ax::mojom::StringAttribute::kValue);
  }

  raw_ptr<LoginPinInputView, DanglingUntriaged> view_ = nullptr;
  int length_ = 0;

  // Generated during the callback response.
  std::optional<std::u16string> submitted_pin_;
  std::optional<bool> is_empty_;
};

// Verifies that pressing 'Return' on the PIN input field triggers an
// unlock attempt by calling OnSubmit with an empty PIN.
TEST_P(LoginPinInputViewTest, PressingReturnTriggersUnlockWithEmptyPin) {
  // Hitting 'Return' should not trigger 'OnSubmit' with an empty PIN when not
  // allowed.
  view_->SetAuthenticateWithEmptyPinOnReturnKey(false);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  ASSERT_FALSE(submitted_pin_.has_value());

  // Hitting 'Return' should trigger 'OnSubmit' with an empty PIN.
  view_->SetAuthenticateWithEmptyPinOnReturnKey(true);
  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  ASSERT_TRUE(submitted_pin_.has_value());
  EXPECT_EQ(u"", *submitted_pin_);
}

// Tests that ChromeVox announces "Enter your PIN" when the
// field gets focused
TEST_P(LoginPinInputViewTest, AccessibleName) {
  ExpectAttribute("Enter your PIN", ax::mojom::StringAttribute::kName);
}

// Tests that ChromeVox announces "X digits remaining" when the
// field gets focused
TEST_P(LoginPinInputViewTest, AccessibleValues) {
  ExpectDescription("6 digits remaining");
  ExpectTextValue("      ");

  PressKeyHelper(ui::KeyboardCode::VKEY_1);
  ExpectDescription("5 digits remaining");
  ExpectTextValue("\u2022     "); /* 1 bullet 5 spaces */

  PressKeyHelper(ui::KeyboardCode::VKEY_1);
  ExpectDescription("4 digits remaining");
  ExpectTextValue("\u2022\u2022    "); /* 2 bullets 4 spaces */

  PressKeyHelper(ui::KeyboardCode::VKEY_1);
  ExpectDescription("3 digits remaining");
  ExpectTextValue("\u2022\u2022\u2022   "); /* 3 bullets 3 spaces */

  PressKeyHelper(ui::KeyboardCode::VKEY_1);
  ExpectTextValue("\u2022\u2022\u2022\u2022  "); /* 4 bullets 2 spaces */
  ExpectDescription("2 digits remaining");

  PressKeyHelper(ui::KeyboardCode::VKEY_1);
  ExpectTextValue("\u2022\u2022\u2022\u2022\u2022 "); /* 5 bullets 1 space */
  ExpectDescription("One digit remaining");
}

TEST_P(LoginPinInputViewTest, ReadOnly) {
  EXPECT_FALSE(view_->IsReadOnly());
  view_->SetReadOnly(true);
  EXPECT_TRUE(view_->IsReadOnly());
  ExpectTextValue("      ");

  // Keys are ignored in the read-only mode.
  PressKeyHelper(ui::KeyboardCode::VKEY_1);
  ExpectTextValue("      ");
  PressKeyHelper(ui::KeyboardCode::VKEY_RETURN);
  EXPECT_FALSE(submitted_pin_.has_value());

  // After unsetting the read-only mode, keys start working again.
  view_->SetReadOnly(false);
  PressKeyHelper(ui::KeyboardCode::VKEY_1);
  ExpectTextValue("\u2022     "); /* 1 bullet 5 spaces */
}

INSTANTIATE_TEST_SUITE_P(PinInputViewTests,
                         LoginPinInputViewTest,
                         testing::Values(6),
                         LoginPinInputViewTest::ParamInfoToString);

}  // namespace ash
