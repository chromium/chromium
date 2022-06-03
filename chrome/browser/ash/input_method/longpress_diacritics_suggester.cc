// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/longpress_diacritics_suggester.h"

#include <algorithm>
#include <string>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/input_method/ui/assistive_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {

namespace {

using AssistiveWindowButton = ui::ime::AssistiveWindowButton;

std::vector<std::u16string> SplitDiacritics(base::StringPiece16 diacritics) {
  return base::SplitString(diacritics, kDiacriticsSeperator,
                           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

AssistiveWindowButton CreateButtonFor(size_t index,
                                      std::u16string announce_string) {
  AssistiveWindowButton button = {
      .id = ui::ime::ButtonId::kSuggestion,
      .window_type =
          ui::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion,
      .index = index,
      .announce_string = announce_string,
  };
  return button;
}

}  // namespace

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
    properties.candidates = SplitDiacritics(it->second);
    std::string error;
    suggestion_handler_->SetAssistiveWindowProperties(
        focused_context_id_.value(), properties, &error);
    if (error.empty()) {
      displayed_window_base_character_ = key_character;
      return true;
    }
    LOG(ERROR) << "Unable to suggest diacritics on longpress: " << error;
  }
  return false;
}

void LongpressDiacriticsSuggester::OnFocus(int context_id) {
  Reset();
  focused_context_id_ = context_id;
}

void LongpressDiacriticsSuggester::OnBlur() {
  focused_context_id_ = absl::nullopt;
  Reset();
}

void LongpressDiacriticsSuggester::OnExternalSuggestionsUpdated(
    const std::vector<ime::TextSuggestion>& suggestions) {
  // Relevant since suggestions are not updated externally.
  return;
}

SuggestionStatus LongpressDiacriticsSuggester::HandleKeyEvent(
    const ui::KeyEvent& event) {
  ui::DomCode code = event.code();
  if (focused_context_id_ == absl::nullopt ||
      displayed_window_base_character_ == absl::nullopt ||
      !GetCurrentShownDiacritics().size())
    return SuggestionStatus::kNotHandled;

  size_t new_index = 0;

  switch (code) {
    case kDismissDomCode:
      DismissSuggestion();
      return SuggestionStatus::kDismiss;
    case kAcceptDomCode:
      if (highlighted_index_.has_value()) {
        AcceptSuggestion(*highlighted_index_);
        return SuggestionStatus::kAccept;
      }
      return SuggestionStatus::kNotHandled;
    case kNextDomCode:
    case kPreviousDomCode:
      if (highlighted_index_ == absl::nullopt) {
        // We want the cursor to start at the end if you press back, and at the
        // beginning if you press next.
        new_index =
            (code == kNextDomCode) ? 0 : GetCurrentShownDiacritics().size() - 1;
      } else {
        if (code == kNextDomCode) {
          new_index =
              (*highlighted_index_ + 1) % GetCurrentShownDiacritics().size();
        } else {
          new_index = (*highlighted_index_ > 0)
                          ? *highlighted_index_ - 1
                          : GetCurrentShownDiacritics().size() - 1;
        }
      }
      SetButtonHighlighted(new_index);
      return SuggestionStatus::kBrowsing;
    default:
      return SuggestionStatus::kNotHandled;
  }
}

bool LongpressDiacriticsSuggester::TrySuggestWithSurroundingText(
    const std::u16string& text,
    int cursor_pos,
    int anchor_pos) {
  if (displayed_window_base_character_ == absl::nullopt ||
      text_changed_since_longpress_ || anchor_pos != cursor_pos ||
      cursor_pos <= 0 || cursor_pos > text.size() ||
      text[cursor_pos - 1] != *displayed_window_base_character_) {
    return false;
  }

  text_changed_since_longpress_ = true;
  return true;
}

bool LongpressDiacriticsSuggester::AcceptSuggestion(size_t index) {
  if (!focused_context_id_.has_value()) {
    LOG(ERROR) << "suggest: Failed to accept suggestion. No context id.";
    return false;
  }

  std::vector<std::u16string> current_suggestions = GetCurrentShownDiacritics();
  if (index >= current_suggestions.size()) {
    return false;
  }
  std::string error;
  suggestion_handler_->AcceptSuggestionCandidate(
      *focused_context_id_, current_suggestions[index],
      /* delete_previous_utf16_len=*/1, &error);

  if (!error.empty()) {
    LOG(ERROR) << "Failed to accept suggestion. " << error;
    return false;
  }

  Reset();
  return true;
}

void LongpressDiacriticsSuggester::DismissSuggestion() {
  if (!focused_context_id_.has_value()) {
    LOG(ERROR) << "suggest: Failed to dismiss suggestion. No context id.";
    return;
  }

  std::string error;
  AssistiveWindowProperties properties;
  properties.type =
      ui::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion;
  properties.visible = false;
  properties.announce_string =
      l10n_util::GetStringUTF16(IDS_SUGGESTION_DISMISSED);

  suggestion_handler_->SetAssistiveWindowProperties(*focused_context_id_,
                                                    properties, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Failed to dismiss suggestion. " << error;
    return;
  }
  Reset();
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

void LongpressDiacriticsSuggester::SetButtonHighlighted(size_t index) {
  if (!focused_context_id_.has_value()) {
    LOG(ERROR) << "suggest: Failed to set button highlighted. No context id.";
    return;
  }
  std::string error;
  suggestion_handler_->SetButtonHighlighted(
      *focused_context_id_,
      CreateButtonFor(index, GetCurrentShownDiacritics()[index]),
      /* highlighted=*/true, &error);

  if (!error.empty()) {
    LOG(ERROR) << "suggest: Failed to set button highlighted. " << error;
  }

  highlighted_index_ = index;
}

std::vector<std::u16string>
LongpressDiacriticsSuggester::GetCurrentShownDiacritics() {
  if (displayed_window_base_character_ == absl::nullopt) {
    return {};
  }

  if (const auto* it =
          kDefaultDiacriticsMap.find(*displayed_window_base_character_);
      it != kDefaultDiacriticsMap.end()) {
    return SplitDiacritics(it->second);
  } else {
    return {};
  }
}

void LongpressDiacriticsSuggester::Reset() {
  text_changed_since_longpress_ = false;
  displayed_window_base_character_ = absl::nullopt;
  highlighted_index_ = absl::nullopt;
}
}  // namespace input_method
}  // namespace ash
