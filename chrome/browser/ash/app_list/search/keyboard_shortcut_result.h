// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_RESULT_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_RESULT_H_

#include "ash/public/mojom/accelerator_info.mojom-forward.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom-forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/keyboard_shortcut_data.h"
#include "chrome/browser/profiles/profile.h"

namespace ash::string_matching {
class TokenizedString;
}

namespace app_list {

namespace test {
class KeyboardShortcutResultTest;
}

class KeyboardShortcutResult : public ChromeSearchResult {
 public:
  explicit KeyboardShortcutResult(Profile* profile,
                                  const KeyboardShortcutData& data,
                                  double relevance);
  KeyboardShortcutResult(
      Profile* profile,
      const ash::shortcut_customization::mojom::SearchResultPtr& search_result);
  KeyboardShortcutResult(const KeyboardShortcutResult&) = delete;
  KeyboardShortcutResult& operator=(const KeyboardShortcutResult&) = delete;

  ~KeyboardShortcutResult() override;

  // ChromeSearchResult:
  void Open(int event_flags) override;

  // Calculates the shortcut's relevance score. Will return a default score if
  // the query is missing or the target is empty.
  static double CalculateRelevance(
      const ash::string_matching::TokenizedString& query_tokenized,
      const std::u16string& target);

 private:
  // ui::KeyboardCode represents icon codes in the backend.
  // ash::SearchResultTextItem::IconCode represents icon codes in the frontend.
  // The supported front-end icon codes are a small subset of the existing
  // backend icon codes. Returns nullopt for unsupported codes.
  static absl::optional<ash::SearchResultTextItem::IconCode>
      GetIconCodeFromKeyboardCode(ui::KeyboardCode);
  // The `key_string` represents the keyboard code's string representation.
  // ash::SearchResultTextItem::IconCode represents icon codes in the frontend.
  // The supported front-end icon codes are a small subset of the existing
  // backend icon codes. Returns nullopt for unsupported codes.
  static absl::optional<ash::SearchResultTextItem::IconCode>
  GetIconCodeByKeyString(base::StringPiece16 key_string);

  // Parse a |template_string| (containing placeholders of the form $i). The
  // output is a TextVector where the TextItem elements can be of three
  // different types:
  //   1. kString: For plain-text portions of the template string.
  //   2. kIconCode: For where a placeholder is replaced with an icon.
  //   3. kIconifiedText: For where a placeholder is replaced with a string
  //      representation of a shortcut key, where an icon for that key is not
  //      supported.
  static ChromeSearchResult::TextVector CreateTextVectorFromTemplateString(
      const std::u16string& template_string,
      const std::vector<std::u16string>& replacement_strings,
      const std::vector<ui::KeyboardCode>& shortcut_key_codes);

  // Add the `accelerator` to the `text_vector`.
  void PopulateTextVector(TextVector* text_vector,
                          const ui::Accelerator& accelerator);

  // Add the `accelerator_parts` to the `text_vector`.
  void PopulateTextVectorWithTextParts(
      TextVector* text_vector,
      const std::vector<ash::mojom::TextAcceleratorPartPtr>& accelerator_parts);

  void UpdateIcon();

  raw_ptr<Profile, ExperimentalAsh> profile_;
  friend class test::KeyboardShortcutResultTest;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_RESULT_H_
