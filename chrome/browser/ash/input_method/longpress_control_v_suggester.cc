// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/longpress_control_v_suggester.h"

#include <string>

#include "chrome/browser/ash/input_method/longpress_suggester.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"

namespace ash::input_method {

LongpressControlVSuggester::LongpressControlVSuggester(
    SuggestionHandlerInterface* suggestion_handler)
    : LongpressSuggester(suggestion_handler) {}

LongpressControlVSuggester::~LongpressControlVSuggester() = default;

SuggestionStatus LongpressControlVSuggester::HandleKeyEvent(
    const ui::KeyEvent& event) {
  // The clipboard history controller handles the mouse and key events that
  // allow users to select an item to paste.
  return SuggestionStatus::kNotHandled;
}

bool LongpressControlVSuggester::TrySuggestWithSurroundingText(
    const std::u16string& text,
    const gfx::Range selection_range) {
  // Pastes cause the surrounding text to change. Continue "suggesting" after
  // such changes so that `this` remains the current suggester.
  return true;
}

bool LongpressControlVSuggester::AcceptSuggestion(size_t index) {
  // TODO(b/267694199): Compare input field states before and after the initial
  // paste to replace the pasted content.
  return true;
}

void LongpressControlVSuggester::DismissSuggestion() {}

AssistiveType LongpressControlVSuggester::GetProposeActionType() {
  return AssistiveType::kLongpressControlV;
}

}  // namespace ash::input_method
