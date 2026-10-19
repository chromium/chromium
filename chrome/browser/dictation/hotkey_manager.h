// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_HOTKEY_MANAGER_H_
#define CHROME_BROWSER_DICTATION_HOTKEY_MANAGER_H_

#include <memory>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/dictation/session_state.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/accelerators/accelerator.h"

class Profile;

namespace ui {
class GlobalAcceleratorListener;
}

namespace dictation {

class DictationKeyedService;
class ScopedGlobalHotkey;

// Returns the Dictation hotkey from prefs, or an empty accelerator if it is
// unset or not registrable. Use this so all callers agree on the hotkey.
ui::Accelerator GetDictationHotkeyFromPrefs(Profile* profile);

// Returns whether `state` counts as listening, i.e. whether the OS-wide hotkey
// should be registered.
bool IsListeningState(SessionState state);

// Manages the hotkey for Dictation.
// It listens to preference changes and registers the hotkey with standard
// priority using a delegate. While Dictation is listening, it also registers
// the hotkey OS-wide so that Dictation can be stopped from any window.
class HotkeyManager : public ui::AcceleratorTarget {
 public:
  // Interface for managing the lifetime of a registered hotkey.
  // Implementations handle the specific registration/unregistration logic
  // for their scope.
  class ScopedHotkeyRegistration {
   public:
    virtual ~ScopedHotkeyRegistration() = default;
  };

  // Delegate for registering hotkeys. Allows for different registration
  // mechanisms.
  class RegistrationDelegate {
   public:
    virtual ~RegistrationDelegate() = default;
    virtual std::unique_ptr<ScopedHotkeyRegistration>
    CreateScopedHotkeyRegistration(Profile* profile,
                                   ui::Accelerator accelerator,
                                   HotkeyManager& hotkey_manager) = 0;
  };

  // `is_listening` is the initial listening state (see
  // OnSessionStateChanged()); when true the OS-wide hotkey is registered
  // immediately.
  HotkeyManager(Profile* profile,
                std::unique_ptr<RegistrationDelegate> registration_delegate,
                bool is_listening);

  HotkeyManager(const HotkeyManager&) = delete;
  HotkeyManager& operator=(const HotkeyManager&) = delete;

  ~HotkeyManager() override;

  // Overrides the listener used for the OS-wide hotkey (by default
  // ui::GlobalAcceleratorListener::GetInstance()) for all managers until the
  // returned object is destroyed. The listener is looked up at registration
  // time, so this also applies to managers that already exist.
  [[nodiscard]] static base::AutoReset<ui::GlobalAcceleratorListener*>
  SetGlobalAcceleratorListenerForTesting(
      ui::GlobalAcceleratorListener* listener);

  // Registers the OS-wide hotkey while `state` is listening (initializing or
  // transcribing), and unregisters it otherwise.
  void OnSessionStateChanged(SessionState state);

  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  // Null unless Dictation is listening.
  ScopedGlobalHotkey* global_hotkey_for_testing() {
    return global_hotkey_.get();
  }

 private:
  void OnHotkeyPrefChanged();
  void UpdateRegistration();
  // Registers, re-registers or unregisters the OS-wide hotkey to match
  // `listening_` and the hotkey pref.
  void UpdateGlobalRegistration();
  void OnGlobalHotkeyPressed();

  // Returns the service for `profile_`. Looked up on use rather than in the
  // constructor, since the service may create this during its own
  // construction, before the factory has registered it.
  DictationKeyedService& service();

  raw_ptr<Profile> profile_;
  std::unique_ptr<RegistrationDelegate> registration_delegate_;

  PrefChangeRegistrar pref_registrar_;
  std::unique_ptr<ScopedHotkeyRegistration> hotkey_registration_;

  bool listening_ = false;
  std::unique_ptr<ScopedGlobalHotkey> global_hotkey_;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_HOTKEY_MANAGER_H_
