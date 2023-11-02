// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_UI_ASSISTIVE_DELEGATE_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_UI_ASSISTIVE_DELEGATE_H_

#include <string>

#include "ui/chromeos/ui_chromeos_export.h"

namespace ui {
namespace ime {

enum class ButtonId {
  kNone,
  kUndo,
  kAddToDictionary,
  kSmartInputsSettingLink,
  kSuggestion,
  kLearnMore,
  kIgnoreSuggestion,
};

enum class AssistiveWindowType {
  kNone,
  kUndoWindow,
  kEmojiSuggestion,
  kPersonalInfoSuggestion,
  kGrammarSuggestion,
  kMultiWordSuggestion,
  kLongpressDiacriticsSuggestion,
};

struct AssistiveWindowButton {
  ButtonId id = ButtonId::kNone;
  AssistiveWindowType window_type = AssistiveWindowType::kNone;
  // TODO(crbug/1101852): Rename index to suggestion_index for further clarity.
  // Currently index is only considered when ButtonId is kSuggestion.
  size_t index = -1;
  std::u16string announce_string;

  bool operator==(const AssistiveWindowButton& other) const {
    return id == other.id && window_type == other.window_type &&
           index == other.index && announce_string == other.announce_string;
  }
};

class UI_CHROMEOS_EXPORT AssistiveDelegate {
 public:
  // Invoked when a button in an assistive window is clicked.
  virtual void AssistiveWindowButtonClicked(
      const AssistiveWindowButton& button) const = 0;

 protected:
  virtual ~AssistiveDelegate() = default;
};

}  // namespace ime
}  // namespace ui

#endif  //  CHROME_BROWSER_ASH_INPUT_METHOD_UI_ASSISTIVE_DELEGATE_H_
