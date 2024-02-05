// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_AUTOCORRECT_MANAGER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_AUTOCORRECT_MANAGER_H_

#include <optional>
#include <string>

#include "ash/system/federated/federated_client_manager.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/input_method/assistive_input_denylist.h"
#include "chrome/browser/ash/input_method/assistive_window_controller.h"
#include "chrome/browser/ash/input_method/autocorrect_enums.h"
#include "chrome/browser/ash/input_method/diacritics_insensitive_string_comparator.h"
#include "chrome/browser/ash/input_method/input_method_engine.h"
#include "chrome/browser/ash/input_method/suggestion_handler_interface.h"
#include "chrome/browser/ash/input_method/text_field_contextual_info_fetcher.h"
#include "chromeos/ash/services/ime/public/cpp/autocorrect.h"

namespace ash {
namespace input_method {

// Implements functionality for chrome.input.ime.autocorrect() extension API.
// This function shows UI to indicate that autocorrect has happened and allows
// it to be undone easily.
class AutocorrectManager {
 public:
  // `profile` and `suggestion_handler` must be alive for the lifetime of this
  // instance.
  explicit AutocorrectManager(SuggestionHandlerInterface* suggestion_handler,
                              Profile* profile);
  ~AutocorrectManager();

  AutocorrectManager(const AutocorrectManager&) = delete;
  AutocorrectManager& operator=(const AutocorrectManager&) = delete;

  // Mark `autocorrect_range` with an underline. `autocorrect_range` is based on
  // the `current_text` contents.
  // NOTE: Technically redundant to require client to supply `current_text` as
  // AutocorrectManager can retrieve it from current text editing state known to
  // IMF. However, due to async situation between browser-process IMF and
  // render-process TextInputClient, it may just get a stale value that way.
  // TODO(crbug/1194424): Remove technically redundant `current_text` param
  // to avoid situation with multiple conflicting sources of truth.
  void HandleAutocorrect(gfx::Range autocorrect_range,
                         const std::u16string& original_text,
                         const std::u16string& current_text);

  // Called when a new input method is activated.
  void OnActivate(const std::string& engine_id);

  // Handles interactions with Undo UI.
  bool OnKeyEvent(const ui::KeyEvent& event);

  // Indicates a new text field is focused, used to save context ID.
  void OnFocus(int context_id);

  // Triggered whenever a connection to the external autocorrect suggestion
  // provider has been initialized successfully.
  void OnConnectedToSuggestionProvider(
      const ime::AutocorrectSuggestionProvider& suggestion_provider);

  // Handles OnBlur event and processes any pending autocorrect range.
  void OnBlur();

  // Processes the state where a user leaves or focuses a text field. At this
  // stage any pending autocorrect range is cleared and relevant metrics are
  // recorded.
  void ProcessTextFieldChange();

  // To show the undo window when cursor is in an autocorrected word, this class
  // is notified of surrounding text changes.
  void OnSurroundingTextChanged(const std::u16string& text,
                                gfx::Range selection_range);

  // Hides undo window if there is any visible.
  void HideUndoWindow();

  void UndoAutocorrect();

  // Whether autocorrect is disabled by some rule.
  bool DisabledByRule();

  // Whether autocorrect is disabled by an "invalid" experiment context. An
  // example of an invalid experiment context could be a provider or decoder
  // parameter set that is not allowed with the currently enabled experiments.
  bool DisabledByInvalidExperimentContext();

  const federated::FederatedClientManager& GetFederatedClientManagerForTest()
      const {
    return federated_manager_;
  }

 private:
  void LogAssistiveAutocorrectAction(AutocorrectActions action);
  void LogRejectionInteractions(AutocorrectActions action);
  void MeasureAndLogAssistiveAutocorrectQualityBreakdown(
      AutocorrectActions action);
  void LogAssistiveAutocorrectInternalState(
      AutocorrectInternalStates internal_state);
  bool AutoCorrectPrefIsPkEnabledByDefault();
  void LogAssistiveAutocorrectQualityBreakdown(
      AutocorrectQualityBreakdown quality_breakdown,
      bool suggestion_accepted,
      bool virtual_keyboard_visible);

  void OnTextFieldContextualInfoChanged(const TextFieldContextualInfo& info);

  // Forces to accept or clear a pending autocorrect suggestion if any. If the
  // autocorrect range is empty, it means the user interacted with the
  // pending autocorrect suggestion and made it invalid, so it considers
  // the autocorrect suggestion as "cleared". Otherwise, it considers the
  // autocorrect suggestion as "accepted". For the both cases, relevant
  // metrics are recorded, state variables are reset and autocorrect range is
  // set to empty.
  void AcceptOrClearPendingAutocorrect();

  // Shows undo window and record the relevant metric if undo window is
  // not already visible.
  void ShowUndoWindow(gfx::Range range, const std::u16string& text);

  // Highlights the appropriate undo or learn more buttons in the undo window
  void HighlightButtons(bool should_highlight_undo,
                        bool should_highlight_learn_more);

  // Processes the result of a set autocorrect range call. An unsuccessful
  // result could mean that autocorrect was not supported by the text input
  // client, so the autocorrect suggestion can be ignored. Otherwise, the
  // autocorrect suggestion will be set as pending and its relevant
  // interactions and metrics will be managed here.
  void ProcessSetAutocorrectRangeDone(const gfx::Range& autocorrect_range,
                                      const std::u16string& original_text,
                                      const std::u16string& current_text,
                                      bool set_range_success);

  // Records any pending metrics that are awaiting a key press from the user.
  void RecordPendingMetricsAwaitingKeyPress();

  struct PendingAutocorrectState {
    explicit PendingAutocorrectState(const std::u16string& original_text,
                                     const std::u16string& suggested_text,
                                     const base::TimeTicks& start_time,
                                     bool virtual_keyboard_visible = false,
                                     bool learn_more_button_visible = false);
    PendingAutocorrectState(const PendingAutocorrectState& other);
    ~PendingAutocorrectState();

    // Original text that is now corrected by autocorrect.
    std::u16string original_text;

    // Autocorrect suggestion that replaced original text.
    std::u16string suggested_text;

    // Specifies if the suggestion is validated in the surrounding text.
    bool is_validated = false;

    // Number of times that validation of autocorrect suggestion in the
    // surrounding text failed.
    int validation_tries = 0;

    // Number of characters inserted anytime after setting the pending
    // autocorrect range. Negative means no autocorrect range is pending or a
    // range has just been set to pending with no OnSurroundingTextChanged
    // called yet.
    int num_inserted_chars = -1;

    // Last known text length from OnSurroundingTextChanged after setting
    // the pending autocorrect range. Negative means no autocorrect range is
    // pending or a range has just been set to pending with no
    // OnSurroundingTextChanged called yet.
    int text_length = -1;

    // Specifies if undo window is visible or not.
    bool undo_window_visible = false;

    // Specifies if undo button is highlighted or not.
    bool undo_button_highlighted = false;

    // Specifies if learn more button is highlighted or not.
    bool learn_more_button_highlighted = false;

    // Specifies if window_shown metric is already incremented for the pending
    // autocorrect or not.
    bool window_shown_logged = false;

    // The time of setting the pending range.
    base::TimeTicks start_time;

    // Specifies if virtual keyboard was visible when suggesting the pending
    // autocorrect or not.
    bool virtual_keyboard_visible = false;

    // Specifies if learn more button is visible or not.
    bool learn_more_button_visible = false;

    // Records the most recent keypress and if control was down for use in
    // metrics.
    std::optional<ui::KeyEvent> last_key_event;

    // The range of the current pending autocorrect.
    gfx::Range last_autocorrect_range = gfx::Range();

    // The range of the selected text or (cursor_pos, cursor_pos] if no text is
    // selected.
    gfx::Range last_selection_range = gfx::Range();

    // Records the difference in length between the previous text and the
    // current |current text| - |prev text|.
    int text_length_diff = 0;
  };

  struct PendingPhysicalKeyboardUserPrefMetric {
    // The currently active engine id.
    std::string engine_id;
  };

  struct PendingSuggestionProviderMetric {
    // Suggestion provider that has been connected.
    ime::AutocorrectSuggestionProvider provider;
  };

  // State variable for pending autocorrect, nullopt means no autocorrect
  // suggestion is pending. The state is kept to avoid issue where
  // InputContext returns stale autocorrect range.
  std::optional<PendingAutocorrectState> pending_autocorrect_;

  // Specifies if the last try for hiding undo window failed. This means
  // undo window is possibly visible while it must not be.
  bool error_on_hiding_undo_window_ = false;

  // The number of autocorrect suggestions that have been handled since
  // focusing on the text field.
  int num_handled_autocorrect_in_text_field_ = 0;

  // Holds the currently active engine_id. There are cases where this could be
  // a nullopt, for example, when the object has been constructed and the
  // OnActivate method has not been invoked.
  std::optional<std::string> active_engine_id_;

  // Holds a pending physical keyboard user preference metric ready to be
  // recorded. This metric should be recorded once per input focused, and only
  // if the user is currently using the physical keyboard.
  std::optional<PendingPhysicalKeyboardUserPrefMetric>
      pending_user_pref_metric_;

  // Holds a pending suggestion provider metric. This metric should be recorded
  // only once per input, and only if the user is currently using the physical
  // keyboard.
  std::optional<PendingSuggestionProviderMetric>
      pending_suggestion_provider_metric_;

  // Holds the suggestion provider enabled for the current input method.
  std::optional<ime::AutocorrectSuggestionProvider> suggestion_provider_;

  // Used to determine if autocorrect should be enabled for a particular input.
  AssistiveInputDenylist denylist_;

  // Holds the identifier of the currently focused input field.
  int context_id_ = 0;

  // Not owned by this class.
  raw_ptr<SuggestionHandlerInterface> suggestion_handler_;
  raw_ptr<Profile> profile_;

  // For logging examples to the CrOS Federated Service.
  federated::FederatedClientManager federated_manager_;

  DiacriticsInsensitiveStringComparator
      diacritics_insensitive_string_comparator_;

  ui::ime::AssistiveWindowButton undo_button_;
  ui::ime::AssistiveWindowButton learn_more_button_;

  bool disabled_by_rule_ = false;

  base::WeakPtrFactory<AutocorrectManager> weak_ptr_factory_{this};
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_AUTOCORRECT_MANAGER_H_:w
