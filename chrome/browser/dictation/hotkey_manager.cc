// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/hotkey_manager.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/logging.h"
#include "chrome/browser/dictation/metrics.h"
#include "chrome/browser/dictation/scoped_global_hotkey.h"
#include "chrome/browser/dictation/session_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/command.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"

namespace dictation {

namespace {

ui::Accelerator ParseValidDictationAccelerator(
    const std::string& accelerator_str) {
  if (accelerator_str.empty()) {
    return ui::Accelerator();
  }

  const ui::Accelerator accelerator =
      ui::Command::StringToAccelerator(accelerator_str);

  // StringToAccelerator() returns an empty accelerator when the pref can't be
  // parsed, and it already requires parsed hotkeys to have a modifier. This is
  // defense in depth: never register a modifier-less hotkey, which would
  // swallow ordinary keystrokes.
  if (ui::Accelerator::MaskOutKeyEventFlags(accelerator.modifiers()) == 0) {
    return ui::Accelerator();
  }

  return accelerator;
}

ui::GlobalAcceleratorListener* g_global_listener_for_testing = nullptr;

ui::GlobalAcceleratorListener* GetGlobalListener() {
  return g_global_listener_for_testing
             ? g_global_listener_for_testing
             : ui::GlobalAcceleratorListener::GetInstance();
}

}  // namespace

ui::Accelerator GetDictationHotkeyFromPrefs(Profile* profile) {
  CHECK(profile);
  return ParseValidDictationAccelerator(
      profile->GetPrefs()->GetString(prefs::kVoiceTypingHotkey));
}

ui::Accelerator GetDictationPressAndHoldHotkeyFromPrefs(Profile* profile) {
  CHECK(profile);
  if (!base::FeatureList::IsEnabled(kDictationPressAndHold)) {
    return ui::Accelerator();
  }
  return ParseValidDictationAccelerator(
      profile->GetPrefs()->GetString(prefs::kVoiceTypingPressAndHoldHotkey));
}

bool IsListeningState(SessionState state) {
  return state == SessionState::kStreamInitializing ||
         state == SessionState::kTranscribing;
}

HotkeyManager::HotkeyManager(
    Profile* profile,
    std::unique_ptr<RegistrationDelegate> registration_delegate,
    bool is_listening)
    : profile_(profile),
      registration_delegate_(std::move(registration_delegate)),
      listening_(is_listening) {
  CHECK(profile_);
  CHECK(registration_delegate_);

  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(
      prefs::kVoiceTypingHotkey,
      base::BindRepeating(&HotkeyManager::OnToggleHotkeyPrefChanged,
                          base::Unretained(this)));
  pref_registrar_.Add(
      prefs::kVoiceTypingPressAndHoldHotkey,
      base::BindRepeating(&HotkeyManager::OnPressAndHoldHotkeyPrefChanged,
                          base::Unretained(this)));

  UpdateHotkeyRegistrations();
  UpdateGlobalRegistration();
}

HotkeyManager::~HotkeyManager() = default;

// static
base::AutoReset<ui::GlobalAcceleratorListener*>
HotkeyManager::SetGlobalAcceleratorListenerForTesting(
    ui::GlobalAcceleratorListener* listener) {
  return base::AutoReset<ui::GlobalAcceleratorListener*>(
      &g_global_listener_for_testing, listener);
}

void HotkeyManager::OnSessionStateChanged(SessionState state) {
  listening_ = IsListeningState(state);
  UpdateGlobalRegistration();
}

DictationKeyedService& HotkeyManager::service() {
  DictationKeyedService* const service = DictationKeyedService::Get(profile_);
  CHECK(service);
  return *service;
}

bool HotkeyManager::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (accelerator.IsRepeat()) {
    return true;
  }
  const ui::Accelerator hold_accelerator =
      GetDictationPressAndHoldHotkeyFromPrefs(profile_);
  if (!hold_accelerator.IsEmpty() && accelerator == hold_accelerator) {
    service().PressAndHoldHotkeyHandler();
    return true;
  }
  CHECK(accelerator == GetDictationHotkeyFromPrefs(profile_));
  service().ToggleHotkeyHandler();
  return true;
}

bool HotkeyManager::CanHandleAccelerators() const {
  return true;
}

void HotkeyManager::OnToggleHotkeyPrefChanged() {
  UpdateHotkeyRegistrations();
  UpdateGlobalRegistration();
}

void HotkeyManager::OnPressAndHoldHotkeyPrefChanged() {
  UpdateHotkeyRegistrations();
}

void HotkeyManager::UpdateHotkeyRegistrations() {
  toggle_hotkey_registration_.reset();
  press_and_hold_hotkey_registration_.reset();

  const ui::Accelerator toggle_accelerator =
      GetDictationHotkeyFromPrefs(profile_);
  if (!toggle_accelerator.IsEmpty()) {
    toggle_hotkey_registration_ =
        registration_delegate_->CreateScopedHotkeyRegistration(
            profile_, toggle_accelerator, *this);
  }

  const ui::Accelerator hold_accelerator =
      GetDictationPressAndHoldHotkeyFromPrefs(profile_);
  // When both modes use the same hotkey, `toggle_hotkey_registration_` already
  // registers the accelerator on the FocusManager for `*this`.
  if (!hold_accelerator.IsEmpty() && hold_accelerator != toggle_accelerator) {
    press_and_hold_hotkey_registration_ =
        registration_delegate_->CreateScopedHotkeyRegistration(
            profile_, hold_accelerator, *this);
  }
}

void HotkeyManager::UpdateGlobalRegistration() {
  const ui::Accelerator accelerator =
      listening_ ? GetDictationHotkeyFromPrefs(profile_) : ui::Accelerator();
  if (accelerator.IsEmpty()) {
    global_hotkey_.reset();
    return;
  }
  if (global_hotkey_ && global_hotkey_->accelerator() == accelerator) {
    return;
  }

  // Unregister any previous accelerator before registering the new one.
  global_hotkey_.reset();
  // Unretained is safe: `this` owns `global_hotkey_`.
  global_hotkey_ = std::make_unique<ScopedGlobalHotkey>(
      GetGlobalListener(), accelerator,
      base::BindRepeating(&HotkeyManager::OnGlobalHotkeyPressed,
                          base::Unretained(this)));
  VT_LOG(profile_) << "Global hotkey registered: "
                   << global_hotkey_->is_registered();
}

void HotkeyManager::OnGlobalHotkeyPressed() {
  // The OS intercepts the global hotkey, so the local one never fires. If this
  // profile's window is focused, keep the in-Chrome toggle behavior.
  BrowserWindowInterface* const active_browser =
      GlobalBrowserCollection::GetInstance()->GetLastActiveBrowser();
  if (active_browser && active_browser->IsActive() &&
      active_browser->GetProfile() == profile_) {
    service().ToggleHotkeyHandler();
    return;
  }

  // The global hotkey is only registered while listening, which implies an
  // attached stream.
  SessionController* const controller = service().session_controller();
  CHECK(controller);
  CHECK(controller->attached_stream_provider());
  VT_LOG(profile_) << "Global hotkey: ending stream";
  controller->EndDictationStream(DictationStreamEndTrigger::kHotkeyToggle);
}

}  // namespace dictation
