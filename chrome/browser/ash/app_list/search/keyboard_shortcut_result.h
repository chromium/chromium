// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_RESULT_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_RESULT_H_

#include <string>
#include <string_view>
#include <vector>

#include "ash/public/mojom/accelerator_info.mojom-forward.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom-forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/profiles/profile.h"

namespace app_list {

namespace test {
class KeyboardShortcutResultTest;
}

class KeyboardShortcutResult : public ChromeSearchResult {
 public:
  KeyboardShortcutResult(
      Profile* profile,
      const ash::shortcut_customization::mojom::SearchResultPtr& search_result);
  KeyboardShortcutResult(const KeyboardShortcutResult&) = delete;
  KeyboardShortcutResult& operator=(const KeyboardShortcutResult&) = delete;

  ~KeyboardShortcutResult() override;

  // ChromeSearchResult:
  void Open(int event_flags) override;

 private:
  // ui::KeyboardCode represents icon codes in the backend.
  // ash::SearchResultTextItem::IconCode represents icon codes in the frontend.
  // The supported front-end icon codes are a small subset of the existing
  // backend icon codes. Returns nullopt for unsupported codes.
  static std::optional<ash::SearchResultTextItem::IconCode>
      GetIconCodeFromKeyboardCode(ui::KeyboardCode);
  // The `key_string` represents the keyboard code's string representation.
  // ash::SearchResultTextItem::IconCode represents icon codes in the frontend.
  // The supported front-end icon codes are a small subset of the existing
  // backend icon codes. Returns nullopt for unsupported codes.
  static std::optional<ash::SearchResultTextItem::IconCode>
  GetIconCodeByKeyString(std::u16string_view key_string);

  // Add the `accelerator` to the `text_vector` and populate
  // `accessible_strings`.
  void PopulateTextVector(TextVector* text_vector,
                          std::vector<std::u16string>& accessible_strings,
                          const ui::Accelerator& accelerator);

  // Add the `accelerator_parts` to the `text_vector` and populate
  // `accessible_strings`.
  void PopulateTextVectorWithTextParts(
      TextVector* text_vector,
      std::vector<std::u16string>& accessible_strings,
      const std::vector<ash::mojom::TextAcceleratorPartPtr>& accelerator_parts);

  // Add the `accelerator_info` to the `text_vector` and populate
  // `accessible_strings`.
  void PopulateTextVector(
      TextVector* text_vector,
      std::vector<std::u16string>& accessible_strings,
      const ash::mojom::AcceleratorInfoPtr& accelerator_info);

  // Add `accelerator_1` and `accelerator_2` to the `text_vector` and populate
  // `accessible_strings`. When there are more than one shortcuts, we only show
  // the first two.
  void PopulateTextVectorWithTwoShortcuts(
      TextVector* text_vector,
      std::vector<std::u16string>& accessible_strings,
      const ash::mojom::AcceleratorInfoPtr& accelerator_1,
      const ash::mojom::AcceleratorInfoPtr& accelerator_2);

  // Populate text vector for 'No shortcut assigned' case.
  void PopulateTextVectorForNoShortcut(
      TextVector* text_vector,
      std::vector<std::u16string>& accessible_strings);

  void UpdateIcon();

  // The following info will be passed to the shortcuts app when a result is
  // clicked so that the selected shortcuts will be displayed in the app.
  std::string accelerator_action_;
  std::string accelerator_category_;

  raw_ptr<Profile> profile_;
  friend class test::KeyboardShortcutResultTest;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_RESULT_H_
