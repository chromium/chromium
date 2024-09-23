// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/input_method/assistive_suggester.h"

#include <string>

#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/input_method/assistive_prefs.h"
#include "chrome/browser/ash/input_method/assistive_suggester_switch.h"
#include "chrome/browser/ash/input_method/suggestion_handler_interface.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "components/exo/wm_helper.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/input_method_ukm.h"
#include "ui/base/ime/ash/text_input_target.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace ash::input_method {

namespace {

using ime::AssistiveSuggestion;
using ime::AssistiveSuggestionMode;
using ime::AssistiveSuggestionType;
using ime::SuggestionsTextContext;

constexpr int kModifierKeysMask = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                  ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN |
                                  ui::EF_FUNCTION_DOWN | ui::EF_ALTGR_DOWN;

const char kMaxTextBeforeCursorLength = 50;

constexpr base::TimeDelta kLongpressActivationDelay = base::Milliseconds(500);

// TODO(b/217560706): Make this different based on current engine after research
// is conducted.
constexpr auto kDefaultLongpressEnabledKeys = base::MakeFixedFlatSet<char>(
    {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
     'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
     'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
     'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'});

void RecordAssistiveMatch(AssistiveType type) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Match", type);

  TextInputTarget* input_context = IMEBridge::Get()->GetInputContextHandler();
  if (!input_context) {
    return;
  }

  auto sourceId = input_context->GetClientSourceForMetrics();
  if (sourceId != ukm::kInvalidSourceId) {
    RecordUkmAssistiveMatch(sourceId, static_cast<int>(type));
  }
}

void RecordAssistiveDisabled(AssistiveType type) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Disabled", type);
}

void RecordAssistiveDisabledReasonForEmoji(DisabledReason reason) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Disabled.Emoji", reason);
}

void RecordAssistiveDisabledReasonForMultiWord(DisabledReason reason) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Disabled.MultiWord",
                                reason);
}

void RecordAssistiveUserPrefForEmoji(bool value) {
  base::UmaHistogramBoolean("InputMethod.Assistive.UserPref.Emoji", value);
}

void RecordAssistiveUserPrefForMultiWord(bool value) {
  base::UmaHistogramBoolean("InputMethod.Assistive.UserPref.MultiWord", value);
}

void RecordAssistiveUserPrefForDiacriticsOnLongpress(bool value) {
  base::UmaHistogramBoolean(
      "InputMethod.Assistive.UserPref.PhysicalKeyboardDiacriticsOnLongpress",
      value);
}

void RecordAssistiveNotAllowed(AssistiveType type) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.NotAllowed", type);
}

void RecordAssistiveCoverage(AssistiveType type) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Coverage", type);
}

void RecordAssistiveSuccess(AssistiveType type) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Success", type);
}

void RecordLongPressDiacriticAutoRepeatSuppressedMetric() {
  base::UmaHistogramEnumeration(
      "InputMethod.PhysicalKeyboard.LongpressDiacritics.Action",
      IMEPKLongpressDiacriticAction::kAutoRepeatSuppressed);
}

bool IsTopResultMultiWord(const std::vector<AssistiveSuggestion>& suggestions) {
  if (suggestions.empty()) {
    return false;
  }
  // There should only ever be one multi word suggestion given if any.
  return suggestions[0].type == AssistiveSuggestionType::kMultiWord;
}

void RecordSuggestionsMatch(
    const std::vector<AssistiveSuggestion>& suggestions) {
  if (suggestions.empty()) {
    return;
  }

  auto top_result = suggestions[0];
  if (top_result.type != AssistiveSuggestionType::kMultiWord) {
    return;
  }

  switch (top_result.mode) {
    case AssistiveSuggestionMode::kCompletion:
      RecordAssistiveMatch(AssistiveType::kMultiWordCompletion);
      return;
    case AssistiveSuggestionMode::kPrediction:
      RecordAssistiveMatch(AssistiveType::kMultiWordPrediction);
      return;
  }
}

bool IsUsEnglishEngine(const std::string& engine_id) {
  return engine_id == "xkb:us::eng";
}

void RecordTextInputStateMetric(AssistiveTextInputState state) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.MultiWord.InputState",
                                state);
}

// Returns whether Ctrl+V is pressed with Ctrl+V long-press behavior enabled.
bool IsLongpressEnabledControlV(const ui::KeyEvent& event) {
  if (!features::IsClipboardHistoryLongpressEnabled()) {
    return false;
  }

  return event.key_code() == ui::VKEY_V &&
         (event.flags() & kModifierKeysMask) == ui::EF_CONTROL_DOWN;
}

// Returns the location to which the clipboard history menu should anchor. When
// possible, this anchor is where a clipboard history item would be pasted if
// the user made a selection; otherwise, this function returns a point at (0,0).
gfx::Rect GetClipboardHistoryMenuAnchor() {
  TextInputTarget* input_context = IMEBridge::Get()->GetInputContextHandler();
  if (!input_context) {
    return gfx::Rect();
  }

  ui::TextInputClient* input_client =
      input_context->GetInputMethod()->GetTextInputClient();
  if (!input_client) {
    return gfx::Rect();
  }

  return input_client->GetCaretBounds();
}

void RecordMultiWordTextInputState(
    PrefService* pref_service,
    const std::string& engine_id,
    const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions) {
  if (!enabled_suggestions.multi_word_suggestions) {
    RecordTextInputStateMetric(
        AssistiveTextInputState::kFeatureBlockedByDenylist);
    return;
  }

  if (!IsUsEnglishEngine(engine_id)) {
    RecordTextInputStateMetric(AssistiveTextInputState::kUnsupportedLanguage);
    return;
  }

  if (!IsPredictiveWritingPrefEnabled(*pref_service, engine_id)) {
    RecordTextInputStateMetric(
        AssistiveTextInputState::kFeatureBlockedByPreference);
    return;
  }

  RecordTextInputStateMetric(AssistiveTextInputState::kFeatureEnabled);
}

}  // namespace

AssistiveSuggester::AssistiveSuggester(
    SuggestionHandlerInterface* suggestion_handler,
    Profile* profile,
    std::unique_ptr<AssistiveSuggesterSwitch> suggester_switch)
    : profile_(profile),
      emoji_suggester_(suggestion_handler, profile),
      multi_word_suggester_(suggestion_handler, profile),
      longpress_diacritics_suggester_(suggestion_handler),
      longpress_control_v_suggester_(suggestion_handler),
      suggester_switch_(std::move(suggester_switch)),
      context_(TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_NONE)) {
  RecordAssistiveUserPrefForEmoji(
      profile_->GetPrefs()->GetBoolean(prefs::kEmojiSuggestionEnabled));
}

AssistiveSuggester::~AssistiveSuggester() = default;

bool AssistiveSuggester::IsAssistiveFeatureEnabled() {
  return IsEmojiSuggestAdditionEnabled() || IsMultiWordSuggestEnabled() ||
         IsDiacriticsOnPhysicalKeyboardLongpressEnabled() ||
         features::IsClipboardHistoryLongpressEnabled();
}

void AssistiveSuggester::FetchEnabledSuggestionsFromBrowserContextThen(
    AssistiveSuggesterSwitch::FetchEnabledSuggestionsCallback callback) {
  suggester_switch_->FetchEnabledSuggestionsThen(std::move(callback), context_);
}

bool AssistiveSuggester::IsEmojiSuggestAdditionEnabled() {
  return profile_->GetPrefs()->GetBoolean(
             prefs::kEmojiSuggestionEnterpriseAllowed) &&
         profile_->GetPrefs()->GetBoolean(prefs::kEmojiSuggestionEnabled);
}

bool AssistiveSuggester::IsMultiWordSuggestEnabled() {
  return base::FeatureList::IsEnabled(features::kAssistMultiWord) &&
         IsPredictiveWritingPrefEnabled(*profile_->GetPrefs(),
                                        active_engine_id_);
}

bool AssistiveSuggester::IsDiacriticsOnPhysicalKeyboardLongpressEnabled() {
  return base::FeatureList::IsEnabled(
             features::kDiacriticsOnPhysicalKeyboardLongpress) &&
         IsUsEnglishEngine(active_engine_id_) &&
         IsDiacriticsOnLongpressPrefEnabled(profile_->GetPrefs(),
                                            active_engine_id_);
}

DisabledReason AssistiveSuggester::GetDisabledReasonForEmoji(
    const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions) {
  if (!profile_->GetPrefs()->GetBoolean(
          prefs::kEmojiSuggestionEnterpriseAllowed)) {
    return DisabledReason::kEnterpriseSettingsOff;
  }
  if (!profile_->GetPrefs()->GetBoolean(prefs::kEmojiSuggestionEnabled)) {
    return DisabledReason::kUserSettingsOff;
  }
  if (!enabled_suggestions.emoji_suggestions) {
    return DisabledReason::kUrlOrAppNotAllowed;
  }
  return DisabledReason::kNone;
}

DisabledReason AssistiveSuggester::GetDisabledReasonForMultiWord(
    const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions) {
  if (!base::FeatureList::IsEnabled(features::kAssistMultiWord)) {
    return DisabledReason::kFeatureFlagOff;
  }
  if (!profile_->GetPrefs()->GetBoolean(
          prefs::kAssistPredictiveWritingEnabled)) {
    return DisabledReason::kUserSettingsOff;
  }
  if (!enabled_suggestions.multi_word_suggestions) {
    return DisabledReason::kUrlOrAppNotAllowed;
  }
  return DisabledReason::kNone;
}

AssistiveSuggester::AssistiveFeature
AssistiveSuggester::GetAssistiveFeatureForType(AssistiveType type) {
  switch (type) {
    case AssistiveType::kEmoji:
      return AssistiveFeature::kEmojiSuggestion;
    case AssistiveType::kMultiWordCompletion:
    case AssistiveType::kMultiWordPrediction:
      return AssistiveFeature::kMultiWordSuggestion;
    default:
      // We should only handle Emoji and Multiword related assistive types.
      //
      // Any assistive types outside of this should not be processed in this
      // class, hence we shall DCHECK here if that ever occurs.
      LOG(DFATAL) << "Unexpected AssistiveType value: "
                  << static_cast<int>(type);
      return AssistiveFeature::kUnknown;
  }
}

bool AssistiveSuggester::IsAssistiveTypeEnabled(AssistiveType type) {
  switch (GetAssistiveFeatureForType(type)) {
    case AssistiveFeature::kEmojiSuggestion:
      return IsEmojiSuggestAdditionEnabled();
    case AssistiveFeature::kMultiWordSuggestion:
      return IsMultiWordSuggestEnabled();
    default:
      LOG(DFATAL) << "Unexpected AssistiveType value: "
                  << static_cast<int>(type);
      return false;
  }
}

bool AssistiveSuggester::IsAssistiveTypeAllowedInBrowserContext(
    AssistiveType type,
    const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions) {
  switch (GetAssistiveFeatureForType(type)) {
    case AssistiveFeature::kEmojiSuggestion:
      return enabled_suggestions.emoji_suggestions;
    case AssistiveFeature::kMultiWordSuggestion:
      return enabled_suggestions.multi_word_suggestions;
    default:
      LOG(DFATAL) << "Unexpected AssistiveType value: "
                  << static_cast<int>(type);
      return false;
  }
}

void AssistiveSuggester::OnFocus(int context_id,
                                 const TextInputMethod::InputContext& context) {
  // Some parts of the code reserve negative/zero context_id for unfocused
  // context. As a result we should make sure it is not being errornously set to
  // a negative number, and cause unexpected behaviour.
  context_ = context;
  DCHECK(context_id > 0);
  focused_context_id_ = context_id;
  emoji_suggester_.OnFocus(context_id);
  multi_word_suggester_.OnFocus(context_id);
  longpress_diacritics_suggester_.OnFocus(context_id);
  longpress_control_v_suggester_.OnFocus(context_id);
  enabled_suggestions_from_last_onfocus_ = std::nullopt;
  suggester_switch_->FetchEnabledSuggestionsThen(
      base::BindOnce(&AssistiveSuggester::HandleEnabledSuggestionsOnFocus,
                     weak_ptr_factory_.GetWeakPtr()),
      context);
}

void AssistiveSuggester::HandleEnabledSuggestionsOnFocus(
    const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions) {
  enabled_suggestions_from_last_onfocus_ = enabled_suggestions;
  AssistiveSuggester::RecordTextInputStateMetrics(enabled_suggestions);
}

void AssistiveSuggester::OnBlur() {
  focused_context_id_ = std::nullopt;
  enabled_suggestions_from_last_onfocus_ = std::nullopt;
  emoji_suggester_.OnBlur();
  multi_word_suggester_.OnBlur();
  longpress_diacritics_suggester_.OnBlur();
  longpress_control_v_suggester_.OnBlur();
}

AssistiveSuggesterKeyResult AssistiveSuggester::OnKeyEvent(
    const ui::KeyEvent& event) {
  if (!focused_context_id_.has_value()) {
    return AssistiveSuggesterKeyResult::kNotHandled;
  }

  // Auto repeat resets whenever a key is pressed/released as long as its not a
  // repeat event.
  if (!event.is_repeat()) {
    auto_repeat_suppress_metric_emitted_ = false;
  }

  // We only track keydown event because the suggesting action is triggered by
  // surrounding text change, which is triggered by a keydown event. As a
  // result, the next key event after suggesting would be a keyup event of the
  // same key, and that event is meaningless to us.
  if (IsSuggestionShown() && event.type() == ui::EventType::kKeyPressed &&
      !event.IsControlDown() && !event.IsAltDown() && !event.IsShiftDown()) {
    SuggestionStatus status = current_suggester_->HandleKeyEvent(event);
    switch (status) {
      case SuggestionStatus::kAccept:
        // Handle a race condition where the current suggester_ is set to
        // nullptr by a simultaneous event (such as a key event causing a
        // onBlur() event).
        // TODO(b/240534923): Figure out how to record metrics when
        // current_suggester_ is set to nullptr prematurely by a different
        // event.
        if (current_suggester_) {
          RecordAssistiveSuccess(current_suggester_->GetProposeActionType());
        }
        current_suggester_ = nullptr;
        return AssistiveSuggesterKeyResult::kHandled;
      case SuggestionStatus::kDismiss:
        current_suggester_ = nullptr;
        return AssistiveSuggesterKeyResult::kHandled;
      case SuggestionStatus::kBrowsing:
        return AssistiveSuggesterKeyResult::kHandled;
      default:
        break;
    }
  }

  return AssistiveSuggester::HandleLongpressEnabledKeyEvent(event);
}

AssistiveSuggesterKeyResult AssistiveSuggester::HandleLongpressEnabledKeyEvent(
    const ui::KeyEvent& event) {
  const bool is_enabled_diacritic_long_press =
      IsDiacriticsOnPhysicalKeyboardLongpressEnabled() &&
      enabled_suggestions_from_last_onfocus_ &&
      enabled_suggestions_from_last_onfocus_->diacritic_suggestions &&
      kDefaultLongpressEnabledKeys.contains(event.GetCharacter());
  if (!is_enabled_diacritic_long_press && !IsLongpressEnabledControlV(event)) {
    return AssistiveSuggesterKeyResult::kNotHandled;
  }

  // Longpress diacritics behaviour overrides the longpress to repeat key
  // behaviour for alphabetical keys.
  if (event.is_repeat()) {
    // Check for cases where auto-repeat behavior is suppressed for characters
    // with no available diacritic suggestion. Only emit the metric if
    // `auto_repeat_suppress_metric_emitted_` is false as the metric should only
    // be emitted once per Press->Release cycle.
    if (!auto_repeat_suppress_metric_emitted_ &&
        !longpress_diacritics_suggester_.HasDiacriticSuggestions(
            event.GetCharacter()) &&
        !IsLongpressEnabledControlV(event)) {
      auto_repeat_suppress_metric_emitted_ = true;
      RecordLongPressDiacriticAutoRepeatSuppressedMetric();
    }
    return AssistiveSuggesterKeyResult::kHandled;
  }

  // Process longpress keydown event.
  if (current_longpress_keydown_ == std::nullopt &&
      event.type() == ui::EventType::kKeyPressed) {
    current_longpress_keydown_ = event;

    if (IsLongpressEnabledControlV(event)) {
      longpress_control_v_suggester_.CachePastedTextStart();
    }

    longpress_timer_.Start(
        FROM_HERE, kLongpressActivationDelay,
        base::BindOnce(&AssistiveSuggester::OnLongpressDetected,
                       weak_ptr_factory_.GetWeakPtr()));
    return AssistiveSuggesterKeyResult::kNotHandledSuppressAutoRepeat;
  }

  // Process longpress interrupted event (key press up before timer callback
  // fired)
  if (current_longpress_keydown_.has_value() &&
      event.type() == ui::EventType::kKeyReleased &&
      current_longpress_keydown_->code() == event.code()) {
    current_longpress_keydown_ = std::nullopt;
    longpress_timer_.Stop();
  }
  return AssistiveSuggesterKeyResult::kNotHandled;
}

void AssistiveSuggester::OnLongpressDetected() {
  if (!(current_longpress_keydown_.has_value() ||
        IsLongpressEnabledControlV(current_longpress_keydown_.value()))) {
    return;
  }

  if (IsLongpressEnabledControlV(current_longpress_keydown_.value())) {
    if (Shell::Get()->clipboard_history_controller()->ShowMenu(
            GetClipboardHistoryMenuAnchor(),
            ui::MenuSourceType::MENU_SOURCE_KEYBOARD,
            crosapi::mojom::ClipboardHistoryControllerShowSource::
                kControlVLongpress,
            base::BindOnce(&AssistiveSuggester::OnClipboardHistoryMenuClosing,
                           weak_ptr_factory_.GetWeakPtr()))) {
      // Only set `current_suggester_` if the clipboard history menu was shown.
      current_suggester_ = &longpress_control_v_suggester_;
    }
  } else if (longpress_diacritics_suggester_.TrySuggestOnLongpress(
                 current_longpress_keydown_->GetCharacter())) {
    current_suggester_ = &longpress_diacritics_suggester_;
  }
  current_longpress_keydown_ = std::nullopt;
}

void AssistiveSuggester::OnClipboardHistoryMenuClosing(bool will_paste_item) {
  DCHECK_EQ(current_suggester_, &longpress_control_v_suggester_);
  if (will_paste_item) {
    // Note: The suggestion index is irrelevant for long-pressed Ctrl+V.
    AcceptSuggestion(/*index=*/-1);
  } else {
    DismissSuggestion();
  }
}

void AssistiveSuggester::OnExternalSuggestionsUpdated(
    const std::vector<AssistiveSuggestion>& suggestions,
    const std::optional<SuggestionsTextContext>& context) {
  if (!IsMultiWordSuggestEnabled()) {
    return;
  }

  suggester_switch_->FetchEnabledSuggestionsThen(
      base::BindOnce(&AssistiveSuggester::ProcessExternalSuggestions,
                     weak_ptr_factory_.GetWeakPtr(), suggestions, context),
      context_);
}

void AssistiveSuggester::ProcessExternalSuggestions(
    const std::vector<AssistiveSuggestion>& suggestions,
    const std::optional<SuggestionsTextContext>& context,
    const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions) {
  RecordSuggestionsMatch(suggestions);

  if (!enabled_suggestions.multi_word_suggestions) {
    if (IsTopResultMultiWord(suggestions)) {
      RecordAssistiveDisabledReasonForMultiWord(
          GetDisabledReasonForMultiWord(enabled_suggestions));
    }
    return;
  }

  if (current_suggester_) {
    current_suggester_->OnExternalSuggestionsUpdated(suggestions, context);
    return;
  }

  if (IsTopResultMultiWord(suggestions)) {
    current_suggester_ = &multi_word_suggester_;
    current_suggester_->OnExternalSuggestionsUpdated(suggestions, context);
    // The multi word suggester may not show the suggestions we pass to it. The
    // suggestions received here may be stale and not valid given the current
    // internal state of the multi word suggester.
    if (current_suggester_->HasSuggestions()) {
      RecordAssistiveCoverage(current_suggester_->GetProposeActionType());
    }
  }
}

void AssistiveSuggester::RecordTextInputStateMetrics(
    const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions) {
  if (base::FeatureList::IsEnabled(features::kAssistMultiWord)) {
    RecordMultiWordTextInputState(profile_->GetPrefs(), active_engine_id_,
                                  enabled_suggestions);
  }
}

void AssistiveSuggester::RecordAssistiveMatchMetricsForAssistiveType(
    AssistiveType type,
    const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions) {
  RecordAssistiveMatch(type);
  if (!IsAssistiveTypeEnabled(type)) {
    RecordAssistiveDisabled(type);
  } else if (!IsAssistiveTypeAllowedInBrowserContext(type,
                                                     enabled_suggestions)) {
    RecordAssistiveNotAllowed(type);
  }
}

void AssistiveSuggester::RecordAssistiveMatchMetrics(
    const std::u16string& text,
    const gfx::Range selection_range,
    const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions) {
  int len = static_cast<int>(text.length());
  const int cursor_pos = selection_range.end();
  if (cursor_pos > 0 && cursor_pos <= len && selection_range.is_empty() &&
      (cursor_pos == len || base::IsAsciiWhitespace(text[cursor_pos]))) {
    int start_pos = std::max(0, cursor_pos - kMaxTextBeforeCursorLength);
    std::u16string text_before_cursor =
        text.substr(start_pos, cursor_pos - start_pos);
    // Emoji suggestion match
    if (emoji_suggester_.ShouldShowSuggestion(text_before_cursor)) {
      RecordAssistiveMatchMetricsForAssistiveType(AssistiveType::kEmoji,
                                                  enabled_suggestions);
      base::RecordAction(
          base::UserMetricsAction("InputMethod.Assistive.EmojiSuggested"));
      RecordAssistiveDisabledReasonForEmoji(
          GetDisabledReasonForEmoji(enabled_suggestions));
    }
  }
}

bool AssistiveSuggester::WithinGrammarFragment() {
  TextInputTarget* input_context = IMEBridge::Get()->GetInputContextHandler();
  if (!input_context) {
    return false;
  }

  std::optional<ui::GrammarFragment> grammar_fragment_opt =
      input_context->GetGrammarFragmentAtCursor();

  return grammar_fragment_opt != std::nullopt;
}

void AssistiveSuggester::OnSurroundingTextChanged(
    const std::u16string& text,
    const gfx::Range selection_range) {
  last_surrounding_text_ = text;
  last_cursor_pos_ = selection_range.end();
  suggester_switch_->FetchEnabledSuggestionsThen(
      base::BindOnce(&AssistiveSuggester::ProcessOnSurroundingTextChanged,
                     weak_ptr_factory_.GetWeakPtr(), text, selection_range),
      context_);
}

void AssistiveSuggester::ProcessOnSurroundingTextChanged(
    const std::u16string& text,
    const gfx::Range selection_range,
    const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions) {
  RecordAssistiveMatchMetrics(text, selection_range, enabled_suggestions);
  if (!IsAssistiveFeatureEnabled() || !focused_context_id_.has_value()) {
    return;
  }

  if (IsMultiWordSuggestEnabled() &&
      enabled_suggestions.multi_word_suggestions) {
    // Only multi word cares about tracking the current state of the text
    // field
    multi_word_suggester_.OnSurroundingTextChanged(text, selection_range);
  }

  if (WithinGrammarFragment() ||
      !TrySuggestWithSurroundingText(text, selection_range,
                                     enabled_suggestions)) {
    DismissSuggestion();
  }
}

bool AssistiveSuggester::TrySuggestWithSurroundingText(
    const std::u16string& text,
    const gfx::Range selection_range,
    const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions) {
  if (IsSuggestionShown()) {
    return current_suggester_->TrySuggestWithSurroundingText(text,
                                                             selection_range);
  }
  if (IsEmojiSuggestAdditionEnabled() &&
      enabled_suggestions.emoji_suggestions &&
      emoji_suggester_.TrySuggestWithSurroundingText(text, selection_range)) {
    current_suggester_ = &emoji_suggester_;
    RecordAssistiveCoverage(current_suggester_->GetProposeActionType());
    return true;
  }
  // No suggestions were shown.
  return false;
}

void AssistiveSuggester::AcceptSuggestion(size_t index) {
  if (current_suggester_ && current_suggester_->AcceptSuggestion(index)) {
    // Handle a race condition where the current suggester_ is set to nullptr by
    // a simultaneous event (such as a mouse click causing a onBlur()
    // event).
    // TODO(b/240534923): Figure out how to record metrics when
    // current_suggester_ is set to nullptr prematurely by a different event.
    if (current_suggester_) {
      RecordAssistiveSuccess(current_suggester_->GetProposeActionType());
      current_suggester_ = nullptr;
    }
  }
}

void AssistiveSuggester::DismissSuggestion() {
  if (current_suggester_) {
    current_suggester_->DismissSuggestion();
  }
  current_suggester_ = nullptr;
}

bool AssistiveSuggester::IsSuggestionShown() {
  return current_suggester_ != nullptr;
}

std::vector<ime::AssistiveSuggestion> AssistiveSuggester::GetSuggestions() {
  if (IsSuggestionShown()) {
    return current_suggester_->GetSuggestions();
  }
  return {};
}

void AssistiveSuggester::OnActivate(const std::string& engine_id) {
  active_engine_id_ = engine_id;
  longpress_diacritics_suggester_.SetEngineId(engine_id);

  if (base::FeatureList::IsEnabled(features::kAssistMultiWord)) {
    RecordAssistiveUserPrefForMultiWord(
        IsPredictiveWritingPrefEnabled(*profile_->GetPrefs(), engine_id));
  }
  if (IsUsEnglishEngine(active_engine_id_)) {
    RecordAssistiveUserPrefForDiacriticsOnLongpress(
        IsDiacriticsOnLongpressPrefEnabled(profile_->GetPrefs(), engine_id));
  }
}

}  // namespace ash::input_method
