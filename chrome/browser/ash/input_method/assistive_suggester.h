// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/input_method/assistive_suggester_switch.h"
#include "chrome/browser/ash/input_method/emoji_suggester.h"
#include "chrome/browser/ash/input_method/longpress_control_v_suggester.h"
#include "chrome/browser/ash/input_method/longpress_diacritics_suggester.h"
#include "chrome/browser/ash/input_method/multi_word_suggester.h"
#include "chrome/browser/ash/input_method/suggester.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "chrome/browser/ash/input_method/suggestion_handler_interface.h"
#include "chrome/browser/ash/input_method/suggestions_source.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"

namespace ash::input_method {

enum class AssistiveSuggesterKeyResult {
  // The key event was not handled by the assistive suggester.
  // The key event should be handled via normal key event flow.
  kNotHandled,
  // The key event was handled by the assistive suggester.
  // The key event should not be propagated as-is. Instead, it should be
  // dispatched as a PROCESS key to prevent the client from triggering the
  // default behaviour for the key.
  kHandled,
  // Same as not kNotHandled, except the key event should not trigger
  // autorepeat.
  kNotHandledSuppressAutoRepeat,
};

// An agent to suggest assistive information when the user types, and adopt or
// dismiss the suggestion according to the user action.
class AssistiveSuggester : public SuggestionsSource {
 public:
  // Features handled by assistive suggester.
  enum class AssistiveFeature {
    kUnknown,  // Includes features not handled by assistive suggester.
    kEmojiSuggestion,
    kMultiWordSuggestion,
  };

  AssistiveSuggester(
      SuggestionHandlerInterface* suggestion_handler,
      Profile* profile,
      std::unique_ptr<AssistiveSuggesterSwitch> suggester_switch);

  ~AssistiveSuggester() override;

  bool IsAssistiveFeatureEnabled();

  // Fetches enabled suggestions in the current browser context then run
  // callback.
  void FetchEnabledSuggestionsFromBrowserContextThen(
      AssistiveSuggesterSwitch::FetchEnabledSuggestionsCallback callback);

  // SuggestionsSource overrides
  std::vector<ime::AssistiveSuggestion> GetSuggestions() override;

  // Called when a new input engine is activated by the system.
  void OnActivate(const std::string& engine_id);

  // Called when a text field gains focus, and suggester starts working.
  void OnFocus(int context_id, const TextInputMethod::InputContext& context);

  // Called when a text field loses focus, and suggester stops working.
  void OnBlur();

  // Called when a surrounding text is changed.
  // Returns true if it changes the surrounding text, e.g. a suggestion is
  // generated or dismissed.
  void OnSurroundingTextChanged(const std::u16string& text,
                                gfx::Range selection_range);

  // Called when the user pressed a key.
  AssistiveSuggesterKeyResult OnKeyEvent(const ui::KeyEvent& event);

  // Called when suggestions are generated outside of the assistive framework.
  void OnExternalSuggestionsUpdated(
      const std::vector<ime::AssistiveSuggestion>& suggestions,
      const std::optional<ime::SuggestionsTextContext>& context);

  // Accepts the suggestion at a given index if a suggester is currently
  // active.
  void AcceptSuggestion(size_t index);

  // Check if suggestion is being shown.
  bool IsSuggestionShown();

  EmojiSuggester* get_emoji_suggester_for_testing() {
    return &emoji_suggester_;
  }

  std::optional<AssistiveSuggesterSwitch::EnabledSuggestions>
  get_enabled_suggestion_from_last_onfocus_for_testing() {
    return enabled_suggestions_from_last_onfocus_;
  }

 private:
  // Callback that is run after enabled_suggestions is received.
  void ProcessOnSurroundingTextChanged(
      const std::u16string& text,
      gfx::Range selection_range,
      const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions);

  // Returns if any suggestion text should be displayed according to the
  // surrounding text information.
  bool TrySuggestWithSurroundingText(
      const std::u16string& text,
      gfx::Range selection_range,
      const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions);

  void DismissSuggestion();

  bool IsEmojiSuggestAdditionEnabled();

  bool IsMultiWordSuggestEnabled();

  bool IsDiacriticsOnPhysicalKeyboardLongpressEnabled();

  // Checks the text before cursor, emits metric if any assistive prefix is
  // matched.
  void RecordAssistiveMatchMetrics(
      const std::u16string& text,
      gfx::Range selection_range,
      const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions);

  void RecordAssistiveMatchMetricsForAssistiveType(
      AssistiveType type,
      const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions);

  // Only the first applicable reason in DisabledReason enum is returned.
  DisabledReason GetDisabledReasonForEmoji(
      const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions);

  // Only the first applicable reason in DisabledReason enum is returned.
  DisabledReason GetDisabledReasonForMultiWord(
      const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions);

  AssistiveFeature GetAssistiveFeatureForType(AssistiveType type);

  bool IsAssistiveTypeEnabled(AssistiveType type);

  bool IsAssistiveTypeAllowedInBrowserContext(
      AssistiveType type,
      const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions);

  bool WithinGrammarFragment();

  void ProcessExternalSuggestions(
      const std::vector<ime::AssistiveSuggestion>& suggestions,
      const std::optional<ime::SuggestionsTextContext>& context,
      const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions);

  // This records any text input state metrics for each relevant assistive
  // feature. It is called once when a text field gains focus.
  void RecordTextInputStateMetrics(
      const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions);

  // Does longpress related processing (if enabled).
  // Returns true if we block the keyevent from passing to IME, and stop
  // dispatch.
  // Returns false, if we want IME to process the event and dispatch it.
  AssistiveSuggesterKeyResult HandleLongpressEnabledKeyEvent(
      const ui::KeyEvent& key_character);

  void HandleEnabledSuggestionsOnFocus(
      const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions);

  void OnLongpressDetected();

  // Accepts or dismisses a Ctrl+V long-press suggestion based on the exit
  // status of the clipboard history menu, as indicated by `will_paste_item`.
  void OnClipboardHistoryMenuClosing(bool will_paste_item);

  raw_ptr<Profile> profile_;
  EmojiSuggester emoji_suggester_;
  MultiWordSuggester multi_word_suggester_;
  LongpressDiacriticsSuggester longpress_diacritics_suggester_;
  LongpressControlVSuggester longpress_control_v_suggester_;
  std::unique_ptr<AssistiveSuggesterSwitch> suggester_switch_;

  // The id of the currently active input engine.
  std::string active_engine_id_;

  // ID of the focused text field, nullopt if none focused.
  std::optional<int> focused_context_id_;

  // KeyEvent of the held down key at key down. nullopt if no longpress in
  // progress.
  std::optional<ui::KeyEvent> current_longpress_keydown_;

  // Timer for longpress. Starts when key is held down. Fires when successfully
  // held down for a specified longpress duration.
  base::OneShotTimer longpress_timer_;

  // The current suggester in use, nullptr means no suggestion is shown.
  raw_ptr<Suggester> current_suggester_ = nullptr;

  std::optional<AssistiveSuggesterSwitch::EnabledSuggestions>
      enabled_suggestions_from_last_onfocus_;

  std::u16string last_surrounding_text_ = u"";

  // Keeps track if there is a key being held down currently which was already
  // recorded for the auto repeat suppressed metric.
  bool auto_repeat_suppress_metric_emitted_ = false;

  int last_cursor_pos_ = 0;

  TextInputMethod::InputContext context_;

  base::WeakPtrFactory<AssistiveSuggester> weak_ptr_factory_{this};
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_H_
