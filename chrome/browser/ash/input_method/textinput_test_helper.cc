// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/textinput_test_helper.h"

#include <string_view>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/init/input_method_factory.h"

namespace ash {
namespace input_method {

TextInputTestBase::TextInputTestBase() = default;
TextInputTestBase::~TextInputTestBase() = default;

ui::InputMethod* TextInputTestBase::GetInputMethod() const {
  return browser()->window()->GetNativeWindow()->GetHost()->GetInputMethod();
}

TextInputTestHelper::TextInputTestHelper(ui::InputMethod* input_method)
    : waiting_type_(NO_WAIT),
      selection_range_(gfx::Range::InvalidRange()),
      focus_state_(false),
      latest_text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      input_method_(input_method) {
  input_method_->AddObserver(this);
}

TextInputTestHelper::~TextInputTestHelper() {
  input_method_->RemoveObserver(this);
}

std::u16string TextInputTestHelper::GetSurroundingText() const {
  return surrounding_text_;
}

gfx::Rect TextInputTestHelper::GetCaretRect() const {
  return caret_rect_;
}

gfx::Rect TextInputTestHelper::GetCompositionHead() const {
  return composition_head_;
}

gfx::Range TextInputTestHelper::GetSelectionRange() const {
  return selection_range_;
}

bool TextInputTestHelper::GetFocusState() const {
  return focus_state_;
}

ui::TextInputType TextInputTestHelper::GetTextInputType() const {
  return latest_text_input_type_;
}

ui::TextInputClient* TextInputTestHelper::GetTextInputClient() const {
  return input_method_->GetTextInputClient();
}

void TextInputTestHelper::OnInputMethodDestroyed(
    const ui::InputMethod* input_method) {}

void TextInputTestHelper::OnFocus() {
  focus_state_ = true;
  if (waiting_type_ == WAIT_ON_FOCUS) {
    if (run_loop_) {
      run_loop_->Quit();
    }
  }
}

void TextInputTestHelper::OnBlur() {
  focus_state_ = false;
  if (waiting_type_ == WAIT_ON_BLUR) {
    if (run_loop_) {
      run_loop_->Quit();
    }
  }
}

void TextInputTestHelper::OnCaretBoundsChanged(
    const ui::TextInputClient* client) {
  gfx::Range text_range;
  if (GetTextInputClient()) {
    if (!GetTextInputClient()->GetTextRange(&text_range) ||
        !GetTextInputClient()->GetTextFromRange(text_range,
                                                &surrounding_text_) ||
        !GetTextInputClient()->GetEditableSelectionRange(&selection_range_)) {
      return;
    }
  }
  if (waiting_type_ == WAIT_ON_CARET_BOUNDS_CHANGED) {
    if (run_loop_) {
      run_loop_->Quit();
    }
  }
}

void TextInputTestHelper::OnTextInputStateChanged(
    const ui::TextInputClient* client) {
  latest_text_input_type_ =
      client ? client->GetTextInputType() : ui::TEXT_INPUT_TYPE_NONE;
  if (waiting_type_ == WAIT_ON_TEXT_INPUT_TYPE_CHANGED) {
    if (run_loop_) {
      run_loop_->Quit();
    }
  }
}

void TextInputTestHelper::WaitForTextInputStateChanged(
    ui::TextInputType expected_type) {
  CHECK_EQ(NO_WAIT, waiting_type_);
  waiting_type_ = WAIT_ON_TEXT_INPUT_TYPE_CHANGED;
  while (latest_text_input_type_ != expected_type) {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }
  waiting_type_ = NO_WAIT;
}

void TextInputTestHelper::WaitForFocus() {
  CHECK_EQ(NO_WAIT, waiting_type_);
  waiting_type_ = WAIT_ON_FOCUS;
  while (focus_state_) {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }
  waiting_type_ = NO_WAIT;
}

void TextInputTestHelper::WaitForBlur() {
  CHECK_EQ(NO_WAIT, waiting_type_);
  waiting_type_ = WAIT_ON_BLUR;
  while (!focus_state_) {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }
  waiting_type_ = NO_WAIT;
}

void TextInputTestHelper::WaitForCaretBoundsChanged(
    const gfx::Rect& expected_caret_rect,
    const gfx::Rect& expected_composition_head) {
  waiting_type_ = WAIT_ON_CARET_BOUNDS_CHANGED;
  while (expected_caret_rect != caret_rect_ ||
         expected_composition_head != composition_head_) {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }
  waiting_type_ = NO_WAIT;
}

void TextInputTestHelper::WaitForSurroundingTextChanged(
    const std::u16string& expected_text) {
  waiting_type_ = WAIT_ON_CARET_BOUNDS_CHANGED;
  while (expected_text != surrounding_text_) {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }
  waiting_type_ = NO_WAIT;
}

void TextInputTestHelper::WaitForSurroundingTextChanged(
    const std::u16string& expected_text,
    const gfx::Range& expected_selection) {
  waiting_type_ = WAIT_ON_CARET_BOUNDS_CHANGED;
  while (expected_text != surrounding_text_ ||
         expected_selection != selection_range_) {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }
  waiting_type_ = NO_WAIT;
}

void TextInputTestHelper::WaitForPassageOfTimeMillis(const int milliseconds) {
  CHECK_EQ(NO_WAIT, waiting_type_);
  waiting_type_ = WAIT_ON_PASSAGE_OF_TIME;
  base::PlatformThread::Sleep(base::Milliseconds(milliseconds));
  waiting_type_ = NO_WAIT;
}

// static
bool TextInputTestHelper::ConvertRectFromString(const std::string& str,
                                                gfx::Rect* rect) {
  DCHECK(rect);
  std::vector<std::string_view> rect_piece = base::SplitStringPiece(
      str, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (rect_piece.size() != 4UL) {
    return false;
  }
  int x, y, width, height;
  if (!base::StringToInt(rect_piece[0], &x)) {
    return false;
  }
  if (!base::StringToInt(rect_piece[1], &y)) {
    return false;
  }
  if (!base::StringToInt(rect_piece[2], &width)) {
    return false;
  }
  if (!base::StringToInt(rect_piece[3], &height)) {
    return false;
  }
  *rect = gfx::Rect(x, y, width, height);
  return true;
}

// static
bool TextInputTestHelper::ClickElement(const std::string& id,
                                       content::WebContents* tab) {
  std::string coordinate =
      content::EvalJs(
          tab, "textinput_helper.retrieveElementCoordinate('" + id + "')")
          .ExtractString();

  gfx::Rect rect;
  if (!ConvertRectFromString(coordinate, &rect)) {
    return false;
  }

  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(rect.CenterPoint().x(),
                                  rect.CenterPoint().y());
  mouse_event.click_count = 1;
  tab->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(mouse_event);

  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  tab->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(mouse_event);
  return true;
}

// static
std::string TextInputTestHelper::GetElementInnerText(
    const std::string& id,
    content::WebContents* tab) {
  return content::EvalJs(tab, "document.getElementById('" + id + "').innerText")
      .ExtractString();
}

}  // namespace input_method
}  // namespace ash
