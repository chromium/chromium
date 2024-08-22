// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/native_input_method_engine.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/input_method/assistive_suggester_client_filter.h"
#include "chrome/browser/ash/input_method/assistive_suggester_switch.h"
#include "chrome/browser/ash/input_method/autocorrect_manager.h"
#include "chrome/browser/ash/input_method/editor_mediator_factory.h"
#include "chrome/browser/ash/input_method/get_current_window_properties.h"
#include "chrome/browser/ash/input_method/grammar_service_client.h"
#include "chrome/browser/ash/input_method/native_input_method_engine_observer.h"
#include "chrome/browser/ash/input_method/suggestions_service_client.h"
#include "chrome/browser/ui/ash/input_method/input_method_menu_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace ash {

namespace input_method {
namespace {
bool ShouldRouteToFirstPartyVietnameseInput(std::string_view engine_id) {
  return base::FeatureList::IsEnabled(features::kFirstPartyVietnameseInput) &&
         (engine_id == "vkd_vi_vni" || engine_id == "vkd_vi_telex");
}
}  // namespace

NativeInputMethodEngine::NativeInputMethodEngine()
    : NativeInputMethodEngine(/*use_ime_service=*/true) {}

NativeInputMethodEngine::NativeInputMethodEngine(bool use_ime_service)
    : use_ime_service_(use_ime_service) {}

// static
std::unique_ptr<NativeInputMethodEngine>
NativeInputMethodEngine::CreateForTesting(bool use_ime_service) {
  return base::WrapUnique<NativeInputMethodEngine>(
      new NativeInputMethodEngine(use_ime_service));
}

NativeInputMethodEngine::~NativeInputMethodEngine() = default;

NativeInputMethodEngine::NativeInputMethodEngine(
    std::unique_ptr<AssistiveSuggesterSwitch> suggester_switch)
    : suggester_switch_(std::move(suggester_switch)) {}

void NativeInputMethodEngine::Initialize(
    std::unique_ptr<InputMethodEngineObserver> observer,
    const char* extension_id,
    Profile* profile) {
  // TODO(crbug.com/40154142): refactor the mix of unique and raw ptr here.
  std::unique_ptr<AssistiveSuggester> assistive_suggester =
      std::make_unique<AssistiveSuggester>(
          this, profile,
          suggester_switch_
              ? std::move(suggester_switch_)
              : std::make_unique<AssistiveSuggesterClientFilter>(
                    base::BindRepeating(&GetFocusedTabUrl),
                    base::BindRepeating(&GetFocusedWindowProperties)));
  assistive_suggester_ = assistive_suggester.get();
  std::unique_ptr<AutocorrectManager> autocorrect_manager =
      std::make_unique<AutocorrectManager>(this, profile);
  autocorrect_manager_ = autocorrect_manager.get();

  auto suggestions_service_client =
      base::FeatureList::IsEnabled(features::kAssistMultiWord)
          ? std::make_unique<SuggestionsServiceClient>()
          : nullptr;

  auto suggestions_collector =
      base::FeatureList::IsEnabled(features::kAssistMultiWord)
          ? std::make_unique<SuggestionsCollector>(
                assistive_suggester_, std::move(suggestions_service_client))
          : nullptr;

  EditorMediator* editor_event_sink =
      chromeos::features::IsOrcaEnabled()
          ? EditorMediatorFactory::GetInstance()->GetForProfile(profile)
          : nullptr;

  chrome_keyboard_controller_client_observer_.Observe(
      ChromeKeyboardControllerClient::Get());

  // Wrap the given observer in our observer that will decide whether to call
  // Mojo directly or forward to the extension.
  auto native_observer = std::make_unique<NativeInputMethodEngineObserver>(
      profile->GetPrefs(), editor_event_sink, std::move(observer),
      std::move(assistive_suggester), std::move(autocorrect_manager),
      std::move(suggestions_collector),
      std::make_unique<GrammarManager>(
          profile, std::make_unique<GrammarServiceClient>(), this),
      use_ime_service_);
  InputMethodEngine::Initialize(std::move(native_observer), extension_id,
                                profile);
}

bool NativeInputMethodEngine::ShouldRouteToNativeMojoEngine(
    const std::string& engine_id) const {
  return use_ime_service_ && CanRouteToNativeMojoEngine(engine_id);
}

void NativeInputMethodEngine::CandidateClicked(uint32_t index) {
  // The parent implementation will try to convert `index` into a candidate ID.
  // The native Mojo engine doesn't use candidate IDs, so we just treat `index`
  // as the ID, without doing a mapping.
  if (ShouldRouteToNativeMojoEngine(GetActiveComponentId())) {
    GetNativeObserver()->OnCandidateClicked(GetActiveComponentId(), index,
                                            MOUSE_BUTTON_LEFT);
  } else {
    InputMethodEngine::CandidateClicked(index);
  }
}

bool NativeInputMethodEngine::IsReadyForTesting() {
  if (ShouldRouteToNativeMojoEngine(GetActiveComponentId())) {
    return GetNativeObserver()->IsReadyForTesting();  // IN-TEST
  }
  return InputMethodEngine::IsReadyForTesting();
}

void NativeInputMethodEngine::OnKeyboardEnabledChanged(bool enabled) {
  // Re-activate the engine whenever the virtual keyboard is enabled or disabled
  // so that the native or extension state is reset correctly.
  Enable(GetActiveComponentId());
}

void NativeInputMethodEngine::OnProfileWillBeDestroyed(Profile* profile) {
  InputMethodEngine::OnProfileWillBeDestroyed(profile);
  GetNativeObserver()->OnProfileWillBeDestroyed();
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

NativeInputMethodEngineObserver* NativeInputMethodEngine::GetNativeObserver()
    const {
  return static_cast<NativeInputMethodEngineObserver*>(observer_.get());
}

bool NativeInputMethodEngine::UpdateMenuItems(
    const std::vector<InputMethodManager::MenuItem>& items,
    std::string* error) {
  // Ignore calls to UpdateMenuItems when the native Mojo engine is active.
  // The menu items are stored in a singleton that is shared between the native
  // Mojo engine and the extension. This method is called when the extension
  // wants to update the menu items.
  // Ignore this if the native Mojo engine is active to prevent the extension
  // from overriding the menu items set by the native Mojo engine.
  if (ShouldRouteToNativeMojoEngine(GetActiveComponentId())) {
    return true;
  }

  return InputMethodEngine::UpdateMenuItems(items, error);
}

void NativeInputMethodEngine::OnInputMethodOptionsChanged() {
  if (ShouldRouteToNativeMojoEngine(GetActiveComponentId()) ||
      ShouldRouteToFirstPartyVietnameseInput(GetActiveComponentId())) {
    Enable(GetActiveComponentId());
  } else {
    InputMethodEngine::OnInputMethodOptionsChanged();
  }
}

}  // namespace input_method
}  // namespace ash
