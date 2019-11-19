// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/stl_util.h"
#include "chrome/browser/chromeos/input_method/textinput_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {
struct InputTypeExpectation {
  std::string node_id;
  ui::TextInputType type;
};
}  // namespace

using TextInput_TextInputStateChangedTest = TextInputTestBase;

IN_PROC_BROWSER_TEST_F(TextInput_TextInputStateChangedTest,
                       SwitchToPasswordFieldTest) {
  TextInputTestHelper helper(GetInputMethod());
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("textinput")),
      base::FilePath(FILE_PATH_LITERAL("ime_enable_disable_test.html")));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  bool worker_finished = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "window.domAutomationController.send(text01_focus());",
      &worker_finished));
  EXPECT_TRUE(worker_finished);
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, helper.GetTextInputType());

  worker_finished = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "window.domAutomationController.send(password01_focus());",
      &worker_finished));
  EXPECT_TRUE(worker_finished);
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, helper.GetTextInputType());
}

IN_PROC_BROWSER_TEST_F(TextInput_TextInputStateChangedTest, FocusOnLoadTest) {
  TextInputTestHelper helper(GetInputMethod());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("textinput")),
      base::FilePath(FILE_PATH_LITERAL("focus_input_on_load.html")));
  ui_test_utils::NavigateToURL(browser(), url);

  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, helper.GetTextInputType());
}

IN_PROC_BROWSER_TEST_F(TextInput_TextInputStateChangedTest,
                       FocusOnContentJSTest) {
  TextInputTestHelper helper(GetInputMethod());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("textinput")),
      base::FilePath(FILE_PATH_LITERAL("focus_input_on_content_js.html")));
  ui_test_utils::NavigateToURL(browser(), url);

  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, helper.GetTextInputType());
}

IN_PROC_BROWSER_TEST_F(TextInput_TextInputStateChangedTest,
                       MouseClickChange) {
  TextInputTestHelper helper(GetInputMethod());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("textinput")),
      base::FilePath(FILE_PATH_LITERAL("focus_input_with_mouse_click.html")));
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(tab);

  ASSERT_TRUE(helper.ClickElement("text_id", tab));
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, helper.GetTextInputType());

  ASSERT_TRUE(helper.ClickElement("password_id", tab));
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, helper.GetTextInputType());
}

IN_PROC_BROWSER_TEST_F(TextInput_TextInputStateChangedTest,
                       FocusChangeOnFocus) {
  TextInputTestHelper helper(GetInputMethod());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("textinput")),
      base::FilePath(FILE_PATH_LITERAL("focus_input_on_anothor_focus.html")));
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::string coordinate;
  ASSERT_TRUE(content::ExecuteScript(
          tab,
          "document.getElementById('text_id').focus();"));

  // Expects PASSWORD text input type because javascript will change the focus
  // to password field in #text_id's onfocus handler.
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, helper.GetTextInputType());

  ASSERT_TRUE(helper.ClickElement("text_id", tab));
  // Expects PASSWORD text input type because javascript will chagne the focus
  // to password field in #text_id's onfocus handler.
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, helper.GetTextInputType());
}

IN_PROC_BROWSER_TEST_F(TextInput_TextInputStateChangedTest,
                       NodeEliminationCase) {
  TextInputTestHelper helper(GetInputMethod());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("textinput")),
      base::FilePath(FILE_PATH_LITERAL("simple_textinput.html")));
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(content::ExecuteScript(
          tab,
          "document.getElementById('text_id').focus();"));
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, helper.GetTextInputType());

  // Changing text input type to password.
  ASSERT_TRUE(content::ExecuteScript(
          tab,
          "document.body.removeChild(document.getElementById('text_id'));"));
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_NONE);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());
}

IN_PROC_BROWSER_TEST_F(TextInput_TextInputStateChangedTest,
                       TextInputTypeChangedByJavaScript) {
  TextInputTestHelper helper(GetInputMethod());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("textinput")),
      base::FilePath(FILE_PATH_LITERAL("simple_textinput.html")));
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(content::ExecuteScript(
          tab,
          "document.getElementById('text_id').focus();"));
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, helper.GetTextInputType());

  // Changing text input type to password.
  ASSERT_TRUE(content::ExecuteScript(
          tab,
          "document.getElementById('text_id').type = 'password';"));
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, helper.GetTextInputType());
}

IN_PROC_BROWSER_TEST_F(TextInput_TextInputStateChangedTest,
                       ChangingToContentEditableCase) {
  TextInputTestHelper helper(GetInputMethod());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("textinput")),
      base::FilePath(FILE_PATH_LITERAL("content_editable.html")));
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  helper.ClickElement("anchor_id", tab);
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE, helper.GetTextInputType());


  // Disabling content editable, then expecting TEXT_INPUT_TYPE_NONE.
  ASSERT_TRUE(content::ExecuteScript(
          tab,
          "document.getElementById('anchor_id').contentEditable = false;"));
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_NONE);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  // Then re-enabling content editable, then expecting CONTENT_EDITABLE.
  ASSERT_TRUE(content::ExecuteScript(
          tab,
          "document.getElementById('anchor_id').contentEditable = true;"));
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE, helper.GetTextInputType());
}

IN_PROC_BROWSER_TEST_F(TextInput_TextInputStateChangedTest,
                       DISABLED_SwitchingAllTextInputTest) {
  TextInputTestHelper helper(GetInputMethod());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("textinput")),
      base::FilePath(FILE_PATH_LITERAL("all_input_node.html")));
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  InputTypeExpectation expectations[] = {
    { "text_id", ui::TEXT_INPUT_TYPE_TEXT },
    { "password_id", ui::TEXT_INPUT_TYPE_PASSWORD },
    { "search_id", ui::TEXT_INPUT_TYPE_SEARCH },
    { "email_id", ui::TEXT_INPUT_TYPE_EMAIL },
    { "number_id", ui::TEXT_INPUT_TYPE_NUMBER },
    { "tel_id", ui::TEXT_INPUT_TYPE_TELEPHONE },
    { "url_id", ui::TEXT_INPUT_TYPE_URL },
    { "textarea_id", ui::TEXT_INPUT_TYPE_TEXT_AREA },
    { "contenteditable_id", ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE },
  };  // The order should be same as tab order in all_input_node.html.

  for (auto& expectation : expectations) {
    content::SimulateKeyPress(tab, ui::DomKey::TAB, ui::DomCode::TAB,
                              ui::VKEY_TAB, false, false, false, false);

    helper.WaitForTextInputStateChanged(expectation.type);
    EXPECT_EQ(expectation.type, helper.GetTextInputType());
  }

  for (auto& expectation : expectations) {
    helper.ClickElement(expectation.node_id, tab);

    helper.WaitForTextInputStateChanged(expectation.type);
    EXPECT_EQ(expectation.type, helper.GetTextInputType());
  }
}

// Flaky on chromeos.  http://crbug.com/391582
IN_PROC_BROWSER_TEST_F(TextInput_TextInputStateChangedTest,
                       DISABLED_OpenNewTabOnloadTest) {
  TextInputTestHelper helper(GetInputMethod());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  GURL base_url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("textinput")),
      base::FilePath(FILE_PATH_LITERAL("all_input_node.html")));
  ui_test_utils::NavigateToURL(browser(), base_url);
  content::WebContents* base_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Focus to password field.
  helper.ClickElement("password_id", base_tab);
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, helper.GetTextInputType());

  // Then opening new foreground tab and wait new TextInputType.
  GURL new_url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("textinput")),
      base::FilePath(FILE_PATH_LITERAL("focus_input_on_load.html")));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), new_url, WindowOpenDisposition::NEW_FOREGROUND_TAB, 0);
  content::WebContents* new_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(base_tab, new_tab);

  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, helper.GetTextInputType());
}

} // namespace chromeos
