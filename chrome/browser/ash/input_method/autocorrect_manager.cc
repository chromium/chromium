// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/autocorrect_manager.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/levenshtein_distance.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/input_method/assistive_input_denylist.h"
#include "chrome/browser/ash/input_method/assistive_prefs.h"
#include "chrome/browser/ash/input_method/assistive_window_properties.h"
#include "chrome/browser/ash/input_method/autocorrect_enums.h"
#include "chrome/browser/ash/input_method/autocorrect_prefs.h"
#include "chrome/browser/ash/input_method/field_trial.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/services/federated/public/mojom/tables.mojom.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "components/strings/grit/components_strings.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/text_input_target.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {

namespace {

constexpr int kMaxEditDistance = 30;
constexpr int kDistanceUntilUnderlineHides = 3;
constexpr int kMaxValidationTries = 4;
constexpr base::TimeDelta kVeryFastInteractionPeriod = base::Milliseconds(200);
constexpr base::TimeDelta kFastInteractionPeriod = base::Milliseconds(500);
constexpr int kUndoWindowShowSettingMaxCount = 50;
constexpr char kUndoWindowShowSettingCount[] = "undo_window.show_setting_count";

bool IsVkAutocorrect() {
  return ChromeKeyboardControllerClient::HasInstance() &&
         ChromeKeyboardControllerClient::Get()->is_keyboard_enabled();
}

bool IsUsEnglishId(const std::string& engine_id) {
  return engine_id == "xkb:us::eng";
}

AutocorrectCompatibilitySummary ConvertActionToCompatibilitySummary(
    AutocorrectActions action) {
  switch (action) {
    case AutocorrectActions::kWindowShown:
      return AutocorrectCompatibilitySummary::kWindowShown;
    case AutocorrectActions::kUnderlined:
      return AutocorrectCompatibilitySummary::kUnderlined;
    case AutocorrectActions::kReverted:
      return AutocorrectCompatibilitySummary::kReverted;
    case AutocorrectActions::kUserAcceptedAutocorrect:
      return AutocorrectCompatibilitySummary::kUserAcceptedAutocorrect;
    case AutocorrectActions::kUserActionClearedUnderline:
      return AutocorrectCompatibilitySummary::kUserActionClearedUnderline;
    case AutocorrectActions::kUserExitedTextFieldWithUnderline:
      return AutocorrectCompatibilitySummary::kUserExitedTextFieldWithUnderline;
    case AutocorrectActions::kInvalidRange:
      return AutocorrectCompatibilitySummary::kInvalidRange;
    default:
      LOG(ERROR) << "Invalid AutocorrectActions: action=" << (int)action;
      return AutocorrectCompatibilitySummary::kInvalidRange;
  }
}

void RecordAppCompatibilityUkm(
    ukm::SourceId source_id,
    bool virtual_keyboard,
    AutocorrectCompatibilitySummary compatibility_summary) {
  if (virtual_keyboard) {
    ukm::builders::InputMethod_Assistive_AutocorrectV2(source_id)
        .SetCompatibilitySummary_VK(static_cast<int>(compatibility_summary))
        .Record(ukm::UkmRecorder::Get());
  } else {
    ukm::builders::InputMethod_Assistive_AutocorrectV2(source_id)
        .SetCompatibilitySummary_PK(static_cast<int>(compatibility_summary))
        .Record(ukm::UkmRecorder::Get());
  }
}

void LogAutocorrectAppCompatibilityUkm(AutocorrectActions action,
                                       base::TimeDelta time_delta,
                                       bool virtual_keyboard_visible) {
  TextInputTarget* input_context = IMEBridge::Get()->GetInputContextHandler();
  if (!input_context) {
    return;
  }

  ukm::SourceId sourceId = input_context->GetClientSourceForMetrics();
  if (sourceId == ukm::kInvalidSourceId) {
    return;
  }

  // Record base interactions.
  RecordAppCompatibilityUkm(sourceId, virtual_keyboard_visible,
                            ConvertActionToCompatibilitySummary(action));

  if (time_delta > kFastInteractionPeriod) {
    return;
  }

  bool is_very_fast = time_delta <= kVeryFastInteractionPeriod;

  AutocorrectCompatibilitySummary latency_compatibility;

  // Convert latency of important interaction to CompatibilitySummary.
  switch (action) {
    case AutocorrectActions::kUserAcceptedAutocorrect:
      latency_compatibility =
          is_very_fast
              ? AutocorrectCompatibilitySummary::kVeryFastAcceptedAutocorrect
              : AutocorrectCompatibilitySummary::kFastAcceptedAutocorrect;
      break;
    case AutocorrectActions::kReverted:
    case AutocorrectActions::kUserActionClearedUnderline:
    case AutocorrectActions::kInvalidRange:
      latency_compatibility =
          is_very_fast
              ? AutocorrectCompatibilitySummary::kVeryFastRejectedAutocorrect
              : AutocorrectCompatibilitySummary::kFastRejectedAutocorrect;
      break;
    case AutocorrectActions::kUserExitedTextFieldWithUnderline:
      latency_compatibility =
          is_very_fast ? AutocorrectCompatibilitySummary::kVeryFastExitField
                       : AutocorrectCompatibilitySummary::kFastExitField;
      break;
    default:
      return;
  }

  RecordAppCompatibilityUkm(sourceId, virtual_keyboard_visible,
                            latency_compatibility);
}

void LogAssistiveAutocorrectDelay(base::TimeDelta delay) {
  base::UmaHistogramMediumTimes("InputMethod.Assistive.Autocorrect.Delay",
                                delay);
}

void LogAssistiveAutocorrectActionLatency(
    const AutocorrectActions action,
    const base::TimeDelta time_delta,
    const bool virtual_keyboard_visible,
    const bool autocorrect_is_pk_enabled_by_default) {
  switch (action) {
    case AutocorrectActions::kUnderlined:
    case AutocorrectActions::kWindowShown:
      // Skip non-terminal actions.
      return;
    case AutocorrectActions::kUserAcceptedAutocorrect:
      if (autocorrect_is_pk_enabled_by_default) {
        base::UmaHistogramMediumTimes(
            "InputMethod.Assistive.AutocorrectV2.Latency.Accept."
            "EnabledByDefault",
            time_delta);
      }
      base::UmaHistogramMediumTimes(
          "InputMethod.Assistive.AutocorrectV2.Latency.Accept", time_delta);
      break;
    case AutocorrectActions::kReverted:
    case AutocorrectActions::kUserActionClearedUnderline:
    case AutocorrectActions::kInvalidRange:
      if (autocorrect_is_pk_enabled_by_default) {
        base::UmaHistogramMediumTimes(
            "InputMethod.Assistive.AutocorrectV2.Latency.Reject."
            "EnabledByDefault",
            time_delta);
      }
      base::UmaHistogramMediumTimes(
          "InputMethod.Assistive.AutocorrectV2.Latency.Reject", time_delta);
      break;
    case AutocorrectActions::kUserExitedTextFieldWithUnderline:
      if (autocorrect_is_pk_enabled_by_default) {
        base::UmaHistogramMediumTimes(
            "InputMethod.Assistive.AutocorrectV2.Latency.ExitField."
            "EnabledByDefault",
            time_delta);
      }
      base::UmaHistogramMediumTimes(
          "InputMethod.Assistive.AutocorrectV2.Latency.ExitField", time_delta);
      break;
    default:
      LOG(ERROR) << "Invalid AutocorrectActions: action=" << (int)action;
      return;
  }

  // Record the duration of the pending autocorrect for VK and PK.
  if (virtual_keyboard_visible) {
    base::UmaHistogramMediumTimes(
        "InputMethod.Assistive.AutocorrectV2.Latency.VkPending", time_delta);
  } else {
    base::UmaHistogramMediumTimes(
        "InputMethod.Assistive.AutocorrectV2.Latency.PkPending", time_delta);
  }
}

AutocorrectRejectionBreakdown LogControlInteractions(
    const ui::DomCode& last_key_press,
    const std::string& histogram_name) {
  switch (last_key_press) {
    case ui::DomCode::BACKSPACE:
    case ui::DomCode::DEL:
      return AutocorrectRejectionBreakdown::kRejectedCtrlBackspace;
    case ui::DomCode::US_Z:
      return AutocorrectRejectionBreakdown::kUndoCtrlZ;
    default:
      return AutocorrectRejectionBreakdown::kRejectionOther;
  }
}

AutocorrectRejectionBreakdown LogSelectionEditInteractions(
    const gfx::Range& last_autocorrect_range,
    const gfx::Range& last_selection_range,
    const std::string& histogram_name) {
  if (last_autocorrect_range.IsBoundedBy(last_selection_range)) {
    if (last_selection_range.IsBoundedBy(last_autocorrect_range)) {
      return AutocorrectRejectionBreakdown::kRejectedTypingFull;
    }
    return AutocorrectRejectionBreakdown::kRejectedTypingFullWithExternal;
  }
  if (last_selection_range.IsBoundedBy(last_autocorrect_range)) {
    return AutocorrectRejectionBreakdown::kRejectedTypingPartial;
  }
  if (last_selection_range.Intersects(last_autocorrect_range)) {
    return AutocorrectRejectionBreakdown::kRejectedTypingPartialWithExternal;
  }
  return AutocorrectRejectionBreakdown::kRejectedSelectedInvalidRange;
}

void MeasureAndLogAssistiveAutocorrectEditDistance(
    const std::u16string& original_text,
    const std::u16string& suggested_text,
    const bool suggestion_accepted,
    const bool virtual_keyboard_visible) {
  const int text_length =
      std::min(static_cast<int>(original_text.length()), kMaxEditDistance);
  const int distance = base::LevenshteinDistance(original_text, suggested_text,
                                                 kMaxEditDistance - 1);
  if (text_length <= 0 || distance <= 0) {
    return;
  }

  const std::string histogram_base_name =
      "InputMethod.Assistive.AutocorrectV2.Distance.";
  std::string keyboard_type_extension =
      virtual_keyboard_visible ? ".Vk" : ".Pk";
  keyboard_type_extension += suggestion_accepted ? "Accepted" : "Rejected";

  // This is a 2d array of size (kMaxEditDistance x kMaxEditDistance) that
  // has been flattened
  const int flattenedLengthVsDistance =
      (text_length - 1) * kMaxEditDistance + distance - 1;
  base::UmaHistogramSparse(
      base::StrCat({histogram_base_name, "OriginalLengthVsLevenshteinDistance",
                    keyboard_type_extension}),
      flattenedLengthVsDistance);
  base::UmaHistogramExactLinear(
      base::StrCat(
          {histogram_base_name, "SuggestedLength", keyboard_type_extension}),
      suggested_text.length(), kMaxEditDistance);
}

void RecordAssistiveCoverage(AssistiveType type) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Coverage", type);
}

void RecordAssistiveSuccess(AssistiveType type) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Success", type);
}

void RecordPhysicalKeyboardAutocorrectPref(const std::string& engine_id,
                                           const AutocorrectPreference& pref) {
  if (IsUsEnglishId(engine_id)) {
    base::UmaHistogramEnumeration(
        "InputMethod.Assistive.AutocorrectV2.PkUserPreference.English", pref);
  }
  base::UmaHistogramEnumeration(
      "InputMethod.Assistive.AutocorrectV2.PkUserPreference.All", pref);
}

void RecordSuggestionProviderMetric(
    const ime::AutocorrectSuggestionProvider& provider) {
  base::UmaHistogramEnumeration(
      "InputMethod.Assistive.AutocorrectV2.SuggestionProvider.Pk", provider);
}

bool CouldTriggerAutocorrectWithSurroundingText(
    const std::u16string& text,
    const gfx::Range selection_range) {
  // TODO(b/161490813): Do not count cases that autocorrect is disabled.
  //    Currently, there are different logics in different places that disable
  //    autocorrect based on settings, domain and text field attributes.
  //    Ideally, all the cases that autocorrect is disabled on a text field
  //    must not be counted here.
  const uint32_t cursor_pos = selection_range.end();
  return selection_range.is_empty() && cursor_pos == text.size() &&
         text.size() >= 2 && base::IsAsciiWhitespace(text.back()) &&
         !base::IsAsciiWhitespace(text[text.size() - 2]);
}

bool IsAutocorrectSuggestionInSurroundingText(
    const std::u16string& surrounding_text,
    const gfx::Range& autocorrect_range,
    const std::u16string& suggested_text) {
  if (autocorrect_range.is_empty() ||
      suggested_text.length() != autocorrect_range.length() ||
      autocorrect_range.end() > surrounding_text.length()) {
    return false;
  }

  return surrounding_text.substr(autocorrect_range.start(),
                                 autocorrect_range.length()) == suggested_text;
}

bool UserInAutocorrectByDefaultBucket(const PrefService& prefs,
                                      const std::string& engine_id) {
  return base::FeatureList::IsEnabled(features::kAutocorrectByDefault) &&
         IsUsEnglishId(engine_id) && !IsVkAutocorrect() &&
         GetPhysicalKeyboardAutocorrectPref(prefs, engine_id) ==
             AutocorrectPreference::kEnabledByDefault;
}

}  // namespace

AutocorrectManager::AutocorrectManager(
    SuggestionHandlerInterface* suggestion_handler,
    Profile* profile)
    : denylist_(DenylistAdditions{
          .autocorrect_denylist_json =
              GetFieldTrialParam(features::kAutocorrectByDefault,
                                 ParamName::kDenylist),
          .multi_word_denylist_json =
              GetFieldTrialParam(features::kAssistMultiWord,
                                 ParamName::kDenylist)}),
      suggestion_handler_(suggestion_handler),
      profile_(profile) {
  undo_button_.id = ui::ime::ButtonId::kUndo;
  undo_button_.window_type = ash::ime::AssistiveWindowType::kUndoWindow;
  learn_more_button_.id = ui::ime::ButtonId::kLearnMore;
  learn_more_button_.announce_string =
      l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  learn_more_button_.window_type = ash::ime::AssistiveWindowType::kLearnMore;
}

AutocorrectManager::~AutocorrectManager() = default;

void AutocorrectManager::HandleAutocorrect(const gfx::Range autocorrect_range,
                                           const std::u16string& original_text,
                                           const std::u16string& current_text) {
  ++num_handled_autocorrect_in_text_field_;

  if (DisabledByRule()) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kHandleSuggestionInDenylistedApp);
  }

  // TODO(crbug/1111135): call setAutocorrectTime() (for metrics)
  // TODO(crbug/1111135): record metric (coverage)
  TextInputTarget* input_context = IMEBridge::Get()->GetInputContextHandler();
  if (!input_context) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kHandleNoInputContext);
    AcceptOrClearPendingAutocorrect();
    return;
  }

  if (pending_autocorrect_.has_value()) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kHandleUnclearedRange);
    AcceptOrClearPendingAutocorrect();
  }

  if (autocorrect_range.is_empty() ||
      autocorrect_range.length() != current_text.length() ||
      original_text.empty()) {
    if (autocorrect_range.is_empty()) {
      LogAssistiveAutocorrectInternalState(
          AutocorrectInternalStates::kHandleEmptyRange);
    } else {
      LogAssistiveAutocorrectInternalState(
          AutocorrectInternalStates::kHandleInvalidArgs);
    }
    input_context->SetAutocorrectRange(gfx::Range(), base::DoNothing());
    return;
  }

  LogAssistiveAutocorrectInternalState(
      AutocorrectInternalStates::kHandleSetRange);

  input_context->SetAutocorrectRange(
      autocorrect_range,
      base::BindOnce(&AutocorrectManager::ProcessSetAutocorrectRangeDone,
                     weak_ptr_factory_.GetWeakPtr(), autocorrect_range,
                     original_text, current_text));  // show underline
}

void AutocorrectManager::ProcessSetAutocorrectRangeDone(
    const gfx::Range& autocorrect_range,
    const std::u16string& original_text,
    const std::u16string& current_text,
    bool set_range_success) {
  if (!set_range_success) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kErrorSetRange);
    return;
  }

  pending_autocorrect_ = AutocorrectManager::PendingAutocorrectState(
      /*original_text=*/original_text, /*suggested_text=*/current_text,
      /*start_time=*/base::TimeTicks::Now(),
      /*virtual_keyboard_visible=*/IsVkAutocorrect(),
      /*learn_more_button_visible=*/
      GetPrefValue(kUndoWindowShowSettingCount, *profile_) <
          kUndoWindowShowSettingMaxCount);

  LogAssistiveAutocorrectInternalState(
      AutocorrectInternalStates::kUnderlineShown);

  LogAssistiveAutocorrectAction(AutocorrectActions::kUnderlined);
  RecordAssistiveCoverage(AssistiveType::kAutocorrectUnderlined);

  if (ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled() &&
      base::FeatureList::IsEnabled(features::kAutocorrectFederatedPhh)) {
    // Report `original_text` to the Federated Service.
    federated_manager_.ReportSingleString(
        /*table_id*/ chromeos::federated::mojom::FederatedExampleTableId::
            INPUT_AUTOCORRECT,
        /*example_feature_name*/ "original_text",
        /*example_str*/ base::UTF16ToUTF8(original_text));
  }
}

void AutocorrectManager::RecordPendingMetricsAwaitingKeyPress() {
  if (pending_user_pref_metric_ && IsVkAutocorrect()) {
    // We only want to record a pending user pref metric if the user is
    // currently using the physical keyboard.
    pending_user_pref_metric_ = std::nullopt;
  }

  if (pending_user_pref_metric_) {
    const std::string& engine_id = pending_user_pref_metric_->engine_id;
    RecordPhysicalKeyboardAutocorrectPref(
        engine_id,
        GetPhysicalKeyboardAutocorrectPref(*(profile_->GetPrefs()), engine_id));
    pending_user_pref_metric_ = std::nullopt;
  }

  if (pending_suggestion_provider_metric_ && IsVkAutocorrect()) {
    // TODO(b/270090192): Unfortunately the virtual keyboard does not support
    // the callback used to inform Chromium of the AutocorrectSuggestionProvider
    // used in the IME service. Once it does then we can record this same metric
    // for the virtual keyboard.
    pending_suggestion_provider_metric_ = std::nullopt;
  }

  if (pending_suggestion_provider_metric_) {
    RecordSuggestionProviderMetric(
        /*provider=*/pending_suggestion_provider_metric_->provider);
    pending_suggestion_provider_metric_ = std::nullopt;
  }
}

bool AutocorrectManager::AutoCorrectPrefIsPkEnabledByDefault() {
  if (!active_engine_id_.has_value() || IsVkAutocorrect()) {
    return false;
  }
  return GetPhysicalKeyboardAutocorrectPref(*(profile_->GetPrefs()),
                                            *active_engine_id_) ==
         AutocorrectPreference::kEnabledByDefault;
}

void AutocorrectManager::LogAssistiveAutocorrectAction(
    AutocorrectActions action) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Autocorrect.Actions",
                                action);

  if (pending_autocorrect_.has_value()) {
    base::TimeDelta latency =
        base::TimeTicks::Now() - pending_autocorrect_->start_time;
    LogAssistiveAutocorrectActionLatency(
        action, latency, pending_autocorrect_->virtual_keyboard_visible,
        AutoCorrectPrefIsPkEnabledByDefault());

    LogAutocorrectAppCompatibilityUkm(
        action, latency, pending_autocorrect_->virtual_keyboard_visible);
    LogRejectionInteractions(action);
  }

  if (pending_autocorrect_.has_value() &&
      pending_autocorrect_->virtual_keyboard_visible) {
    base::UmaHistogramEnumeration(
        "InputMethod.Assistive.Autocorrect.Actions.VK", action);
    base::UmaHistogramEnumeration(
        "InputMethod.Assistive.AutocorrectV2.Actions.VK", action);
  } else {
    if (AutoCorrectPrefIsPkEnabledByDefault()) {
      base::UmaHistogramEnumeration(
          "InputMethod.Assistive.AutocorrectV2.Actions.PK.EnabledByDefault",
          action);
    }
    base::UmaHistogramEnumeration(
        "InputMethod.Assistive.AutocorrectV2.Actions.PK", action);
  }
}

void AutocorrectManager::LogRejectionInteractions(
    const AutocorrectActions action) {
  if ((action != AutocorrectActions::kUserActionClearedUnderline &&
       action != AutocorrectActions::kReverted) ||
      !pending_autocorrect_.has_value()) {
    return;
  }
  const ui::DomCode& last_key_press =
      pending_autocorrect_->last_key_event.has_value()
          ? pending_autocorrect_->last_key_event->code()
          : ui::DomCode::NONE;
  const std::string histogram_name =
      pending_autocorrect_->virtual_keyboard_visible
          ? "InputMethod.Assistive.AutocorrectV2.Rejection.VK"
          : "InputMethod.Assistive.AutocorrectV2.Rejection.PK";
  base::UmaHistogramEnumeration(
      histogram_name, AutocorrectRejectionBreakdown::kSuggestionRejected);

  if (action == AutocorrectActions::kReverted) {
    if (last_key_press == ui::DomCode::ENTER) {
      base::UmaHistogramEnumeration(
          histogram_name, AutocorrectRejectionBreakdown::kUndoWithKeyboard);
      return;
    }
    base::UmaHistogramEnumeration(
        histogram_name, AutocorrectRejectionBreakdown::kUndoWithoutKeyboard);
    return;
  }
  if (pending_autocorrect_->last_key_event.has_value() &&
      pending_autocorrect_->last_key_event->IsControlDown()) {
    base::UmaHistogramEnumeration(
        histogram_name, LogControlInteractions(last_key_press, histogram_name));
    return;
  }
  if (!pending_autocorrect_->last_selection_range.is_empty()) {
    base::UmaHistogramEnumeration(
        histogram_name,
        LogSelectionEditInteractions(
            pending_autocorrect_->last_autocorrect_range,
            pending_autocorrect_->last_selection_range, histogram_name));
    return;
  }
  if (pending_autocorrect_->text_length_diff < 0) {
    base::UmaHistogramEnumeration(
        histogram_name, AutocorrectRejectionBreakdown::kRemovedLetters);
    if (last_key_press == ui::DomCode::BACKSPACE ||
        last_key_press == ui::DomCode::DEL) {
      base::UmaHistogramEnumeration(
          histogram_name, AutocorrectRejectionBreakdown::kRejectedBackspace);
    }
    return;
  }
  if (pending_autocorrect_->text_length_diff > 0) {
    base::UmaHistogramEnumeration(
        histogram_name,
        AutocorrectRejectionBreakdown::kRejectedTypingNoSelection);
    return;
  }
  base::UmaHistogramEnumeration(histogram_name,
                                AutocorrectRejectionBreakdown::kRejectionOther);
}

void AutocorrectManager::MeasureAndLogAssistiveAutocorrectQualityBreakdown(
    AutocorrectActions action) {
  if (!pending_autocorrect_.has_value() ||
      pending_autocorrect_->suggested_text.empty() ||
      pending_autocorrect_->original_text.empty() ||
      (action != AutocorrectActions::kUserAcceptedAutocorrect &&
       action != AutocorrectActions::kUserActionClearedUnderline &&
       action != AutocorrectActions::kReverted)) {
    return;
  }

  bool suggestion_accepted =
      action == AutocorrectActions::kUserAcceptedAutocorrect;
  bool virtual_keyboard_visible =
      pending_autocorrect_->virtual_keyboard_visible;

  const std::u16string& original_text = pending_autocorrect_->original_text;
  const std::u16string& suggested_text = pending_autocorrect_->suggested_text;

  LogAssistiveAutocorrectQualityBreakdown(
      AutocorrectQualityBreakdown::kSuggestionResolved, suggestion_accepted,
      virtual_keyboard_visible);
  MeasureAndLogAssistiveAutocorrectEditDistance(original_text, suggested_text,
                                                suggestion_accepted,
                                                virtual_keyboard_visible);

  if (diacritics_insensitive_string_comparator_.Equal(original_text,
                                                      suggested_text)) {
    LogAssistiveAutocorrectQualityBreakdown(
        AutocorrectQualityBreakdown::kSuggestionChangedAccent,
        suggestion_accepted, virtual_keyboard_visible);
  }

  if (suggested_text.find(' ') != std::u16string::npos) {
    LogAssistiveAutocorrectQualityBreakdown(
        AutocorrectQualityBreakdown::kSuggestionSplittedWord,
        suggestion_accepted, virtual_keyboard_visible);
  }

  if (original_text.size() < suggested_text.size()) {
    LogAssistiveAutocorrectQualityBreakdown(
        AutocorrectQualityBreakdown::kSuggestionInsertedLetters,
        suggestion_accepted, virtual_keyboard_visible);
  } else if (original_text.size() == suggested_text.size()) {
    LogAssistiveAutocorrectQualityBreakdown(
        AutocorrectQualityBreakdown::kSuggestionMutatedLetters,
        suggestion_accepted, virtual_keyboard_visible);
  } else {
    LogAssistiveAutocorrectQualityBreakdown(
        AutocorrectQualityBreakdown::kSuggestionRemovedLetters,
        suggestion_accepted, virtual_keyboard_visible);
  }

  if (base::i18n::ToLower(original_text) ==
      base::i18n::ToLower(suggested_text)) {
    LogAssistiveAutocorrectQualityBreakdown(
        AutocorrectQualityBreakdown::kSuggestionChangeLetterCases,
        suggestion_accepted, virtual_keyboard_visible);
  }

  if (base::IsAsciiLower(original_text[0]) &&
      base::IsAsciiUpper(suggested_text[0])) {
    LogAssistiveAutocorrectQualityBreakdown(
        AutocorrectQualityBreakdown::kSuggestionCapitalizedWord,
        suggestion_accepted, virtual_keyboard_visible);
  } else if (base::IsAsciiUpper(original_text[0]) &&
             base::IsAsciiLower(suggested_text[0])) {
    LogAssistiveAutocorrectQualityBreakdown(
        AutocorrectQualityBreakdown::kSuggestionLowerCasedWord,
        suggestion_accepted, virtual_keyboard_visible);
  }

  if (base::IsStringASCII(original_text)) {
    LogAssistiveAutocorrectQualityBreakdown(
        AutocorrectQualityBreakdown::kOriginalTextIsAscii, suggestion_accepted,
        virtual_keyboard_visible);
  }
  if (base::IsStringASCII(suggested_text)) {
    LogAssistiveAutocorrectQualityBreakdown(
        AutocorrectQualityBreakdown::kSuggestedTextIsAscii, suggestion_accepted,
        virtual_keyboard_visible);
  }
}

void AutocorrectManager::LogAssistiveAutocorrectInternalState(
    AutocorrectInternalStates internal_state) {
  if (IsVkAutocorrect()) {
    base::UmaHistogramEnumeration(
        "InputMethod.Assistive.AutocorrectV2.Internal.VkState", internal_state);
    return;
  }
  if (AutoCorrectPrefIsPkEnabledByDefault()) {
    base::UmaHistogramEnumeration(
        "InputMethod.Assistive.AutocorrectV2.Internal.PkState."
        "EnabledByDefault",
        internal_state);
  }
  base::UmaHistogramEnumeration(
      "InputMethod.Assistive.AutocorrectV2.Internal.PkState", internal_state);
}

void AutocorrectManager::LogAssistiveAutocorrectQualityBreakdown(
    AutocorrectQualityBreakdown quality_breakdown,
    bool suggestion_accepted,
    bool virtual_keyboard_visible) {
  std::string histogram_name = "InputMethod.Assistive.AutocorrectV2.Quality.";
  histogram_name =
      base::StrCat({histogram_name, virtual_keyboard_visible ? "Vk" : "Pk",
                    suggestion_accepted ? "Accepted" : "Rejected"});

  base::UmaHistogramEnumeration(histogram_name, quality_breakdown);

  if (AutoCorrectPrefIsPkEnabledByDefault()) {
    base::UmaHistogramEnumeration(histogram_name + ".EnabledByDefault",
                                  quality_breakdown);
  }
}

void AutocorrectManager::OnActivate(const std::string& engine_id) {
  active_engine_id_ = engine_id;
  // Reset the previously stored suggestion_provider, we should expect a new
  // provider to be returned on the next OnConnectedToSuggestionProvider call.
  suggestion_provider_ = std::nullopt;

  PrefService* pref_service = profile_->GetPrefs();
  auto autocorrect_pref =
      GetPhysicalKeyboardAutocorrectPref(*pref_service, engine_id);

  if (!crosapi::browser_util::IsLacrosEnabled() &&
      base::FeatureList::IsEnabled(features::kAutocorrectByDefault) &&
      autocorrect_pref == AutocorrectPreference::kDefault &&
      IsUsEnglishId(engine_id) &&
      // This class is instantiated with NativeInputMethodEngineObserver, which
      // must exist at all times in the system to provide typing (including
      // login screens, guest sessions, etc). Make sure we are only recording
      // this metric when a real user has logged into their profile.
      ProfileHelper::IsUserProfile(profile_) && profile_->IsRegularProfile() &&
      !profile_->IsGuestSession() && !chromeos::IsKioskSession()) {
    SetPhysicalKeyboardAutocorrectAsEnabledByDefault(pref_service, engine_id);
  }
}

bool AutocorrectManager::OnKeyEvent(const ui::KeyEvent& event) {
  RecordPendingMetricsAwaitingKeyPress();

  if (!pending_autocorrect_.has_value() ||
      event.type() != ui::EventType::kKeyPressed) {
    return false;
  }
  // TODO(b:253549747): call pending_autocorrect_->last_key_event.reset() if
  // user changes the text using Mouse or Touch Screen
  pending_autocorrect_->last_key_event = event;

  // OnKeyEvent is only used for interacting with the undo UI.
  if (!pending_autocorrect_->undo_window_visible) {
    return false;
  }

  if (event.code() == ui::DomCode::ESCAPE) {
    HideUndoWindow();
    return true;
  }
  if (!event.IsAltDown() && !event.IsControlDown() &&
      (event.code() == ui::DomCode::ARROW_UP ||
       event.code() == ui::DomCode::TAB)) {
    HighlightButtons(/*should_highlight_undo=*/true,
                     /*should_highlight_learn_more=*/false);
    return true;
  }
  if (event.code() == ui::DomCode::ARROW_LEFT &&
      pending_autocorrect_->learn_more_button_highlighted) {
    HighlightButtons(/*should_highlight_undo=*/true,
                     /*should_highlight_learn_more=*/false);
    return true;
  }
  if (event.code() == ui::DomCode::ARROW_RIGHT &&
      pending_autocorrect_->undo_button_highlighted &&
      pending_autocorrect_->learn_more_button_visible) {
    HighlightButtons(/*should_highlight_undo=*/false,
                     /*should_highlight_learn_more=*/true);
    return true;
  }
  if (event.code() == ui::DomCode::ENTER) {
    if (pending_autocorrect_->undo_button_highlighted) {
      UndoAutocorrect();
      return true;
    }
    if (pending_autocorrect_->learn_more_button_highlighted) {
      HideUndoWindow();
      suggestion_handler_->ClickButton(learn_more_button_);
      return true;
    }
  }
  return false;
}

void AutocorrectManager::OnSurroundingTextChanged(
    const std::u16string& text,
    const gfx::Range selection_range) {
  if (error_on_hiding_undo_window_) {
    HideUndoWindow();
  }

  if (CouldTriggerAutocorrectWithSurroundingText(text, selection_range)) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kCouldTriggerAutocorrect);
  }

  if (!pending_autocorrect_.has_value()) {
    return;
  }

  std::string error;
  TextInputTarget* input_context = IMEBridge::Get()->GetInputContextHandler();

  // Null input context invalidates the range so consider the pending
  // range as implicitly rejected/cleared.
  if (!input_context) {
    AcceptOrClearPendingAutocorrect();
    return;
  }

  const gfx::Range range = input_context->GetAutocorrectRange();
  if (!pending_autocorrect_->is_validated) {
    // Validate that the surrounding text matches with pending autocorrect
    // suggestion. Because of delays in update of surrounding text and
    // autocorrect range, the validation waits until all these information are
    // matching with each others (a.k.a. updated). This is necessary for
    // implementation of autocorrect interactions such as implicit acceptance.
    pending_autocorrect_->is_validated =
        IsAutocorrectSuggestionInSurroundingText(
            text, range, pending_autocorrect_->suggested_text);
    pending_autocorrect_->validation_tries++;

    if (!pending_autocorrect_->is_validated) {
      // Clear suggestion if multiple trials of validation fails.
      // This is a guard to prevent unwanted situation that can keep
      // autocorrect suggestion pending forever.
      if (pending_autocorrect_->validation_tries >= kMaxValidationTries) {
        AcceptOrClearPendingAutocorrect();
      }
      return;
    }
  }

  pending_autocorrect_->text_length_diff =
      pending_autocorrect_->text_length != -1
          ? text.length() - pending_autocorrect_->text_length
          : 0;

  // If range is empty, it means user has mutated suggestion. So, clear range
  // and consider autocorrect suggestion as implicitly rejected.
  if (range.is_empty()) {
    AcceptOrClearPendingAutocorrect();
    return;
  }

  // If it is the first call of the event after handling autocorrect range,
  // initialize the variables and do not process the empty range as it is
  // potentially stale.
  if (pending_autocorrect_->num_inserted_chars < 0) {
    pending_autocorrect_->num_inserted_chars = 0;
  } else if (static_cast<int>(text.length()) >
             pending_autocorrect_->text_length) {
    // TODO(b/161490813): Fix double counting of emojis and some CJK chars.
    // TODO(b/161490813): Fix logic for text replace.

    // Count characters added between two calls of the event.
    pending_autocorrect_->num_inserted_chars +=
        text.length() - pending_autocorrect_->text_length;
  }
  pending_autocorrect_->text_length = text.length();

  // If the number of added characters after setting the pending range is above
  // the threshold, then accept the pending range.
  if (pending_autocorrect_->num_inserted_chars >=
      kDistanceUntilUnderlineHides) {
    AcceptOrClearPendingAutocorrect();
    return;
  }

  const uint32_t cursor_pos = selection_range.end();

  // If cursor is inside autocorrect range (inclusive), show undo window and
  // record relevant metrics.
  // On Lacros, the async behaviors accidentally delay the update of autocorrect
  // range after the onSurroundingTextChanged. When users delete a few
  // characters of the suggested words, this code still uses the outdated range,
  // hence allowing the undo window to show.
  // TODO(b/278616918): Consider remove the
  // IsAutocorrectSuggestionInSurroundingText logic once async behaviors are
  // corrected.
  if (IsAutocorrectSuggestionInSurroundingText(
          text, range, pending_autocorrect_->suggested_text) &&
      cursor_pos >= range.start() && cursor_pos <= range.end() &&
      selection_range.is_empty()) {
    ShowUndoWindow(range, text);
  } else {
    // Ensure undo window is hidden when cursor is not inside the autocorrect
    // range.
    HideUndoWindow();
  }

  // Only update at the end so that the metrics can use the cursor selection
  // just before the edit
  pending_autocorrect_->last_autocorrect_range = range;
  pending_autocorrect_->last_selection_range =
      gfx::Range(selection_range.GetMin(), selection_range.GetMax());
}

void AutocorrectManager::OnFocus(int context_id) {
  if (active_engine_id_) {
    pending_user_pref_metric_ =
        PendingPhysicalKeyboardUserPrefMetric{.engine_id = *active_engine_id_};
  }

  if (base::FeatureList::IsEnabled(ash::features::kImeRuleConfig)) {
    GetTextFieldContextualInfo(
        base::BindOnce(&AutocorrectManager::OnTextFieldContextualInfoChanged,
                       base::Unretained(this)));
  }

  num_handled_autocorrect_in_text_field_ = 0;

  LogAssistiveAutocorrectInternalState(
      AutocorrectInternalStates::kOnFocusEvent);
  if (pending_autocorrect_.has_value()) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kOnFocusEventWithPendingSuggestion);
  }

  context_id_ = context_id;
  ProcessTextFieldChange();
}

void AutocorrectManager::OnConnectedToSuggestionProvider(
    const ime::AutocorrectSuggestionProvider& suggestion_provider) {
  suggestion_provider_ = suggestion_provider;
  if (active_engine_id_ && IsUsEnglishId(*active_engine_id_)) {
    pending_suggestion_provider_metric_ =
        PendingSuggestionProviderMetric{.provider = suggestion_provider};
  }
}

void AutocorrectManager::OnBlur() {
  LogAssistiveAutocorrectInternalState(AutocorrectInternalStates::kOnBlurEvent);

  if (pending_autocorrect_.has_value()) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kOnBlurEventWithPendingSuggestion);
  }

  if (num_handled_autocorrect_in_text_field_ > 0) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kTextFieldEditsWithAtLeastOneSuggestion);
  }

  ProcessTextFieldChange();
}

void AutocorrectManager::ProcessTextFieldChange() {
  TextInputTarget* input_context = IMEBridge::Get()->GetInputContextHandler();

  // Clear autocorrect range if any.
  if (input_context) {
    HideUndoWindow();
    input_context->SetAutocorrectRange(gfx::Range(), base::DoNothing());
  }

  if (pending_autocorrect_.has_value()) {
    LogAssistiveAutocorrectAction(
        AutocorrectActions::kUserExitedTextFieldWithUnderline);
    pending_autocorrect_.reset();
  }
}

void AutocorrectManager::UndoAutocorrect() {
  if (!pending_autocorrect_.has_value() ||
      !pending_autocorrect_->is_validated) {
    return;
  }

  HideUndoWindow();

  TextInputTarget* input_context = IMEBridge::Get()->GetInputContextHandler();
  const gfx::Range autocorrect_range = input_context->GetAutocorrectRange();

  if (input_context->HasCompositionText()) {
    input_context->SetComposingRange(autocorrect_range.start(),
                                     autocorrect_range.end(), {});
    input_context->CommitText(
        pending_autocorrect_->original_text,
        ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  } else {
    // NOTE: GetSurroundingTextInfo() could return a stale cache that no longer
    // reflects reality, due to async-ness between IMF and TextInputClient.
    // TODO(crbug/1194424): Work around the issue or fix
    // GetSurroundingTextInfo().
    const SurroundingTextInfo surrounding_text =
        input_context->GetSurroundingTextInfo();

    // Delete the autocorrected text.
    // This will not quite work properly if there is text actually highlighted,
    // and cursor is at end of the highlight block, but no easy way around it.
    // First delete everything before cursor.
    DCHECK(surrounding_text.selection_range.IsBoundedBy(autocorrect_range));
    const uint32_t before =
        surrounding_text.selection_range.start() - autocorrect_range.start();
    const uint32_t after =
        autocorrect_range.end() - surrounding_text.selection_range.end();

    if (base::FeatureList::IsEnabled(
            features::kAutocorrectUseReplaceSurroundingText) &&
        !crosapi::browser_util::IsLacrosEnabled()) {
      input_context->ReplaceSurroundingText(
          before, after, pending_autocorrect_->original_text);
    } else {
      input_context->DeleteSurroundingText(before, after);
      // Replace with the original text.
      input_context->CommitText(
          pending_autocorrect_->original_text,
          ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
    }
  }

  MeasureAndLogAssistiveAutocorrectQualityBreakdown(
      AutocorrectActions::kReverted);
  LogAssistiveAutocorrectAction(AutocorrectActions::kReverted);
  RecordAssistiveCoverage(AssistiveType::kAutocorrectReverted);
  RecordAssistiveSuccess(AssistiveType::kAutocorrectReverted);
  LogAssistiveAutocorrectDelay(base::TimeTicks::Now() -
                               pending_autocorrect_->start_time);
  pending_autocorrect_.reset();
}

void AutocorrectManager::ShowUndoWindow(gfx::Range range,
                                        const std::u16string& text) {
  if (!pending_autocorrect_.has_value() ||
      !pending_autocorrect_->is_validated ||
      pending_autocorrect_->undo_window_visible) {
    return;
  }

  std::string error;
  const std::u16string autocorrected_text =
      text.substr(range.start(), range.length());
  AssistiveWindowProperties properties;
  properties.type = ash::ime::AssistiveWindowType::kUndoWindow;
  properties.visible = true;
  properties.show_setting_link =
      pending_autocorrect_->learn_more_button_visible;
  properties.announce_string = l10n_util::GetStringFUTF16(
      IDS_SUGGESTION_AUTOCORRECT_UNDO_WINDOW_SHOWN,
      pending_autocorrect_->original_text, autocorrected_text);
  suggestion_handler_->SetAssistiveWindowProperties(context_id_, properties,
                                                    &error);

  LogAssistiveAutocorrectInternalState(
      AutocorrectInternalStates::kShowUndoWindow);

  if (!error.empty()) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kErrorShowUndoWindow);
    LOG(ERROR) << "Failed to show autocorrect undo window.";
    return;
  }

  // Showing a new undo window overrides the current shown undo window. So
  // there is no need to first trying to hide the previous one.
  error_on_hiding_undo_window_ = false;

  if (!pending_autocorrect_->window_shown_logged) {
    LogAssistiveAutocorrectAction(AutocorrectActions::kWindowShown);
    RecordAssistiveCoverage(AssistiveType::kAutocorrectWindowShown);
    if (pending_autocorrect_->learn_more_button_visible) {
      base::UmaHistogramBoolean(
          "InputMethod.Assistive.AutocorrectV2.UndoWindow.LearnMoreButtonShown",
          true);
    }
    IncrementPrefValueUntilCapped(kUndoWindowShowSettingCount,
                                  kUndoWindowShowSettingMaxCount, *profile_);
    pending_autocorrect_->window_shown_logged = true;
  }

  pending_autocorrect_->undo_button_highlighted = false;
  pending_autocorrect_->undo_window_visible = true;
}

void AutocorrectManager::HideUndoWindow() {
  if (!error_on_hiding_undo_window_ &&
      (!pending_autocorrect_.has_value() ||
       !pending_autocorrect_->undo_window_visible)) {
    return;
  }

  std::string error;
  AssistiveWindowProperties properties;
  properties.type = ash::ime::AssistiveWindowType::kUndoWindow;
  properties.visible = false;
  suggestion_handler_->SetAssistiveWindowProperties(context_id_, properties,
                                                    &error);

  LogAssistiveAutocorrectInternalState(
      AutocorrectInternalStates::kHideUndoWindow);

  if (!error.empty()) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kErrorHideUndoWindow);
    LOG(ERROR) << "Failed to hide autocorrect undo window.";
    error_on_hiding_undo_window_ = true;
    return;
  }

  error_on_hiding_undo_window_ = false;

  if (pending_autocorrect_.has_value()) {
    pending_autocorrect_->undo_button_highlighted = false;
    pending_autocorrect_->learn_more_button_highlighted = false;
    pending_autocorrect_->undo_window_visible = false;
  }
}

void AutocorrectManager::HighlightButtons(
    const bool should_highlight_undo,
    const bool should_highlight_learn_more) {
  if (!pending_autocorrect_.has_value() ||
      !pending_autocorrect_->undo_window_visible) {
    return;
  }

  std::string error;
  undo_button_.announce_string =
      l10n_util::GetStringFUTF16(IDS_SUGGESTION_AUTOCORRECT_UNDO_BUTTON,
                                 pending_autocorrect_->original_text);
  suggestion_handler_->SetButtonHighlighted(context_id_, undo_button_,
                                            should_highlight_undo, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Failed to highlight undo button.";
    return;
  }
  learn_more_button_.announce_string =
      l10n_util::GetStringUTF16(IDS_SUGGESTION_AUTOCORRECT_LEARN_MORE);
  suggestion_handler_->SetButtonHighlighted(
      context_id_, learn_more_button_, should_highlight_learn_more, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Failed to highlight learn more button.";
    return;
  }

  pending_autocorrect_->undo_button_highlighted = should_highlight_undo;
  pending_autocorrect_->learn_more_button_highlighted =
      should_highlight_learn_more;
}

void AutocorrectManager::AcceptOrClearPendingAutocorrect() {
  if (!pending_autocorrect_.has_value()) {
    return;
  }

  TextInputTarget* input_context = IMEBridge::Get()->GetInputContextHandler();

  LogAssistiveAutocorrectInternalState(
      AutocorrectInternalStates::kSuggestionResolved);

  if (!input_context) {
    LogAssistiveAutocorrectAction(AutocorrectActions::kInvalidRange);
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kNoInputContext);
  } else if (!pending_autocorrect_->is_validated) {
    LogAssistiveAutocorrectAction(AutocorrectActions::kInvalidRange);
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kErrorRangeNotValidated);
  } else if (!input_context->GetAutocorrectRange().is_empty()) {
    MeasureAndLogAssistiveAutocorrectQualityBreakdown(
        AutocorrectActions::kUserAcceptedAutocorrect);
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kSuggestionAccepted);
    // Non-empty autocorrect range means that the user has not modified
    // autocorrect suggestion to invalidate it. So, it is considered as
    // accepted.
    LogAssistiveAutocorrectAction(AutocorrectActions::kUserAcceptedAutocorrect);
  } else {
    MeasureAndLogAssistiveAutocorrectQualityBreakdown(
        AutocorrectActions::kUserActionClearedUnderline);
    LogAssistiveAutocorrectAction(
        AutocorrectActions::kUserActionClearedUnderline);
  }

  if (input_context) {
    input_context->SetAutocorrectRange(gfx::Range(),
                                       base::DoNothing());  // clear underline
  }

  HideUndoWindow();
  pending_autocorrect_.reset();
}

void AutocorrectManager::OnTextFieldContextualInfoChanged(
    const TextFieldContextualInfo& info) {
  disabled_by_rule_ =
      denylist_.Contains(info.tab_url) || chromeos::IsKioskSession();
  if (disabled_by_rule_) {
    LogAssistiveAutocorrectInternalState(
        AutocorrectInternalStates::kAppIsInDenylist);
  }
}

bool AutocorrectManager::DisabledByRule() {
  return disabled_by_rule_;
}

bool AutocorrectManager::DisabledByInvalidExperimentContext() {
  if (!active_engine_id_ || !UserInAutocorrectByDefaultBucket(
                                *(profile_->GetPrefs()), *active_engine_id_)) {
    return false;
  }

  // If the user is in the autocorrect by default bucket, and the en840 model is
  // not available or the updated parameter list is not enabled, then disable
  // autocorrect.
  return !(
      suggestion_provider_ &&
      (suggestion_provider_ ==
           ime::AutocorrectSuggestionProvider::kUsEnglish840 ||
       suggestion_provider_ ==
           ime::AutocorrectSuggestionProvider::kUsEnglish840V2) &&
      base::FeatureList::IsEnabled(ash::features::kImeFstDecoderParamsUpdate));
}

AutocorrectManager::PendingAutocorrectState::PendingAutocorrectState(
    const std::u16string& original_text,
    const std::u16string& suggested_text,
    const base::TimeTicks& start_time,
    bool virtual_keyboard_visible,
    bool learn_more_button_visible)
    : original_text(original_text),
      suggested_text(suggested_text),
      start_time(start_time),
      virtual_keyboard_visible(virtual_keyboard_visible),
      learn_more_button_visible(learn_more_button_visible) {}

AutocorrectManager::PendingAutocorrectState::PendingAutocorrectState(
    const PendingAutocorrectState& other) = default;

AutocorrectManager::PendingAutocorrectState::~PendingAutocorrectState() =
    default;

}  // namespace input_method
}  // namespace ash
