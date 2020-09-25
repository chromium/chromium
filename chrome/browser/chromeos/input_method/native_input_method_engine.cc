// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/native_input_method_engine.h"
#include "base/feature_list.h"
#include "base/i18n/i18n_constants.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

namespace chromeos {

namespace {

// Returns the current input context. This may change during the session, even
// if the IME engine does not change.
ui::IMEInputContextHandlerInterface* GetInputContext() {
  return ui::IMEBridge::Get()->GetInputContextHandler();
}

bool ShouldUseRuleBasedMojoEngine(const std::string& engine_id) {
  return base::StartsWith(engine_id, "vkd_", base::CompareCase::SENSITIVE);
}

bool ShouldUseFstMojoEngine(const std::string& engine_id) {
  return base::FeatureList::IsEnabled(
             chromeos::features::kSystemLatinPhysicalTyping) &&
         base::StartsWith(engine_id, "xkb:", base::CompareCase::SENSITIVE);
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

std::string NormalizeString(const std::string& str) {
  std::string normalized_str;
  base::ConvertToUtf8AndNormalize(str, base::kCodepageUTF8, &normalized_str);
  return normalized_str;
}

ime::mojom::ModifierStatePtr ModifierStateFromEvent(
    const InputMethodEngineBase::KeyboardEvent& event) {
  auto modifier_state = ime::mojom::ModifierState::New();
  modifier_state->alt = event.alt_key;
  modifier_state->alt_graph = event.altgr_key;
  modifier_state->caps_lock = event.caps_lock;
  modifier_state->control = event.ctrl_key;
  modifier_state->shift = event.shift_key;
  return modifier_state;
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

void LogLatency(const char* name, const base::TimeDelta& latency) {
  base::UmaHistogramCustomCounts(name, latency.InMilliseconds(), 0, 1000, 50);
}

}  // namespace

NativeInputMethodEngine::NativeInputMethodEngine() = default;

NativeInputMethodEngine::~NativeInputMethodEngine() = default;

void NativeInputMethodEngine::Initialize(
    std::unique_ptr<InputMethodEngineBase::Observer> observer,
    const char* extension_id,
    Profile* profile) {
  std::unique_ptr<AssistiveSuggester> assistive_suggester =
      std::make_unique<AssistiveSuggester>(this, profile);
  // Wrap the given observer in our observer that will decide whether to call
  // Mojo directly or forward to the extension.
  assistive_suggester_ = assistive_suggester.get();
  auto native_observer =
      std::make_unique<chromeos::NativeInputMethodEngine::ImeObserver>(
          std::move(observer), std::move(assistive_suggester));
  InputMethodEngine::Initialize(std::move(native_observer), extension_id,
                                profile);
}

void NativeInputMethodEngine::FlushForTesting() {
  GetNativeObserver()->FlushForTesting();
}

bool NativeInputMethodEngine::IsConnectedForTesting() const {
  return GetNativeObserver()->IsConnectedForTesting();
}

NativeInputMethodEngine::ImeObserver*
NativeInputMethodEngine::GetNativeObserver() const {
  return static_cast<ImeObserver*>(observer_.get());
}

NativeInputMethodEngine::ImeObserver::ImeObserver(
    std::unique_ptr<InputMethodEngineBase::Observer> base_observer,
    std::unique_ptr<AssistiveSuggester> assistive_suggester)
    : base_observer_(std::move(base_observer)),
      receiver_from_engine_(this),
      assistive_suggester_(std::move(assistive_suggester)) {
}

NativeInputMethodEngine::ImeObserver::~ImeObserver() = default;

void NativeInputMethodEngine::ImeObserver::OnActivate(
    const std::string& engine_id) {
  if (ShouldUseRuleBasedMojoEngine(engine_id) ||
      ShouldUseFstMojoEngine(engine_id)) {
    if (!remote_manager_.is_bound()) {
      auto* ime_manager = input_method::InputMethodManager::Get();
      const auto start = base::Time::Now();
      ime_manager->ConnectInputEngineManager(
          remote_manager_.BindNewPipeAndPassReceiver());
      LogLatency("InputMethod.Mojo.Extension.ServiceInitLatency",
                 base::Time::Now() - start);
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
  } else {
    // Release the IME service.
    // TODO(b/147709499): A better way to cleanup all.
    remote_manager_.reset();
  }
  base_observer_->OnActivate(engine_id);
}
void NativeInputMethodEngine::ImeObserver::ProcessMessage(
    const std::vector<uint8_t>& message,
    ProcessMessageCallback callback) {
  // NativeInputMethodEngine doesn't use binary messages, but it must run the
  // callback to avoid dropping the connection.
  std::move(callback).Run(std::vector<uint8_t>());
}

void NativeInputMethodEngine::ImeObserver::OnFocus(
    const IMEEngineHandlerInterface::InputContext& context) {
  if (assistive_suggester_->IsAssistiveFeatureEnabled())
    assistive_suggester_->OnFocus(context.id);

  if (active_engine_id_ && ShouldUseFstMojoEngine(*active_engine_id_) &&
      remote_to_engine_.is_bound()) {
    remote_to_engine_->OnFocus();
  }

  base_observer_->OnFocus(context);
}

void NativeInputMethodEngine::ImeObserver::OnBlur(int context_id) {
  if (assistive_suggester_->IsAssistiveFeatureEnabled())
    assistive_suggester_->OnBlur();

  base_observer_->OnBlur(context_id);
}

void NativeInputMethodEngine::ImeObserver::OnKeyEvent(
    const std::string& engine_id,
    const InputMethodEngineBase::KeyboardEvent& event,
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback) {
  if (assistive_suggester_->IsAssistiveFeatureEnabled()) {
    if (assistive_suggester_->OnKeyEvent(event)) {
      std::move(callback).Run(true);
      return;
    }
  }
  if (ShouldUseRuleBasedMojoEngine(engine_id) && remote_to_engine_.is_bound()) {
    remote_to_engine_->ProcessKeypressForRulebased(
        ime::mojom::PhysicalKeyEvent::New(
            event.type == "keydown" ? ime::mojom::KeyEventType::kKeyDown
                                    : ime::mojom::KeyEventType::kKeyUp,
            event.code, event.key, ModifierStateFromEvent(event)),
        base::BindOnce(&ImeObserver::OnKeyEventResponse, base::Unretained(this),
                       base::Time::Now(), std::move(callback)));
  } else {
    base_observer_->OnKeyEvent(engine_id, event, std::move(callback));
  }
}

void NativeInputMethodEngine::ImeObserver::OnReset(
    const std::string& engine_id) {
  if (ShouldUseRuleBasedMojoEngine(engine_id) && remote_to_engine_.is_bound()) {
    remote_to_engine_->ResetForRulebased();
  }
  base_observer_->OnReset(engine_id);
}

void NativeInputMethodEngine::ImeObserver::OnDeactivated(
    const std::string& engine_id) {
  if (ShouldUseRuleBasedMojoEngine(engine_id)) {
    remote_to_engine_.reset();
  }
  base_observer_->OnDeactivated(engine_id);
}

void NativeInputMethodEngine::ImeObserver::OnCompositionBoundsChanged(
    const std::vector<gfx::Rect>& bounds) {
  base_observer_->OnCompositionBoundsChanged(bounds);
}

void NativeInputMethodEngine::ImeObserver::OnSurroundingTextChanged(
    const std::string& engine_id,
    const base::string16& text,
    int cursor_pos,
    int anchor_pos,
    int offset_pos) {
  assistive_suggester_->RecordAssistiveMatchMetrics(text, cursor_pos,
                                                    anchor_pos);
  if (assistive_suggester_->IsAssistiveFeatureEnabled()) {
    assistive_suggester_->OnSurroundingTextChanged(text, cursor_pos,
                                                   anchor_pos);
  }
  base_observer_->OnSurroundingTextChanged(engine_id, text, cursor_pos,
                                           anchor_pos, offset_pos);
}

void NativeInputMethodEngine::ImeObserver::OnInputContextUpdate(
    const IMEEngineHandlerInterface::InputContext& context) {
  base_observer_->OnInputContextUpdate(context);
}

void NativeInputMethodEngine::ImeObserver::OnCandidateClicked(
    const std::string& component_id,
    int candidate_id,
    InputMethodEngineBase::MouseButtonEvent button) {
  base_observer_->OnCandidateClicked(component_id, candidate_id, button);
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
    case ui::ime::ButtonId::kAddToDictionary:
    case ui::ime::ButtonId::kNone:
      base_observer_->OnAssistiveWindowButtonClicked(button);
      break;
  }
}

void NativeInputMethodEngine::ImeObserver::OnMenuItemActivated(
    const std::string& component_id,
    const std::string& menu_id) {
  base_observer_->OnMenuItemActivated(component_id, menu_id);
}

void NativeInputMethodEngine::ImeObserver::OnScreenProjectionChanged(
    bool is_projected) {
  base_observer_->OnScreenProjectionChanged(is_projected);
}

void NativeInputMethodEngine::ImeObserver::OnSuggestionsChanged(
    const std::vector<std::string>& suggestions) {
  base_observer_->OnSuggestionsChanged(suggestions);
}

void NativeInputMethodEngine::ImeObserver::OnInputMethodOptionsChanged(
    const std::string& engine_id) {
  base_observer_->OnInputMethodOptionsChanged(engine_id);
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
  LogLatency("InputMethod.Mojo.Extension.ActivateIMELatency",
             base::Time::Now() - start);
  LogEvent(bound ? ImeServiceEvent::kActivateImeSuccess
                 : ImeServiceEvent::kActivateImeSuccess);

  active_engine_id_ = engine_id;
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

void NativeInputMethodEngine::ImeObserver::OnKeyEventResponse(
    base::Time start,
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback,
    ime::mojom::KeypressResponseForRulebasedPtr response) {
  LogLatency("InputMethod.Mojo.Extension.Rulebased.ProcessLatency",
             base::Time::Now() - start);

  for (const auto& op : response->operations) {
    switch (op->method) {
      case ime::mojom::OperationMethodForRulebased::COMMIT_TEXT:
        GetInputContext()->CommitText(NormalizeString(op->arguments));
        break;
      case ime::mojom::OperationMethodForRulebased::SET_COMPOSITION:
        ui::CompositionText composition;
        composition.text = base::UTF8ToUTF16(NormalizeString(op->arguments));
        GetInputContext()->UpdateCompositionText(
            composition, composition.text.length(), /*visible=*/true);
        break;
    }
  }
  std::move(callback).Run(response->result);
}

}  // namespace chromeos
