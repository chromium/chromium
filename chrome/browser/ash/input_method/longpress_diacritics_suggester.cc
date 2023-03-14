// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/longpress_diacritics_suggester.h"

#include <algorithm>
#include <string>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
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

std::vector<std::u16string> GetDiacriticsFor(char key_character,
                                             base::StringPiece engine_id) {
  // Currently only supporting US English.
  // TODO(b/260915965): Add support for other engines.
  if (engine_id != "xkb:us::eng") {
    return {};
  }

  // Current diacritics ordering is based on the Gboard ordering so it keeps
  // distance from target key consistent.
  // TODO(b/260915965): Add more sets here for other engines.
  static constexpr auto kUSEnglishDiacriticsMap =
      base::MakeFixedFlatMap<char, base::StringPiece16>(
          {{'a', u"à;á;â;ä;æ;ã;å;ā"},
           {'A', u"À;Á;Â;Ä;Æ;Ã;Å;Ā"},
           {'c', u"ç"},
           {'C', u"Ç"},
           {'e', u"é;è;ê;ë;ē"},
           {'E', u"É;È;Ê;Ë;Ē"},
           {'i', u"í;î;ï;ī;ì"},
           {'I', u"Í;Î;Ï;Ī;Ì"},
           {'n', u"ñ"},
           {'N', u"Ñ"},
           {'o', u"ó;ô;ö;ò;œ;ø;ō;õ"},
           {'O', u"Ó;Ô;Ö;Ò;Œ;Ø;Ō;Õ"},
           {'s', u"ß"},
           {'S', u"ẞ"},
           {'u', u"ú;û;ü;ù;ū"},
           {'U', u"Ú;Û;Ü;Ù;Ū"}});

  if (const auto* it = kUSEnglishDiacriticsMap.find(key_character);
      it != kUSEnglishDiacriticsMap.end()) {
    return SplitDiacritics(it->second);
  }
  return {};
}

AssistiveWindowButton CreateButtonFor(size_t index,
                                      std::u16string announce_string) {
  AssistiveWindowButton button = {
      .id = ui::ime::ButtonId::kSuggestion,
      .window_type =
          ash::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion,
      .index = index,
      .announce_string = announce_string,
  };
  return button;
}

void RecordActionMetric(IMEPKLongpressDiacriticAction action) {
  base::UmaHistogramEnumeration(
      "InputMethod.PhysicalKeyboard.LongpressDiacritics.Action", action);
}

void RecordAcceptanceCharCodeMetric(const std::u16string diacritic) {
  // Recording -1 as default value just in case there are issues with
  // encoding in utf-16 that means some character isn't
  // properly captured in one utf-16 char (for example if emojis are added in
  // the future).
  int char_code = -1;
  if (diacritic.length() == 1) {
    char_code = int(diacritic[0]);
  }

  base::UmaHistogramSparse(
      "InputMethod.PhysicalKeyboard.LongpressDiacritics.AcceptedChar",
      char_code);
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
  std::vector<std::u16string> diacritics_candidates =
      GetDiacriticsFor(key_character, engine_id_);
  if (!diacritics_candidates.empty()) {
    AssistiveWindowProperties properties;
    properties.type =
        ash::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion;
    properties.visible = true;
    properties.candidates = diacritics_candidates;
    properties.announce_string =
        l10n_util::GetStringUTF16(IDS_SUGGESTION_DIACRITICS_OPEN);

    std::string error;
    suggestion_handler_->SetAssistiveWindowProperties(
        focused_context_id_.value(), properties, &error);
    if (error.empty()) {
      displayed_window_base_character_ = key_character;
      RecordActionMetric(IMEPKLongpressDiacriticAction::kShowWindow);
      return true;
    }
    LOG(ERROR) << "Unable to suggest diacritics on longpress: " << error;
  }
  return false;
}

void LongpressDiacriticsSuggester::SetEngineId(const std::string& engine_id) {
  engine_id_ = engine_id;
}

bool LongpressDiacriticsSuggester::HasDiacriticSuggestions(char c) {
  return !GetDiacriticsFor(c, engine_id_).empty();
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
    const std::vector<ime::AssistiveSuggestion>& suggestions) {
  // Relevant since suggestions are not updated externally.
  return;
}

SuggestionStatus LongpressDiacriticsSuggester::HandleKeyEvent(
    const ui::KeyEvent& event) {
  ui::DomCode code = event.code();
  // The diacritic suggester is not set up.
  if (focused_context_id_ == absl::nullopt ||
      displayed_window_base_character_ == absl::nullopt ||
      !GetCurrentShownDiacritics().size()) {
    return SuggestionStatus::kNotHandled;
  }
  // The diacritic suggester is displaying, but its just the repeat key of the
  // base character (probably because user is still holding down the key).
  if (*displayed_window_base_character_ == event.GetCharacter() &&
      event.is_repeat()) {
    return SuggestionStatus::kNotHandled;
  }

  size_t new_index = 0;
  bool move_next = false;
  switch (code) {
    case kDismissDomCode:
      DismissSuggestion();
      RecordActionMetric(IMEPKLongpressDiacriticAction::kDismiss);
      return SuggestionStatus::kDismiss;
    case kAcceptDomCode:
      if (highlighted_index_.has_value()) {
        AcceptSuggestion(*highlighted_index_);
        return SuggestionStatus::kAccept;
      }
      return SuggestionStatus::kNotHandled;
    case kNextDomCode:
    case kTabDomCode:
    case kPreviousDomCode:
      move_next = (code == kNextDomCode || code == kTabDomCode);
      if (highlighted_index_ == absl::nullopt) {
        // We want the cursor to start at the end if you press back, and at the
        // beginning if you press next.
        new_index = move_next ? 0 : GetCurrentShownDiacritics().size() - 1;
      } else {
        SetButtonHighlighted(*highlighted_index_, false);
        if (move_next) {
          new_index =
              (*highlighted_index_ + 1) % GetCurrentShownDiacritics().size();
        } else {
          new_index = (*highlighted_index_ > 0)
                          ? *highlighted_index_ - 1
                          : GetCurrentShownDiacritics().size() - 1;
        }
      }
      SetButtonHighlighted(new_index, true);
      highlighted_index_ = new_index;
      return SuggestionStatus::kBrowsing;
    default:
      size_t key_number = 0;
      // If the key value is a number then accept the corresponding suggestion.
      if (base::StringToSizeT(std::u16string(1, event.GetCharacter()),
                              &key_number)) {
        // Ignore 0 values, make sure the key numbers are valid.
        if (1 <= key_number && key_number <= 9 &&
            key_number <= GetCurrentShownDiacritics().size()) {
          // The "key" char value starts from 1.
          // The actual index of the suggestions start at 0.
          size_t index_to_accept = key_number - 1;
          if (AcceptSuggestion(index_to_accept)) {
            return SuggestionStatus::kAccept;
          }
        }
      }

      // Commit current text if there is a selection.
      if (highlighted_index_.has_value()) {
        AcceptSuggestion(*highlighted_index_);
      } else {
        DismissSuggestion();
        RecordActionMetric(IMEPKLongpressDiacriticAction::kDismiss);
      }
      // NotHandled is passed so that the IME will let the key event pass
      // through.
      return SuggestionStatus::kNotHandled;
  }
}

bool LongpressDiacriticsSuggester::TrySuggestWithSurroundingText(
    const std::u16string& text,
    const gfx::Range selection_range) {
  // Should dismiss on text change.
  return false;
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
  if (error.empty()) {
    suggestion_handler_->Announce(
        l10n_util::GetStringUTF16(IDS_SUGGESTION_DIACRITICS_INSERTED));
  } else {
    LOG(ERROR) << "Failed to accept suggestion. " << error;
    return false;
  }
  RecordActionMetric(IMEPKLongpressDiacriticAction::kAccept);
  RecordAcceptanceCharCodeMetric(current_suggestions[index]);
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
      ash::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion;
  properties.visible = false;
  properties.announce_string =
      l10n_util::GetStringUTF16(IDS_SUGGESTION_DIACRITICS_DISMISSED);

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
  return AssistiveType::kLongpressDiacritics;
}

bool LongpressDiacriticsSuggester::HasSuggestions() {
  // Unused.
  return false;
}

std::vector<ime::AssistiveSuggestion>
LongpressDiacriticsSuggester::GetSuggestions() {
  // Unused.
  return {};
}

void LongpressDiacriticsSuggester::SetButtonHighlighted(size_t index,
                                                        bool highlighted) {
  if (!focused_context_id_.has_value()) {
    LOG(ERROR) << "suggest: Failed to set button highlighted. No context id.";
    return;
  }
  std::string error;
  suggestion_handler_->SetButtonHighlighted(
      *focused_context_id_,
      CreateButtonFor(index, GetCurrentShownDiacritics()[index]),
      /* highlighted=*/highlighted, &error);

  if (!error.empty()) {
    LOG(ERROR) << "suggest: Failed to set button highlighted. " << error;
  }
}

std::vector<std::u16string>
LongpressDiacriticsSuggester::GetCurrentShownDiacritics() {
  if (displayed_window_base_character_ == absl::nullopt) {
    return {};
  }
  return GetDiacriticsFor(*displayed_window_base_character_, engine_id_);
}

void LongpressDiacriticsSuggester::Reset() {
  displayed_window_base_character_ = absl::nullopt;
  highlighted_index_ = absl::nullopt;
}
}  // namespace input_method
}  // namespace ash
