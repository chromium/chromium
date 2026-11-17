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
#include "chrome/browser/dictation/hotkey_hold_watcher.h"
#include "chrome/browser/dictation/logging.h"
#include "chrome/browser/dictation/metrics.h"
#include "chrome/browser/dictation/scoped_global_hotkey.h"
#include "chrome/browser/dictation/session_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/command.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"
#include "ui/views/widget/widget.h"

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
  if (!listening_) {
    hold_watcher_.reset();
  }
  UpdateGlobalRegistration();
}

void HotkeyManager::StartHoldWatcher() {
  const ui::Accelerator hold_accelerator =
      GetDictationPressAndHoldHotkeyFromPrefs(profile_);
  CHECK(!hold_accelerator.IsEmpty());

  BrowserWindowInterface* const browser =
      GlobalBrowserCollection::GetInstance()->GetLastActiveBrowser();
  if (!browser || browser->GetProfile() != profile_) {
    return;
  }
  tabs::TabInterface* const tab = browser->GetActiveTabInterface();
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  views::Widget* const widget =
      browser_view ? browser_view->GetWidget() : nullptr;
  if (!tab || !widget || !widget->GetNativeWindow()) {
    return;
  }

  const bool is_shared_hotkey =
      hold_accelerator == GetDictationHotkeyFromPrefs(profile_);
  const base::TimeDelta hold_threshold =
      is_shared_hotkey ? kDictationSharedHotkeyPressAndHoldThreshold.Get()
                       : kDictationPressAndHoldThreshold.Get();

  // `base::Unretained(this)` is safe because `this` owns `hold_watcher_`.
  hold_watcher_ = std::make_unique<HotkeyHoldWatcher>(
      *browser, *tab, *widget, hold_accelerator,
      base::BindOnce(&HotkeyManager::OnHotkeyHoldEnded, base::Unretained(this)),
      hold_threshold);
}

void HotkeyManager::OnHotkeyHoldEnded(
    std::optional<DictationStreamEndTrigger> trigger) {
  hold_watcher_.reset();
  UpdateGlobalRegistration();

  // If `trigger` is nullopt, the key was released before the hold threshold,
  // so Dictation stays active in toggle mode.
  if (!trigger.has_value()) {
    return;
  }
  if (SessionController* const controller = service().session_controller();
      controller && controller->attached_stream_provider()) {
    controller->EndDictationStream(*trigger);
  }
}

DictationKeyedService& HotkeyManager::service() {
  DictationKeyedService* const service = DictationKeyedService::Get(profile_);
  CHECK(service);
  return *service;
}

bool HotkeyManager::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (accelerator.IsRepeat() || is_hold_watcher_active()) {
    return true;
  }
  const ui::Accelerator hold_accelerator =
      GetDictationPressAndHoldHotkeyFromPrefs(profile_);
  if (!hold_accelerator.IsEmpty() && accelerator == hold_accelerator) {
    // Construct `hold_watcher_` before triggering the session so that
    // `UpdateGlobalRegistration()` sees `hold_watcher_` and defers registering
    // the OS-wide global hotkey while the hold watcher is active.
    StartHoldWatcher();
    if (!service().PressAndHoldHotkeyHandler() || !listening_) {
      hold_watcher_.reset();
      UpdateGlobalRegistration();
    } else if (!hold_watcher_) {
      // If `PressAndHoldHotkeyHandler()` switched from a previous session in
      // another window (which briefly transitioned through a non-listening
      // state and cleared `hold_watcher_`), recreate it for the new session.
      StartHoldWatcher();
      UpdateGlobalRegistration();
    }
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
      (listening_ && !hold_watcher_) ? GetDictationHotkeyFromPrefs(profile_)
                                     : ui::Accelerator();
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
