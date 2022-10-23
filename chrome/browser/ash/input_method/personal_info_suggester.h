// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_PERSONAL_INFO_SUGGESTER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_PERSONAL_INFO_SUGGESTER_H_

#include <string>

#include "base/time/time.h"
#include "chrome/browser/ash/input_method/input_method_engine.h"
#include "chrome/browser/ash/input_method/suggester.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "chrome/browser/ash/input_method/suggestion_handler_interface.h"
#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {
class PersonalDataManager;
}  // namespace autofill

class Profile;

namespace ash {
namespace input_method {

const char kPersonalInfoSuggesterAcceptanceCount[] =
    "personal_info_suggester_acceptance_count";
const int kMaxAcceptanceCount = 10;
const char kPersonalInfoSuggesterShowSettingCount[] =
    "personal_info_suggester_show_setting_count";
const int kMaxShowSettingCount = 10;

AssistiveType ProposePersonalInfoAssistiveAction(const std::u16string& text);

// An agent to suggest personal information when the user types, and adopt or
// dismiss the suggestion according to the user action.
class PersonalInfoSuggester : public Suggester {
 public:
  // If |personal_data_manager| is nullptr, we will obtain it from
  // |PersonalDataManagerFactory| according to |profile|.
  PersonalInfoSuggester(
      SuggestionHandlerInterface* suggestion_handler,
      Profile* profile,
      autofill::PersonalDataManager* personal_data_manager = nullptr);
  ~PersonalInfoSuggester() override;

  bool IsFirstShown() { return first_shown_; }

  // Suggester overrides:
  void OnFocus(int context_id) override;
  void OnBlur() override;
  void OnExternalSuggestionsUpdated(
      const std::vector<ime::AssistiveSuggestion>& suggestions) override;
  SuggestionStatus HandleKeyEvent(const ui::KeyEvent& event) override;
  bool TrySuggestWithSurroundingText(const std::u16string& text,
                                     int cursor_pos,
                                     int anchor_pos) override;
  // index defaults to 0 as not required for this suggester.
  bool AcceptSuggestion(size_t index = 0) override;
  void DismissSuggestion() override;
  AssistiveType GetProposeActionType() override;
  bool HasSuggestions() override;
  std::vector<ime::AssistiveSuggestion> GetSuggestions() override;

 private:
  // Get the suggestion according to |text|.
  std::u16string GetSuggestion(const std::u16string& text);

  void ShowSuggestion(const std::u16string& text,
                      const size_t confirmed_length);

  int GetPrefValue(const std::string& pref_name);

  // Increment int value for the given pref_name by 1 every time the function is
  // called. The function has no effect after the int value becomes equal to the
  // max_value.
  void IncrementPrefValueTilCapped(const std::string& pref_name, int max_value);

  void SetButtonHighlighted(const ui::ime::AssistiveWindowButton& button,
                            bool highlighted);

  SuggestionHandlerInterface* const suggestion_handler_;

  // ID of the focused text field, nullopt if nothing is focused.
  absl::optional<int> focused_context_id_;

  // Assistive type of the last proposed assistive action.
  AssistiveType proposed_action_type_ = AssistiveType::kGenericAction;

  // User's Chrome user profile.
  Profile* const profile_;

  // Personal data manager provided by autofill service.
  autofill::PersonalDataManager* const personal_data_manager_;

  // If we are showing a suggestion right now.
  bool suggestion_shown_ = false;

  // If we are showing the suggestion for the first time.
  bool first_shown_ = false;

  // The current suggestion text shown.
  std::u16string suggestion_;

  std::vector<ui::ime::AssistiveWindowButton> buttons_;
  int highlighted_index_;
  ui::ime::AssistiveWindowButton suggestion_button_;
  ui::ime::AssistiveWindowButton settings_button_;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_PERSONAL_INFO_SUGGESTER_H_
