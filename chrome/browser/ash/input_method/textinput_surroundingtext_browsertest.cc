// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/input_method/textinput_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace input_method {
namespace {
using TextInput_SurroundingTextChangedTest = TextInputTestBase;
}  // namespace

IN_PROC_BROWSER_TEST_F(TextInput_SurroundingTextChangedTest,
                       SurroundingTextChangedWithInsertText) {
  TextInputTestHelper helper(GetInputMethod());
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("textinput")),
      base::FilePath(FILE_PATH_LITERAL("simple_textarea.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(
      content::ExecJs(tab, "document.getElementById('text_id').focus()"));
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_TEXT_AREA);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT_AREA, helper.GetTextInputType());

  const std::u16string sample_text1 = u"abcde";
  const std::u16string sample_text2 = u"fghij";
  const std::u16string surrounding_text2 = sample_text1 + sample_text2;
  gfx::Range expected_range1(5, 5);
  gfx::Range expected_range2(10, 10);

  ASSERT_TRUE(helper.GetTextInputClient());

  helper.GetTextInputClient()->InsertText(
      sample_text1,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  helper.WaitForSurroundingTextChanged(sample_text1, expected_range1);
  EXPECT_EQ(sample_text1, helper.GetSurroundingText());
  EXPECT_EQ(expected_range1, helper.GetSelectionRange());

  helper.GetTextInputClient()->InsertText(
      sample_text2,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  helper.WaitForSurroundingTextChanged(surrounding_text2, expected_range2);
  EXPECT_EQ(surrounding_text2, helper.GetSurroundingText());
  EXPECT_EQ(expected_range2, helper.GetSelectionRange());
}

IN_PROC_BROWSER_TEST_F(TextInput_SurroundingTextChangedTest,
                       SurroundingTextChangedWithComposition) {
  TextInputTestHelper helper(GetInputMethod());
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("textinput")),
      base::FilePath(FILE_PATH_LITERAL("simple_textarea.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(
      content::ExecJs(tab, "document.getElementById('text_id').focus()"));
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_TEXT_AREA);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT_AREA, helper.GetTextInputType());

  const std::u16string sample_text = u"abcde";
  gfx::Range expected_range(5, 5);

  ui::CompositionText composition_text;
  composition_text.text = sample_text;
  composition_text.selection.set_start(expected_range.length());
  composition_text.selection.set_end(expected_range.length());

  ASSERT_TRUE(helper.GetTextInputClient());
  helper.GetTextInputClient()->SetCompositionText(composition_text);
  ASSERT_TRUE(helper.GetTextInputClient()->HasCompositionText());
  // TODO(nona): Make sure there is no IPC from renderer.
  helper.GetTextInputClient()->InsertText(
      sample_text,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  helper.GetTextInputClient()->ClearCompositionText();

  ASSERT_FALSE(helper.GetTextInputClient()->HasCompositionText());
  helper.WaitForSurroundingTextChanged(sample_text, expected_range);
  EXPECT_EQ(sample_text, helper.GetSurroundingText());
  EXPECT_EQ(expected_range, helper.GetSelectionRange());
}

IN_PROC_BROWSER_TEST_F(TextInput_SurroundingTextChangedTest,
                       FocusToTextContainingTextAreaByClickingCase) {
  TextInputTestHelper helper(GetInputMethod());
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("textinput")),
      base::FilePath(FILE_PATH_LITERAL("textarea_with_preset_text.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  const gfx::Range zero_range(0, 0);

  // We expect no surrounding texts.
  helper.ClickElement("empty_textarea", tab);
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_TEXT_AREA);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT_AREA, helper.GetTextInputType());
  helper.WaitForSurroundingTextChanged(std::u16string(), zero_range);
  EXPECT_TRUE(helper.GetSurroundingText().empty());
  EXPECT_EQ(zero_range, helper.GetSelectionRange());

  // Click textarea containing text, so expecting new surrounding text comes.
  helper.ClickElement("filled_textarea", tab);
  const std::u16string expected_text = u"abcde";
  const gfx::Range expected_range(5, 5);
  helper.WaitForSurroundingTextChanged(expected_text, expected_range);
  EXPECT_EQ(expected_text, helper.GetSurroundingText());
  EXPECT_EQ(expected_range, helper.GetSelectionRange());

  // Then, back to empty text area: expecting empty string.
  helper.ClickElement("empty_textarea", tab);
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_TEXT_AREA);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT_AREA, helper.GetTextInputType());
  helper.WaitForSurroundingTextChanged(std::u16string(), zero_range);
  EXPECT_TRUE(helper.GetSurroundingText().empty());
  EXPECT_EQ(zero_range, helper.GetSelectionRange());
}

// TODO(nona): Add test for JavaScript focusing to textarea containing text.
// TODO(nona): Add test for text changing by JavaScript.
// TODO(nona): Add test for onload focusing to textarea containing text.
}  // namespace input_method
}  // namespace ash
