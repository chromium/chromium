// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/longpress_diacritics_suggester.h"
#include <algorithm>
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/input_method/ui/assistive_delegate.h"

namespace ash {
namespace input_method {

LongpressDiacriticsSuggester::LongpressDiacriticsSuggester(
    SuggestionHandlerInterface* suggestion_handler)
    : suggestion_handler_(suggestion_handler) {}

LongpressDiacriticsSuggester::~LongpressDiacriticsSuggester() = default;

bool LongpressDiacriticsSuggester::TrySuggestOnLongpress(char key_character) {
  if (!focused_context_id_.has_value()) {
    LOG(ERROR) << "Unable to suggest diacritics on longpress, no context_id";
    return false;
  }

  if (const auto* it = kDefaultDiacriticsMap.find(key_character);
      it != kDefaultDiacriticsMap.end()) {
    AssistiveWindowProperties properties;
    properties.type =
        ui::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion;
    properties.visible = true;
    properties.candidates =
        base::SplitString(it->second, kDiacriticsSeperator,
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    std::string error;
    suggestion_handler_->SetAssistiveWindowProperties(
        focused_context_id_.value(), properties, &error);
    if (error.empty()) {
      return true;
    }
    LOG(ERROR) << "Unable to suggest diacritics on longpress: " << error;
  }
  return false;
}

void LongpressDiacriticsSuggester::OnFocus(int context_id) {
  focused_context_id_ = context_id;
};

void LongpressDiacriticsSuggester::OnBlur() {
  focused_context_id_ = absl::nullopt;
}

void LongpressDiacriticsSuggester::OnExternalSuggestionsUpdated(
    const std::vector<ime::TextSuggestion>& suggestions) {
  // Relevant since suggestions are not updated externally.
  return;
}

SuggestionStatus LongpressDiacriticsSuggester::HandleKeyEvent(
    const ui::KeyEvent& event) {
  // TODO(b/217560706): Handle key events.
  return SuggestionStatus::kDismiss;
}

bool LongpressDiacriticsSuggester::TrySuggestWithSurroundingText(
    const std::u16string& text,
    int cursor_pos,
    int anchor_pos) {
  // TODO(b/217560706): Should dismiss when surrounding text changes.
  return false;
}

bool LongpressDiacriticsSuggester::AcceptSuggestion(size_t index) {
  // TODO(b/217560706): Handle accept suggestion.
  return false;
}

void LongpressDiacriticsSuggester::DismissSuggestion() {
  // TODO(b/217560706): Should handle dismiss.
  return;
}

AssistiveType LongpressDiacriticsSuggester::GetProposeActionType() {
  // TODO(b/217560706): Should handle action.
  return AssistiveType::kGenericAction;
}

bool LongpressDiacriticsSuggester::HasSuggestions() {
  // Unused.
  return false;
}

std::vector<ime::TextSuggestion>
LongpressDiacriticsSuggester::GetSuggestions() {
  // Unused.
  return {};
}

}  // namespace input_method
}  // namespace ash
