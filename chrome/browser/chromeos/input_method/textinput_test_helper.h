// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_TEXTINPUT_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_TEXTINPUT_TEST_HELPER_H_

#include "base/macros.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/mock_input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

namespace content {
class WebContents;
}  // namespace content

namespace chromeos {

// The base class of text input testing.
class TextInputTestBase : public InProcessBrowserTest {
 public:
  TextInputTestBase();
  ~TextInputTestBase() override;

  ui::InputMethod* GetInputMethod() const;

 private:
  ui::ScopedTestInputMethodFactory scoped_test_input_method_factory_;

  DISALLOW_COPY_AND_ASSIGN(TextInputTestBase);
};

// Provides text input test utilities.
class TextInputTestHelper : public ui::InputMethodObserver {
 public:
  explicit TextInputTestHelper(ui::InputMethod* input_method);
  ~TextInputTestHelper() override;

  // Returns the latest status notified to ui::InputMethod
  base::string16 GetSurroundingText() const;
  gfx::Rect GetCaretRect() const;
  gfx::Rect GetCompositionHead() const;
  gfx::Range GetSelectionRange() const;
  bool GetFocusState() const;
  ui::TextInputType GetTextInputType() const;

  ui::TextInputClient* GetTextInputClient() const;

  // Waiting function for each input method events. These functions runs message
  // loop until the expected event comes.
  void WaitForTextInputStateChanged(ui::TextInputType expected_type);
  void WaitForFocus();
  void WaitForBlur();
  void WaitForCaretBoundsChanged(const gfx::Rect& expected_caret_rect,
                                 const gfx::Rect& expected_composition_head);
  void WaitForSurroundingTextChanged(const base::string16& expected_text,
                                     const gfx::Range& expected_selection);
  void WaitForPassageOfTimeMillis(const int milliseconds);

  // Converts from string to gfx::Rect. The string should be "x,y,width,height".
  // Returns false if the conversion failed.
  static bool ConvertRectFromString(const std::string& str, gfx::Rect* rect);

  // Sends mouse clicking event to DOM element which has |id| id.
  static bool ClickElement(const std::string& id, content::WebContents* tab);

 private:
  enum WaitImeEventType {
    NO_WAIT,
    WAIT_ON_BLUR,
    WAIT_ON_CARET_BOUNDS_CHANGED,
    WAIT_ON_FOCUS,
    WAIT_ON_TEXT_INPUT_TYPE_CHANGED,
    WAIT_ON_PASSAGE_OF_TIME,
  };

  // ui::InputMethodObserver overrides.
  void OnFocus() override;
  void OnBlur() override;
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override;
  void OnTextInputStateChanged(const ui::TextInputClient* client) override;
  void OnShowVirtualKeyboardIfEnabled() override;
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override;

  // Represents waiting type of text input event.
  WaitImeEventType waiting_type_;

  base::string16 surrounding_text_;
  gfx::Rect caret_rect_;
  gfx::Rect composition_head_;
  gfx::Range selection_range_;
  bool focus_state_;
  ui::TextInputType latest_text_input_type_;
  ui::InputMethod* input_method_;

  DISALLOW_COPY_AND_ASSIGN(TextInputTestHelper);
};

} // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_TEXTINPUT_TEST_HELPER_H_
