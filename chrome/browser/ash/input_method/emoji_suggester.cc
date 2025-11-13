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

const int kMaxCandidateSize = 5;
const char kSpaceChar = ' ';
constexpr char kTrimLeadingChars[] = "(";
constexpr char kEmojiMapFilePathName[] = "/emoji/emoji-map.csv";
const int kMaxSuggestionIndex = 31;
const int kMaxSuggestionSize = kMaxSuggestionIndex + 1;

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

}  // namespace

EmojiSuggester::EmojiSuggester() {
  LoadEmojiMap();
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

void EmojiSuggester::OnFocus(int context_id) {
  // Some parts of the code reserve negative/zero context_id for unfocused
  // context. As a result we should make sure it is not being erroneously set to
  // a negative number, and cause unexpected behaviour.
  DCHECK(context_id > 0);
}

void EmojiSuggester::OnBlur() {
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

}  // namespace input_method
}  // namespace ash
