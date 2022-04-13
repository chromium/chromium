// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/input_method/assistive_suggester.h"
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/services/ime/public/cpp/suggestions.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/input_method/assistive_suggester_prefs.h"
#include "chrome/browser/ash/input_method/assistive_suggester_switch.h"
#include "chrome/browser/ash/input_method/suggestion_handler_interface.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/exo/wm_helper.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/ime_input_context_handler_interface.h"
#include "ui/base/ime/ash/input_method_ukm.h"
#include "url/gurl.h"

namespace ash {
namespace input_method {

namespace {

using ime::TextSuggestion;
using ime::TextSuggestionMode;
using ime::TextSuggestionType;

const char kMaxTextBeforeCursorLength = 50;

void RecordAssistiveMatch(AssistiveType type) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Match", type);

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context)
    return;

  auto sourceId = input_context->GetClientSourceForMetrics();
  if (sourceId != ukm::kInvalidSourceId) {
    ui::RecordUkmAssistiveMatch(sourceId, static_cast<int>(type));
  }
}

void RecordAssistiveDisabled(AssistiveType type) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Disabled", type);
}

void RecordAssistiveDisabledReasonForPersonalInfo(DisabledReason reason) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Disabled.PersonalInfo",
                                reason);
}

void RecordAssistiveDisabledReasonForEmoji(DisabledReason reason) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Disabled.Emoji", reason);
}

void RecordAssistiveDisabledReasonForMultiWord(DisabledReason reason) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Disabled.MultiWord",
                                reason);
}

void RecordAssistiveUserPrefForPersonalInfo(bool value) {
  base::UmaHistogramBoolean("InputMethod.Assistive.UserPref.PersonalInfo",
                            value);
}

void RecordAssistiveUserPrefForEmoji(bool value) {
  base::UmaHistogramBoolean("InputMethod.Assistive.UserPref.Emoji", value);
}

void RecordAssistiveUserPrefForMultiWord(bool value) {
  base::UmaHistogramBoolean("InputMethod.Assistive.UserPref.MultiWord", value);
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

bool IsTopResultMultiWord(const std::vector<TextSuggestion>& suggestions) {
  if (suggestions.empty())
    return false;
  // There should only ever be one multi word suggestion given if any.
  return suggestions[0].type == TextSuggestionType::kMultiWord;
}

void RecordSuggestionsMatch(const std::vector<TextSuggestion>& suggestions) {
  if (suggestions.empty())
    return;

  auto top_result = suggestions[0];
  if (top_result.type != TextSuggestionType::kMultiWord)
    return;

  switch (top_result.mode) {
    case TextSuggestionMode::kCompletion:
      RecordAssistiveMatch(AssistiveType::kMultiWordCompletion);
      return;
    case TextSuggestionMode::kPrediction:
      RecordAssistiveMatch(AssistiveType::kMultiWordPrediction);
      return;
  }
}

bool IsUsEnglishEngine(const std::string& engine_id) {
  return engine_id == "xkb:us::eng";
}

bool IsLacrosEnabled() {
  return base::FeatureList::IsEnabled(chromeos::features::kLacrosSupport);
}

void RecordTextInputStateMetric(AssistiveTextInputState state) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.MultiWord.InputState",
                                state);
}

void RecordMultiWordTextInputState(
    PrefService* pref_service,
    const std::string& engine_id,
    const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions) {
  if (IsLacrosEnabled()) {
    RecordTextInputStateMetric(AssistiveTextInputState::kUnsupportedClient);
    return;
  }

  if (!enabled_suggestions.multi_word_suggestions) {
    RecordTextInputStateMetric(
        AssistiveTextInputState::kFeatureBlockedByDenylist);
    return;
  }

  if (!IsUsEnglishEngine(engine_id)) {
    RecordTextInputStateMetric(AssistiveTextInputState::kUnsupportedLanguage);
    return;
  }

  if (!IsPredictiveWritingPrefEnabled(pref_service, engine_id)) {
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
    std::unique_ptr<AssistiveSuggesterSwitch> suggester_switch,
    autofill::PersonalDataManager* personal_data_manager_for_testing)
    : profile_(profile),
      personal_info_suggester_(suggestion_handler,
                               profile,
                               personal_data_manager_for_testing),
      emoji_suggester_(suggestion_handler, profile),
      multi_word_suggester_(suggestion_handler, profile),
      suggester_switch_(std::move(suggester_switch)) {
  RecordAssistiveUserPrefForPersonalInfo(
      profile_->GetPrefs()->GetBoolean(prefs::kAssistPersonalInfoEnabled));
  RecordAssistiveUserPrefForEmoji(
      profile_->GetPrefs()->GetBoolean(prefs::kEmojiSuggestionEnabled));
}

AssistiveSuggester::~AssistiveSuggester() = default;

bool AssistiveSuggester::IsAssistiveFeatureEnabled() {
  return IsAssistPersonalInfoEnabled() || IsEmojiSuggestAdditionEnabled() ||
         IsMultiWordSuggestEnabled() || IsEnhancedEmojiSuggestEnabled();
}

void AssistiveSuggester::FetchEnabledSuggestionsFromBrowserContextThen(
    AssistiveSuggesterSwitch::FetchEnabledSuggestionsCallback callback) {
  suggester_switch_->FetchEnabledSuggestionsThen(std::move(callback));
}

bool AssistiveSuggester::IsAssistPersonalInfoEnabled() {
  return base::FeatureList::IsEnabled(features::kAssistPersonalInfo) &&
         profile_->GetPrefs()->GetBoolean(prefs::kAssistPersonalInfoEnabled);
}

bool AssistiveSuggester::IsEmojiSuggestAdditionEnabled() {
  return profile_->GetPrefs()->GetBoolean(
             prefs::kEmojiSuggestionEnterpriseAllowed) &&
         profile_->GetPrefs()->GetBoolean(prefs::kEmojiSuggestionEnabled);
}

bool AssistiveSuggester::IsEnhancedEmojiSuggestEnabled() {
  return IsEmojiSuggestAdditionEnabled() &&
         base::FeatureList::IsEnabled(features::kAssistEmojiEnhanced);
}

bool AssistiveSuggester::IsMultiWordSuggestEnabled() {
  return features::IsAssistiveMultiWordEnabled() &&
         IsPredictiveWritingPrefEnabled(profile_->GetPrefs(),
                                        active_engine_id_);
}

bool AssistiveSuggester::IsExpandedMultiWordSuggestEnabled() {
  return IsMultiWordSuggestEnabled() &&
         base::FeatureList::IsEnabled(features::kAssistMultiWordExpanded);
}

DisabledReason AssistiveSuggester::GetDisabledReasonForPersonalInfo(
    const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions) {
  if (!base::FeatureList::IsEnabled(features::kAssistPersonalInfo)) {
    return DisabledReason::kFeatureFlagOff;
  }
  if (!profile_->GetPrefs()->GetBoolean(prefs::kAssistPersonalInfoEnabled)) {
    return DisabledReason::kUserSettingsOff;
  }
  if (!enabled_suggestions.personal_info_suggestions) {
    return DisabledReason::kUrlOrAppNotAllowed;
  }
  return DisabledReason::kNone;
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
  if (!features::IsAssistiveMultiWordEnabled()) {
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
    case AssistiveType::kPersonalEmail:
    case AssistiveType::kPersonalAddress:
    case AssistiveType::kPersonalPhoneNumber:
    case AssistiveType::kPersonalName:
    case AssistiveType::kPersonalNumber:
    case AssistiveType::kPersonalFirstName:
    case AssistiveType::kPersonalLastName:
      return AssistiveFeature::kPersonalInfoSuggestion;
    case AssistiveType::kEmoji:
      return AssistiveFeature::kEmojiSuggestion;
    case AssistiveType::kMultiWordCompletion:
    case AssistiveType::kMultiWordPrediction:
      return AssistiveFeature::kMultiWordSuggestion;
    default:
      // We should only handle Personal Info, Emoji, and Multiword related
      // assistive types.
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
    case AssistiveFeature::kPersonalInfoSuggestion:
      // TODO: Use value from settings when crbug/1068457 is done.
      return IsAssistPersonalInfoEnabled();
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
    case AssistiveFeature::kPersonalInfoSuggestion:
      return enabled_suggestions.personal_info_suggestions;
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

void AssistiveSuggester::OnFocus(int context_id) {
  context_id_ = context_id;
  personal_info_suggester_.OnFocus(context_id_);
  emoji_suggester_.OnFocus(context_id_);
  multi_word_suggester_.OnFocus(context_id_);
  suggester_switch_->FetchEnabledSuggestionsThen(
      base::BindOnce(&AssistiveSuggester::RecordTextInputStateMetrics,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AssistiveSuggester::OnBlur() {
  context_id_ = -1;
  personal_info_suggester_.OnBlur();
  emoji_suggester_.OnBlur();
  multi_word_suggester_.OnBlur();
}

bool AssistiveSuggester::OnKeyEvent(const ui::KeyEvent& event) {
  if (context_id_ == -1)
    return false;

  // We only track keydown event because the suggesting action is triggered by
  // surrounding text change, which is triggered by a keydown event. As a
  // result, the next key event after suggesting would be a keyup event of the
  // same key, and that event is meaningless to us.
  if (IsSuggestionShown() && event.type() == ui::ET_KEY_PRESSED &&
      !event.IsControlDown() && !event.IsAltDown() && !event.IsShiftDown()) {
    SuggestionStatus status = current_suggester_->HandleKeyEvent(event);
    switch (status) {
      case SuggestionStatus::kAccept:
        RecordAssistiveSuccess(current_suggester_->GetProposeActionType());
        current_suggester_ = nullptr;
        return true;
      case SuggestionStatus::kDismiss:
        current_suggester_ = nullptr;
        return true;
      case SuggestionStatus::kBrowsing:
        return true;
      default:
        break;
    }
  }

  return false;
}

void AssistiveSuggester::OnExternalSuggestionsUpdated(
    const std::vector<TextSuggestion>& suggestions) {
  if (!IsMultiWordSuggestEnabled())
    return;

  suggester_switch_->FetchEnabledSuggestionsThen(
      base::BindOnce(&AssistiveSuggester::ProcessExternalSuggestions,
                     weak_ptr_factory_.GetWeakPtr(), suggestions));
}

void AssistiveSuggester::ProcessExternalSuggestions(
    const std::vector<TextSuggestion>& suggestions,
    const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions) {
  RecordSuggestionsMatch(suggestions);

  if (!enabled_suggestions.multi_word_suggestions &&
      !IsExpandedMultiWordSuggestEnabled()) {
    if (IsTopResultMultiWord(suggestions))
      RecordAssistiveDisabledReasonForMultiWord(
          GetDisabledReasonForMultiWord(enabled_suggestions));
    return;
  }

  if (current_suggester_) {
    current_suggester_->OnExternalSuggestionsUpdated(suggestions);
    return;
  }

  if (IsTopResultMultiWord(suggestions)) {
    current_suggester_ = &multi_word_suggester_;
    current_suggester_->OnExternalSuggestionsUpdated(suggestions);
    RecordAssistiveCoverage(current_suggester_->GetProposeActionType());
  }
}

void AssistiveSuggester::RecordTextInputStateMetrics(
    const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions) {
  if (features::IsAssistiveMultiWordEnabled()) {
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
    int cursor_pos,
    int anchor_pos,
    const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions) {
  int len = static_cast<int>(text.length());
  if (cursor_pos > 0 && cursor_pos <= len && cursor_pos == anchor_pos &&
      (cursor_pos == len || base::IsAsciiWhitespace(text[cursor_pos]))) {
    int start_pos = std::max(0, cursor_pos - kMaxTextBeforeCursorLength);
    std::u16string text_before_cursor =
        text.substr(start_pos, cursor_pos - start_pos);
    // Personal info suggestion match
    AssistiveType type = ProposePersonalInfoAssistiveAction(text_before_cursor);
    if (type != AssistiveType::kGenericAction) {
      RecordAssistiveMatchMetricsForAssistiveType(type, enabled_suggestions);
      RecordAssistiveDisabledReasonForPersonalInfo(
          GetDisabledReasonForPersonalInfo(enabled_suggestions));
      // Emoji suggestion match
    } else if (emoji_suggester_.ShouldShowSuggestion(text_before_cursor)) {
      RecordAssistiveMatchMetricsForAssistiveType(AssistiveType::kEmoji,
                                                  enabled_suggestions);
      base::RecordAction(
          base::UserMetricsAction("InputMethod.Assistive.EmojiSuggested"));
      RecordAssistiveDisabledReasonForEmoji(
          GetDisabledReasonForEmoji(enabled_suggestions));
    }
  }
}

bool AssistiveSuggester::WithinGrammarFragment(int cursor_pos, int anchor_pos) {
  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context)
    return false;

  gfx::Range cursor_range = cursor_pos <= anchor_pos
                                ? gfx::Range(cursor_pos, anchor_pos)
                                : gfx::Range(anchor_pos, cursor_pos);
  absl::optional<ui::GrammarFragment> grammar_fragment_opt =
      input_context->GetGrammarFragment(cursor_range);

  return grammar_fragment_opt != absl::nullopt;
}

void AssistiveSuggester::OnSurroundingTextChanged(const std::u16string& text,
                                                  int cursor_pos,
                                                  int anchor_pos) {
  suggester_switch_->FetchEnabledSuggestionsThen(base::BindOnce(
      &AssistiveSuggester::ProcessOnSurroundingTextChanged,
      weak_ptr_factory_.GetWeakPtr(), text, cursor_pos, anchor_pos));
}

void AssistiveSuggester::ProcessOnSurroundingTextChanged(
    const std::u16string& text,
    int cursor_pos,
    int anchor_pos,
    const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions) {
  RecordAssistiveMatchMetrics(text, cursor_pos, anchor_pos,
                              enabled_suggestions);
  if (!IsAssistiveFeatureEnabled() || context_id_ == -1)
    return;

  if (IsMultiWordSuggestEnabled()) {
    // Only multi word cares about tracking the current state of the text field
    multi_word_suggester_.OnSurroundingTextChanged(text, cursor_pos,
                                                   anchor_pos);
  }

  if (WithinGrammarFragment(cursor_pos, anchor_pos) ||
      !TrySuggestWithSurroundingText(text, cursor_pos, anchor_pos,
                                     enabled_suggestions)) {
    DismissSuggestion();
  }
}

bool AssistiveSuggester::TrySuggestWithSurroundingText(
    const std::u16string& text,
    int cursor_pos,
    int anchor_pos,
    const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions) {
  int len = static_cast<int>(text.length());
  if (cursor_pos > 0 && cursor_pos <= len && cursor_pos == anchor_pos &&
      (cursor_pos == len || base::IsAsciiWhitespace(text[cursor_pos])) &&
      (base::IsAsciiWhitespace(text[cursor_pos - 1]) || IsSuggestionShown())) {
    if (IsSuggestionShown()) {
      return current_suggester_->TrySuggestWithSurroundingText(text,
                                                               cursor_pos);
    }
    if (IsAssistPersonalInfoEnabled() &&
        enabled_suggestions.personal_info_suggestions &&
        personal_info_suggester_.TrySuggestWithSurroundingText(text,
                                                               cursor_pos)) {
      current_suggester_ = &personal_info_suggester_;
      if (personal_info_suggester_.IsFirstShown()) {
        RecordAssistiveCoverage(current_suggester_->GetProposeActionType());
      }
      return true;
    } else if (IsEmojiSuggestAdditionEnabled() &&
               !IsEnhancedEmojiSuggestEnabled() &&
               enabled_suggestions.emoji_suggestions &&
               emoji_suggester_.TrySuggestWithSurroundingText(text,
                                                              cursor_pos)) {
      current_suggester_ = &emoji_suggester_;
      RecordAssistiveCoverage(current_suggester_->GetProposeActionType());
      return true;
    }
  }
  return false;
}

void AssistiveSuggester::AcceptSuggestion(size_t index) {
  if (current_suggester_ && current_suggester_->AcceptSuggestion(index)) {
    RecordAssistiveSuccess(current_suggester_->GetProposeActionType());
    current_suggester_ = nullptr;
  }
}

void AssistiveSuggester::DismissSuggestion() {
  if (current_suggester_)
    current_suggester_->DismissSuggestion();
  current_suggester_ = nullptr;
}

bool AssistiveSuggester::IsSuggestionShown() {
  return current_suggester_ != nullptr;
}

std::vector<ime::TextSuggestion> AssistiveSuggester::GetSuggestions() {
  if (IsSuggestionShown())
    return current_suggester_->GetSuggestions();
  return {};
}

void AssistiveSuggester::OnActivate(const std::string& engine_id) {
  if (features::IsAssistiveMultiWordEnabled()) {
    active_engine_id_ = engine_id;
    RecordAssistiveUserPrefForMultiWord(
        IsPredictiveWritingPrefEnabled(profile_->GetPrefs(), engine_id));
  }
}

}  // namespace input_method
}  // namespace ash
