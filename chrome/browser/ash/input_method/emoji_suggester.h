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

class Profile;

namespace ash {
namespace input_method {

constexpr int kEmojiSuggesterShowSettingMaxCount = 10;

// An agent to suggest emoji when the user types, and adopt or
// dismiss the suggestion according to the user action.
class EmojiSuggester : public Suggester {
 public:
  EmojiSuggester(SuggestionHandlerInterface* engine, Profile* profile);
  ~EmojiSuggester() override;

  // Suggester overrides:
  void OnFocus(int context_id) override;
  void OnBlur() override;
  void OnExternalSuggestionsUpdated(
      const std::vector<ime::AssistiveSuggestion>& suggestions,
      const std::optional<ime::SuggestionsTextContext>& context) override;
  SuggestionStatus HandleKeyEvent(const ui::KeyEvent& event) override;
  bool TrySuggestWithSurroundingText(const std::u16string& text,
                                     gfx::Range selection_range) override;
  bool AcceptSuggestion(size_t index) override;
  void DismissSuggestion() override;
  AssistiveType GetProposeActionType() override;
  bool HasSuggestions() override;
  std::vector<ime::AssistiveSuggestion> GetSuggestions() override;

  bool ShouldShowSuggestion(const std::u16string& text);

  // TODO(crbug/1223666): Remove when we no longer need to prod private vars
  //     for unit testing.
  void LoadEmojiMapForTesting(const std::string& emoji_data);
  size_t GetCandidatesSizeForTesting() const;

 private:
  void ShowSuggestion(const std::string& text);
  void ShowSuggestionWindow();
  void LoadEmojiMap();
  void OnEmojiDataLoaded(const std::string& emoji_data);
  void RecordAcceptanceIndex(int index);

  void SetButtonHighlighted(const ui::ime::AssistiveWindowButton& button,
                            bool highlighted);

  const raw_ptr<SuggestionHandlerInterface, DanglingUntriaged>
      suggestion_handler_;
  raw_ptr<Profile, DanglingUntriaged> profile_;

  // ID of the focused text field, nullopt if none is focused.
  std::optional<int> focused_context_id_;

  // If we are showing a suggestion right now.
  bool suggestion_shown_ = false;

  // The current list of candidates.
  std::vector<std::u16string> candidates_;
  AssistiveWindowProperties properties_;

  std::vector<ui::ime::AssistiveWindowButton> buttons_;
  int highlighted_index_;
  ui::ime::AssistiveWindowButton suggestion_button_;
  ui::ime::AssistiveWindowButton learn_more_button_;

  // The map holding one-word-mapping to emojis.
  std::map<std::string, std::vector<std::u16string>> emoji_map_;

  // Pointer for callback, must be the last declared in the file.
  base::WeakPtrFactory<EmojiSuggester> weak_factory_{this};
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EMOJI_SUGGESTER_H_
