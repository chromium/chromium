// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/native_input_method_engine.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/i18n/i18n_constants.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/autocorrect_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace chromeos {

namespace {

// Returns the current input context. This may change during the session, even
// if the IME engine does not change.
ui::IMEInputContextHandlerInterface* GetInputContext() {
  return ui::IMEBridge::Get()->GetInputContextHandler();
}

// TODO(https://crbug/1166082): Use a separate InputMethodEngine for rule-based.
bool ShouldRouteToRuleBasedEngine(const std::string& engine_id) {
  return base::StartsWith(engine_id, "vkd_", base::CompareCase::SENSITIVE);
}

bool ShouldRouteToFstMojoEngine(const std::string& engine_id) {
  // To avoid handling tricky cases where the user types with both the virtual
  // and the physical keyboard, only run the native code path if the virtual
  // keyboard is disabled. Otherwise, just let the extension handle any physical
  // key events.
  return base::FeatureList::IsEnabled(
             chromeos::features::kSystemLatinPhysicalTyping) &&
         base::StartsWith(engine_id, "xkb:", base::CompareCase::SENSITIVE) &&
         !ChromeKeyboardControllerClient::Get()->GetKeyboardEnabled();
}

bool IsPhysicalKeyboardAutocorrectEnabled(PrefService* prefs,
                                          const std::string& engine_id) {
  // The FST Mojo engine is only needed if autocorrect is enabled.
  const base::DictionaryValue* input_method_settings =
      prefs->GetDictionary(prefs::kLanguageInputMethodSpecificSettings);
  const base::Value* autocorrect_setting = input_method_settings->FindPath(
      engine_id + ".physicalKeyboardAutoCorrectionLevel");
  return autocorrect_setting && autocorrect_setting->GetIfInt().value_or(0) > 0;
}

std::string NormalizeEngineId(const std::string engine_id) {
  // For legacy reasons, |engine_id| starts with "vkd_" in the input method
  // manifest, but the InputEngineManager expects the prefix "m17n:".
  // TODO(https://crbug.com/1012490): Migrate to m17n prefix and remove this.
  if (base::StartsWith(engine_id, "vkd_", base::CompareCase::SENSITIVE)) {
    return "m17n:" + engine_id.substr(4);
  }
  return engine_id;
}

std::u16string ConvertToUtf16AndNormalize(const std::string& str) {
  // TODO(https://crbug.com/1185629): Add a new helper in
  // base/i18n/icu_string_conversions.h that does the conversion directly
  // without a redundant UTF16->UTF8 conversion.
  std::string normalized_str;
  base::ConvertToUtf8AndNormalize(str, base::kCodepageUTF8, &normalized_str);
  return base::UTF8ToUTF16(normalized_str);
}

ime::mojom::ModifierStatePtr ModifierStateFromEvent(const ui::KeyEvent& event) {
  auto modifier_state = ime::mojom::ModifierState::New();
  modifier_state->alt = event.flags() & ui::EF_ALT_DOWN;
  modifier_state->alt_graph = event.flags() & ui::EF_ALTGR_DOWN;
  modifier_state->caps_lock = event.flags() & ui::EF_CAPS_LOCK_ON;
  modifier_state->control = event.flags() & ui::EF_CONTROL_DOWN;
  modifier_state->shift = event.flags() & ui::EF_SHIFT_DOWN;
  return modifier_state;
}

ime::mojom::InputFieldType TextInputTypeToMojoType(ui::TextInputType type) {
  using ime::mojom::InputFieldType;
  switch (type) {
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      return InputFieldType::kPassword;
    case ui::TEXT_INPUT_TYPE_SEARCH:
      return InputFieldType::kSearch;
    case ui::TEXT_INPUT_TYPE_EMAIL:
      return InputFieldType::kEmail;
    case ui::TEXT_INPUT_TYPE_TELEPHONE:
      return InputFieldType::kTelephone;
    case ui::TEXT_INPUT_TYPE_URL:
      return InputFieldType::kURL;
    case ui::TEXT_INPUT_TYPE_NUMBER:
      return InputFieldType::kNumber;
    case ui::TEXT_INPUT_TYPE_NULL:
      return InputFieldType::kNoIME;
    case ui::TEXT_INPUT_TYPE_TEXT:
      return InputFieldType::kText;
    default:
      return InputFieldType::kText;
  }
}

ime::mojom::AutocorrectMode AutocorrectFlagsToMojoType(int flags) {
  if ((flags & ui::TEXT_INPUT_FLAG_AUTOCORRECT_OFF) ||
      (flags & ui::TEXT_INPUT_FLAG_SPELLCHECK_OFF)) {
    return ime::mojom::AutocorrectMode::kDisabled;
  }
  return ime::mojom::AutocorrectMode::kEnabled;
}

enum class ImeServiceEvent {
  kUnknown = 0,
  kInitSuccess = 1,
  kInitFailed = 2,
  kActivateImeSuccess = 3,
  kActivateImeFailed = 4,
  kServiceDisconnected = 5,
  kMaxValue = kServiceDisconnected
};

void LogEvent(ImeServiceEvent event) {
  UMA_HISTOGRAM_ENUMERATION("InputMethod.Mojo.Extension.Event", event);
}

ime::mojom::PhysicalKeyEventPtr CreatePhysicalKeyEventFromKeyEvent(
    const ui::KeyEvent& event) {
  return ime::mojom::PhysicalKeyEvent::New(
      event.type() == ui::ET_KEY_PRESSED ? ime::mojom::KeyEventType::kKeyDown
                                         : ime::mojom::KeyEventType::kKeyUp,
      ui::KeycodeConverter::DomCodeToCodeString(event.code()),
      ui::KeycodeConverter::DomKeyToKeyString(event.GetDomKey()),
      ModifierStateFromEvent(event));
}

}  // namespace

NativeInputMethodEngine::NativeInputMethodEngine() = default;

NativeInputMethodEngine::~NativeInputMethodEngine() = default;

void NativeInputMethodEngine::Initialize(
    std::unique_ptr<InputMethodEngineBase::Observer> observer,
    const char* extension_id,
    Profile* profile) {
  // TODO(crbug/1141231): refactor the mix of unique and raw ptr here.
  std::unique_ptr<AssistiveSuggester> assistive_suggester =
      std::make_unique<AssistiveSuggester>(this, profile);
  assistive_suggester_ = assistive_suggester.get();
  std::unique_ptr<AutocorrectManager> autocorrect_manager =
      std::make_unique<AutocorrectManager>(this);
  autocorrect_manager_ = autocorrect_manager.get();

  chrome_keyboard_controller_client_observer_.Observe(
      ChromeKeyboardControllerClient::Get());

  if (base::FeatureList::IsEnabled(
          chromeos::features::kSystemLatinPhysicalTyping)) {
    pref_change_registrar_.Init(profile->GetPrefs());
    pref_change_registrar_.Add(
        prefs::kLanguageInputMethodSpecificSettings,
        base::BindRepeating(&NativeInputMethodEngine::OnInputMethodPrefsChanged,
                            base::Unretained(this)));
  }

  // Wrap the given observer in our observer that will decide whether to call
  // Mojo directly or forward to the extension.
  auto native_observer =
      std::make_unique<chromeos::NativeInputMethodEngine::ImeObserver>(
          profile->GetPrefs(), std::move(observer),
          std::move(assistive_suggester), std::move(autocorrect_manager));
  InputMethodEngine::Initialize(std::move(native_observer), extension_id,
                                profile);
}

void NativeInputMethodEngine::OnKeyboardEnabledChanged(bool enabled) {
  // Re-activate the engine whenever the virtual keyboard is enabled or disabled
  // so that the native or extension state is reset correctly.
  Enable(GetActiveComponentId());
}

void NativeInputMethodEngine::FlushForTesting() {
  GetNativeObserver()->FlushForTesting();
}

bool NativeInputMethodEngine::IsConnectedForTesting() const {
  return GetNativeObserver()->IsConnectedForTesting();
}

void NativeInputMethodEngine::OnAutocorrect(
    const std::u16string& typed_word,
    const std::u16string& corrected_word,
    int start_index) {
  autocorrect_manager_->HandleAutocorrect(
      gfx::Range(start_index, start_index + corrected_word.length()),
      typed_word, corrected_word);
}

NativeInputMethodEngine::ImeObserver*
NativeInputMethodEngine::GetNativeObserver() const {
  return static_cast<ImeObserver*>(observer_.get());
}

NativeInputMethodEngine::ImeObserver::ImeObserver(
    PrefService* prefs,
    std::unique_ptr<InputMethodEngineBase::Observer> ime_base_observer,
    std::unique_ptr<AssistiveSuggester> assistive_suggester,
    std::unique_ptr<AutocorrectManager> autocorrect_manager)
    : prefs_(prefs),
      ime_base_observer_(std::move(ime_base_observer)),
      receiver_from_engine_(this),
      assistive_suggester_(std::move(assistive_suggester)),
      autocorrect_manager_(std::move(autocorrect_manager)) {}

NativeInputMethodEngine::ImeObserver::~ImeObserver() = default;

void NativeInputMethodEngine::ImeObserver::OnActivate(
    const std::string& engine_id) {
  // TODO(b/181077907): Always launch the IME service and let IME service decide
  // whether it should shutdown or not.
  if (ShouldRouteToFstMojoEngine(engine_id) &&
      !IsPhysicalKeyboardAutocorrectEnabled(prefs_, engine_id)) {
    remote_manager_.reset();
    remote_to_engine_.reset();
    receiver_from_engine_.reset();
    return;
  }

  if (ShouldRouteToRuleBasedEngine(engine_id) ||
      ShouldRouteToFstMojoEngine(engine_id)) {
    if (!remote_manager_.is_bound()) {
      auto* ime_manager = input_method::InputMethodManager::Get();
      ime_manager->ConnectInputEngineManager(
          remote_manager_.BindNewPipeAndPassReceiver());
      remote_manager_.set_disconnect_handler(base::BindOnce(
          &ImeObserver::OnError, base::Unretained(this), base::Time::Now()));
      LogEvent(ImeServiceEvent::kInitSuccess);
    }

    const auto new_engine_id = NormalizeEngineId(engine_id);

    // Deactivate any existing engine.
    remote_to_engine_.reset();
    receiver_from_engine_.reset();

    remote_manager_->ConnectToImeEngine(
        new_engine_id, remote_to_engine_.BindNewPipeAndPassReceiver(),
        receiver_from_engine_.BindNewPipeAndPassRemote(), {},
        base::BindOnce(&ImeObserver::OnConnected, base::Unretained(this),
                       base::Time::Now(), new_engine_id));

    active_engine_id_ = new_engine_id;
    remote_to_engine_->OnInputMethodChanged(new_engine_id);

    if (ShouldRouteToRuleBasedEngine(engine_id)) {
      ime_base_observer_->OnActivate(engine_id);
    }
  } else {
    // Release the IME service.
    // TODO(b/147709499): A better way to cleanup all.
    remote_manager_.reset();

    ime_base_observer_->OnActivate(engine_id);
  }
}
void NativeInputMethodEngine::ImeObserver::ProcessMessage(
    const std::vector<uint8_t>& message,
    ProcessMessageCallback callback) {
  // NativeInputMethodEngine doesn't use binary messages, but it must run the
  // callback to avoid dropping the connection.
  std::move(callback).Run(std::vector<uint8_t>());
}

void NativeInputMethodEngine::ImeObserver::OnFocus(
    int context_id,
    const IMEEngineHandlerInterface::InputContext& context) {
  if (assistive_suggester_->IsAssistiveFeatureEnabled()) {
    assistive_suggester_->OnFocus(context_id);
  }
  autocorrect_manager_->OnFocus(context_id);
  if (active_engine_id_ && ShouldRouteToFstMojoEngine(*active_engine_id_)) {
    if (remote_to_engine_.is_bound()) {
      remote_to_engine_->OnFocus(ime::mojom::InputFieldInfo::New(
          TextInputTypeToMojoType(context.type),
          AutocorrectFlagsToMojoType(context.flags),
          context.should_do_learning
              ? ime::mojom::PersonalizationMode::kEnabled
              : ime::mojom::PersonalizationMode::kDisabled));
    }
  } else {
    ime_base_observer_->OnFocus(context_id, context);
  }
}

void NativeInputMethodEngine::ImeObserver::OnBlur(int context_id) {
  if (assistive_suggester_->IsAssistiveFeatureEnabled())
    assistive_suggester_->OnBlur();

  if (active_engine_id_ && ShouldRouteToFstMojoEngine(*active_engine_id_)) {
    if (remote_to_engine_.is_bound()) {
      remote_to_engine_->OnBlur();
    }
  } else {
    ime_base_observer_->OnBlur(context_id);
  }
}

void NativeInputMethodEngine::ImeObserver::OnKeyEvent(
    const std::string& engine_id,
    const ui::KeyEvent& event,
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback) {
  if (assistive_suggester_->IsAssistiveFeatureEnabled()) {
    if (assistive_suggester_->OnKeyEvent(event)) {
      std::move(callback).Run(true);
      return;
    }
  }
  if (autocorrect_manager_->OnKeyEvent(event)) {
    std::move(callback).Run(true);
    return;
  }

  if (ShouldRouteToRuleBasedEngine(engine_id) && remote_to_engine_.is_bound()) {
    remote_to_engine_->ProcessKeypressForRulebased(
        CreatePhysicalKeyEventFromKeyEvent(event),
        base::BindOnce(&ImeObserver::OnRuleBasedKeyEventResponse,
                       base::Unretained(this), base::Time::Now(),
                       std::move(callback)));
  } else if (ShouldRouteToFstMojoEngine(engine_id)) {
    if (remote_to_engine_.is_bound()) {
      // CharacterComposer only takes KEY_PRESSED events.
      const bool filtered = event.type() == ui::ET_KEY_PRESSED &&
                            character_composer_.FilterKeyPress(event);

      // Don't send dead keys to the system IME. Dead keys should be handled at
      // the OS level and not exposed to IMEs.
      if (event.GetDomKey().IsDeadKey()) {
        std::move(callback).Run(true);
        return;
      }

      auto key_event = CreatePhysicalKeyEventFromKeyEvent(event);
      if (filtered) {
        // TODO(b/174612548): Transform the corresponding KEY_RELEASED event to
        // use the composed character as well.
        key_event->key =
            base::UTF16ToUTF8(character_composer_.composed_character());
      }

      remote_to_engine_->OnKeyEvent(std::move(key_event), std::move(callback));
    } else {
      std::move(callback).Run(false);
    }
  } else {
    ime_base_observer_->OnKeyEvent(engine_id, event, std::move(callback));
  }
}

void NativeInputMethodEngine::ImeObserver::OnReset(
    const std::string& engine_id) {
  if (remote_to_engine_.is_bound() && ShouldRouteToRuleBasedEngine(engine_id)) {
    remote_to_engine_->ResetForRulebased();
  } else if (ShouldRouteToFstMojoEngine(engine_id)) {
    if (remote_to_engine_.is_bound()) {
      remote_to_engine_->OnCompositionCanceled();
    }
  } else {
    ime_base_observer_->OnReset(engine_id);
  }
}

void NativeInputMethodEngine::ImeObserver::OnDeactivated(
    const std::string& engine_id) {
  if (ShouldRouteToRuleBasedEngine(engine_id)) {
    remote_to_engine_.reset();
  }
  ime_base_observer_->OnDeactivated(engine_id);
}

void NativeInputMethodEngine::ImeObserver::OnCompositionBoundsChanged(
    const std::vector<gfx::Rect>& bounds) {
  ime_base_observer_->OnCompositionBoundsChanged(bounds);
}

void NativeInputMethodEngine::ImeObserver::OnSurroundingTextChanged(
    const std::string& engine_id,
    const std::u16string& text,
    int cursor_pos,
    int anchor_pos,
    int offset_pos) {
  assistive_suggester_->RecordAssistiveMatchMetrics(text, cursor_pos,
                                                    anchor_pos);
  if (assistive_suggester_->IsAssistiveFeatureEnabled()) {
    assistive_suggester_->OnSurroundingTextChanged(text, cursor_pos,
                                                   anchor_pos);
  }
  autocorrect_manager_->OnSurroundingTextChanged(text, cursor_pos, anchor_pos);
  if (ShouldRouteToFstMojoEngine(engine_id)) {
    if (remote_to_engine_.is_bound()) {
      std::vector<size_t> selection_indices = {anchor_pos, cursor_pos};
      std::string utf8_text =
          base::UTF16ToUTF8AndAdjustOffsets(text, &selection_indices);

      auto selection = ime::mojom::SelectionRange::New();
      selection->anchor = selection_indices[0];
      selection->focus = selection_indices[1];

      remote_to_engine_->OnSurroundingTextChanged(
          std::move(utf8_text), offset_pos, std::move(selection));
    }
  } else {
    ime_base_observer_->OnSurroundingTextChanged(engine_id, text, cursor_pos,
                                                 anchor_pos, offset_pos);
  }
}

void NativeInputMethodEngine::ImeObserver::OnCandidateClicked(
    const std::string& component_id,
    int candidate_id,
    InputMethodEngineBase::MouseButtonEvent button) {
  ime_base_observer_->OnCandidateClicked(component_id, candidate_id, button);
}

void NativeInputMethodEngine::ImeObserver::OnAssistiveWindowButtonClicked(
    const ui::ime::AssistiveWindowButton& button) {
  switch (button.id) {
    case ui::ime::ButtonId::kSmartInputsSettingLink:
      base::RecordAction(base::UserMetricsAction(
          "ChromeOS.Settings.SmartInputs.PersonalInfoSuggestions.Open"));
      // TODO(crbug/1101689): Add subpath for personal info suggestions
      // settings.
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          ProfileManager::GetActiveUserProfile(),
          chromeos::settings::mojom::kSmartInputsSubpagePath);
      break;
    case ui::ime::ButtonId::kLearnMore:
      if (button.window_type ==
          ui::ime::AssistiveWindowType::kEmojiSuggestion) {
        base::RecordAction(base::UserMetricsAction(
            "ChromeOS.Settings.SmartInputs.EmojiSuggestions.Open"));
        // TODO(crbug/1101689): Add subpath for emoji suggestions settings.
        chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
            ProfileManager::GetActiveUserProfile(),
            chromeos::settings::mojom::kSmartInputsSubpagePath);
      }
      break;
    case ui::ime::ButtonId::kSuggestion:
      if (assistive_suggester_->IsAssistiveFeatureEnabled()) {
        assistive_suggester_->AcceptSuggestion(button.index);
      }
      break;
    case ui::ime::ButtonId::kUndo:
      autocorrect_manager_->UndoAutocorrect();
      break;
    case ui::ime::ButtonId::kAddToDictionary:
    case ui::ime::ButtonId::kNone:
      ime_base_observer_->OnAssistiveWindowButtonClicked(button);
      break;
  }
}

void NativeInputMethodEngine::ImeObserver::OnMenuItemActivated(
    const std::string& component_id,
    const std::string& menu_id) {
  ime_base_observer_->OnMenuItemActivated(component_id, menu_id);
}

void NativeInputMethodEngine::ImeObserver::OnScreenProjectionChanged(
    bool is_projected) {
  ime_base_observer_->OnScreenProjectionChanged(is_projected);
}

void NativeInputMethodEngine::ImeObserver::OnSuggestionsChanged(
    const std::vector<std::string>& suggestions) {
  ime_base_observer_->OnSuggestionsChanged(suggestions);
}

void NativeInputMethodEngine::ImeObserver::OnInputMethodOptionsChanged(
    const std::string& engine_id) {
  ime_base_observer_->OnInputMethodOptionsChanged(engine_id);
}

void NativeInputMethodEngine::ImeObserver::CommitText(
    const std::string& text,
    ime::mojom::CommitTextCursorBehavior cursor_behavior) {
  GetInputContext()->CommitText(
      ConvertToUtf16AndNormalize(text),
      cursor_behavior ==
              ime::mojom::CommitTextCursorBehavior::kMoveCursorBeforeText
          ? ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorBeforeText
          : ui::TextInputClient::InsertTextCursorBehavior::
                kMoveCursorAfterText);
}

void NativeInputMethodEngine::ImeObserver::SetComposition(
    const std::string& text) {
  ui::CompositionText composition;
  composition.text = ConvertToUtf16AndNormalize(text);
  GetInputContext()->UpdateCompositionText(
      composition, /*cursor_pos=*/composition.text.length(), /*visible=*/true);
}

void NativeInputMethodEngine::ImeObserver::SetCompositionRange(
    uint32_t start_byte_index,
    uint32_t end_byte_index) {
  const auto ordered_range = std::minmax(start_byte_index, end_byte_index);
  GetInputContext()->SetComposingRange(
      ordered_range.first, ordered_range.second,
      {ui::ImeTextSpan(
          ui::ImeTextSpan::Type::kComposition, /*start_offset=*/0,
          /*end_offset=*/ordered_range.second - ordered_range.first)});
}

void NativeInputMethodEngine::ImeObserver::FinishComposition() {
  GetInputContext()->ConfirmCompositionText(/*reset_engine=*/false,
                                            /*keep_selection=*/true);
}

void NativeInputMethodEngine::ImeObserver::DeleteSurroundingText(
    uint32_t num_bytes_before_cursor,
    uint32_t num_bytes_after_cursor) {
  GetInputContext()->DeleteSurroundingText(
      /*offset=*/-static_cast<int>(num_bytes_before_cursor),
      /*length=*/num_bytes_before_cursor + num_bytes_after_cursor);
}

void NativeInputMethodEngine::ImeObserver::HandleAutocorrect(
    ime::mojom::AutocorrectSpanPtr autocorrect_span) {
  autocorrect_manager_->HandleAutocorrect(
      autocorrect_span->autocorrect_range,
      base::UTF8ToUTF16(autocorrect_span->original_text),
      base::UTF8ToUTF16(autocorrect_span->current_text));
}

void NativeInputMethodEngine::ImeObserver::FlushForTesting() {
  remote_manager_.FlushForTesting();
  if (remote_to_engine_.is_bound())
    receiver_from_engine_.FlushForTesting();
  if (remote_to_engine_.is_bound())
    remote_to_engine_.FlushForTesting();
}

void NativeInputMethodEngine::ImeObserver::OnConnected(base::Time start,
                                                       std::string engine_id,
                                                       bool bound) {
  LogEvent(bound ? ImeServiceEvent::kActivateImeSuccess
                 : ImeServiceEvent::kActivateImeSuccess);
}

void NativeInputMethodEngine::ImeObserver::OnError(base::Time start) {
  LOG(ERROR) << "IME Service connection error";

  // If the Mojo pipe disconnection happens in 1000 ms after the service
  // is initialized, we consider it as a failure. Normally it's caused
  // by the Mojo service itself or misconfigured on Chrome OS.
  if (base::Time::Now() - start < base::TimeDelta::FromMilliseconds(1000)) {
    LogEvent(ImeServiceEvent::kInitFailed);
  } else {
    LogEvent(ImeServiceEvent::kServiceDisconnected);
  }

  active_engine_id_.reset();
}

void NativeInputMethodEngine::ImeObserver::OnRuleBasedKeyEventResponse(
    base::Time start,
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback,
    ime::mojom::KeypressResponseForRulebasedPtr response) {
  for (const auto& op : response->operations) {
    switch (op->method) {
      case ime::mojom::OperationMethodForRulebased::COMMIT_TEXT:
        GetInputContext()->CommitText(
            ConvertToUtf16AndNormalize(op->arguments),
            ui::TextInputClient::InsertTextCursorBehavior::
                kMoveCursorAfterText);
        break;
      case ime::mojom::OperationMethodForRulebased::SET_COMPOSITION:
        ui::CompositionText composition;
        composition.text = ConvertToUtf16AndNormalize(op->arguments);
        GetInputContext()->UpdateCompositionText(
            composition, composition.text.length(), /*visible=*/true);
        break;
    }
  }
  std::move(callback).Run(response->result);
}

void NativeInputMethodEngine::OnInputMethodPrefsChanged() {
  Enable(GetActiveComponentId());
}

}  // namespace chromeos
