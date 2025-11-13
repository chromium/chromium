// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EMOJI_SUGGESTER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EMOJI_SUGGESTER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/input_method/input_method_engine.h"
#include "chrome/browser/ash/input_method/suggester.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "chrome/browser/ash/input_method/suggestion_handler_interface.h"
#include "chrome/browser/ui/ash/input_method/assistive_delegate.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"

namespace ash {
namespace input_method {

// An agent to suggest emoji when the user types, and adopt or
// dismiss the suggestion according to the user action.
class EmojiSuggester {
 public:
  EmojiSuggester();
  ~EmojiSuggester();

  void OnFocus(int context_id);
  void OnBlur();
  bool ShouldShowSuggestion(const std::u16string& text);

  // TODO(crbug/1223666): Remove when we no longer need to prod private vars
  //     for unit testing.
  void LoadEmojiMapForTesting(const std::string& emoji_data);

 private:
  void LoadEmojiMap();
  void OnEmojiDataLoaded(const std::string& emoji_data);

  // The map holding one-word-mapping to emojis.
  std::map<std::string, std::vector<std::u16string>> emoji_map_;

  // Pointer for callback, must be the last declared in the file.
  base::WeakPtrFactory<EmojiSuggester> weak_factory_{this};
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EMOJI_SUGGESTER_H_
