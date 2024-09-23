// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/autocorrect_manager.h"

#include "ash/constants/ash_features.h"
#include "ash/system/federated/federated_client_manager.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/input_method/autocorrect_enums.h"
#include "chrome/browser/ash/input_method/autocorrect_prefs.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/ui/ash/input_method/suggestion_details.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/federated/federated_client.h"
#include "chromeos/ash/services/federated/public/cpp/fake_service_connection.h"
#include "chromeos/ash/services/federated/public/cpp/service_connection.h"
#include "chromeos/ash/services/ime/public/cpp/autocorrect.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/input_method_ash.h"
#include "ui/base/ime/ash/mock_ime_input_context_handler.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {

namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::ExpectationSet;
using ::testing::Return;
using ::testing::SetArgPointee;

using ime::AutocorrectSuggestionProvider;
using UkmEntry = ukm::builders::InputMethod_Assistive_AutocorrectV2;

constexpr char kCoverageHistogramName[] = "InputMethod.Assistive.Coverage";
constexpr char kSuccessHistogramName[] = "InputMethod.Assistive.Success";
constexpr char kDelayHistogramName[] =
    "InputMethod.Assistive.Autocorrect.Delay";
constexpr char kAutocorrectActionHistogramName[] =
    "InputMethod.Assistive.Autocorrect.Actions";
constexpr char kVKAutocorrectActionHistogramName[] =
    "InputMethod.Assistive.Autocorrect.Actions.VK";
constexpr char kVKAutocorrectV2ActionHistogramName[] =
    "InputMethod.Assistive.AutocorrectV2.Actions.VK";
constexpr char kPKAutocorrectV2ActionHistogramName[] =
    "InputMethod.Assistive.AutocorrectV2.Actions.PK";
constexpr char kPKAutocorrectV2ActionEnabledByDefaultHistogramName[] =
    "InputMethod.Assistive.AutocorrectV2.Actions.PK.EnabledByDefault";
constexpr char kAutocorrectV2AcceptLatency[] =
    "InputMethod.Assistive.AutocorrectV2.Latency.Accept";
constexpr char kAutocorrectV2AcceptLatencyEnabledByDefault[] =
    "InputMethod.Assistive.AutocorrectV2.Latency.Accept.EnabledByDefault";
constexpr char kAutocorrectV2RejectLatency[] =
    "InputMethod.Assistive.AutocorrectV2.Latency.Reject";
constexpr char kAutocorrectV2RejectLatencyEnabledByDefault[] =
    "InputMethod.Assistive.AutocorrectV2.Latency.Reject.EnabledByDefault";
constexpr char kAutocorrectV2ExitFieldLatency[] =
    "InputMethod.Assistive.AutocorrectV2.Latency.ExitField";
constexpr char kAutocorrectV2ExitFieldLatencyEnabledByDefault[] =
    "InputMethod.Assistive.AutocorrectV2.Latency.ExitField.EnabledByDefault";
constexpr char kAutocorrectV2VkPendingLatency[] =
    "InputMethod.Assistive.AutocorrectV2.Latency.VkPending";
constexpr char kAutocorrectV2PkPendingLatency[] =
    "InputMethod.Assistive.AutocorrectV2.Latency.PkPending";
constexpr char kAutocorrectV2QualityVkAcceptedHistName[] =
    "InputMethod.Assistive.AutocorrectV2.Quality.VkAccepted";
constexpr char kAutocorrectV2QualityVkRejectedHistName[] =
    "InputMethod.Assistive.AutocorrectV2.Quality.VkRejected";
constexpr char kAutocorrectV2QualityPkAcceptedHistName[] =
    "InputMethod.Assistive.AutocorrectV2.Quality.PkAccepted";
constexpr char kAutocorrectV2QualityDefaultPkAcceptedHistName[] =
    "InputMethod.Assistive.AutocorrectV2.Quality.PkAccepted.EnabledByDefault";
constexpr char kAutocorrectV2QualityPkRejectedHistName[] =
    "InputMethod.Assistive.AutocorrectV2.Quality.PkRejected";
constexpr char kAutocorrectV2QualityDefaultPkRejectedHistName[] =
    "InputMethod.Assistive.AutocorrectV2.Quality.PkRejected.EnabledByDefault";
constexpr char kAutocorrectV2Distance2dVkAcceptedHistName[] =
    "InputMethod.Assistive.AutocorrectV2.Distance."
    "OriginalLengthVsLevenshteinDistance.VkAccepted";
constexpr char kAutocorrectV2Distance2dVkRejectedHistName[] =
    "InputMethod.Assistive.AutocorrectV2.Distance."
    "OriginalLengthVsLevenshteinDistance.VkRejected";
constexpr char kAutocorrectV2Distance2dPkAcceptedHistName[] =
    "InputMethod.Assistive.AutocorrectV2.Distance."
    "OriginalLengthVsLevenshteinDistance.PkAccepted";
constexpr char kAutocorrectV2Distance2dPkRejectedHistName[] =
    "InputMethod.Assistive.AutocorrectV2.Distance."
    "OriginalLengthVsLevenshteinDistance.PkRejected";
constexpr char kAutocorrectV2DistanceSuggestedVkAcceptedHistName[] =
    "InputMethod.Assistive.AutocorrectV2.Distance.SuggestedLength.VkAccepted";
constexpr char kAutocorrectV2DistanceSuggestedVkRejectedHistName[] =
    "InputMethod.Assistive.AutocorrectV2.Distance.SuggestedLength.VkRejected";
constexpr char kAutocorrectV2DistanceSuggestedPkAcceptedHistName[] =
    "InputMethod.Assistive.AutocorrectV2.Distance.SuggestedLength.PkAccepted";
constexpr char kAutocorrectV2DistanceSuggestedPkRejectedHistName[] =
    "InputMethod.Assistive.AutocorrectV2.Distance.SuggestedLength.PkRejected";
constexpr char kAutocorrectV2PkUserPreferenceAll[] =
    "InputMethod.Assistive.AutocorrectV2.PkUserPreference.All";
constexpr char kAutocorrectV2PkUserPreferenceEnglish[] =
    "InputMethod.Assistive.AutocorrectV2.PkUserPreference.English";
constexpr char kAutocorrectV2PkRejectionHistName[] =
    "InputMethod.Assistive.AutocorrectV2.Rejection.PK";
constexpr char kAutocorrectV2VkRejectionHistName[] =
    "InputMethod.Assistive.AutocorrectV2.Rejection.VK";
constexpr char kAutocorrectV2PkSuggestionProviderHistName[] =
    "InputMethod.Assistive.AutocorrectV2.SuggestionProvider.Pk";

constexpr char kUsEnglishEngineId[] = "xkb:us::eng";
constexpr char kUsInternationalEngineId[] = "xkb:us:intl:eng";
constexpr char kSpainSpanishEngineId[] = "xkb:es::spa";
constexpr char kLatinAmericaSpanishEngineId[] = "xkb:latam::spa";
constexpr char kBrazilPortugeseEngineId[] = "xkb:br::por";
constexpr char kFranceFrenchEngineId[] = "xkb:fr::fra";

constexpr int kContextId = 5;

// A helper for testing autocorrect histograms. There are redundant metrics
// for each autocorrect action and the helper ensures that all the relevant
// metrics for one action are updated properly.
void ExpectAutocorrectHistograms(const base::HistogramTester& histogram_tester,
                                 bool visible_vk,
                                 int window_shown,
                                 int underlined,
                                 int reverted,
                                 int accepted,
                                 int cleared_underline,
                                 int exited_text_field_with_underline = 0,
                                 int invalid_range = 0,
                                 bool enabled_by_default = false) {
  // Window shown metrics.
  histogram_tester.ExpectBucketCount(kCoverageHistogramName,
                                     AssistiveType::kAutocorrectWindowShown,
                                     window_shown);
  histogram_tester.ExpectBucketCount(kAutocorrectActionHistogramName,
                                     AutocorrectActions::kWindowShown,
                                     window_shown);
  if (visible_vk) {
    histogram_tester.ExpectBucketCount(kVKAutocorrectActionHistogramName,
                                       AutocorrectActions::kWindowShown,
                                       window_shown);
    histogram_tester.ExpectBucketCount(kVKAutocorrectV2ActionHistogramName,
                                       AutocorrectActions::kWindowShown,
                                       window_shown);
  } else {
    histogram_tester.ExpectBucketCount(kPKAutocorrectV2ActionHistogramName,
                                       AutocorrectActions::kWindowShown,
                                       window_shown);
    histogram_tester.ExpectBucketCount(
        kPKAutocorrectV2ActionEnabledByDefaultHistogramName,
        AutocorrectActions::kWindowShown,
        enabled_by_default ? window_shown : 0);
  }

  // Underlined metrics.
  histogram_tester.ExpectBucketCount(kCoverageHistogramName,
                                     AssistiveType::kAutocorrectUnderlined,
                                     underlined);
  histogram_tester.ExpectBucketCount(kAutocorrectActionHistogramName,
                                     AutocorrectActions::kUnderlined,
                                     underlined);
  if (visible_vk) {
    histogram_tester.ExpectBucketCount(kVKAutocorrectActionHistogramName,
                                       AutocorrectActions::kUnderlined,
                                       underlined);
    histogram_tester.ExpectBucketCount(kVKAutocorrectV2ActionHistogramName,
                                       AutocorrectActions::kUnderlined,
                                       underlined);
  } else {
    histogram_tester.ExpectBucketCount(kPKAutocorrectV2ActionHistogramName,
                                       AutocorrectActions::kUnderlined,
                                       underlined);
    histogram_tester.ExpectBucketCount(
        kPKAutocorrectV2ActionEnabledByDefaultHistogramName,
        AutocorrectActions::kUnderlined, enabled_by_default ? underlined : 0);
  }

  // Revert metrics.
  histogram_tester.ExpectBucketCount(
      kCoverageHistogramName, AssistiveType::kAutocorrectReverted, reverted);
  histogram_tester.ExpectBucketCount(
      kSuccessHistogramName, AssistiveType::kAutocorrectReverted, reverted);
  histogram_tester.ExpectBucketCount(kAutocorrectActionHistogramName,
                                     AutocorrectActions::kReverted, reverted);
  if (visible_vk) {
    histogram_tester.ExpectBucketCount(kVKAutocorrectActionHistogramName,
                                       AutocorrectActions::kReverted, reverted);
    histogram_tester.ExpectBucketCount(kVKAutocorrectV2ActionHistogramName,
                                       AutocorrectActions::kReverted, reverted);
  } else {
    histogram_tester.ExpectBucketCount(kPKAutocorrectV2ActionHistogramName,
                                       AutocorrectActions::kReverted, reverted);
    histogram_tester.ExpectBucketCount(
        kPKAutocorrectV2ActionEnabledByDefaultHistogramName,
        AutocorrectActions::kReverted, enabled_by_default ? reverted : 0);
  }

  // Accept metrics.
  histogram_tester.ExpectBucketCount(
      kAutocorrectActionHistogramName,
      AutocorrectActions::kUserAcceptedAutocorrect, accepted);
  if (visible_vk) {
    histogram_tester.ExpectBucketCount(
        kVKAutocorrectActionHistogramName,
        AutocorrectActions::kUserAcceptedAutocorrect, accepted);
    histogram_tester.ExpectBucketCount(
        kVKAutocorrectV2ActionHistogramName,
        AutocorrectActions::kUserAcceptedAutocorrect, accepted);
  } else {
    histogram_tester.ExpectBucketCount(
        kPKAutocorrectV2ActionHistogramName,
        AutocorrectActions::kUserAcceptedAutocorrect, accepted);
    histogram_tester.ExpectBucketCount(
        kPKAutocorrectV2ActionEnabledByDefaultHistogramName,
        AutocorrectActions::kUserAcceptedAutocorrect,
        enabled_by_default ? accepted : 0);
  }

  // Clear underline metrics.
  histogram_tester.ExpectBucketCount(
      kAutocorrectActionHistogramName,
      AutocorrectActions::kUserActionClearedUnderline, cleared_underline);
  if (visible_vk) {
    histogram_tester.ExpectBucketCount(
        kVKAutocorrectActionHistogramName,
        AutocorrectActions::kUserActionClearedUnderline, cleared_underline);
    histogram_tester.ExpectBucketCount(
        kVKAutocorrectV2ActionHistogramName,
        AutocorrectActions::kUserActionClearedUnderline, cleared_underline);
  } else {
    histogram_tester.ExpectBucketCount(
        kPKAutocorrectV2ActionHistogramName,
        AutocorrectActions::kUserActionClearedUnderline, cleared_underline);
    histogram_tester.ExpectBucketCount(
        kPKAutocorrectV2ActionEnabledByDefaultHistogramName,
        AutocorrectActions::kUserActionClearedUnderline,
        enabled_by_default ? cleared_underline : 0);
  }

  // Invalid Range metrics.
  histogram_tester.ExpectBucketCount(kAutocorrectActionHistogramName,
                                     AutocorrectActions::kInvalidRange,
                                     invalid_range);
  if (visible_vk) {
    histogram_tester.ExpectBucketCount(kVKAutocorrectActionHistogramName,
                                       AutocorrectActions::kInvalidRange,
                                       invalid_range);
    histogram_tester.ExpectBucketCount(kVKAutocorrectV2ActionHistogramName,
                                       AutocorrectActions::kInvalidRange,
                                       invalid_range);
  } else {
    histogram_tester.ExpectBucketCount(kPKAutocorrectV2ActionHistogramName,
                                       AutocorrectActions::kInvalidRange,
                                       invalid_range);
    histogram_tester.ExpectBucketCount(
        kPKAutocorrectV2ActionEnabledByDefaultHistogramName,
        AutocorrectActions::kInvalidRange,
        enabled_by_default ? invalid_range : 0);
  }

  // Exited text field with underline.
  histogram_tester.ExpectBucketCount(
      kAutocorrectActionHistogramName,
      AutocorrectActions::kUserExitedTextFieldWithUnderline,
      exited_text_field_with_underline);
  if (visible_vk) {
    histogram_tester.ExpectBucketCount(
        kVKAutocorrectActionHistogramName,
        AutocorrectActions::kUserExitedTextFieldWithUnderline,
        exited_text_field_with_underline);
    histogram_tester.ExpectBucketCount(
        kVKAutocorrectV2ActionHistogramName,
        AutocorrectActions::kUserExitedTextFieldWithUnderline,
        exited_text_field_with_underline);
  } else {
    histogram_tester.ExpectBucketCount(
        kPKAutocorrectV2ActionHistogramName,
        AutocorrectActions::kUserExitedTextFieldWithUnderline,
        exited_text_field_with_underline);
    histogram_tester.ExpectBucketCount(
        kPKAutocorrectV2ActionEnabledByDefaultHistogramName,
        AutocorrectActions::kUserExitedTextFieldWithUnderline,
        enabled_by_default ? exited_text_field_with_underline : 0);
  }

  const int total_actions = window_shown + underlined + reverted + accepted +
                            cleared_underline +
                            exited_text_field_with_underline + invalid_range;
  const int total_coverage = window_shown + underlined + reverted;

  // Count total bucket to test side-effects and make the helper robust against
  // future changes of the metric buckets.
  histogram_tester.ExpectTotalCount(kCoverageHistogramName, total_coverage);
  histogram_tester.ExpectTotalCount(kSuccessHistogramName, reverted);
  histogram_tester.ExpectTotalCount(kAutocorrectActionHistogramName,
                                    total_actions);
  histogram_tester.ExpectTotalCount(kVKAutocorrectActionHistogramName,
                                    visible_vk ? total_actions : 0);
  histogram_tester.ExpectTotalCount(kVKAutocorrectV2ActionHistogramName,
                                    visible_vk ? total_actions : 0);
  histogram_tester.ExpectTotalCount(kPKAutocorrectV2ActionHistogramName,
                                    visible_vk ? 0 : total_actions);
  histogram_tester.ExpectTotalCount(
      kPKAutocorrectV2ActionEnabledByDefaultHistogramName,
      (visible_vk || !enabled_by_default) ? 0 : total_actions);

  // Latency metrics.
  histogram_tester.ExpectTotalCount(kDelayHistogramName, reverted);
  histogram_tester.ExpectTotalCount(kAutocorrectV2AcceptLatency, accepted);
  histogram_tester.ExpectTotalCount(kAutocorrectV2AcceptLatencyEnabledByDefault,
                                    enabled_by_default ? accepted : 0);
  histogram_tester.ExpectTotalCount(kAutocorrectV2ExitFieldLatency,
                                    exited_text_field_with_underline);
  histogram_tester.ExpectTotalCount(
      kAutocorrectV2ExitFieldLatencyEnabledByDefault,
      enabled_by_default ? exited_text_field_with_underline : 0);
  histogram_tester.ExpectTotalCount(
      kAutocorrectV2RejectLatency,
      reverted + cleared_underline + invalid_range);
  histogram_tester.ExpectTotalCount(
      kAutocorrectV2RejectLatencyEnabledByDefault,
      enabled_by_default ? reverted + cleared_underline + invalid_range : 0);
  histogram_tester.ExpectTotalCount(
      kAutocorrectV2VkPendingLatency,
      visible_vk ? cleared_underline + reverted + accepted + invalid_range +
                       exited_text_field_with_underline
                 : 0);
  histogram_tester.ExpectTotalCount(
      kAutocorrectV2PkPendingLatency,
      visible_vk ? 0
                 : cleared_underline + reverted + accepted + invalid_range +
                       exited_text_field_with_underline);
}

// A helper to create properties for hidden undo window.
AssistiveWindowProperties CreateHiddenUndoWindowProperties() {
  AssistiveWindowProperties window_properties;
  window_properties.type = ash::ime::AssistiveWindowType::kUndoWindow;
  window_properties.visible = false;
  return window_properties;
}

// A helper to create properties for shown undo window.
AssistiveWindowProperties CreateVisibleUndoWindowProperties(
    const std::u16string& original_text,
    const std::u16string& autocorrected_text) {
  AssistiveWindowProperties window_properties;
  window_properties.type = ash::ime::AssistiveWindowType::kUndoWindow;
  window_properties.visible = true;
  window_properties.announce_string =
      l10n_util::GetStringFUTF16(IDS_SUGGESTION_AUTOCORRECT_UNDO_WINDOW_SHOWN,
                                 original_text, autocorrected_text);
  return window_properties;
}

// A helper to create properties for shown undo window with additional learn
// more button.
AssistiveWindowProperties CreateVisibleUndoWindowWithLearnMoreButtonProperties(
    const std::u16string& original_text,
    const std::u16string& autocorrected_text) {
  AssistiveWindowProperties window_properties;
  window_properties.type = ash::ime::AssistiveWindowType::kUndoWindow;
  window_properties.visible = true;
  window_properties.show_setting_link = true;
  window_properties.announce_string =
      l10n_util::GetStringFUTF16(IDS_SUGGESTION_AUTOCORRECT_UNDO_WINDOW_SHOWN,
                                 original_text, autocorrected_text);
  return window_properties;
}

// A helper to create highlighted undo button in assistive window.
ui::ime::AssistiveWindowButton CreateHighlightedUndoButton(
    const std::u16string& original_text) {
  ui::ime::AssistiveWindowButton button = ui::ime::AssistiveWindowButton();
  button.id = ui::ime::ButtonId::kUndo;
  button.window_type = ash::ime::AssistiveWindowType::kUndoWindow;
  button.announce_string = l10n_util::GetStringFUTF16(
      IDS_SUGGESTION_AUTOCORRECT_UNDO_BUTTON, original_text);
  return button;
}

// A helper to create highlighted learn more button in assistive window.
ui::ime::AssistiveWindowButton CreateHighlightedLearnMoreButton() {
  ui::ime::AssistiveWindowButton button = ui::ime::AssistiveWindowButton();
  button.id = ui::ime::ButtonId::kLearnMore;
  button.announce_string =
      l10n_util::GetStringUTF16(IDS_SUGGESTION_AUTOCORRECT_LEARN_MORE);
  button.window_type = ash::ime::AssistiveWindowType::kLearnMore;
  return button;
}

// A helper for creating key event.
ui::KeyEvent CreateKeyEvent(ui::DomKey key, ui::DomCode code) {
  return ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_UNKNOWN, code,
                      ui::EF_NONE, key, ui::EventTimeForNow());
}

ui::KeyEvent PressKeyWithCtrl(const ui::DomCode& code) {
  return ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_UNKNOWN, code,
                      ui::EF_CONTROL_DOWN, ui::DomKey::NONE,
                      ui::EventTimeForNow());
}

ui::KeyEvent KeyA() {
  return CreateKeyEvent(ui::DomKey::FromCharacter('a'), ui::DomCode::US_A);
}

void SetAutocorrectPreferenceTo(Profile& profile,
                                const std::string& engine_id,
                                int autocorrect_level) {
  base::Value::Dict input_method_setting;
  input_method_setting.SetByDottedPath(
      engine_id + ".physicalKeyboardAutoCorrectionLevel", autocorrect_level);
  profile.GetPrefs()->Set(::prefs::kLanguageInputMethodSpecificSettings,
                          base::Value(std::move(input_method_setting)));
}

void EnableAutocorrect(Profile& profile, const std::string& engine_id) {
  SetAutocorrectPreferenceTo(/*profile=*/profile,
                             /*engine_id=*/engine_id,
                             /*autocorrect_level=*/1);
}

void DisableAutocorrect(Profile& profile, const std::string& engine_id) {
  SetAutocorrectPreferenceTo(/*profile=*/profile,
                             /*engine_id=*/engine_id,
                             /*autocorrect_level=*/0);
}

std::string ToString(const AutocorrectSuggestionProvider& provider) {
  switch (provider) {
    case AutocorrectSuggestionProvider::kUsEnglish840:
      return "UsEnglish840";
    case AutocorrectSuggestionProvider::kUsEnglish840V2:
      return "UsEnglish840V2";
    case AutocorrectSuggestionProvider::kUsEnglishDownloaded:
      return "UsEnglishDownloaded";
    case AutocorrectSuggestionProvider::kUsEnglishPrebundled:
      return "UsEnglishPrebundled";
    case AutocorrectSuggestionProvider::kUnknown:
    default:
      return "Unknown";
  }
}

class MockSuggestionHandler : public SuggestionHandlerInterface {
 public:
  MOCK_METHOD(bool,
              DismissSuggestion,
              (int context_id, std::string* error),
              (override));
  MOCK_METHOD(bool,
              SetSuggestion,
              (int context_id,
               const ui::ime::SuggestionDetails& details,
               std::string* error),
              (override));
  MOCK_METHOD(bool,
              AcceptSuggestion,
              (int context_id, std::string* error),
              (override));
  MOCK_METHOD(void,
              OnSuggestionsChanged,
              (const std::vector<std::string>& suggestions),
              (override));
  MOCK_METHOD(bool,
              SetButtonHighlighted,
              (int context_id,
               const ui::ime::AssistiveWindowButton& button,
               bool highlighted,
               std::string* error),
              (override));
  MOCK_METHOD(void,
              ClickButton,
              (const ui::ime::AssistiveWindowButton& button),
              (override));
  MOCK_METHOD(bool,
              AcceptSuggestionCandidate,
              (int context_id,
               const std::u16string& candidate,
               size_t delete_previous_utf16_len,
               bool use_replace_surrounding_text,
               std::string* error),
              (override));
  MOCK_METHOD(bool,
              SetAssistiveWindowProperties,
              (int context_id,
               const AssistiveWindowProperties& assistive_window,
               std::string* error),
              (override));
  MOCK_METHOD(void, Announce, (const std::u16string& text), (override));
};

std::vector<base::test::FeatureRef> DisabledFeatures() {
  return {ash::features::kImeRuleConfig};
}

std::vector<base::test::FeatureRef>
DisabledFeaturesIncludingAutocorrectByDefault() {
  return {ash::features::kImeRuleConfig, ash::features::kAutocorrectByDefault};
}

std::vector<base::test::FeatureRef> RequiredForAutocorrectByDefault() {
  return {ash::features::kAutocorrectByDefault,
          ash::features::kImeFstDecoderParamsUpdate,
          ash::features::kImeUsEnglishModelUpdate};
}

class AutocorrectManagerTest : public testing::Test {
 protected:
  AutocorrectManagerTest()
      : profile_(std::make_unique<TestingProfile>()),
        manager_(&mock_suggestion_handler_, profile_.get()),
        scoped_federated_fake_for_test_(&fake_federated_service_connection_) {
    // Disable ImeRulesConfigs by default.
    feature_list_.InitWithFeatures({}, DisabledFeatures());

    // TODO(b/b/289140140): Refactor FederatedClientManager such that the
    // testing framework for clients can be simpler.
    ash::FederatedClient::InitializeFake();
    federated::FederatedClientManager::UseFakeAshInteractionForTest();

    IMEBridge::Get()->SetInputContextHandler(&mock_ime_input_context_handler_);
    keyboard_client_ = ChromeKeyboardControllerClient::CreateForTest();
    keyboard_client_->set_keyboard_enabled_for_test(false);
  }

  void TearDown() override { ash::FederatedClient::Shutdown(); }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ::base::test::ScopedFeatureList feature_list_;
  MockIMEInputContextHandler mock_ime_input_context_handler_;
  ::testing::StrictMock<MockSuggestionHandler> mock_suggestion_handler_;
  std::unique_ptr<Profile> profile_;
  std::unique_ptr<ChromeKeyboardControllerClient> keyboard_client_;
  AutocorrectManager manager_;
  base::HistogramTester histogram_tester_;

  ash::federated::FakeServiceConnectionImpl fake_federated_service_connection_;
  ash::federated::ScopedFakeServiceConnectionForTest
      scoped_federated_fake_for_test_;
};

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectSetsRangeWhenNoPendingAutocorrectExists) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(0, 3));
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectSetsRangeWhenPendingAutocorrectExists) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.HandleAutocorrect(gfx::Range(4, 7), u"cn", u"can");
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(4, 7));
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectDoesNotSetRangeWhenInputContextIsNull) {
  IMEBridge::Get()->SetInputContextHandler(nullptr);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"cn", u"can");
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range());
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectClearsRangeWithEmptyInputRange) {
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(3, 7),
                                                      base::DoNothing());
  manager_.HandleAutocorrect(gfx::Range(), u"", u"");
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range());
}

TEST_F(AutocorrectManagerTest, OnKeyEventDoesNotClearAutocorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  const ui::KeyEvent key_event =
      CreateKeyEvent(ui::DomKey::FromCharacter('a'), ui::DomCode::US_A);
  EXPECT_FALSE(manager_.OnKeyEvent(key_event));
  EXPECT_FALSE(manager_.OnKeyEvent(key_event));
  EXPECT_FALSE(manager_.OnKeyEvent(key_event));
  EXPECT_FALSE(manager_.OnKeyEvent(key_event));
  EXPECT_FALSE(manager_.OnKeyEvent(key_event));

  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(0, 3));
}

TEST_F(AutocorrectManagerTest,
       TypingFewCharsAfterRangeDoesNotClearAutocorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"the a", gfx::Range(5));
  manager_.OnSurroundingTextChanged(u"the ab", gfx::Range(6));

  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(0, 3));
}

TEST_F(AutocorrectManagerTest,
       TypingEnoughCharsAfterRangeClearsAutocorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"the a", gfx::Range(5));
  manager_.OnSurroundingTextChanged(u"the ab", gfx::Range(6));
  manager_.OnSurroundingTextChanged(u"the abc", gfx::Range(7));

  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range());
}

TEST_F(AutocorrectManagerTest,
       TypingFewCharsBeforeRangeDoesNotClearAutocorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(1, 4), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u" the ", gfx::Range(5));

  // Move cursor to position 0.
  manager_.OnSurroundingTextChanged(u" the ", gfx::Range(0));
  // Add two chars and move the ranges accordingly.
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(2, 5),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"a the ", gfx::Range(1));
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(3, 6),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"ab the ", gfx::Range(2));

  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(3, 6));
}

TEST_F(AutocorrectManagerTest,
       TypingEnoughCharsBeforeRangeClearsAutocorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(1, 4), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u" the ", gfx::Range(5));

  // Move cursor to position 0.
  manager_.OnSurroundingTextChanged(u" the ", gfx::Range(0));
  // Add three chars and move the range accordingly.
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(2, 5),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"a the ", gfx::Range(1));
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(3, 6),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"ab the ", gfx::Range(2));
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(4, 7),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"abc the ", gfx::Range(3));

  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range());
}

TEST_F(AutocorrectManagerTest,
       TypingFewCharsBeforeAndAfterRangeDoesNotClearAutocorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(1, 4), u"teh", u"the");

  manager_.OnSurroundingTextChanged(u" the ", gfx::Range(5));
  manager_.OnSurroundingTextChanged(u" the a", gfx::Range(6));
  manager_.OnSurroundingTextChanged(u" the a", gfx::Range(0));
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(2, 5),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"b the a", gfx::Range(1));

  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(2, 5));
}

TEST_F(AutocorrectManagerTest,
       TypingEnoughCharsAfterAndBeforeRangeClearsAutocorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(1, 4), u"teh", u"the");

  manager_.OnSurroundingTextChanged(u" the ", gfx::Range(5));
  manager_.OnSurroundingTextChanged(u" the a", gfx::Range(6));
  manager_.OnSurroundingTextChanged(u" the a", gfx::Range(0));
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(2, 5),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"b the a", gfx::Range(1));
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(3, 6),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"bc the a", gfx::Range(2));

  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range());
}

TEST_F(AutocorrectManagerTest,
       RemovingCharsCloseToRangeDoesNotClearAutocorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Add characters.
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"the ab", gfx::Range(6));
  // Now remove them.
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(0, 3));
}

TEST_F(AutocorrectManagerTest,
       PastingEnoughCharsAndRemovingFewStillClearsAutocorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Add characters.
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"the ab", gfx::Range(6));
  // Now removing them should not be counted.
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  // Now addition of a new character must trigger the clearance process,
  // to ensure backspaced does not impact the output.
  manager_.OnSurroundingTextChanged(u"the a", gfx::Range(5));

  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range());
}

TEST_F(AutocorrectManagerTest,
       PastingFewCharsBeforeRangeDoesNotClearAutocorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(1, 4), u"teh", u"the");

  manager_.OnSurroundingTextChanged(u" the ", gfx::Range(5));
  manager_.OnSurroundingTextChanged(u" the ", gfx::Range(0));
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(3, 6),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"ab the ", gfx::Range(2));

  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(3, 6));
}

TEST_F(AutocorrectManagerTest,
       PastingEnoughCharsBeforeRangeClearsAutocorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(1, 4), u"teh", u"the");

  manager_.OnSurroundingTextChanged(u" the ", gfx::Range(5));
  manager_.OnSurroundingTextChanged(u" the ", gfx::Range(0));
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(4, 7),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"abc the ", gfx::Range(3));

  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range());
}

TEST_F(AutocorrectManagerTest, OnBlurClearsAutocorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(1, 4), u"teh", u"the");
  manager_.OnBlur();

  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range());
}

TEST_F(AutocorrectManagerTest, OnFocusClearsAutocorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(1, 4), u"teh", u"the");
  manager_.OnFocus(1);

  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range());
}

TEST_F(AutocorrectManagerTest, MovingCursorInsideRangeShowsAssistiveWindow) {
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  AssistiveWindowProperties properties =
      CreateVisibleUndoWindowWithLearnMoreButtonProperties(u"teh", u"the");
  EXPECT_CALL(mock_suggestion_handler_,
              SetAssistiveWindowProperties(_, properties, _));

  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(1));
}

TEST_F(AutocorrectManagerTest,
       MovingCursorInsideRangeDoesNotShowUndoWindowWhenRangeNotValidated) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Range is not validate validated yet. So, no expectation on show undo
  // window call. If it happens, test will fail by StrictMock.
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(1));
}

TEST_F(AutocorrectManagerTest, MovingCursorOutsideRangeHidesAssistiveWindow) {
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  {
    ::testing::InSequence seq;

    AssistiveWindowProperties shown_properties =
        CreateVisibleUndoWindowWithLearnMoreButtonProperties(u"teh", u"the");
    EXPECT_CALL(mock_suggestion_handler_,
                SetAssistiveWindowProperties(_, shown_properties, _));

    AssistiveWindowProperties hidden_properties =
        CreateHiddenUndoWindowProperties();
    EXPECT_CALL(mock_suggestion_handler_,
                SetAssistiveWindowProperties(_, hidden_properties, _));
  }

  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(1));
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
}

TEST_F(AutocorrectManagerTest,
       MovingCursorInsideRangeAndRemovingCharactersHidesAssistiveWindow) {
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  AssistiveWindowProperties properties =
      CreateVisibleUndoWindowWithLearnMoreButtonProperties(u"teh", u"the");

  {
    ::testing::InSequence seq;

    AssistiveWindowProperties shown_properties =
        CreateVisibleUndoWindowWithLearnMoreButtonProperties(u"teh", u"the");
    EXPECT_CALL(mock_suggestion_handler_,
                SetAssistiveWindowProperties(_, shown_properties, _));

    AssistiveWindowProperties hidden_properties =
        CreateHiddenUndoWindowProperties();
    EXPECT_CALL(mock_suggestion_handler_,
                SetAssistiveWindowProperties(_, hidden_properties, _));
  }

  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(2));
  manager_.OnSurroundingTextChanged(u"te ", gfx::Range(1));
}

TEST_F(AutocorrectManagerTest, MovingCursorRetriesPrevFailedUndoWindowHide) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  // Show undo window.
  AssistiveWindowProperties shown_properties =
      CreateVisibleUndoWindowWithLearnMoreButtonProperties(u"teh", u"the");
  EXPECT_CALL(mock_suggestion_handler_,
              SetAssistiveWindowProperties(_, shown_properties, _));
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(1));

  // Accept autocorrect implicitly and make the request to hide the window
  // fail.
  AssistiveWindowProperties hidden_properties =
      CreateHiddenUndoWindowProperties();
  EXPECT_CALL(mock_suggestion_handler_,
              SetAssistiveWindowProperties(_, hidden_properties, _))
      .WillOnce(DoAll(SetArgPointee<2>("Error"), Return(false)))
      .RetiresOnSaturation();
  manager_.OnSurroundingTextChanged(u"the abcd", gfx::Range(8));

  // Now moving cursor should retry hiding autocorrect range.
  EXPECT_CALL(mock_suggestion_handler_,
              SetAssistiveWindowProperties(_, hidden_properties, _));
  manager_.OnSurroundingTextChanged(u"the abcd", gfx::Range(7));
}

TEST_F(AutocorrectManagerTest,
       MovingCursorInsideRangeRetriesPrevFailedUndoWindowHide) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  // Show undo window.
  AssistiveWindowProperties shown_properties =
      CreateVisibleUndoWindowWithLearnMoreButtonProperties(u"teh", u"the");
  EXPECT_CALL(mock_suggestion_handler_,
              SetAssistiveWindowProperties(_, shown_properties, _));
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(1));

  // Make first try to hide the window fail.
  AssistiveWindowProperties hidden_properties =
      CreateHiddenUndoWindowProperties();
  EXPECT_CALL(mock_suggestion_handler_,
              SetAssistiveWindowProperties(_, hidden_properties, _))
      .WillOnce(DoAll(SetArgPointee<2>("Error"), Return(false)));
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  {
    ::testing::InSequence seq;

    // Retries previously failed undo window hiding which now
    // succeed.
    EXPECT_CALL(mock_suggestion_handler_,
                SetAssistiveWindowProperties(_, hidden_properties, _));

    // Showing new undo window.
    EXPECT_CALL(mock_suggestion_handler_,
                SetAssistiveWindowProperties(_, shown_properties, _));
  }

  // Try hiding undo window before showing it again.
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(1));
}

TEST_F(AutocorrectManagerTest,
       ShowingNewUndoWindowStopsRetryingPrevFailedUndoWindowHide) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  // Show the undo window first time.
  AssistiveWindowProperties shown_properties =
      CreateVisibleUndoWindowWithLearnMoreButtonProperties(u"teh", u"the");
  EXPECT_CALL(mock_suggestion_handler_,
              SetAssistiveWindowProperties(_, shown_properties, _));
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(1));

  // Make first two call to hide undo window to fail.
  AssistiveWindowProperties hidden_properties =
      CreateHiddenUndoWindowProperties();
  EXPECT_CALL(mock_suggestion_handler_,
              SetAssistiveWindowProperties(_, hidden_properties, _))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgPointee<2>("Error"), Return(false)))
      .RetiresOnSaturation();

  // Handle a new range to allow triggering an undo window override.
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Show a new undo window.
  EXPECT_CALL(mock_suggestion_handler_,
              SetAssistiveWindowProperties(_, shown_properties, _));
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(1));

  // No retry should be applied to hide undo window as it is overridden.
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(2));
}

TEST_F(AutocorrectManagerTest, FocusChangeHidesUndoWindow) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  // Show a window.
  AssistiveWindowProperties shown_properties =
      CreateVisibleUndoWindowWithLearnMoreButtonProperties(u"teh", u"the");
  EXPECT_CALL(mock_suggestion_handler_,
              SetAssistiveWindowProperties(_, shown_properties, _));
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(1));

  // OnFocus should try hiding the window.
  AssistiveWindowProperties hidden_properties =
      CreateHiddenUndoWindowProperties();
  EXPECT_CALL(mock_suggestion_handler_,
              SetAssistiveWindowProperties(_, hidden_properties, _));

  manager_.OnFocus(1);
}

TEST_F(AutocorrectManagerTest, EscapeHidesUndoWindow) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  // Show a window.
  AssistiveWindowProperties shown_properties =
      CreateVisibleUndoWindowWithLearnMoreButtonProperties(u"teh", u"the");
  EXPECT_CALL(mock_suggestion_handler_,
              SetAssistiveWindowProperties(_, shown_properties, _));
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(1));

  // OnFocus should try hiding the window.
  AssistiveWindowProperties hidden_properties =
      CreateHiddenUndoWindowProperties();
  EXPECT_CALL(mock_suggestion_handler_,
              SetAssistiveWindowProperties(_, hidden_properties, _));

  manager_.OnKeyEvent(CreateKeyEvent(ui::DomKey::NONE, ui::DomCode::ESCAPE));
}

TEST_F(AutocorrectManagerTest, OnFocusRetriesHidingUndoWindow) {
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Show undo window.
  AssistiveWindowProperties shown_properties =
      CreateVisibleUndoWindowWithLearnMoreButtonProperties(u"teh", u"the");
  EXPECT_CALL(mock_suggestion_handler_,
              SetAssistiveWindowProperties(_, shown_properties, _));
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(1));

  // Make it fail to hide window for OnBlur.
  AssistiveWindowProperties hidden_properties =
      CreateHiddenUndoWindowProperties();
  EXPECT_CALL(mock_suggestion_handler_,
              SetAssistiveWindowProperties(_, hidden_properties, _))
      .WillOnce(DoAll(SetArgPointee<2>("Error"), Return(false)));
  manager_.OnBlur();

  // OnFocus must try to hide undo window.
  EXPECT_CALL(mock_suggestion_handler_,
              SetAssistiveWindowProperties(_, hidden_properties, _));
  manager_.OnFocus(1);
}

TEST_F(AutocorrectManagerTest,
       PressingUpArrowKeyHighlightsUndoButtonWhenUndoWindowIsVisible) {
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  {
    ::testing::InSequence seq;

    AssistiveWindowProperties shown_properties =
        CreateVisibleUndoWindowWithLearnMoreButtonProperties(u"teh", u"the");

    EXPECT_CALL(mock_suggestion_handler_,
                SetAssistiveWindowProperties(_, shown_properties, _));

    ui::ime::AssistiveWindowButton undo_button =
        CreateHighlightedUndoButton(u"teh");
    EXPECT_CALL(mock_suggestion_handler_,
                SetButtonHighlighted(_, undo_button, true, _));

    ui::ime::AssistiveWindowButton learn_more_button =
        CreateHighlightedLearnMoreButton();
    EXPECT_CALL(mock_suggestion_handler_,
                SetButtonHighlighted(_, learn_more_button, false, _));
  }

  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(1));
  manager_.OnKeyEvent(CreateKeyEvent(ui::DomKey::NONE, ui::DomCode::ARROW_UP));
}

TEST_F(AutocorrectManagerTest,
       PressingTabKeyHighlightsUndoButtonWhenUndoWindowIsVisible) {
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  {
    ::testing::InSequence seq;

    AssistiveWindowProperties shown_properties =
        CreateVisibleUndoWindowWithLearnMoreButtonProperties(u"teh", u"the");
    ui::ime::AssistiveWindowButton undo_button =
        CreateHighlightedUndoButton(u"teh");
    ui::ime::AssistiveWindowButton learn_more_button =
        CreateHighlightedLearnMoreButton();

    EXPECT_CALL(mock_suggestion_handler_,
                SetAssistiveWindowProperties(_, shown_properties, _));

    EXPECT_CALL(mock_suggestion_handler_,
                SetButtonHighlighted(_, undo_button, true, _));
    EXPECT_CALL(mock_suggestion_handler_,
                SetButtonHighlighted(_, learn_more_button, false, _));
  }

  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(1));
  manager_.OnKeyEvent(CreateKeyEvent(ui::DomKey::NONE, ui::DomCode::TAB));
}

TEST_F(
    AutocorrectManagerTest,
    PressingRightArrowKeyHighlightsLearnMoreButtonWhenUndoButtonIsHighlighted) {
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  {
    ::testing::InSequence seq;

    AssistiveWindowProperties shown_properties =
        CreateVisibleUndoWindowWithLearnMoreButtonProperties(u"teh", u"the");
    ui::ime::AssistiveWindowButton undo_button =
        CreateHighlightedUndoButton(u"teh");
    ui::ime::AssistiveWindowButton learn_more_button =
        CreateHighlightedLearnMoreButton();

    EXPECT_CALL(mock_suggestion_handler_,
                SetAssistiveWindowProperties(_, shown_properties, _));

    EXPECT_CALL(mock_suggestion_handler_,
                SetButtonHighlighted(_, undo_button, true, _));
    EXPECT_CALL(mock_suggestion_handler_,
                SetButtonHighlighted(_, learn_more_button, false, _));

    EXPECT_CALL(mock_suggestion_handler_,
                SetButtonHighlighted(_, undo_button, false, _));
    EXPECT_CALL(mock_suggestion_handler_,
                SetButtonHighlighted(_, learn_more_button, true, _));
  }

  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(1));
  manager_.OnKeyEvent(CreateKeyEvent(ui::DomKey::NONE, ui::DomCode::TAB));
  manager_.OnKeyEvent(
      CreateKeyEvent(ui::DomKey::NONE, ui::DomCode::ARROW_RIGHT));
}

TEST_F(AutocorrectManagerTest,
       PressingEnterKeyHidesUndoWindowWhenButtonIsHighlighted) {
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  {
    ::testing::InSequence seq;

    AssistiveWindowProperties shown_properties =
        CreateVisibleUndoWindowWithLearnMoreButtonProperties(u"teh", u"the");

    EXPECT_CALL(mock_suggestion_handler_,
                SetAssistiveWindowProperties(_, shown_properties, _));

    ui::ime::AssistiveWindowButton undo_button =
        CreateHighlightedUndoButton(u"teh");
    EXPECT_CALL(mock_suggestion_handler_,
                SetButtonHighlighted(_, undo_button, true, _));

    ui::ime::AssistiveWindowButton learn_more_button =
        CreateHighlightedLearnMoreButton();
    EXPECT_CALL(mock_suggestion_handler_,
                SetButtonHighlighted(_, learn_more_button, false, _));

    AssistiveWindowProperties hidden_properties =
        CreateHiddenUndoWindowProperties();
    EXPECT_CALL(mock_suggestion_handler_,
                SetAssistiveWindowProperties(_, hidden_properties, _));
  }

  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(1));
  manager_.OnKeyEvent(CreateKeyEvent(ui::DomKey::NONE, ui::DomCode::ARROW_UP));
  manager_.OnKeyEvent(CreateKeyEvent(ui::DomKey::NONE, ui::DomCode::ENTER));
}

TEST_F(AutocorrectManagerTest,
       PressingEnterKeyHidesUndoWindowWhenLearnMoreButtonIsHighlighted) {
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  {
    ::testing::InSequence seq;

    AssistiveWindowProperties shown_properties =
        CreateVisibleUndoWindowWithLearnMoreButtonProperties(u"teh", u"the");
    AssistiveWindowProperties hidden_properties =
        CreateHiddenUndoWindowProperties();
    ui::ime::AssistiveWindowButton undo_button =
        CreateHighlightedUndoButton(u"teh");
    ui::ime::AssistiveWindowButton learn_more_button =
        CreateHighlightedLearnMoreButton();

    EXPECT_CALL(mock_suggestion_handler_,
                SetAssistiveWindowProperties(_, shown_properties, _));

    EXPECT_CALL(mock_suggestion_handler_,
                SetButtonHighlighted(_, undo_button, true, _));
    EXPECT_CALL(mock_suggestion_handler_,
                SetButtonHighlighted(_, learn_more_button, false, _));

    EXPECT_CALL(mock_suggestion_handler_,
                SetButtonHighlighted(_, undo_button, false, _));
    EXPECT_CALL(mock_suggestion_handler_,
                SetButtonHighlighted(_, learn_more_button, true, _));

    EXPECT_CALL(mock_suggestion_handler_,
                SetAssistiveWindowProperties(_, hidden_properties, _));
    EXPECT_CALL(mock_suggestion_handler_, ClickButton(learn_more_button));
  }

  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(1));
  manager_.OnKeyEvent(CreateKeyEvent(ui::DomKey::NONE, ui::DomCode::ARROW_UP));
  manager_.OnKeyEvent(
      CreateKeyEvent(ui::DomKey::NONE, ui::DomCode::ARROW_RIGHT));
  manager_.OnKeyEvent(CreateKeyEvent(ui::DomKey::NONE, ui::DomCode::ENTER));
}

TEST_F(AutocorrectManagerTest, LearnMoreButtonOnlyShown50Times) {
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  {
    ::testing::InSequence seq;

    AssistiveWindowProperties shown_properties =
        CreateVisibleUndoWindowWithLearnMoreButtonProperties(u"teh", u"the");
    AssistiveWindowProperties hidden_properties =
        CreateHiddenUndoWindowProperties();

    ExpectationSet learn_more_call_series;

    // Expects the learn more button to show and hide for 50 times.
    for (int i = 0; i < 50; ++i) {
      learn_more_call_series +=
          EXPECT_CALL(mock_suggestion_handler_,
                      SetAssistiveWindowProperties(_, shown_properties, _));
      learn_more_call_series +=
          EXPECT_CALL(mock_suggestion_handler_,
                      SetAssistiveWindowProperties(_, hidden_properties, _));
    }
    shown_properties = CreateVisibleUndoWindowProperties(u"teh", u"the");

    // After learn more button is shown 50 times, it expires and never shows
    // again.
    EXPECT_CALL(mock_suggestion_handler_,
                SetAssistiveWindowProperties(_, shown_properties, _))
        .After(learn_more_call_series);
  }

  std::u16string surrounding_text = u"the ";
  manager_.OnSurroundingTextChanged(surrounding_text, gfx::Range(1));

  for (int i = 0; i < 50; ++i) {
    // For each iteration:
    // First inserts "the " into the text input field, and place the cursor at
    // the end of the text.
    surrounding_text += u"the ";
    int cursor_pos = surrounding_text.length();
    manager_.OnSurroundingTextChanged(surrounding_text, gfx::Range(cursor_pos));

    // Then handles an autocorrection that occurs on the text that is just
    // inserted.
    manager_.HandleAutocorrect(gfx::Range(cursor_pos - 4, cursor_pos - 1),
                               u"teh", u"the");

    // Finally, moves the cursor in the middle of the new word to trigger the
    // learn more button to show.
    manager_.OnSurroundingTextChanged(surrounding_text,
                                      gfx::Range(cursor_pos - 3));
  }
}

TEST_F(AutocorrectManagerTest, UndoAutocorrectSingleWordInComposition) {
  ui::FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh ime(nullptr);
  IMEBridge::Get()->SetInputContextHandler(&ime);
  ime.SetFocusedTextInputClient(&fake_text_input_client);

  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  // Move cursor to the middle of 'the' and bring the text into composition.
  fake_text_input_client.SetTextAndSelection(u"the ", gfx::Range(2));
  ime.SetComposingRange(0, 3, {});

  manager_.UndoAutocorrect();

  EXPECT_EQ(fake_text_input_client.text(), u"teh ");
}

TEST_F(AutocorrectManagerTest, UndoAutocorrectDoesNotApplyOnRangeNotValidated) {
  ui::FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh ime(nullptr);
  IMEBridge::Get()->SetInputContextHandler(&ime);
  ime.SetFocusedTextInputClient(&fake_text_input_client);

  // No OnSurroundingTextChanged is called to validate the suggestion.
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Move cursor to the middle of 'the' and bring the text into composition.
  fake_text_input_client.SetTextAndSelection(u"the ", gfx::Range(2));
  ime.SetComposingRange(0, 3, {});

  manager_.UndoAutocorrect();

  // Undo is not applied.
  EXPECT_EQ(fake_text_input_client.text(), u"the ");
}

TEST_F(AutocorrectManagerTest, UndoAutocorrectMultipleWordInComposition) {
  ui::FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  InputMethodAsh ime(nullptr);
  IMEBridge::Get()->SetInputContextHandler(&ime);
  ime.SetFocusedTextInputClient(&fake_text_input_client);

  manager_.HandleAutocorrect(gfx::Range(0, 11), u"helloworld", u"hello world");

  manager_.OnSurroundingTextChanged(u"hello world ", gfx::Range(12));

  // Move cursor to the middle of 'hello' and bring the word into composition.
  fake_text_input_client.SetTextAndSelection(u"hello world ", gfx::Range(2));
  ime.SetComposingRange(0, 5, {});

  manager_.UndoAutocorrect();

  EXPECT_EQ(fake_text_input_client.text(), u"helloworld ");
}

TEST_F(AutocorrectManagerTest, MovingCursorDoesNotAcceptAutocorrect) {
  manager_.HandleAutocorrect(gfx::Range(5, 8), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"abcd the efghij", gfx::Range(4));

  // Move cursor to different positions in one session does not
  // accept or clear the the autocorrect range implicitly.
  manager_.OnSurroundingTextChanged(u"abcd the efghij", gfx::Range(15));
  manager_.OnSurroundingTextChanged(u"abcd the efghij", gfx::Range(0));
  manager_.OnSurroundingTextChanged(u"abcd the efghij", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"abcd the efghij", gfx::Range(9));

  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(5, 8));
}

TEST_F(AutocorrectManagerTest,
       InsertingFewCharsDoesNotRecordMetricsForPendingAutocorrect) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  // Add characters.
  manager_.OnSurroundingTextChanged(u" the b", gfx::Range(6));
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       InsertingEnoughCharsRecordsMetricWhenAcceptingAutocorrect) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Add characters.
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"c the b", gfx::Range(7));
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/1,
                              /*cleared_underline=*/0);
}

TEST_F(
    AutocorrectManagerTest,
    InsertingEnoughCharsRecordsMetricWhenAcceptingAutocorrectEnabledByDefault) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures({features::kAutocorrectByDefault},
                                 DisabledFeatures());
  manager_.OnActivate(kUsEnglishEngineId);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Add characters.
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"c the b", gfx::Range(7));

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/1,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/0,
                              /*invalid_range=*/0,
                              /*enabled_by_default=*/true);
}

TEST_F(AutocorrectManagerTest,
       RemovingCharsDoesNotRecordMetricsForPendingAutocorrect) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Add characters.
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"the ab", gfx::Range(6));
  // Now remove them.
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       InsertingCharsRecordsMetricsWhenClearingAutocorrect) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"the a", gfx::Range(5));

  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u" the b", gfx::Range(6));
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/1);
}

TEST_F(
    AutocorrectManagerTest,
    InsertingCharsRecordsMetricsWhenClearingAutocorrectWhenEnabledByDefault) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures({features::kAutocorrectByDefault},
                                 DisabledFeatures());
  manager_.OnActivate(kUsEnglishEngineId);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"the a", gfx::Range(5));

  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u" the b", gfx::Range(6));

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/1,
                              /*exited_text_field_with_underline=*/0,
                              /*invalid_range=*/0,
                              /*enabled_by_default=*/true);
}

TEST_F(AutocorrectManagerTest,
       InsertingCharsDoesNotRecordsMetricsWhenSetRangeFails) {
  // Disable autocorrect.
  mock_ime_input_context_handler_.set_autocorrect_enabled(false);

  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"the a", gfx::Range(5));

  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u" the b", gfx::Range(6));
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/0,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       OnSurroundingCallDoesNotRecordMetricsWhenClearingInvalidRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Range not validated yet.
  manager_.OnSurroundingTextChanged(u"t ", gfx::Range(2));

  // Clear range.
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());

  // Empty range is received and ignored because the new suggestion is still
  // not validated.
  manager_.OnSurroundingTextChanged(u"th ", gfx::Range(3));

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       OnSurroundingCallRecordsMetricsWhenClearingValidatedRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Validate the range.
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  // Clear the range.
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  // Process the cleared range ('the' is mutated to implicitly reject it).
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/1);
}

TEST_F(AutocorrectManagerTest,
       OnSurroundingRecordsMetricsCorrectlyForNullInputContext) {
  // Create a pending autocorrect range.
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Make Input context null.
  IMEBridge::Get()->SetInputContextHandler(nullptr);
  // Null input context invalidates the previous range even if rules are
  // triggered to accept the range.
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"the abc", gfx::Range(7));
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline*/ 0,
                              /*invalid_range*/ 1);
}

TEST_F(
    AutocorrectManagerTest,
    OnSurroundingRecordsMetricsCorrectlyForNullInputContextWhenEnabledByDefault) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures({features::kAutocorrectByDefault},
                                 DisabledFeatures());
  manager_.OnActivate(kUsEnglishEngineId);
  // Create a pending autocorrect range.
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Make Input context null.
  IMEBridge::Get()->SetInputContextHandler(nullptr);
  // Null input context invalidates the previous range even if rules are
  // triggered to accept the range.
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"the abc", gfx::Range(7));

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline*/ 0,
                              /*invalid_range*/ 1,
                              /*enabled_by_default=*/true);
}

TEST_F(AutocorrectManagerTest,
       MovingCursorDoesNotRecordMetricsForPendingAutocorrect) {
  manager_.HandleAutocorrect(gfx::Range(4, 7), u"teh", u"the");

  manager_.OnSurroundingTextChanged(u"abc the def", gfx::Range(8));
  manager_.OnSurroundingTextChanged(u"abc the def", gfx::Range(1));
  manager_.OnSurroundingTextChanged(u"abc the def", gfx::Range(10));
  manager_.OnSurroundingTextChanged(u"abc the def", gfx::Range(3));
  manager_.OnSurroundingTextChanged(u"abc the def", gfx::Range(8));
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       MovingCursorToRangeStartRecordsMetricsForShownUndoWindow) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // This suppresses strict mock.
  EXPECT_CALL(mock_suggestion_handler_, SetAssistiveWindowProperties(_, _, _));

  manager_.OnSurroundingTextChanged(u"the", gfx::Range(0));
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/1, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       MovingCursorToRangeEndRecordsMetricsForShownUndoWindow) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // This suppresses strict mock.
  EXPECT_CALL(mock_suggestion_handler_, SetAssistiveWindowProperties(_, _, _));

  // Moving cursor inside the range does not increase window_shown.
  manager_.OnSurroundingTextChanged(u"the", gfx::Range(3));
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/1, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       MovingCursorToRangeEndRecordsMetricsForShownUndoWindowEnableByDefault) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures({features::kAutocorrectByDefault},
                                 DisabledFeatures());
  manager_.OnActivate(kUsEnglishEngineId);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // This suppresses strict mock.
  EXPECT_CALL(mock_suggestion_handler_, SetAssistiveWindowProperties(_, _, _));
  // Moving cursor inside the range does not increase window_shown.
  manager_.OnSurroundingTextChanged(u"the", gfx::Range(3));

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/1, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/0,
                              /*invalid_range=*/0,
                              /*enabled_by_default=*/true);
}

TEST_F(AutocorrectManagerTest,
       KeepingCursorInsideRangeRecordsMetricsForShownUndoWindowOnce) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // This suppresses strict mock.
  EXPECT_CALL(mock_suggestion_handler_, SetAssistiveWindowProperties(_, _, _));

  manager_.OnSurroundingTextChanged(u"the", gfx::Range(0));
  manager_.OnSurroundingTextChanged(u"the", gfx::Range(3));
  manager_.OnSurroundingTextChanged(u"the", gfx::Range(2));
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/1, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       MovingCursorOutThenInsideRangeDoesNotRecordsMetricsTwice) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // The function is called two times for show and one time for hide.
  // This suppresses strict mock.
  EXPECT_CALL(mock_suggestion_handler_, SetAssistiveWindowProperties(_, _, _))
      .Times(3);

  // Moving cursor first inside range, then outside the range and then again
  // back to the range increments the metric for shown window twice.
  manager_.OnSurroundingTextChanged(u"the", gfx::Range(1));
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"the", gfx::Range(3));
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/1, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       OnKeyEventDoesNotRecordMetricsForAcceptingOrClearingAutocorrect) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  const ui::KeyEvent key_event =
      CreateKeyEvent(ui::DomKey::FromCharacter('a'), ui::DomCode::US_A);
  manager_.OnKeyEvent(key_event);
  manager_.OnKeyEvent(key_event);
  manager_.OnKeyEvent(key_event);
  manager_.OnKeyEvent(key_event);
  manager_.OnKeyEvent(key_event);

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       OnKeyEventDoesNotRecordMetricsAfterClearingRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  const ui::KeyEvent key_event =
      CreateKeyEvent(ui::DomKey::FromCharacter('a'), ui::DomCode::US_A);
  manager_.OnKeyEvent(key_event);
  manager_.OnKeyEvent(key_event);
  manager_.OnKeyEvent(key_event);
  manager_.OnKeyEvent(key_event);
  manager_.OnKeyEvent(key_event);

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest, OnBlurRecordsMetricsWhenClearingRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnBlur();
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/1);
}

TEST_F(AutocorrectManagerTest,
       OnBlurRecordsMetricsWhenClearingRangeEnabledByDefault) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures({features::kAutocorrectByDefault},
                                 DisabledFeatures());
  manager_.OnActivate(kUsEnglishEngineId);

  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnBlur();

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/1,
                              /*invalid_range=*/0,
                              /*enabled_by_default=*/true);
}

TEST_F(AutocorrectManagerTest,
       OnBlurDoesNoRecordMetricsWhenNoPendingAutocorrectExists) {
  manager_.OnBlur();
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/0,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       OnBlurDoesNoRecordMetricsWhenInputContextIsNull) {
  // Make Input context null.
  IMEBridge::Get()->SetInputContextHandler(nullptr);
  manager_.OnBlur();
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/0,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/0);
}

TEST_F(AutocorrectManagerTest, OnFocusRecordsMetricsWhenClearingRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnFocus(1);
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/1);
}

TEST_F(AutocorrectManagerTest,
       OnFocusDoesNoRecordMetricsWhenNoPendingAutocorrectExists) {
  manager_.OnFocus(1);
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/0,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectRecordsMetricsWhenNoPendingAutocorrectExists) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectDoesNotRecordMetricsWhenSetRangeFails) {
  // Disable autocorrect.
  mock_ime_input_context_handler_.set_autocorrect_enabled(false);

  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/0,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectRecordsMetricsWhenAcceptingPendingAutocorrect) {
  // Create a pending autocorrect range.
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  // Create a new autocorrect range.
  manager_.HandleAutocorrect(gfx::Range(4, 7), u"cn", u"can");

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/2,
                              /*reverted=*/0, /*accepted=*/1,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectRecordsMetricsWithPendingRangeAndFailedSetRange) {
  // Enable Autocorrect.
  mock_ime_input_context_handler_.set_autocorrect_enabled(true);

  // Create a pending autocorrect range.
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  // Disable autocorrect.
  mock_ime_input_context_handler_.set_autocorrect_enabled(false);

  // Create a new autocorrect range.
  manager_.HandleAutocorrect(gfx::Range(4, 7), u"cn", u"can");

  // This case should not happen in practice, but the expected result
  // is counting the first autocorrect as rejected given there is no way
  // to know if it was accepted.
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/1);
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectRecordsMetricsWhenClearingPendingAutocorrect) {
  // Create a pending autocorrect range.
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  // Clear the previous autocorrect range.
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());

  // Handle a new range.
  manager_.HandleAutocorrect(gfx::Range(4, 7), u"cn", u"can");

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/2,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/1);
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectRecordsMetricsCorrectlyForNullInputContext) {
  // Create a pending autocorrect range.
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  // Make Input context null.
  IMEBridge::Get()->SetInputContextHandler(nullptr);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // The pending range must be counted as invalid, but `underlined` metric must
  // not be incremented with the empty input context.
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline*/ 0,
                              /*invalid_range*/ 1);

  // When there is no pending autocorrect range, nothing is incremented.
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline*/ 0,
                              /*invalid_range*/ 1);
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectRecordsMetricsForEmptyInputAndNoPendingRange) {
  // Empty input range does not change autocorrect range.
  manager_.HandleAutocorrect(gfx::Range(), u"", u"");
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/0,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectRecordsMetricsForEmptyInputAndPendingRange) {
  // When there is a pending autocorrect, empty input range makes the pending
  // to be counted as accepted.
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  manager_.HandleAutocorrect(gfx::Range(), u"", u"");
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/1,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       HandleAutocorrectRecordsMetricsForEmptyInputAndClearedPending) {
  // When there is a pending autocorrect, but cleared beforehand,
  // empty input range makes the pending to be counted as cleared.
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  manager_.HandleAutocorrect(gfx::Range(), u"", u"");
  manager_.OnSurroundingTextChanged(u"", gfx::Range(0));

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/1);
}

TEST_F(AutocorrectManagerTest,
       InsertingCharsDoesNotRecordMetricsForStaleAndAcceptedRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"the abc", gfx::Range(7));
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/1,
                              /*cleared_underline=*/0);

  // Set stale autocorrect range.
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(0, 3),
                                                      base::DoNothing());

  // Adding extra character should not double count.
  manager_.OnSurroundingTextChanged(u"the abcd", gfx::Range(8));
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/1,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       InsertingCharsDoesNotRecordMetricsForStaleAndClearedRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/1);

  // Set stale cleared range.
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(0, 3),
                                                      base::DoNothing());

  manager_.OnSurroundingTextChanged(u"the abc", gfx::Range(7));
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/1);
}

TEST_F(AutocorrectManagerTest,
       AutocorrectHandlerDoesNotRecordMetricsForStaleAutocorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"the abc", gfx::Range(7));
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/1,
                              /*cleared_underline=*/0);

  // Set stale autocorrect range.
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(0, 3),
                                                      base::DoNothing());

  // Handle a new autocorrect and ensure the metric is not increased twice.
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/2,
                              /*reverted=*/0, /*accepted=*/1,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       OnBlurDoesNotRecordMetricsForStaleAutocorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"the abc", gfx::Range(7));
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/1,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/0);

  // Set stale autocorrect range.
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(0, 3),
                                                      base::DoNothing());

  // Handle a new autocorrect and ensure the metric is not increase twice.
  manager_.OnBlur();
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/1,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       OnFocusDoesNotRecordMetricsForStaleAutocorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  // Accept autocorrect implicitly.
  manager_.OnBlur();
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/1);

  // Set stale autocorrect range.
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(0, 3),
                                                      base::DoNothing());

  // Handle a new autocorrect and ensure the metric is not increase twice.
  manager_.OnFocus(1);
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/1);
}

TEST_F(AutocorrectManagerTest, ImplicitAcceptanceClearsAutocorrectRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 7), u"smeone", u"someone");

  manager_.OnSurroundingTextChanged(u"someone ", gfx::Range(8));

  // Ensure range is as expected.
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(0, 7));

  // Implicitly accept autocorrect by three character insertion.
  manager_.OnSurroundingTextChanged(u"someone abc", gfx::Range(11));

  EXPECT_TRUE(mock_ime_input_context_handler_.GetAutocorrectRange().is_empty());
}

TEST_F(AutocorrectManagerTest, AsyncDelayDoesNotMakeAutocorrectAccepted) {
  // To commit autocorrect, a delete operation is first applied then an insert.
  // In the case of async delay, the surrounding text related to each of these
  // operations might be received after handling the range but needs to be
  // ignored by validation process.

  manager_.HandleAutocorrect(gfx::Range(0, 7), u"smeone", u"someone");

  // Late surrounding text related to IME delete.
  manager_.OnSurroundingTextChanged(u"s ", gfx::Range(1));
  // Late surrounding text related to IME insert.
  manager_.OnSurroundingTextChanged(u"someone ", gfx::Range(8));

  // Autocorrect range is not cleared by the stale surrounding text.
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(0, 7));
}

TEST_F(AutocorrectManagerTest,
       ImplicitAcceptanceRecordsMetricsAndIgnoresAsyncStaleData) {
  manager_.HandleAutocorrect(gfx::Range(0, 7), u"smeone", u"someone");

  // Late surrounding text related to IME delete.
  manager_.OnSurroundingTextChanged(u"s ", gfx::Range(1));
  // Late surrounding text related to IME insert.
  manager_.OnSurroundingTextChanged(u"someone ", gfx::Range(8));
  // User adds two characters.
  manager_.OnSurroundingTextChanged(u"someone ab", gfx::Range(10));
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(0, 7));

  // Third character, implicitly accepts autocorrect.
  manager_.OnSurroundingTextChanged(u"someone abc", gfx::Range(11));

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/1,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       RecordMetricsForVkWhenVkWasVisibleAtUnderlineTime) {
  // VK is visible at the time of suggesting an autocorrect.
  keyboard_client_->set_keyboard_enabled_for_test(true);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // To suppress strict mock.
  EXPECT_CALL(mock_suggestion_handler_, SetAssistiveWindowProperties(_, _, _));

  // VK is made hidden, but still the metrics need to be recorded for VK
  // given VK was visible at underline time.
  keyboard_client_->set_keyboard_enabled_for_test(false);
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(1));

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/true,
                              /*window_shown=*/1, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       DoesNotRecordMetricsForVkWhenVkWasNotVisibleAtUnderlineTime) {
  // VK is not visible at the time of suggesting an autocorrect.
  keyboard_client_->set_keyboard_enabled_for_test(false);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // To suppress strict mock.
  EXPECT_CALL(mock_suggestion_handler_, SetAssistiveWindowProperties(_, _, _));

  // VK is made visible, but still metrics must not be recorded for VK
  // as it was not visible at the time of underline.
  keyboard_client_->set_keyboard_enabled_for_test(true);
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(1));

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/1, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest, UndoRecordsMetricsAfterRevert) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  manager_.UndoAutocorrect();
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/1, /*accepted=*/0,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/0);
}

TEST_F(AutocorrectManagerTest, UndoRecordsMetricsAfterRevertEnableByDefault) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures({features::kAutocorrectByDefault},
                                 DisabledFeatures());
  manager_.OnActivate(kUsEnglishEngineId);

  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  manager_.UndoAutocorrect();
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/1, /*accepted=*/0,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/0,
                              /*invalid_range=*/0,
                              /*enabled_by_default=*/true);
}

TEST_F(AutocorrectManagerTest, HandleAutocorrectRecordsMetricsWhenVkIsVisible) {
  keyboard_client_->set_keyboard_enabled_for_test(true);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/true,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest, ExitingTextFieldRecordsMetricsWhenVkIsVisible) {
  keyboard_client_->set_keyboard_enabled_for_test(true);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnBlur();
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/true,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/1);
}

TEST_F(AutocorrectManagerTest,
       AcceptingAutocorrectRecordsMetricsWhenVkIsVisible) {
  keyboard_client_->set_keyboard_enabled_for_test(true);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  // Implicitly accept autocorrect
  manager_.OnSurroundingTextChanged(u"the abc", gfx::Range(7));
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/true,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/1,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/0);
}

TEST_F(AutocorrectManagerTest, ThreeValidationFailuresDoesNotClearRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Three validation failures.
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));

  // Range is not cleared.
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(0, 3));
}

TEST_F(AutocorrectManagerTest, FourValidationFailuresClearsRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Four validation failure.
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));

  EXPECT_TRUE(mock_ime_input_context_handler_.GetAutocorrectRange().is_empty());
}

TEST_F(AutocorrectManagerTest, InvalidRangeFailsValidationAndClearsRange) {
  manager_.HandleAutocorrect(gfx::Range(2, 5), u"teh", u"the");

  // Four validation failure because the range is invalid.
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  EXPECT_TRUE(mock_ime_input_context_handler_.GetAutocorrectRange().is_empty());
}

TEST_F(AutocorrectManagerTest,
       FourValidationFailuresRecordsMetricsForInvalidRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Four validation failure.
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/0,
                              /*invalid_range*/ 1);
}

TEST_F(AutocorrectManagerTest, UndoRecordsMetricsWhenVkIsVisible) {
  keyboard_client_->set_keyboard_enabled_for_test(true);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  manager_.UndoAutocorrect();
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/true,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/1, /*accepted=*/0,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/0);
}

TEST_F(AutocorrectManagerTest,
       ClearingAutocorrectRecordsMetricsWhenVkIsVisible) {
  keyboard_client_->set_keyboard_enabled_for_test(true);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));

  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/true,
                              /*window_shown=*/0, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/1,
                              /*exited_text_field_with_underline=*/0);
}

TEST_F(AutocorrectManagerTest, ConsistentAsyncDelayClearsRangeIncorrectly) {
  // This is a case that if happens in practice will cause the Autocorrect
  // logic to fail. Here, imagine that with any user input, autocorrect range
  // is updated in the text input client with a delay. So, validation never
  // succeeds because the range is always one step old.

  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Each OnSurroundingTextChanged is received with stale autocorrect range
  // belonging to the previous state.
  manager_.OnSurroundingTextChanged(u"athe ", gfx::Range(1));
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(1, 4),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"abthe ", gfx::Range(2));
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(2, 5),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"abcthe ", gfx::Range(3));
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(3, 6),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"abcdthe ", gfx::Range(4));

  // Expect that the validation fails.
  EXPECT_TRUE(mock_ime_input_context_handler_.GetAutocorrectRange().is_empty());
}

TEST_F(AutocorrectManagerTest,
       AutocorrectIsIncorrectlyAcceptedWhenStaleRangeAndNewSuggestionMatch) {
  // This is a case that has no solution to prevent and can result in
  // an incorrect autocorrect behaviour when happening.

  manager_.HandleAutocorrect(gfx::Range(0, 4), u"ths", u"this");
  manager_.HandleAutocorrect(gfx::Range(5, 9), u"ths", u"this");

  // Set stale autocorrect range.
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(0, 4),
                                                      base::DoNothing());

  // Surrounding text changed is stale (updated one is 'this this').
  // The range is now validated because stale range matches the new suggestion.
  manager_.OnSurroundingTextChanged(u"this t", gfx::Range(6));

  // Updated surrounding text counts three insertions.
  manager_.OnSurroundingTextChanged(u"this this ", gfx::Range(10));

  // The range is accepted incorrectly.
  EXPECT_TRUE(mock_ime_input_context_handler_.GetAutocorrectRange().is_empty());
}

TEST_F(AutocorrectManagerTest, InvalidOriginalTextArgClearsRange) {
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(0, 3),
                                                      base::DoNothing());
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(0, 3));

  // Original text is empty and invalid.
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"", u"the");
  EXPECT_TRUE(mock_ime_input_context_handler_.GetAutocorrectRange().is_empty());
}

TEST_F(AutocorrectManagerTest, InvalidOriginalTextArgDoesNotRecordMetrics) {
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(0, 3),
                                                      base::DoNothing());
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(0, 3));

  manager_.HandleAutocorrect(gfx::Range(0, 3), u"", u"the");
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/0,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/0);
}

TEST_F(AutocorrectManagerTest, EmptySuggestedTextArgClearsRange) {
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(0, 3),
                                                      base::DoNothing());
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(0, 3));

  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"");
  EXPECT_TRUE(mock_ime_input_context_handler_.GetAutocorrectRange().is_empty());
}

TEST_F(AutocorrectManagerTest, EmptySuggestedTextArgDoesNotRecordMetrics) {
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(0, 3),
                                                      base::DoNothing());
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(0, 3));

  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"");
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/0,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/0);
}

TEST_F(AutocorrectManagerTest, RangeAndSuggestionMismatchDoesNotRecordMetrics) {
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(0, 3),
                                                      base::DoNothing());
  EXPECT_EQ(mock_ime_input_context_handler_.GetAutocorrectRange(),
            gfx::Range(0, 3));

  manager_.HandleAutocorrect(gfx::Range(0, 4), u"teh", u"");
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/false,
                              /*window_shown=*/0, /*underlined=*/0,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0,
                              /*exited_text_field_with_underline=*/0);
}

TEST_F(AutocorrectManagerTest, ShowingUndoWindowRecordsMetricsWhenVkIsVisible) {
  keyboard_client_->set_keyboard_enabled_for_test(true);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // This suppresses strict mock.
  EXPECT_CALL(mock_suggestion_handler_, SetAssistiveWindowProperties(_, _, _));

  manager_.OnSurroundingTextChanged(u"the", gfx::Range(0));
  ExpectAutocorrectHistograms(histogram_tester_, /*visible_vk=*/true,
                              /*window_shown=*/1, /*underlined=*/1,
                              /*reverted=*/0, /*accepted=*/0,
                              /*cleared_underline=*/0);
}

TEST_F(AutocorrectManagerTest, RecordQualityBreakdownForAccentChange) {
  manager_.HandleAutocorrect(gfx::Range(0, 8), u"francais", u"français");

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"français ", gfx::Range(9));
  manager_.OnSurroundingTextChanged(u"français abc", gfx::Range(12));

  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestionChangedAccent, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestionMutatedLetters, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkAcceptedHistName,
      AutocorrectQualityBreakdown::kOriginalTextIsAscii, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestionResolved, 1);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2QualityPkAcceptedHistName,
                                     4);
}

TEST_F(AutocorrectManagerTest, RecordQualityBreakdownForWordSplit) {
  manager_.HandleAutocorrect(gfx::Range(0, 11), u"helloworld", u"hello world");

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"hello world ", gfx::Range(12));
  manager_.OnSurroundingTextChanged(u"hello world abc", gfx::Range(15));

  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestionSplittedWord, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestionInsertedLetters, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkAcceptedHistName,
      AutocorrectQualityBreakdown::kOriginalTextIsAscii, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestedTextIsAscii, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestionResolved, 1);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2QualityPkAcceptedHistName,
                                     5);
}

TEST_F(AutocorrectManagerTest, RecordQualityBreakdownForRemovingLetters) {
  manager_.HandleAutocorrect(gfx::Range(0, 8), u"françaisss", u"français");

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"français ", gfx::Range(9));
  manager_.OnSurroundingTextChanged(u"français abc", gfx::Range(12));

  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestionRemovedLetters, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestionResolved, 1);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2QualityPkAcceptedHistName,
                                     2);
}

TEST_F(AutocorrectManagerTest, RecordQualityBreakdownForCapitalizedWorld) {
  manager_.HandleAutocorrect(gfx::Range(0, 1), u"i", u"I");

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"I ", gfx::Range(2));
  manager_.OnSurroundingTextChanged(u"I have", gfx::Range(6));

  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestionCapitalizedWord, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkAcceptedHistName,
      AutocorrectQualityBreakdown::kOriginalTextIsAscii, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestedTextIsAscii, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestionMutatedLetters, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestionChangeLetterCases, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestionResolved, 1);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2QualityPkAcceptedHistName,
                                     6);
}

TEST_F(AutocorrectManagerTest, RecordQualityBreakdownForLowerCasedLetter) {
  manager_.HandleAutocorrect(gfx::Range(0, 8), u"Français", u"français");

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"français ", gfx::Range(9));
  manager_.OnSurroundingTextChanged(u"français abc", gfx::Range(12));

  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestionLowerCasedWord, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestionResolved, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestionMutatedLetters, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestionChangeLetterCases, 1);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2QualityPkAcceptedHistName,
                                     4);
}

TEST_F(AutocorrectManagerTest, RecordQualityBreakdownForDefaultPkAccepted) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures({features::kAutocorrectByDefault},
                                 DisabledFeatures());
  manager_.OnActivate(kUsEnglishEngineId);
  manager_.HandleAutocorrect(gfx::Range(0, 8), u"françaisss", u"français");

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"français ", gfx::Range(9));
  manager_.OnSurroundingTextChanged(u"français abc", gfx::Range(12));

  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityDefaultPkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestionRemovedLetters, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityDefaultPkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestionResolved, 1);
  histogram_tester_.ExpectTotalCount(
      kAutocorrectV2QualityDefaultPkAcceptedHistName, 2);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2QualityPkAcceptedHistName,
                                     2);
}

TEST_F(AutocorrectManagerTest, RecordQualityBreakdownForVkAccepted) {
  keyboard_client_->set_keyboard_enabled_for_test(true);
  manager_.HandleAutocorrect(gfx::Range(0, 8), u"françaisss", u"français");

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"français ", gfx::Range(9));
  manager_.OnSurroundingTextChanged(u"français abc", gfx::Range(12));

  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityVkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestionRemovedLetters, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityVkAcceptedHistName,
      AutocorrectQualityBreakdown::kSuggestionResolved, 1);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2QualityVkAcceptedHistName,
                                     2);
}

TEST_F(AutocorrectManagerTest, RecordQualityBreakdownForVkRejected) {
  keyboard_client_->set_keyboard_enabled_for_test(true);
  manager_.HandleAutocorrect(gfx::Range(0, 8), u"françaisss", u"français");

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"français ", gfx::Range(9));
  // Clear range.
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"franças ", gfx::Range(8));

  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityVkRejectedHistName,
      AutocorrectQualityBreakdown::kSuggestionRemovedLetters, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityVkRejectedHistName,
      AutocorrectQualityBreakdown::kSuggestionResolved, 1);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2QualityVkRejectedHistName,
                                     2);
}

TEST_F(AutocorrectManagerTest, RecordQualityBreakdownForPkRejected) {
  manager_.HandleAutocorrect(gfx::Range(0, 8), u"françaisss", u"français");

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"français ", gfx::Range(9));
  // Clear range.
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"franças ", gfx::Range(8));

  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkRejectedHistName,
      AutocorrectQualityBreakdown::kSuggestionRemovedLetters, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityPkRejectedHistName,
      AutocorrectQualityBreakdown::kSuggestionResolved, 1);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2QualityPkRejectedHistName,
                                     2);
  histogram_tester_.ExpectTotalCount(
      kAutocorrectV2QualityDefaultPkRejectedHistName, 0);
}

TEST_F(AutocorrectManagerTest, RecordQualityBreakdownDefaultForPkRejected) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures({features::kAutocorrectByDefault},
                                 DisabledFeatures());
  manager_.OnActivate(kUsEnglishEngineId);
  manager_.HandleAutocorrect(gfx::Range(0, 8), u"françaisss", u"français");

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"français ", gfx::Range(9));
  // Clear range.
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"franças ", gfx::Range(8));

  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityDefaultPkRejectedHistName,
      AutocorrectQualityBreakdown::kSuggestionRemovedLetters, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2QualityDefaultPkRejectedHistName,
      AutocorrectQualityBreakdown::kSuggestionResolved, 1);
  histogram_tester_.ExpectTotalCount(
      kAutocorrectV2QualityDefaultPkRejectedHistName, 2);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2QualityPkRejectedHistName,
                                     2);
}

TEST_F(AutocorrectManagerTest, RecordDistanceMetricForVkAccepted) {
  keyboard_client_->set_keyboard_enabled_for_test(true);
  manager_.HandleAutocorrect(gfx::Range(0, 4), u"cafe", u"cafè");
  // (|cafe|-1) * MAX_LENGTH + (|{'e'->'è'}| - 1)
  int expected_value = (4 - 1) * 30 + (1 - 1);

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"cafè ", gfx::Range(5));
  manager_.OnSurroundingTextChanged(u"cafè abc", gfx::Range(8));

  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2Distance2dVkAcceptedHistName, expected_value, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2DistanceSuggestedVkAcceptedHistName, 4, 1);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2Distance2dVkAcceptedHistName,
                                     1);
  histogram_tester_.ExpectTotalCount(
      kAutocorrectV2DistanceSuggestedVkAcceptedHistName, 1);
}

TEST_F(AutocorrectManagerTest, RecordDistanceMetricForPkAccepted) {
  manager_.HandleAutocorrect(gfx::Range(0, 1), u"i", u"I");
  //  (|i|-1) * MAX_LENGTH + (|{'i'->'I'}| - 1)
  int expected_value = (1 - 1) * 30 + (1 - 1);

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"I ", gfx::Range(2));
  manager_.OnSurroundingTextChanged(u"I abc", gfx::Range(5));

  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2Distance2dPkAcceptedHistName, expected_value, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2DistanceSuggestedPkAcceptedHistName, 1, 1);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2Distance2dPkAcceptedHistName,
                                     1);
  histogram_tester_.ExpectTotalCount(
      kAutocorrectV2DistanceSuggestedPkAcceptedHistName, 1);
}

TEST_F(AutocorrectManagerTest, RecordDistanceMetricForVkRejected) {
  keyboard_client_->set_keyboard_enabled_for_test(true);
  manager_.HandleAutocorrect(gfx::Range(0, 12), u"ecauserthy", u"because they");
  //  (|ecauserthy|-1) * MAX_LENGTH + (|{''->'b'}, {'r'->' '}, {''->'e'}| - 1)
  int expected_value = (10 - 1) * 30 + (3 - 1);

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"because they ", gfx::Range(13));
  // Clear range.
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"because ", gfx::Range(8));

  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2Distance2dVkRejectedHistName, expected_value, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2DistanceSuggestedVkRejectedHistName, 12, 1);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2Distance2dVkRejectedHistName,
                                     1);
  histogram_tester_.ExpectTotalCount(
      kAutocorrectV2DistanceSuggestedVkRejectedHistName, 1);
}

TEST_F(AutocorrectManagerTest, RecordDistanceMetricForPkRejected) {
  manager_.HandleAutocorrect(
      gfx::Range(0, 42),
      u"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      u"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  //  (min(|<word1>|, MAX_LENGTH) - 1) * MAX_LENGTH +
  //                          (min(<number of changes>, MAX_LENGTH) - 1)
  int expected_value = (30 - 1) * 30 + (30 - 1);

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(
      u"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb ", gfx::Range(43));
  // Clear range.
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(
      u"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa ",
      gfx::Range(55));

  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2Distance2dPkRejectedHistName, expected_value, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2DistanceSuggestedPkRejectedHistName, 30, 1);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2Distance2dPkRejectedHistName,
                                     1);
  histogram_tester_.ExpectTotalCount(
      kAutocorrectV2DistanceSuggestedPkRejectedHistName, 1);
}

TEST_F(AutocorrectManagerTest, DistanceMetricNoChange) {
  manager_.HandleAutocorrect(gfx::Range(0, 9), u"no change", u"no change");
  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"no change ", gfx::Range(10));
  manager_.OnSurroundingTextChanged(u"no change abc", gfx::Range(13));
  manager_.HandleAutocorrect(gfx::Range(0, 9), u"", u"not empty");

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"not empty ", gfx::Range(10));
  manager_.OnSurroundingTextChanged(u"not empty abc", gfx::Range(13));

  histogram_tester_.ExpectTotalCount(
      kAutocorrectV2DistanceSuggestedPkAcceptedHistName, 0);
  histogram_tester_.ExpectTotalCount(
      kAutocorrectV2DistanceSuggestedPkAcceptedHistName, 0);
}

TEST_F(AutocorrectManagerTest, RecordDistanceMetricNoOverlap) {
  manager_.HandleAutocorrect(gfx::Range(0, 32), u"aaaa",
                             u"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  //  (|aaaa|-1) * MAX_LENGTH + (min(<number of changes>, MAX_LENGTH) - 1)
  int expected_value1 = (4 - 1) * 30 + (30 - 1);
  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb ",
                                    gfx::Range(33));
  manager_.OnSurroundingTextChanged(u"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb abc",
                                    gfx::Range(35));

  manager_.HandleAutocorrect(gfx::Range(0, 4), u"aaaaa", u"aaaa");
  //  (|aaaaa|-1) * MAX_LENGTH + (|{'a'->''}| - 1)
  int expected_value2 = (5 - 1) * 30 + (1 - 1);
  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"aaaa ", gfx::Range(5));
  manager_.OnSurroundingTextChanged(u"aaaa abc", gfx::Range(8));

  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2Distance2dPkAcceptedHistName, expected_value1, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2Distance2dPkAcceptedHistName, expected_value2, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2DistanceSuggestedPkAcceptedHistName, 30, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2DistanceSuggestedPkAcceptedHistName, 4, 1);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2Distance2dPkAcceptedHistName,
                                     2);
  histogram_tester_.ExpectTotalCount(
      kAutocorrectV2DistanceSuggestedPkAcceptedHistName, 2);
}

TEST_F(AutocorrectManagerTest, RecordDistanceMetricAlmostMaxLength) {
  manager_.HandleAutocorrect(gfx::Range(0, 1), u"iiiiiiiiiiiiiiiiiiiiiiiiiiiii",
                             u"I");
  // (|<word1>| - 1) * MAX_LENGTH + (<number of changes> - 1)
  int expected_value = (29 - 1) * 30 + (29 - 1);

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"I ", gfx::Range(2));
  manager_.OnSurroundingTextChanged(u"I abc", gfx::Range(5));

  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2Distance2dPkAcceptedHistName, expected_value, 1);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2Distance2dPkAcceptedHistName,
                                     1);
}

TEST_F(AutocorrectManagerTest, RecordRejectionForPkUndoWithKeyboard) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  {
    ::testing::InSequence seq;

    AssistiveWindowProperties shown_properties =
        CreateVisibleUndoWindowWithLearnMoreButtonProperties(u"teh", u"the");

    EXPECT_CALL(mock_suggestion_handler_,
                SetAssistiveWindowProperties(_, shown_properties, _));

    ui::ime::AssistiveWindowButton undo_button =
        CreateHighlightedUndoButton(u"teh");
    EXPECT_CALL(mock_suggestion_handler_,
                SetButtonHighlighted(_, undo_button, true, _));

    ui::ime::AssistiveWindowButton learn_more_button =
        CreateHighlightedLearnMoreButton();
    EXPECT_CALL(mock_suggestion_handler_,
                SetButtonHighlighted(_, learn_more_button, false, _));

    AssistiveWindowProperties hidden_properties =
        CreateHiddenUndoWindowProperties();
    EXPECT_CALL(mock_suggestion_handler_,
                SetAssistiveWindowProperties(_, hidden_properties, _));
  }

  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(1));
  manager_.OnKeyEvent(CreateKeyEvent(ui::DomKey::NONE, ui::DomCode::ARROW_UP));
  manager_.OnKeyEvent(CreateKeyEvent(ui::DomKey::NONE, ui::DomCode::ENTER));

  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2PkRejectionHistName,
      AutocorrectRejectionBreakdown::kUndoWithKeyboard, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2PkRejectionHistName,
      AutocorrectRejectionBreakdown::kSuggestionRejected, 1);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2PkRejectionHistName, 2);
}

TEST_F(AutocorrectManagerTest, RecordRejectionForPkUndoControlZ) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  manager_.OnKeyEvent(PressKeyWithCtrl(ui::DomCode::US_Z));
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));

  histogram_tester_.ExpectBucketCount(kAutocorrectV2PkRejectionHistName,
                                      AutocorrectRejectionBreakdown::kUndoCtrlZ,
                                      1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2PkRejectionHistName,
      AutocorrectRejectionBreakdown::kSuggestionRejected, 1);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2PkRejectionHistName, 2);
}

TEST_F(AutocorrectManagerTest, RecordRejectionForPkControlBackspace) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  manager_.OnKeyEvent(PressKeyWithCtrl(ui::DomCode::BACKSPACE));
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"", gfx::Range(0));

  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2PkRejectionHistName,
      AutocorrectRejectionBreakdown::kRejectedCtrlBackspace, 1);
  histogram_tester_.ExpectBucketCount(
      kAutocorrectV2PkRejectionHistName,
      AutocorrectRejectionBreakdown::kSuggestionRejected, 1);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2PkRejectionHistName, 2);
}

TEST_F(
    AutocorrectManagerTest,
    IsNotDisabledWhenNoSuggestionProviderAndAutocorrectByDefaultFlagIsDisabled) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/DisabledFeaturesIncludingAutocorrectByDefault());

  manager_.OnActivate(kUsEnglishEngineId);
  manager_.OnFocus(kContextId);

  EXPECT_FALSE(manager_.DisabledByInvalidExperimentContext());
}

TEST_F(AutocorrectManagerTest,
       IsNotDisabledWhenNoSuggestionProviderAndUserExplicitlyEnablesPref) {
  EnableAutocorrect(/*profile=*/*profile_, /*engine_id=*/kUsEnglishEngineId);
  feature_list_.Reset();
  feature_list_.InitWithFeatures(RequiredForAutocorrectByDefault(),
                                 DisabledFeatures());

  manager_.OnActivate(kUsEnglishEngineId);
  manager_.OnFocus(kContextId);

  EXPECT_FALSE(manager_.DisabledByInvalidExperimentContext());
}

TEST_F(AutocorrectManagerTest,
       IsNotDisabledWhenNoSuggestionProviderAndUserExplicitlyDisablesPref) {
  DisableAutocorrect(/*profile=*/*profile_, /*engine_id=*/kUsEnglishEngineId);
  feature_list_.Reset();
  feature_list_.InitWithFeatures(RequiredForAutocorrectByDefault(),
                                 DisabledFeatures());

  manager_.OnActivate(kUsEnglishEngineId);
  manager_.OnFocus(kContextId);

  EXPECT_FALSE(manager_.DisabledByInvalidExperimentContext());
}

TEST_F(AutocorrectManagerTest,
       IsNotDisabledWhenNoSuggestionProviderAndVkIsVisible) {
  keyboard_client_->set_keyboard_enabled_for_test(true);
  feature_list_.Reset();
  feature_list_.InitWithFeatures(RequiredForAutocorrectByDefault(),
                                 DisabledFeatures());

  manager_.OnActivate(kUsEnglishEngineId);
  manager_.OnFocus(kContextId);

  EXPECT_FALSE(manager_.DisabledByInvalidExperimentContext());
}

class NotDisabledByInvalidSuggestionProvider
    : public AutocorrectManagerTest,
      public testing::WithParamInterface<AutocorrectSuggestionProvider> {};

INSTANTIATE_TEST_SUITE_P(
    AutocorrectManagerTest,
    NotDisabledByInvalidSuggestionProvider,
    testing::ValuesIn<AutocorrectSuggestionProvider>({
        AutocorrectSuggestionProvider::kUnknown,
        AutocorrectSuggestionProvider::kUsEnglishPrebundled,
        AutocorrectSuggestionProvider::kUsEnglishDownloaded,
        AutocorrectSuggestionProvider::kUsEnglish840,
        AutocorrectSuggestionProvider::kUsEnglish840V2,
    }),
    [](const testing::TestParamInfo<AutocorrectSuggestionProvider> info) {
      return ToString(info.param);
    });

TEST_P(NotDisabledByInvalidSuggestionProvider,
       WhenAutocorrectByDefaultFlagDisabled) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/DisabledFeaturesIncludingAutocorrectByDefault());
  const AutocorrectSuggestionProvider& provider = GetParam();

  manager_.OnActivate(kUsEnglishEngineId);
  manager_.OnFocus(kContextId);
  manager_.OnConnectedToSuggestionProvider(provider);

  EXPECT_FALSE(manager_.DisabledByInvalidExperimentContext());
}

TEST_P(NotDisabledByInvalidSuggestionProvider, WhenUserExplicitlyEnablesPref) {
  const AutocorrectSuggestionProvider& provider = GetParam();
  EnableAutocorrect(/*profile=*/*profile_, /*engine_id=*/kUsEnglishEngineId);
  feature_list_.Reset();
  feature_list_.InitWithFeatures(RequiredForAutocorrectByDefault(),
                                 DisabledFeatures());

  manager_.OnActivate(kUsEnglishEngineId);
  manager_.OnFocus(kContextId);
  manager_.OnConnectedToSuggestionProvider(provider);

  EXPECT_FALSE(manager_.DisabledByInvalidExperimentContext());
}

TEST_P(NotDisabledByInvalidSuggestionProvider, WhenUserExplicitlyDisablesPref) {
  const AutocorrectSuggestionProvider& provider = GetParam();
  DisableAutocorrect(/*profile=*/*profile_, /*engine_id=*/kUsEnglishEngineId);
  feature_list_.Reset();
  feature_list_.InitWithFeatures(RequiredForAutocorrectByDefault(),
                                 DisabledFeatures());

  manager_.OnActivate(kUsEnglishEngineId);
  manager_.OnFocus(kContextId);
  manager_.OnConnectedToSuggestionProvider(provider);

  EXPECT_FALSE(manager_.DisabledByInvalidExperimentContext());
}

TEST_P(NotDisabledByInvalidSuggestionProvider, WhenVkIsVisible) {
  const AutocorrectSuggestionProvider& provider = GetParam();
  keyboard_client_->set_keyboard_enabled_for_test(true);
  feature_list_.Reset();
  feature_list_.InitWithFeatures(RequiredForAutocorrectByDefault(),
                                 DisabledFeatures());

  manager_.OnActivate(kUsEnglishEngineId);
  manager_.OnFocus(kContextId);
  manager_.OnConnectedToSuggestionProvider(provider);

  EXPECT_FALSE(manager_.DisabledByInvalidExperimentContext());
}

TEST_F(AutocorrectManagerTest,
       IsDisabledWhenNoSuggestionProviderAndUserInDefaultBucket) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(RequiredForAutocorrectByDefault(),
                                 DisabledFeatures());

  manager_.OnActivate(kUsEnglishEngineId);
  manager_.OnFocus(kContextId);

  EXPECT_TRUE(manager_.DisabledByInvalidExperimentContext());
}

TEST_F(AutocorrectManagerTest,
       IsDisabledWhenMissingNewModelParametersButEn840Enabled) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures({ash::features::kAutocorrectByDefault},
                                 {ash::features::kImeFstDecoderParamsUpdate,
                                  ash::features::kImeRuleConfig});

  manager_.OnActivate(kUsEnglishEngineId);
  manager_.OnFocus(kContextId);
  manager_.OnConnectedToSuggestionProvider(
      AutocorrectSuggestionProvider::kUsEnglish840);

  EXPECT_TRUE(manager_.DisabledByInvalidExperimentContext());
}

class EnabledByValidSuggestionProvider
    : public AutocorrectManagerTest,
      public testing::WithParamInterface<AutocorrectSuggestionProvider> {};

INSTANTIATE_TEST_SUITE_P(
    AutocorrectManagerTest,
    EnabledByValidSuggestionProvider,
    testing::ValuesIn<>({
        AutocorrectSuggestionProvider::kUsEnglish840,
        AutocorrectSuggestionProvider::kUsEnglish840V2,
    }),
    [](const testing::TestParamInfo<AutocorrectSuggestionProvider> info) {
      return ToString(info.param);
    });

TEST_P(EnabledByValidSuggestionProvider,
       IsNotDisabledWhenUserInDefaultBucketAndValidSuggestionProviderUsed) {
  const AutocorrectSuggestionProvider& provider = GetParam();
  feature_list_.Reset();
  feature_list_.InitWithFeatures(RequiredForAutocorrectByDefault(),
                                 DisabledFeatures());

  manager_.OnActivate(kUsEnglishEngineId);
  manager_.OnFocus(kContextId);
  manager_.OnConnectedToSuggestionProvider(provider);

  EXPECT_FALSE(manager_.DisabledByInvalidExperimentContext());
}

class DisabledByInvalidSuggestionProvider
    : public AutocorrectManagerTest,
      public testing::WithParamInterface<AutocorrectSuggestionProvider> {};

INSTANTIATE_TEST_SUITE_P(
    AutocorrectManagerTest,
    DisabledByInvalidSuggestionProvider,
    testing::ValuesIn<>({
        AutocorrectSuggestionProvider::kUnknown,
        AutocorrectSuggestionProvider::kUsEnglishPrebundled,
        AutocorrectSuggestionProvider::kUsEnglishDownloaded,
    }),
    [](const testing::TestParamInfo<AutocorrectSuggestionProvider> info) {
      return ToString(info.param);
    });

TEST_P(DisabledByInvalidSuggestionProvider, WhenUserInDefaultExperiment) {
  const AutocorrectSuggestionProvider& provider = GetParam();
  feature_list_.Reset();
  feature_list_.InitWithFeatures(RequiredForAutocorrectByDefault(),
                                 DisabledFeatures());

  manager_.OnActivate(kUsEnglishEngineId);
  manager_.OnFocus(kContextId);
  manager_.OnConnectedToSuggestionProvider(provider);

  EXPECT_TRUE(manager_.DisabledByInvalidExperimentContext());
}

struct RejectCase {
  std::string test_name;
  bool vk_enabled;
  std::string histogram_name;
};

class RejectMetric : public AutocorrectManagerTest,
                     public testing::WithParamInterface<RejectCase> {};

TEST_P(RejectMetric, RecordRejectionForMetricOther) {
  const RejectCase& test_case = GetParam();
  keyboard_client_->set_keyboard_enabled_for_test(test_case.vk_enabled);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // Accept autocorrect implicitly.
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));
  // Clear range.
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));

  histogram_tester_.ExpectBucketCount(
      test_case.histogram_name, AutocorrectRejectionBreakdown::kRejectionOther,
      1);
  histogram_tester_.ExpectBucketCount(
      test_case.histogram_name,
      AutocorrectRejectionBreakdown::kSuggestionRejected, 1);
  histogram_tester_.ExpectTotalCount(test_case.histogram_name, 2);
}

TEST_P(RejectMetric, RecordRejectionForVkUndo) {
  const RejectCase& test_case = GetParam();
  keyboard_client_->set_keyboard_enabled_for_test(test_case.vk_enabled);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  manager_.UndoAutocorrect();

  histogram_tester_.ExpectBucketCount(
      test_case.histogram_name,
      AutocorrectRejectionBreakdown::kUndoWithoutKeyboard, 1);
  histogram_tester_.ExpectBucketCount(
      test_case.histogram_name,
      AutocorrectRejectionBreakdown::kSuggestionRejected, 1);
  histogram_tester_.ExpectTotalCount(test_case.histogram_name, 2);
}

TEST_P(RejectMetric, RecordRejectionForBackspace) {
  const RejectCase& test_case = GetParam();
  keyboard_client_->set_keyboard_enabled_for_test(test_case.vk_enabled);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  if (!test_case.vk_enabled) {
    manager_.OnKeyEvent(
        CreateKeyEvent(ui::DomKey::NONE, ui::DomCode::BACKSPACE));
  }
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"th", gfx::Range(2));

  histogram_tester_.ExpectBucketCount(
      test_case.histogram_name,
      AutocorrectRejectionBreakdown::kRejectedBackspace,
      test_case.vk_enabled ? 0 : 1);
  histogram_tester_.ExpectBucketCount(
      test_case.histogram_name, AutocorrectRejectionBreakdown::kRemovedLetters,
      1);
  histogram_tester_.ExpectBucketCount(
      test_case.histogram_name,
      AutocorrectRejectionBreakdown::kSuggestionRejected, 1);
  histogram_tester_.ExpectTotalCount(test_case.histogram_name,
                                     test_case.vk_enabled ? 2 : 3);
}

TEST_P(RejectMetric, RecordRejectionForFullSelectionTyping) {
  const RejectCase& test_case = GetParam();
  keyboard_client_->set_keyboard_enabled_for_test(test_case.vk_enabled);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(0, 3));
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"new ", gfx::Range(4));

  histogram_tester_.ExpectBucketCount(
      test_case.histogram_name,
      AutocorrectRejectionBreakdown::kRejectedTypingFull, 1);
  histogram_tester_.ExpectBucketCount(
      test_case.histogram_name,
      AutocorrectRejectionBreakdown::kSuggestionRejected, 1);
  histogram_tester_.ExpectTotalCount(test_case.histogram_name, 2);
}

TEST_P(RejectMetric, RecordRejectionForPartialSelectionTyping) {
  const RejectCase& test_case = GetParam();
  keyboard_client_->set_keyboard_enabled_for_test(test_case.vk_enabled);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(0, 2));
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"newe ", gfx::Range(3));

  histogram_tester_.ExpectBucketCount(
      test_case.histogram_name,
      AutocorrectRejectionBreakdown::kRejectedTypingPartial, 1);
  histogram_tester_.ExpectBucketCount(
      test_case.histogram_name,
      AutocorrectRejectionBreakdown::kSuggestionRejected, 1);
  histogram_tester_.ExpectTotalCount(test_case.histogram_name, 2);
}

TEST_P(RejectMetric, RecordRejectionForFullWithExternalSelectionTyping) {
  const RejectCase& test_case = GetParam();
  keyboard_client_->set_keyboard_enabled_for_test(test_case.vk_enabled);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(0, 4));
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"new ", gfx::Range(4));

  histogram_tester_.ExpectBucketCount(
      test_case.histogram_name,
      AutocorrectRejectionBreakdown::kRejectedTypingFullWithExternal, 1);
  histogram_tester_.ExpectBucketCount(
      test_case.histogram_name,
      AutocorrectRejectionBreakdown::kSuggestionRejected, 1);
  histogram_tester_.ExpectTotalCount(test_case.histogram_name, 2);
}

TEST_P(RejectMetric, RecordRejectionForPartialWithExternalSelectionTyping) {
  const RejectCase& test_case = GetParam();
  keyboard_client_->set_keyboard_enabled_for_test(test_case.vk_enabled);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(2, 4));
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"thnew", gfx::Range(5));

  histogram_tester_.ExpectBucketCount(
      test_case.histogram_name,
      AutocorrectRejectionBreakdown::kRejectedTypingPartialWithExternal, 1);
  histogram_tester_.ExpectBucketCount(
      test_case.histogram_name,
      AutocorrectRejectionBreakdown::kSuggestionRejected, 1);
  histogram_tester_.ExpectTotalCount(test_case.histogram_name, 2);
}

TEST_P(RejectMetric, RecordRejectionForTypingNoSelection) {
  const RejectCase& test_case = GetParam();
  keyboard_client_->set_keyboard_enabled_for_test(test_case.vk_enabled);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  {
    ::testing::InSequence seq;

    AssistiveWindowProperties shown_properties =
        CreateVisibleUndoWindowWithLearnMoreButtonProperties(u"teh", u"the");

    EXPECT_CALL(mock_suggestion_handler_,
                SetAssistiveWindowProperties(_, shown_properties, _));

    AssistiveWindowProperties hidden_properties =
        CreateHiddenUndoWindowProperties();
    EXPECT_CALL(mock_suggestion_handler_,
                SetAssistiveWindowProperties(_, hidden_properties, _));
  }

  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(2));
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(),
                                                      base::DoNothing());
  manager_.OnSurroundingTextChanged(u"thee ", gfx::Range(3));

  histogram_tester_.ExpectBucketCount(
      test_case.histogram_name,
      AutocorrectRejectionBreakdown::kRejectedTypingNoSelection, 1);
  histogram_tester_.ExpectBucketCount(
      test_case.histogram_name,
      AutocorrectRejectionBreakdown::kSuggestionRejected, 1);
  histogram_tester_.ExpectTotalCount(test_case.histogram_name, 2);
}

INSTANTIATE_TEST_SUITE_P(
    AutocorrectManagerTest,
    RejectMetric,
    testing::ValuesIn<RejectCase>({
        {"VkEnabled",
         /*vk_enabled=*/true,
         /*histogram_name=*/kAutocorrectV2VkRejectionHistName},
        {"VkDisabled",
         /*vk_enabled=*/false,
         /*histogram_name=*/kAutocorrectV2PkRejectionHistName},
    }),
    [](const testing::TestParamInfo<RejectCase> info) {
      return info.param.test_name;
    });

struct PkUserPrefCase {
  std::string test_name;
  std::string engine_id;
  std::optional<int> autocorrect_level;
  AutocorrectPreference expected_pref;
};

class PkEnglishUserPreferenceMetric
    : public AutocorrectManagerTest,
      public testing::WithParamInterface<PkUserPrefCase> {};

INSTANTIATE_TEST_SUITE_P(
    AutocorrectManagerTest,
    PkEnglishUserPreferenceMetric,
    testing::ValuesIn<PkUserPrefCase>({
        {"UsEnglishEnabled",
         /*engine_id=*/kUsEnglishEngineId,
         /*autocorrect_level=*/1,
         /*expected_pref=*/AutocorrectPreference::kEnabled},
        {"UsEnglishDisabled",
         /*engine_id=*/kUsEnglishEngineId,
         /*autocorrect_level=*/0,
         /*expected_pref=*/AutocorrectPreference::kDisabled},
        {"UsEnglishDefault",
         /*engine_id=*/kUsEnglishEngineId,
         /*autocorrect_level=*/std::nullopt,
         /*expected_pref=*/AutocorrectPreference::kEnabledByDefault},
    }),
    [](const testing::TestParamInfo<PkUserPrefCase> info) {
      return info.param.test_name;
    });

TEST_P(PkEnglishUserPreferenceMetric, IsNotRecordedWhenKeyEventNotEncountered) {
  const PkUserPrefCase& test_case = GetParam();
  if (test_case.autocorrect_level) {
    SetAutocorrectPreferenceTo(
        /*profile=*/*profile_,
        /*engine_id=*/test_case.engine_id,
        /*autocorrect_level=*/*test_case.autocorrect_level);
  }

  manager_.OnActivate(test_case.engine_id);
  manager_.OnFocus(kContextId);

  histogram_tester_.ExpectTotalCount(kAutocorrectV2PkUserPreferenceAll, 0);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2PkUserPreferenceEnglish, 0);
}

TEST_P(PkEnglishUserPreferenceMetric, IsNotRecordedWhenKeyEventCameFromTheVk) {
  const PkUserPrefCase& test_case = GetParam();
  keyboard_client_->set_keyboard_enabled_for_test(true);
  if (test_case.autocorrect_level) {
    SetAutocorrectPreferenceTo(
        /*profile=*/*profile_,
        /*engine_id=*/test_case.engine_id,
        /*autocorrect_level=*/*test_case.autocorrect_level);
  }

  manager_.OnActivate(test_case.engine_id);
  manager_.OnFocus(kContextId);
  manager_.OnKeyEvent(KeyA());

  histogram_tester_.ExpectTotalCount(kAutocorrectV2PkUserPreferenceAll, 0);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2PkUserPreferenceEnglish, 0);
}

TEST_P(PkEnglishUserPreferenceMetric,
       IsRecordedCorrectlyAfterOnFocusThenOnKeyEvent) {
  const PkUserPrefCase& test_case = GetParam();
  if (test_case.autocorrect_level) {
    SetAutocorrectPreferenceTo(
        /*profile=*/*profile_,
        /*engine_id=*/test_case.engine_id,
        /*autocorrect_level=*/*test_case.autocorrect_level);
  }

  manager_.OnActivate(test_case.engine_id);
  manager_.OnFocus(kContextId);
  manager_.OnKeyEvent(KeyA());

  histogram_tester_.ExpectTotalCount(kAutocorrectV2PkUserPreferenceAll, 1);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2PkUserPreferenceEnglish, 1);
  histogram_tester_.ExpectBucketCount(kAutocorrectV2PkUserPreferenceAll,
                                      test_case.expected_pref, 1);
  histogram_tester_.ExpectBucketCount(kAutocorrectV2PkUserPreferenceEnglish,
                                      test_case.expected_pref, 1);
}

TEST_P(PkEnglishUserPreferenceMetric,
       IsRecordedForEveryOnFocusAndOnKeyEventSequence) {
  const PkUserPrefCase& test_case = GetParam();
  if (test_case.autocorrect_level) {
    SetAutocorrectPreferenceTo(
        /*profile=*/*profile_,
        /*engine_id=*/test_case.engine_id,
        /*autocorrect_level=*/*test_case.autocorrect_level);
  }

  manager_.OnActivate(test_case.engine_id);
  manager_.OnFocus(kContextId);
  manager_.OnKeyEvent(KeyA());
  manager_.OnFocus(kContextId);
  manager_.OnKeyEvent(KeyA());
  manager_.OnKeyEvent(KeyA());

  histogram_tester_.ExpectTotalCount(kAutocorrectV2PkUserPreferenceAll, 2);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2PkUserPreferenceEnglish, 2);
  histogram_tester_.ExpectBucketCount(kAutocorrectV2PkUserPreferenceAll,
                                      test_case.expected_pref, 2);
  histogram_tester_.ExpectBucketCount(kAutocorrectV2PkUserPreferenceEnglish,
                                      test_case.expected_pref, 2);
}

class PkAllLangsUserPreferenceMetric
    : public AutocorrectManagerTest,
      public testing::WithParamInterface<PkUserPrefCase> {};

INSTANTIATE_TEST_SUITE_P(
    AutocorrectManagerTest,
    PkAllLangsUserPreferenceMetric,
    testing::ValuesIn<PkUserPrefCase>({
        {"UsInternationalEnabled",
         /*engine_id=*/kUsInternationalEngineId,
         /*autocorrect_level=*/1,
         /*expected_pref=*/AutocorrectPreference::kEnabled},
        {"UsInternationalDisabled",
         /*engine_id=*/kUsInternationalEngineId,
         /*autocorrect_level=*/0,
         /*expected_pref=*/AutocorrectPreference::kDisabled},
        {"UsInternationalDefault",
         /*engine_id=*/kUsInternationalEngineId,
         /*autocorrect_level=*/std::nullopt,
         /*expected_pref=*/AutocorrectPreference::kDefault},

        {"SpainSpanishEnabled",
         /*engine_id=*/kSpainSpanishEngineId,
         /*autocorrect_level=*/1,
         /*expected_pref=*/AutocorrectPreference::kEnabled},
        {"SpainSpanishDisabled",
         /*engine_id=*/kSpainSpanishEngineId,
         /*autocorrect_level=*/0,
         /*expected_pref=*/AutocorrectPreference::kDisabled},
        {"SpainSpanishDefault",
         /*engine_id=*/kSpainSpanishEngineId,
         /*autocorrect_level=*/std::nullopt,
         /*expected_pref=*/AutocorrectPreference::kDefault},

        {"LatinAmericaSpanishEnabled",
         /*engine_id=*/kLatinAmericaSpanishEngineId,
         /*autocorrect_level=*/1,
         /*expected_pref=*/AutocorrectPreference::kEnabled},
        {"LatinAmericaSpanishDisabled",
         /*engine_id=*/kLatinAmericaSpanishEngineId,
         /*autocorrect_level=*/0,
         /*expected_pref=*/AutocorrectPreference::kDisabled},
        {"LatinAmericaSpanishDefault",
         /*engine_id=*/kLatinAmericaSpanishEngineId,
         /*autocorrect_level=*/std::nullopt,
         /*expected_pref=*/AutocorrectPreference::kDefault},

        {"BrazilPortugeseEnabled",
         /*engine_id=*/kBrazilPortugeseEngineId,
         /*autocorrect_level=*/1,
         /*expected_pref=*/AutocorrectPreference::kEnabled},
        {"BrazilPortugeseDisabled",
         /*engine_id=*/kBrazilPortugeseEngineId,
         /*autocorrect_level=*/0,
         /*expected_pref=*/AutocorrectPreference::kDisabled},
        {"BrazilPortugeseDefault",
         /*engine_id=*/kBrazilPortugeseEngineId,
         /*autocorrect_level=*/std::nullopt,
         /*expected_pref=*/AutocorrectPreference::kDefault},

        {"FranceFrenchEnabled",
         /*engine_id=*/kFranceFrenchEngineId,
         /*autocorrect_level=*/1,
         /*expected_pref=*/AutocorrectPreference::kEnabled},
        {"FranceFrenchDisabled",
         /*engine_id=*/kFranceFrenchEngineId,
         /*autocorrect_level=*/0,
         /*expected_pref=*/AutocorrectPreference::kDisabled},
        {"FranceFrenchDefault",
         /*engine_id=*/kFranceFrenchEngineId,
         /*autocorrect_level=*/std::nullopt,
         /*expected_pref=*/AutocorrectPreference::kDefault},
    }),
    [](const testing::TestParamInfo<PkUserPrefCase> info) {
      return info.param.test_name;
    });

TEST_P(PkAllLangsUserPreferenceMetric,
       IsRecordedCorrectlyAfterOnFocusThenOnKeyEvent) {
  const PkUserPrefCase& test_case = GetParam();
  if (test_case.autocorrect_level) {
    SetAutocorrectPreferenceTo(
        /*profile=*/*profile_,
        /*engine_id=*/test_case.engine_id,
        /*autocorrect_level=*/*test_case.autocorrect_level);
  }

  manager_.OnActivate(test_case.engine_id);
  manager_.OnFocus(kContextId);
  manager_.OnKeyEvent(KeyA());

  histogram_tester_.ExpectTotalCount(kAutocorrectV2PkUserPreferenceAll, 1);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2PkUserPreferenceEnglish, 0);
  histogram_tester_.ExpectBucketCount(kAutocorrectV2PkUserPreferenceAll,
                                      test_case.expected_pref, 1);
}

TEST_F(AutocorrectManagerTest,
       RecordsCorrectMetricForEnabledByDefaultWithEnglish) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures({features::kAutocorrectByDefault},
                                 DisabledFeatures());

  manager_.OnActivate(kUsEnglishEngineId);
  manager_.OnFocus(kContextId);
  manager_.OnKeyEvent(KeyA());

  histogram_tester_.ExpectTotalCount(kAutocorrectV2PkUserPreferenceAll, 1);
  histogram_tester_.ExpectTotalCount(kAutocorrectV2PkUserPreferenceEnglish, 1);
  histogram_tester_.ExpectBucketCount(kAutocorrectV2PkUserPreferenceAll,
                                      AutocorrectPreference::kEnabledByDefault,
                                      1);
  histogram_tester_.ExpectBucketCount(kAutocorrectV2PkUserPreferenceEnglish,
                                      AutocorrectPreference::kEnabledByDefault,
                                      1);
}

struct PkEnabledByDefaultCase {
  std::string test_name;
  std::string engine_id;
  std::optional<int> autocorrect_level;
  AutocorrectPreference preference_before;
  AutocorrectPreference preference_after;
};

class PkEnabledByDefaultTest
    : public AutocorrectManagerTest,
      public testing::WithParamInterface<PkEnabledByDefaultCase> {};

TEST_P(PkEnabledByDefaultTest, ItIsEnabledByDefaultWhenFlagIsEnabled) {
  const PkEnabledByDefaultCase& test_case = GetParam();
  PrefService* prefs = profile_->GetPrefs();
  feature_list_.Reset();
  feature_list_.InitWithFeatures({features::kAutocorrectByDefault},
                                 DisabledFeatures());
  if (test_case.autocorrect_level) {
    SetAutocorrectPreferenceTo(*profile_, test_case.engine_id,
                               *test_case.autocorrect_level);
  }

  auto before = GetPhysicalKeyboardAutocorrectPref(*prefs, test_case.engine_id);
  manager_.OnActivate(test_case.engine_id);
  auto after = GetPhysicalKeyboardAutocorrectPref(*prefs, test_case.engine_id);

  EXPECT_EQ(before, test_case.preference_before);
  EXPECT_EQ(after, test_case.preference_after);
}

TEST_P(PkEnabledByDefaultTest, ItIsNotEnabledByDefaultWhenFlagIsDisabled) {
  const PkEnabledByDefaultCase& test_case = GetParam();
  PrefService* prefs = profile_->GetPrefs();
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/DisabledFeaturesIncludingAutocorrectByDefault());
  if (test_case.autocorrect_level) {
    SetAutocorrectPreferenceTo(*profile_, kUsEnglishEngineId,
                               *test_case.autocorrect_level);
  }

  auto before = GetPhysicalKeyboardAutocorrectPref(*prefs, kUsEnglishEngineId);
  manager_.OnActivate(test_case.engine_id);
  auto after = GetPhysicalKeyboardAutocorrectPref(*prefs, kUsEnglishEngineId);

  // Because the flag is disabled then the preference should not change.
  EXPECT_EQ(before, test_case.preference_before);
  EXPECT_EQ(after, test_case.preference_before);
}

INSTANTIATE_TEST_SUITE_P(
    AutocorrectManagerTest,
    PkEnabledByDefaultTest,
    testing::ValuesIn<PkEnabledByDefaultCase>({
        PkEnabledByDefaultCase{
            "EnglishDefaultToEnabledByDefault",
            /*engine_id=*/kUsEnglishEngineId,
            /*autocorrect_level=*/std::nullopt,
            /*preference_before=*/AutocorrectPreference::kDefault,
            /*preference_after=*/AutocorrectPreference::kEnabledByDefault},
        PkEnabledByDefaultCase{
            "EnglishDisabledRemainsDisabled",
            /*engine_id=*/kUsEnglishEngineId,
            /*autocorrect_level=*/0,
            /*preference_before=*/AutocorrectPreference::kDisabled,
            /*preference_after=*/AutocorrectPreference::kDisabled},
        PkEnabledByDefaultCase{
            "EnglishEnabledRemainsEnabled",
            /*engine_id=*/kUsEnglishEngineId,
            /*autocorrect_level=*/1,
            /*preference_before=*/AutocorrectPreference::kEnabled,
            /*preference_after=*/AutocorrectPreference::kEnabled},
        PkEnabledByDefaultCase{
            "EnglishAggressiveRemainsEnabled",
            /*engine_id=*/kUsEnglishEngineId,
            /*autocorrect_level=*/2,
            /*preference_before=*/AutocorrectPreference::kEnabled,
            /*preference_after=*/AutocorrectPreference::kEnabled},

        PkEnabledByDefaultCase{
            "PortugeseDefaultRemainsDefault",
            /*engine_id=*/kBrazilPortugeseEngineId,
            /*autocorrect_level=*/std::nullopt,
            /*preference_before=*/AutocorrectPreference::kDefault,
            /*preference_after=*/AutocorrectPreference::kDefault},
        PkEnabledByDefaultCase{
            "PortugeseDisabledRemainsDisabled",
            /*engine_id=*/kBrazilPortugeseEngineId,
            /*autocorrect_level=*/0,
            /*preference_before=*/AutocorrectPreference::kDisabled,
            /*preference_after=*/AutocorrectPreference::kDisabled},
        PkEnabledByDefaultCase{
            "PortugeseEnabledRemainsEnabled",
            /*engine_id=*/kBrazilPortugeseEngineId,
            /*autocorrect_level=*/1,
            /*preference_before=*/AutocorrectPreference::kEnabled,
            /*preference_after=*/AutocorrectPreference::kEnabled},
        PkEnabledByDefaultCase{
            "PortugeseAggressiveRemainsEnabled",
            /*engine_id=*/kBrazilPortugeseEngineId,
            /*autocorrect_level=*/2,
            /*preference_before=*/AutocorrectPreference::kEnabled,
            /*preference_after=*/AutocorrectPreference::kEnabled},

        PkEnabledByDefaultCase{
            "SpainSpanishDefaultRemainsDefault",
            /*engine_id=*/kSpainSpanishEngineId,
            /*autocorrect_level=*/std::nullopt,
            /*preference_before=*/AutocorrectPreference::kDefault,
            /*preference_after=*/AutocorrectPreference::kDefault},
        PkEnabledByDefaultCase{
            "SpainSpanishDisabledRemainsDisabled",
            /*engine_id=*/kSpainSpanishEngineId,
            /*autocorrect_level=*/0,
            /*preference_before=*/AutocorrectPreference::kDisabled,
            /*preference_after=*/AutocorrectPreference::kDisabled},
        PkEnabledByDefaultCase{
            "SpainSpanishEnabledRemainsEnabled",
            /*engine_id=*/kSpainSpanishEngineId,
            /*autocorrect_level=*/1,
            /*preference_before=*/AutocorrectPreference::kEnabled,
            /*preference_after=*/AutocorrectPreference::kEnabled},
        PkEnabledByDefaultCase{
            "SpainSpanishAggressiveRemainsEnabled",
            /*engine_id=*/kSpainSpanishEngineId,
            /*autocorrect_level=*/2,
            /*preference_before=*/AutocorrectPreference::kEnabled,
            /*preference_after=*/AutocorrectPreference::kEnabled},
    }),
    [](const testing::TestParamInfo<PkEnabledByDefaultCase> info) {
      return info.param.test_name;
    });

class AutocorrectSuggestionProviderMetric
    : public AutocorrectManagerTest,
      public testing::WithParamInterface<AutocorrectSuggestionProvider> {};

INSTANTIATE_TEST_SUITE_P(
    AutocorrectManagerTest,
    AutocorrectSuggestionProviderMetric,
    testing::ValuesIn<AutocorrectSuggestionProvider>({
        AutocorrectSuggestionProvider::kUnknown,
        AutocorrectSuggestionProvider::kUsEnglishPrebundled,
        AutocorrectSuggestionProvider::kUsEnglishDownloaded,
        AutocorrectSuggestionProvider::kUsEnglish840,
        AutocorrectSuggestionProvider::kUsEnglish840V2,
    }),
    [](const testing::TestParamInfo<AutocorrectSuggestionProvider> info) {
      return ToString(info.param);
    });

TEST_P(AutocorrectSuggestionProviderMetric, IsNotRecordedOnFocus) {
  const AutocorrectSuggestionProvider& provider = GetParam();

  manager_.OnActivate(kUsEnglishEngineId);
  manager_.OnFocus(kContextId);
  manager_.OnConnectedToSuggestionProvider(provider);

  histogram_tester_.ExpectTotalCount(
      /*name=*/kAutocorrectV2PkSuggestionProviderHistName,
      /*expected_count=*/0);
}

TEST_P(AutocorrectSuggestionProviderMetric, IsNotRecordedWhenVkIsVisible) {
  const AutocorrectSuggestionProvider& provider = GetParam();
  keyboard_client_->set_keyboard_enabled_for_test(true);

  manager_.OnActivate(kUsEnglishEngineId);
  manager_.OnFocus(kContextId);
  manager_.OnConnectedToSuggestionProvider(provider);
  manager_.OnKeyEvent(KeyA());

  histogram_tester_.ExpectTotalCount(
      /*name=*/kAutocorrectV2PkSuggestionProviderHistName,
      /*expected_count=*/0);
}

TEST_P(AutocorrectSuggestionProviderMetric,
       IsNotRecordedWhenAnEngineOtherThenEnglishIsActive) {
  const AutocorrectSuggestionProvider& provider = GetParam();

  manager_.OnActivate(kSpainSpanishEngineId);
  manager_.OnFocus(kContextId);
  manager_.OnConnectedToSuggestionProvider(provider);
  manager_.OnKeyEvent(KeyA());

  histogram_tester_.ExpectTotalCount(
      /*name=*/kAutocorrectV2PkSuggestionProviderHistName,
      /*expected_count=*/0);
}

TEST_P(AutocorrectSuggestionProviderMetric, IsRecordedCorrectly) {
  const AutocorrectSuggestionProvider& provider = GetParam();

  manager_.OnActivate(kUsEnglishEngineId);
  manager_.OnFocus(kContextId);
  manager_.OnConnectedToSuggestionProvider(provider);
  manager_.OnKeyEvent(KeyA());

  histogram_tester_.ExpectTotalCount(
      /*name=*/kAutocorrectV2PkSuggestionProviderHistName,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      /*name=*/kAutocorrectV2PkSuggestionProviderHistName,
      /*sample*/ provider, /*expected_count=*/1);
}

TEST_P(AutocorrectSuggestionProviderMetric, IsRecordedOnlyOncePerInput) {
  const AutocorrectSuggestionProvider& provider = GetParam();

  manager_.OnActivate(kUsEnglishEngineId);
  manager_.OnFocus(kContextId);
  manager_.OnConnectedToSuggestionProvider(provider);
  manager_.OnKeyEvent(KeyA());
  manager_.OnKeyEvent(KeyA());
  manager_.OnKeyEvent(KeyA());

  histogram_tester_.ExpectTotalCount(
      /*name=*/kAutocorrectV2PkSuggestionProviderHistName,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      /*name=*/kAutocorrectV2PkSuggestionProviderHistName,
      /*sample*/ provider, /*expected_count=*/1);
}

class AutocorrectManagerUkmMetricsTest : public AutocorrectManagerTest {
 protected:
  AutocorrectManagerUkmMetricsTest() {
    ukm::SourceId source_id = test_recorder_.GetNewSourceID();
    test_recorder_.UpdateSourceURL(source_id,
                                   GURL("https://test.example.com/"));

    fake_text_input_client_.set_source_id(source_id);
    IMEBridge::Get()->SetInputContextHandler(&mock_input_method_ash_);

    mock_input_method_ash_.SetFocusedTextInputClient(&fake_text_input_client_);
  }

  ui::FakeTextInputClient fake_text_input_client_{ui::TEXT_INPUT_TYPE_TEXT};
  InputMethodAsh mock_input_method_ash_{nullptr};
  ukm::TestAutoSetUkmRecorder test_recorder_;
};

TEST_F(AutocorrectManagerUkmMetricsTest,
       RecordsAppCompatUkmForUnderlinedSuggestion) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  auto ukm_entries = test_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[0], UkmEntry::kCompatibilitySummary_PKName,
      static_cast<int>(AutocorrectCompatibilitySummary::kUnderlined));
}

TEST_F(AutocorrectManagerUkmMetricsTest,
       DoesNotRecordsAppCompatUkmForInvalidSourceId) {
  fake_text_input_client_.set_source_id(ukm::kInvalidSourceId);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  auto ukm_entries = test_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(0u, ukm_entries.size());
}

TEST_F(AutocorrectManagerUkmMetricsTest,
       RecordsAppCompatUkmForVKUnderlinedSuggestion) {
  keyboard_client_->set_keyboard_enabled_for_test(true);
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  auto ukm_entries = test_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[0], UkmEntry::kCompatibilitySummary_VKName,
      static_cast<int>(AutocorrectCompatibilitySummary::kUnderlined));
}

TEST_F(AutocorrectManagerUkmMetricsTest, RecordsAppCompatUkmForInvalidRange) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  task_environment_.FastForwardBy(base::Milliseconds(501));

  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));

  auto ukm_entries = test_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(2u, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[1], UkmEntry::kCompatibilitySummary_PKName,
      static_cast<int>(AutocorrectCompatibilitySummary::kInvalidRange));
}

TEST_F(AutocorrectManagerUkmMetricsTest,
       RecordsAppCompatUkmForRevertedSuggestion) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  task_environment_.FastForwardBy(base::Milliseconds(501));

  manager_.UndoAutocorrect();

  auto ukm_entries = test_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(2u, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[1], UkmEntry::kCompatibilitySummary_PKName,
      static_cast<int>(AutocorrectCompatibilitySummary::kReverted));
}

TEST_F(AutocorrectManagerUkmMetricsTest, RecordsAppCompatUkmForWindowShown) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");

  // This suppresses strict mock.
  EXPECT_CALL(mock_suggestion_handler_, SetAssistiveWindowProperties(_, _, _));

  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(0));

  auto ukm_entries = test_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(2u, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[1], UkmEntry::kCompatibilitySummary_PKName,
      static_cast<int>(AutocorrectCompatibilitySummary::kWindowShown));
}

TEST_F(AutocorrectManagerUkmMetricsTest,
       RecordsAppCompatUkmForVeryFastAcceptedSuggestion) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  task_environment_.FastForwardBy(base::Milliseconds(200));

  // Implicitly accept autocorrect.
  manager_.OnSurroundingTextChanged(u"the abc", gfx::Range(7));

  auto ukm_entries = test_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(3u, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[1], UkmEntry::kCompatibilitySummary_PKName,
      static_cast<int>(
          AutocorrectCompatibilitySummary::kUserAcceptedAutocorrect));
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[2], UkmEntry::kCompatibilitySummary_PKName,
      static_cast<int>(
          AutocorrectCompatibilitySummary::kVeryFastAcceptedAutocorrect));
}

TEST_F(AutocorrectManagerUkmMetricsTest,
       RecordsAppCompatUkmForFastAcceptedSuggestion) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  task_environment_.FastForwardBy(base::Milliseconds(500));

  // Implicitly accept autocorrect.
  manager_.OnSurroundingTextChanged(u"the abc", gfx::Range(7));

  auto ukm_entries = test_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(3u, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[1], UkmEntry::kCompatibilitySummary_PKName,
      static_cast<int>(
          AutocorrectCompatibilitySummary::kUserAcceptedAutocorrect));
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[2], UkmEntry::kCompatibilitySummary_PKName,
      static_cast<int>(
          AutocorrectCompatibilitySummary::kFastAcceptedAutocorrect));
}

TEST_F(AutocorrectManagerUkmMetricsTest,
       RecordsAppCompatUkmForAcceptedSuggestion) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  task_environment_.FastForwardBy(base::Milliseconds(501));

  // Implicitly accept autocorrect.
  manager_.OnSurroundingTextChanged(u"the abc", gfx::Range(7));

  auto ukm_entries = test_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(2u, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[1], UkmEntry::kCompatibilitySummary_PKName,
      static_cast<int>(
          AutocorrectCompatibilitySummary::kUserAcceptedAutocorrect));
}

TEST_F(AutocorrectManagerUkmMetricsTest,
       RecordsAppCompatUkmForVeryFastRejectedSuggestion) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  task_environment_.FastForwardBy(base::Milliseconds(200));

  // Clear the range.
  mock_input_method_ash_.SetAutocorrectRange(gfx::Range(), base::DoNothing());
  // Process the cleared range ('the' is mutated to implicitly reject it).
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));

  auto ukm_entries = test_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(3u, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[1], UkmEntry::kCompatibilitySummary_PKName,
      static_cast<int>(
          AutocorrectCompatibilitySummary::kUserActionClearedUnderline));
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[2], UkmEntry::kCompatibilitySummary_PKName,
      static_cast<int>(
          AutocorrectCompatibilitySummary::kVeryFastRejectedAutocorrect));
}

TEST_F(AutocorrectManagerUkmMetricsTest,
       RecordsAppCompatUkmForFastRejectedSuggestion) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  task_environment_.FastForwardBy(base::Milliseconds(500));

  // Clear the range.
  mock_input_method_ash_.SetAutocorrectRange(gfx::Range(), base::DoNothing());
  // Process the cleared range ('the' is mutated to implicitly reject it).
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));

  auto ukm_entries = test_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(3u, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[1], UkmEntry::kCompatibilitySummary_PKName,
      static_cast<int>(
          AutocorrectCompatibilitySummary::kUserActionClearedUnderline));
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[2], UkmEntry::kCompatibilitySummary_PKName,
      static_cast<int>(
          AutocorrectCompatibilitySummary::kFastRejectedAutocorrect));
}

TEST_F(AutocorrectManagerUkmMetricsTest,
       RecordsAppCompatUkmForRejectedSuggestion) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  task_environment_.FastForwardBy(base::Milliseconds(501));

  // Clear the range.
  mock_input_method_ash_.SetAutocorrectRange(gfx::Range(), base::DoNothing());
  // Process the cleared range ('the' is mutated to implicitly reject it).
  manager_.OnSurroundingTextChanged(u"teh ", gfx::Range(4));

  auto ukm_entries = test_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(2u, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[1], UkmEntry::kCompatibilitySummary_PKName,
      static_cast<int>(
          AutocorrectCompatibilitySummary::kUserActionClearedUnderline));
}

TEST_F(AutocorrectManagerUkmMetricsTest,
       RecordsAppCompatUkmForVeryFastExitField) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  task_environment_.FastForwardBy(base::Milliseconds(200));
  manager_.OnBlur();

  auto ukm_entries = test_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(3u, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[1], UkmEntry::kCompatibilitySummary_PKName,
      static_cast<int>(
          AutocorrectCompatibilitySummary::kUserExitedTextFieldWithUnderline));
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[2], UkmEntry::kCompatibilitySummary_PKName,
      static_cast<int>(AutocorrectCompatibilitySummary::kVeryFastExitField));
}

TEST_F(AutocorrectManagerUkmMetricsTest, RecordsAppCompatUkmForFastExitField) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  task_environment_.FastForwardBy(base::Milliseconds(500));
  manager_.OnBlur();

  auto ukm_entries = test_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[1], UkmEntry::kCompatibilitySummary_PKName,
      static_cast<int>(
          AutocorrectCompatibilitySummary::kUserExitedTextFieldWithUnderline));
  EXPECT_EQ(3u, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[2], UkmEntry::kCompatibilitySummary_PKName,
      static_cast<int>(AutocorrectCompatibilitySummary::kFastExitField));
}

TEST_F(AutocorrectManagerUkmMetricsTest, RecordsAppCompatUkmForExitField) {
  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  manager_.OnSurroundingTextChanged(u"the ", gfx::Range(4));

  task_environment_.FastForwardBy(base::Milliseconds(501));
  manager_.OnBlur();

  auto ukm_entries = test_recorder_.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(2u, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[1], UkmEntry::kCompatibilitySummary_PKName,
      static_cast<int>(
          AutocorrectCompatibilitySummary::kUserExitedTextFieldWithUnderline));
}

// TODO(b/319190264): Consider parameterizing these federated tests on UMA
// consent status, pending outcome of FederatedClientManager unit testing
// refactor.
TEST_F(AutocorrectManagerTest, FederatedLoggingWhenUmaEnabled) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures({features::kAutocorrectFederatedPhh},
                                 DisabledFeatures());
  bool chrome_metrics_enabled = true;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &chrome_metrics_enabled);

  EXPECT_EQ(0, manager_.GetFederatedClientManagerForTest()
                   .get_num_successful_reports_for_test());

  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  // The handling of an autocorrection triggers a federated logging event.
  EXPECT_EQ(1, manager_.GetFederatedClientManagerForTest()
                   .get_num_successful_reports_for_test());
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

TEST_F(AutocorrectManagerTest, NoFederatedLoggingWhenUmaDisabled) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures({features::kAutocorrectFederatedPhh},
                                 DisabledFeatures());
  bool chrome_metrics_enabled = false;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &chrome_metrics_enabled);

  EXPECT_EQ(0, manager_.GetFederatedClientManagerForTest()
                   .get_num_successful_reports_for_test());

  manager_.HandleAutocorrect(gfx::Range(0, 3), u"teh", u"the");
  // No federated logging despite enabled feature flag, because Chrome metrics
  // collection is disabled.
  EXPECT_EQ(0, manager_.GetFederatedClientManagerForTest()
                   .get_num_successful_reports_for_test());
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

}  // namespace
}  // namespace input_method
}  // namespace ash
