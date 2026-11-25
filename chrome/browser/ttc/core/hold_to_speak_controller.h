// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_HOLD_TO_SPEAK_CONTROLLER_H_
#define CHROME_BROWSER_TTC_CORE_HOLD_TO_SPEAK_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "chrome/browser/voice/hotkeys/hotkey_hold_watcher.h"
#include "chrome/browser/voice/hotkeys/hotkey_registration_delegate.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/accelerators/accelerator.h"

namespace ttc {

class SessionViewDelegate;

// Hold-to-speak for a TTC session. While the session is live, holding the
// hold-to-speak shortcut (prefs::kTtcHoldToSpeakShortcut) for
// kTtcHoldToSpeakThreshold turns the microphone on, and releasing it turns
// the microphone off. Shorter presses do nothing. The hotkey works while any of
// the profile's browser windows, or the session's voice plate, has focus.
//
// Owned by SessionViewImpl, so the hotkey is only registered during a session.
class HoldToSpeakController : public ui::AcceleratorTarget {
 public:
  // The hotkey is registered with the profile's browser windows through
  // `browser_registration_delegate`, and with the session's voice plate, which
  // has its own FocusManager, through `overlay_registration_delegate`.
  // `delegate` and `overlay_registration_delegate` must outlive `this`. The
  // constructor only calls `delegate.GetProfile()`, since the delegate may
  // still be being constructed.
  HoldToSpeakController(
      SessionViewDelegate& delegate,
      std::unique_ptr<voice::HotkeyRegistrationDelegate>
          browser_registration_delegate,
      voice::HotkeyRegistrationDelegate& overlay_registration_delegate);
  HoldToSpeakController(const HoldToSpeakController&) = delete;
  HoldToSpeakController& operator=(const HoldToSpeakController&) = delete;
  ~HoldToSpeakController() override;

  voice::HotkeyHoldWatcher* hold_watcher_for_testing() {
    return hold_watcher_.get();
  }

  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

 private:
  // Returns the hotkey from prefs, or an empty accelerator if there is none.
  ui::Accelerator GetHotkey() const;
  void UpdateRegistration();

  void OnHoldThresholdReached();
  void OnHoldEnded(voice::HotkeyHoldWatcher::Result result);

  const raw_ref<SessionViewDelegate> delegate_;
  const std::unique_ptr<voice::HotkeyRegistrationDelegate>
      browser_registration_delegate_;
  const raw_ref<voice::HotkeyRegistrationDelegate>
      overlay_registration_delegate_;
  PrefChangeRegistrar pref_registrar_;
  // The hotkey's registrations through the delegates above.
  std::unique_ptr<voice::HotkeyRegistrationDelegate::ScopedRegistration>
      browser_registration_;
  std::unique_ptr<voice::HotkeyRegistrationDelegate::ScopedRegistration>
      overlay_registration_;
  // Watches the hotkey from its press until its release.
  std::unique_ptr<voice::HotkeyHoldWatcher> hold_watcher_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_HOLD_TO_SPEAK_CONTROLLER_H_
