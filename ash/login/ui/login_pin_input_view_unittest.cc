// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_pin_input_view.h"
#include <memory>
#include <string>
#include "ash/login/ui/login_test_base.h"
#include "base/bind.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
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

  void OnPinSubmit(const base::string16& pin) {
    submitted_pin_ = base::make_optional(pin);
  }

  void OnPinChanged(const bool is_empty) {
    is_empty_ = base::make_optional(is_empty);
  }

  void PressKeyHelper(ui::KeyboardCode key) {
    GetEventGenerator()->PressKey(key, ui::EF_NONE);
    // Wait until the keypress is processed.
    base::RunLoop().RunUntilIdle();
  }

  void ExpectAttribute(const std::string& value,
                       ax::mojom::StringAttribute attribute) {
    LoginPinInputView::TestApi test_api(view_);
    ui::AXNodeData ax_node_data;
    test_api.code_input()->GetAccessibleNodeData(&ax_node_data);
    EXPECT_EQ(value, ax_node_data.GetStringAttribute(attribute));
  }

  void ExpectDescription(const std::string& value) {
    ExpectAttribute(value, ax::mojom::StringAttribute::kDescription);
  }

  void ExpectTextValue(const std::string& value) {
    ExpectAttribute(value, ax::mojom::StringAttribute::kValue);
  }

  LoginPinInputView* view_ = nullptr;
  int length_ = 0;

  // Generated during the callback response.
  base::Optional<base::string16> submitted_pin_;
  base::Optional<bool> is_empty_;
};

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
  ExpectTextValue("\u2022     ");                     /* 1 bullet 5 spaces */

  PressKeyHelper(ui::KeyboardCode::VKEY_1);
  ExpectDescription("4 digits remaining");
  ExpectTextValue("\u2022\u2022    ");                /* 2 bullets 4 spaces */

  PressKeyHelper(ui::KeyboardCode::VKEY_1);
  ExpectDescription("3 digits remaining");
  ExpectTextValue("\u2022\u2022\u2022   ");           /* 3 bullets 3 spaces */

  PressKeyHelper(ui::KeyboardCode::VKEY_1);
  ExpectTextValue("\u2022\u2022\u2022\u2022  ");      /* 4 bullets 2 spaces */
  ExpectDescription("2 digits remaining");

  PressKeyHelper(ui::KeyboardCode::VKEY_1);
  ExpectTextValue("\u2022\u2022\u2022\u2022\u2022 "); /* 5 bullets 1 space */
  ExpectDescription("One digit remaining");
}

INSTANTIATE_TEST_SUITE_P(PinInputViewTests,
                         LoginPinInputViewTest,
                         testing::Values(6),
                         LoginPinInputViewTest::ParamInfoToString);

}  // namespace ash
