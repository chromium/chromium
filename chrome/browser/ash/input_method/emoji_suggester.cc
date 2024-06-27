// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/emoji_suggester.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/files/file_util.h"
#include "base/i18n/number_formatting.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/input_method/assistive_prefs.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/services/ime/constants.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {

namespace {

using AssistiveSuggestion = ime::AssistiveSuggestion;
using AssistiveSuggestionMode = ime::AssistiveSuggestionMode;
using AssistiveSuggestionType = ime::AssistiveSuggestionType;
using SuggestionsTextContext = ime::SuggestionsTextContext;

constexpr char kEmojiSuggesterShowSettingCount[] =
    "emoji_suggester.show_setting_count";
const int kMaxCandidateSize = 5;
const char kSpaceChar = ' ';
constexpr char kTrimLeadingChars[] = "(";
constexpr char kEmojiMapFilePathName[] = "/emoji/emoji-map.csv";
const int kMaxSuggestionIndex = 31;
const int kMaxSuggestionSize = kMaxSuggestionIndex + 1;
const int kNoneHighlighted = -1;

std::string ReadEmojiDataFromFile() {
  if (!base::DirectoryExists(
          base::FilePath(ime::kBundledInputMethodsDirPath))) {
    return std::string();
  }

  std::string emoji_data;
  base::FilePath::StringType path(ime::kBundledInputMethodsDirPath);
  path.append(FILE_PATH_LITERAL(kEmojiMapFilePathName));
  if (!base::ReadFileToString(base::FilePath(path), &emoji_data)) {
    LOG(WARNING) << "Emoji map file missing.";
  }
  return emoji_data;
}

std::vector<std::string> SplitString(const std::string& str,
                                     const std::string& delimiter) {
  return base::SplitString(str, delimiter, base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

std::string GetLastWord(const std::string& str) {
  // We only suggest if last char is a white space so search for last word from
  // second last char.
  DCHECK_EQ(kSpaceChar, str.back());
  size_t last_pos_to_search = str.length() - 2;

  auto space_before_last_word = str.find_last_of(" \n", last_pos_to_search);

  // If not found, return the entire string up to the last position to search
  // else return the last word.
  const std::string last_word =
      space_before_last_word == std::string::npos
          ? str.substr(0, last_pos_to_search + 1)
          : str.substr(space_before_last_word + 1,
                       last_pos_to_search - space_before_last_word);

  // Remove any leading special characters
  return base::ToLowerASCII(
      base::TrimString(last_word, kTrimLeadingChars, base::TRIM_LEADING));
}

AssistiveSuggestion MapToAssistiveSuggestion(std::u16string candidate_string) {
  return {.mode = AssistiveSuggestionMode::kPrediction,
          .type = AssistiveSuggestionType::kAssistiveEmoji,
          .text = base::UTF16ToUTF8(candidate_string)};
}

}  // namespace

EmojiSuggester::EmojiSuggester(SuggestionHandlerInterface* suggestion_handler,
                               Profile* profile)
    : suggestion_handler_(suggestion_handler),
      profile_(profile),
      highlighted_index_(kNoneHighlighted) {
  LoadEmojiMap();

  properties_.type = ash::ime::AssistiveWindowType::kEmojiSuggestion;
  suggestion_button_.id = ui::ime::ButtonId::kSuggestion;
  suggestion_button_.window_type =
      ash::ime::AssistiveWindowType::kEmojiSuggestion;
  learn_more_button_.id = ui::ime::ButtonId::kLearnMore;
  learn_more_button_.announce_string =
      l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  learn_more_button_.window_type =
      ash::ime::AssistiveWindowType::kEmojiSuggestion;
}

EmojiSuggester::~EmojiSuggester() = default;

void EmojiSuggester::LoadEmojiMap() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&ReadEmojiDataFromFile),
      base::BindOnce(&EmojiSuggester::OnEmojiDataLoaded,
                     weak_factory_.GetWeakPtr()));
}

void EmojiSuggester::LoadEmojiMapForTesting(const std::string& emoji_data) {
  OnEmojiDataLoaded(emoji_data);
}

void EmojiSuggester::OnEmojiDataLoaded(const std::string& emoji_data) {
  // Split data into lines.
  for (const auto& line : SplitString(emoji_data, "\n")) {
    // Get a word and a string of emojis from the line.
    const auto comma_pos = line.find_first_of(",");
    DCHECK(comma_pos != std::string::npos);
    std::string word = line.substr(0, comma_pos);
    std::u16string emojis = base::UTF8ToUTF16(line.substr(comma_pos + 1));
    // Build emoji_map_ from splitting the string of emojis.
    emoji_map_[word] = base::SplitString(emojis, u";", base::TRIM_WHITESPACE,
                                         base::SPLIT_WANT_NONEMPTY);
    // TODO(crbug/1093179): Implement arrow to indicate more emojis available.
    // Only loads 5 emojis for now until arrow is implemented.
    if (emoji_map_[word].size() > kMaxCandidateSize) {
      emoji_map_[word].resize(kMaxCandidateSize);
    }
    DCHECK_LE(static_cast<int>(emoji_map_[word].size()), kMaxSuggestionSize);
  }
}

void EmojiSuggester::RecordAcceptanceIndex(int index) {
  base::UmaHistogramExactLinear(
      "InputMethod.Assistive.EmojiSuggestAddition.AcceptanceIndex", index,
      kMaxSuggestionIndex);
}

void EmojiSuggester::OnFocus(int context_id) {
  // Some parts of the code reserve negative/zero context_id for unfocused
  // context. As a result we should make sure it is not being erroneously set to
  // a negative number, and cause unexpected behaviour.
  DCHECK(context_id > 0);
  focused_context_id_ = context_id;
}

void EmojiSuggester::OnBlur() {
  focused_context_id_ = std::nullopt;
}

void EmojiSuggester::OnExternalSuggestionsUpdated(
    const std::vector<AssistiveSuggestion>& suggestions,
    const std::optional<SuggestionsTextContext>& context) {
  // EmojiSuggester doesn't utilize any suggestions produced externally, so
  // ignore this call.
}

SuggestionStatus EmojiSuggester::HandleKeyEvent(const ui::KeyEvent& event) {
  if (!suggestion_shown_) {
    return SuggestionStatus::kNotHandled;
  }

  if (event.code() == ui::DomCode::ESCAPE) {
    DismissSuggestion();
    return SuggestionStatus::kDismiss;
  }
  if (highlighted_index_ == kNoneHighlighted && buttons_.size() > 0) {
    if (event.code() == ui::DomCode::ARROW_DOWN ||
        event.code() == ui::DomCode::ARROW_UP) {
      highlighted_index_ =
          event.code() == ui::DomCode::ARROW_DOWN ? 0 : buttons_.size() - 1;
      SetButtonHighlighted(buttons_[highlighted_index_], true);
      return SuggestionStatus::kBrowsing;
    }
  } else {
    if (event.code() == ui::DomCode::ENTER) {
      switch (buttons_[highlighted_index_].id) {
        case ui::ime::ButtonId::kSuggestion:
          AcceptSuggestion(highlighted_index_);
          return SuggestionStatus::kAccept;
        case ui::ime::ButtonId::kLearnMore:
          suggestion_handler_->ClickButton(buttons_[highlighted_index_]);
          return SuggestionStatus::kOpenSettings;
        default:
          break;
      }
    } else if (event.code() == ui::DomCode::ARROW_UP ||
               event.code() == ui::DomCode::ARROW_DOWN) {
      SetButtonHighlighted(buttons_[highlighted_index_], false);
      if (event.code() == ui::DomCode::ARROW_UP) {
        highlighted_index_ =
            (highlighted_index_ + buttons_.size() - 1) % buttons_.size();
      } else {
        highlighted_index_ = (highlighted_index_ + 1) % buttons_.size();
      }
      SetButtonHighlighted(buttons_[highlighted_index_], true);
      return SuggestionStatus::kBrowsing;
    }
  }

  return SuggestionStatus::kNotHandled;
}

bool EmojiSuggester::ShouldShowSuggestion(const std::u16string& text) {
  if (text[text.length() - 1] != kSpaceChar) {
    return false;
  }

  std::string last_word =
      base::ToLowerASCII(GetLastWord(base::UTF16ToUTF8(text)));
  if (!last_word.empty() && emoji_map_.count(last_word)) {
    return true;
  }
  return false;
}

bool EmojiSuggester::TrySuggestWithSurroundingText(
    const std::u16string& text,
    const gfx::Range selection_range) {
  if (emoji_map_.empty() || !focused_context_id_.has_value()) {
    return false;
  }

  // All these below conditions are required for a emoji suggestion to be
  // triggered.
  // eg. "wow |" where '|' denotes cursor position should trigger an emoji
  // suggestion.
  const uint32_t len = text.length();
  const uint32_t cursor_pos = selection_range.start();
  if (!(len && cursor_pos == len  // text not empty and cursor is end of text
        && selection_range.is_empty()          // no selection
        && text[cursor_pos - 1] == kSpaceChar  // space before cursor
        )) {
    return false;
  }

  std::string last_word =
      base::ToLowerASCII(GetLastWord(base::UTF16ToUTF8(text)));
  if (!last_word.empty() && emoji_map_.count(last_word)) {
    ShowSuggestion(last_word);
    return true;
  }
  return false;
}

void EmojiSuggester::ShowSuggestion(const std::string& text) {
  if (ChromeKeyboardControllerClient::Get()->is_keyboard_visible()) {
    return;
  }

  highlighted_index_ = kNoneHighlighted;

  std::string error;
  // TODO(crbug.com/40137521): Move suggestion_show_ after checking for error
  // and fix tests.
  suggestion_shown_ = true;
  candidates_ = emoji_map_.at(text);
  properties_.visible = true;
  properties_.candidates = candidates_;
  properties_.announce_string =
      l10n_util::GetStringUTF16(IDS_SUGGESTION_EMOJI_SUGGESTED);
  properties_.show_setting_link =
      GetPrefValue(kEmojiSuggesterShowSettingCount, *profile_) <
      kEmojiSuggesterShowSettingMaxCount;
  IncrementPrefValueUntilCapped(kEmojiSuggesterShowSettingCount,
                                kEmojiSuggesterShowSettingMaxCount, *profile_);
  ShowSuggestionWindow();

  buttons_.clear();
  for (size_t i = 0; i < candidates_.size(); i++) {
    suggestion_button_.suggestion_index = i;
    suggestion_button_.announce_string = l10n_util::GetStringFUTF16(
        IDS_SUGGESTION_EMOJI_CHOSEN, candidates_[i], base::FormatNumber(i + 1),
        base::FormatNumber(candidates_.size()));
    buttons_.push_back(suggestion_button_);
  }
  if (properties_.show_setting_link) {
    buttons_.push_back(learn_more_button_);
  }
}

void EmojiSuggester::ShowSuggestionWindow() {
  if (!focused_context_id_.has_value()) {
    LOG(ERROR) << "suggest: Failed to show suggestion. No context id.";
  }

  std::string error;
  suggestion_handler_->SetAssistiveWindowProperties(*focused_context_id_,
                                                    properties_, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Fail to show suggestion. " << error;
  }
}

bool EmojiSuggester::AcceptSuggestion(size_t index) {
  if (!focused_context_id_.has_value()) {
    LOG(ERROR) << "suggest: Failed to accept suggestion. No context id.";
    return false;
  }

  if (index < 0 || index >= candidates_.size()) {
    return false;
  }

  std::string error;
  suggestion_handler_->AcceptSuggestionCandidate(
      *focused_context_id_, candidates_[index],
      /* delete_previous_utf16_len=*/0, /*use_replace_surrounding_text=*/false,
      &error);

  if (!error.empty()) {
    LOG(ERROR) << "Failed to accept suggestion. " << error;
    return false;
  }

  suggestion_shown_ = false;
  RecordAcceptanceIndex(index);
  return true;
}

void EmojiSuggester::DismissSuggestion() {
  if (!focused_context_id_.has_value()) {
    LOG(ERROR) << "suggest: Failed to dismiss suggestion. No context id.";
    return;
  }

  std::string error;
  properties_.visible = false;
  properties_.announce_string =
      l10n_util::GetStringUTF16(IDS_SUGGESTION_DISMISSED);
  suggestion_handler_->SetAssistiveWindowProperties(*focused_context_id_,
                                                    properties_, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Failed to dismiss suggestion. " << error;
    return;
  }
  suggestion_shown_ = false;
}

void EmojiSuggester::SetButtonHighlighted(
    const ui::ime::AssistiveWindowButton& button,
    bool highlighted) {
  if (!focused_context_id_.has_value()) {
    LOG(ERROR) << "suggest: Failed to set button highlighted. No context id.";
    return;
  }
  std::string error;
  suggestion_handler_->SetButtonHighlighted(*focused_context_id_, button,
                                            highlighted, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Failed to set button highlighted. " << error;
  }
}

AssistiveType EmojiSuggester::GetProposeActionType() {
  return AssistiveType::kEmoji;
}

bool EmojiSuggester::HasSuggestions() {
  return suggestion_shown_;
}

std::vector<AssistiveSuggestion> EmojiSuggester::GetSuggestions() {
  std::vector<AssistiveSuggestion> suggestions;
  if (HasSuggestions()) {
    for (const auto& candidate : candidates_) {
      suggestions.emplace_back(MapToAssistiveSuggestion(candidate));
    }
  }
  return suggestions;
}

size_t EmojiSuggester::GetCandidatesSizeForTesting() const {
  return candidates_.size();
}

}  // namespace input_method
}  // namespace ash
