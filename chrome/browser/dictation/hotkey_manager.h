// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_HOTKEY_MANAGER_H_
#define CHROME_BROWSER_DICTATION_HOTKEY_MANAGER_H_

#include <memory>
#include <optional>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/dictation/session_state.h"
#include "chrome/browser/dictation/stream_provider.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/accelerators/accelerator.h"

class Profile;

namespace ui {
class GlobalAcceleratorListener;
}

namespace dictation {

class DictationKeyedService;
class HotkeyHoldWatcher;
class ScopedGlobalHotkey;

// Returns the Dictation hotkey from prefs, or an empty accelerator if it is
// unset or not registrable. Use this so all callers agree on the hotkey.
ui::Accelerator GetDictationHotkeyFromPrefs(Profile* profile);

// Returns the Dictation press-and-hold hotkey from prefs, or an empty
// accelerator if it is unset or not registrable. May equal
// GetDictationHotkeyFromPrefs() when the user configures the same shortcut for
// both toggle and press-and-hold.
ui::Accelerator GetDictationPressAndHoldHotkeyFromPrefs(Profile* profile);

// Returns whether `state` counts as listening, i.e. whether the OS-wide hotkey
// should be registered.
bool IsListeningState(SessionState state);

// Manages the hotkeys for Dictation:
// - Toggle hotkey: pressing once starts listening, and pressing again stops
//   listening.
// - Press-and-hold hotkey: holding the shortcut starts listening and releasing
//   it stops listening (push-to-talk).
// It listens to preference changes and registers both hotkeys with standard
// priority using a delegate. While Dictation is listening, it also registers
// the toggle hotkey OS-wide so that Dictation can be stopped from any window.
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
  // transcribing), and unregisters it otherwise. Also clears any active
  // `hold_watcher_` when leaving a listening state.
  void OnSessionStateChanged(SessionState state);

  bool is_hold_watcher_active() const { return hold_watcher_ != nullptr; }
  HotkeyHoldWatcher* hold_watcher_for_testing() { return hold_watcher_.get(); }

  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  // Null unless Dictation is listening.
  ScopedGlobalHotkey* global_hotkey_for_testing() {
    return global_hotkey_.get();
  }

 private:
  void OnToggleHotkeyPrefChanged();
  void OnPressAndHoldHotkeyPrefChanged();
  void UpdateHotkeyRegistrations();
  // Registers, re-registers or unregisters the OS-wide hotkey to match
  // `listening_`, `hold_watcher_`, and the hotkey pref.
  void UpdateGlobalRegistration();
  void OnGlobalHotkeyPressed();
  // Starts watching the active browser window and tab for release of the
  // press-and-hold hotkey. While active, defers OS-wide global hotkey
  // registration so local key-release events are not intercepted.
  void StartHoldWatcher();
  void OnHotkeyHoldEnded(std::optional<DictationStreamEndTrigger> trigger);

  // Returns the service for `profile_`. Looked up on use rather than in the
  // constructor, since the service may create this during its own
  // construction, before the factory has registered it.
  DictationKeyedService& service();

  raw_ptr<Profile> profile_;
  std::unique_ptr<RegistrationDelegate> registration_delegate_;

  PrefChangeRegistrar pref_registrar_;
  std::unique_ptr<ScopedHotkeyRegistration> toggle_hotkey_registration_;
  std::unique_ptr<ScopedHotkeyRegistration> press_and_hold_hotkey_registration_;

  bool listening_ = false;
  std::unique_ptr<HotkeyHoldWatcher> hold_watcher_;
  std::unique_ptr<ScopedGlobalHotkey> global_hotkey_;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_HOTKEY_MANAGER_H_
