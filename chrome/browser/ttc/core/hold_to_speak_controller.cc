// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/hold_to_speak_controller.h"

#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/prefs.h"
#include "chrome/browser/ttc/core/session_view_delegate.h"
#include "chrome/browser/ttc/core/states.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/voice/hotkeys/hotkey_pref_util.h"
#include "components/prefs/pref_service.h"
#include "ui/views/widget/widget.h"

namespace ttc {

HoldToSpeakController::HoldToSpeakController(
    SessionViewDelegate& delegate,
    std::unique_ptr<voice::HotkeyRegistrationDelegate>
        browser_registration_delegate,
    voice::HotkeyRegistrationDelegate& overlay_registration_delegate)
    : delegate_(delegate),
      browser_registration_delegate_(std::move(browser_registration_delegate)),
      overlay_registration_delegate_(overlay_registration_delegate) {
  CHECK(browser_registration_delegate_);
  pref_registrar_.Init(CHECK_DEREF(delegate_->GetProfile()).GetPrefs());
  // `base::Unretained(this)` is safe because `this` owns `pref_registrar_`.
  pref_registrar_.Add(
      prefs::kTtcHoldToSpeakShortcut,
      base::BindRepeating(&HoldToSpeakController::UpdateRegistration,
                          base::Unretained(this)));
  UpdateRegistration();
}

HoldToSpeakController::~HoldToSpeakController() = default;

bool HoldToSpeakController::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  // Ignore auto-repeats, and presses while a hold is already being watched.
  if (accelerator.IsRepeat() || hold_watcher_) {
    return true;
  }
  // Don't CHECK: an outdated registration may still be dispatched.
  if (accelerator != GetHotkey()) {
    return false;
  }
  // Only a live session's microphone can be turned on.
  if (delegate_->GetSessionLifecycle() != SessionLifecycle::kLive) {
    return true;
  }

  BrowserWindowInterface* const browser =
      GlobalBrowserCollection::GetInstance()->GetLastActiveBrowser();
  if (!browser || browser->GetProfile() != delegate_->GetProfile()) {
    return true;
  }
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  views::Widget* const widget =
      browser_view ? browser_view->GetWidget() : nullptr;
  if (!widget || !widget->GetNativeWindow()) {
    return true;
  }

  // `base::Unretained(this)` is safe because `this` owns `hold_watcher_`.
  hold_watcher_ = std::make_unique<voice::HotkeyHoldWatcher>(
      *browser, *widget, accelerator, kTtcHoldToSpeakThreshold.Get(),
      base::BindOnce(&HoldToSpeakController::OnHoldThresholdReached,
                     base::Unretained(this)),
      base::BindOnce(&HoldToSpeakController::OnHoldEnded,
                     base::Unretained(this)));
  return true;
}

bool HoldToSpeakController::CanHandleAccelerators() const {
  return true;
}

ui::Accelerator HoldToSpeakController::GetHotkey() const {
  return voice::ParseHotkeyPref(
      pref_registrar_.prefs()->GetString(prefs::kTtcHoldToSpeakShortcut));
}

void HoldToSpeakController::UpdateRegistration() {
  browser_registration_.reset();
  overlay_registration_.reset();
  const ui::Accelerator hotkey = GetHotkey();
  if (!hotkey.IsEmpty()) {
    browser_registration_ =
        browser_registration_delegate_->Register(hotkey, *this);
    overlay_registration_ =
        overlay_registration_delegate_->Register(hotkey, *this);
  }
}

void HoldToSpeakController::OnHoldThresholdReached() {
  // No-op if the microphone is already on, or the session is no longer live.
  delegate_->SetMicMuted(false);
}

void HoldToSpeakController::OnHoldEnded(
    voice::HotkeyHoldWatcher::Result result) {
  hold_watcher_.reset();
  // A short press does nothing. Otherwise the hotkey was released, or the hold
  // was interrupted (e.g. the window lost focus) so its release can't be seen:
  // either way, turn the microphone off so it doesn't stay on after the hold.
  if (result != voice::HotkeyHoldWatcher::Result::kTapped) {
    delegate_->SetMicMuted(true);
  }
}

}  // namespace ttc
