// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_interactive_test_base.h"

#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/password_manager/passwords_navigation_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/events/keycodes/dom_us_layout_data.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace {
enum ReturnCodes {  // Possible results of the JavaScript code.
  RETURN_CODE_OK,
  RETURN_CODE_NO_ELEMENT,
  RETURN_CODE_WRONG_VALUE,
  RETURN_CODE_INVALID,
};
}  // namespace

PasswordManagerInteractiveTestBase::PasswordManagerInteractiveTestBase() =
    default;

PasswordManagerInteractiveTestBase::~PasswordManagerInteractiveTestBase() =
    default;

void PasswordManagerInteractiveTestBase::FillElementWithValue(
    const std::string& element_id,
    const std::string& value,
    const std::string& expected_value) {
  ASSERT_TRUE(content::ExecJs(
      RenderFrameHost(),
      base::StringPrintf("document.getElementById('%s').focus();",
                         element_id.c_str())));
  for (char16_t character : value) {
    ui::DomKey dom_key = ui::DomKey::FromCharacter(character);
    const ui::PrintableCodeEntry* code_entry =
        base::ranges::find_if(ui::kPrintableCodeMap,
                              [character](const ui::PrintableCodeEntry& entry) {
                                return entry.character[0] == character ||
                                       entry.character[1] == character;
                              });
    ASSERT_TRUE(code_entry != std::end(ui::kPrintableCodeMap));
    bool shift = code_entry->character[1] == character;
    ui::DomCode dom_code = code_entry->dom_code;
    content::SimulateKeyPress(WebContents(), dom_key, dom_code,
                              ui::DomCodeToUsLayoutKeyboardCode(dom_code),
                              false, shift, false, false);
  }
  // Enforce that the keystroke were processed. It's very important because
  // keystrokes aren't synchronized with JS and they take longer to process. The
  // test could move the focus later and divert the keystrokes to another input.
  WaitForElementValue(element_id, expected_value);
}

void PasswordManagerInteractiveTestBase::FillElementWithValue(
    const std::string& element_id,
    const std::string& value) {
  CheckElementValue(element_id, std::string());
  FillElementWithValue(element_id, value, value);
}

void PasswordManagerInteractiveTestBase::WaitForElementValue(
    const std::string& element_id,
    const std::string& expected_value) {
  const std::string value_check_function = base::StringPrintf(
      "function valueCheck() {"
      "  var element = document.getElementById('%s');"
      "  return element && element.value == '%s';"
      "}",
      element_id.c_str(), expected_value.c_str());
  const std::string script =
      value_check_function +
      base::StringPrintf(
          "new Promise(resolve => {"
          "  if (valueCheck()) {"
          "    resolve(%d);"
          "  } else {"
          "    var element = document.getElementById('%s');"
          "    if (!element)"
          "      resolve(%d);"
          "    element.oninput = function() {"
          "      if (valueCheck()) {"
          "        resolve(%d);"
          "        element.oninput = undefined;"
          "      }"
          "    };"
          "  }"
          "});",
          RETURN_CODE_OK, element_id.c_str(), RETURN_CODE_NO_ELEMENT,
          RETURN_CODE_OK);
  EXPECT_EQ(RETURN_CODE_OK,
            content::EvalJs(RenderFrameHost(), script,
                            content::EXECUTE_SCRIPT_NO_USER_GESTURE))
      << "element_id = " << element_id
      << ", expected_value = " << expected_value;
}

void PasswordManagerInteractiveTestBase::VerifyPasswordIsSavedAndFilled(
    const std::string& filename,
    const std::string& username_id,
    const std::string& password_id,
    const std::string& submission_script) {
  EXPECT_FALSE(password_id.empty());

  NavigateToFile(filename);

  PasswordsNavigationObserver observer(WebContents());
  const char kUsername[] = "user";
  const char kPassword[] = "123";
  if (!username_id.empty()) {
    FillElementWithValue(username_id, kUsername);
  }
  FillElementWithValue(password_id, kPassword);
  ASSERT_TRUE(content::ExecJs(RenderFrameHost(), submission_script));
  ASSERT_TRUE(observer.Wait());
  WaitForPasswordStore();

  BubbleObserver(WebContents()).AcceptSavePrompt();

  // Spin the message loop to make sure the password store had a chance to save
  // the password.
  WaitForPasswordStore();
  CheckThatCredentialsStored(username_id.empty() ? "" : kUsername, kPassword);

  NavigateToFile(filename);

  // Let the user interact with the page, so that DOM gets modification events,
  // needed for autofilling fields.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));

  // Wait until that interaction causes the password value to be revealed.
  if (!username_id.empty()) {
    WaitForElementValue(username_id, kUsername);
  }
  WaitForElementValue(password_id, kPassword);
}

// Erases all characters that have been typed into |field_id|.
void PasswordManagerInteractiveTestBase::SimulateUserDeletingFieldContent(
    const std::string& field_id) {
  SCOPED_TRACE(::testing::Message()
               << "SimulateUserDeletingFieldContent " << field_id);
  std::string focus("document.getElementById('" + field_id + "').focus();");
  ASSERT_TRUE(content::ExecJs(WebContents(), focus));
  std::string select("document.getElementById('" + field_id + "').select();");
  ASSERT_TRUE(content::ExecJs(WebContents(), select));
  content::SimulateKeyPress(WebContents(), ui::DomKey::BACKSPACE,
                            ui::DomCode::BACKSPACE, ui::VKEY_BACK, false, false,
                            false, false);
  // A test may rely on empty field value.
  WaitForElementValue(field_id, std::string());
}
