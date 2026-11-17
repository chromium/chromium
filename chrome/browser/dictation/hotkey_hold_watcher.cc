// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/hotkey_hold_watcher.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/views/event_monitor.h"
#include "ui/views/widget/widget.h"

namespace dictation {

// static
bool HotkeyHoldWatcher::IsChordKey(const ui::Accelerator& hotkey,
                                   ui::KeyboardCode key_code) {
  if (key_code == hotkey.key_code()) {
    return true;
  }
  const int modifiers = hotkey.modifiers();
  if ((modifiers & ui::EF_SHIFT_DOWN) &&
      (key_code == ui::VKEY_SHIFT || key_code == ui::VKEY_LSHIFT ||
       key_code == ui::VKEY_RSHIFT)) {
    return true;
  }
  if ((modifiers & ui::EF_CONTROL_DOWN) &&
      (key_code == ui::VKEY_CONTROL || key_code == ui::VKEY_LCONTROL ||
       key_code == ui::VKEY_RCONTROL)) {
    return true;
  }
  if ((modifiers & ui::EF_ALT_DOWN) &&
      (key_code == ui::VKEY_MENU || key_code == ui::VKEY_LMENU ||
       key_code == ui::VKEY_RMENU)) {
    return true;
  }
  if ((modifiers & ui::EF_COMMAND_DOWN) &&
      (key_code == ui::VKEY_COMMAND || key_code == ui::VKEY_LWIN ||
       key_code == ui::VKEY_RWIN || key_code == ui::VKEY_APPS)) {
    return true;
  }
  return false;
}

HotkeyHoldWatcher::HotkeyHoldWatcher(BrowserWindowInterface& browser,
                                     tabs::TabInterface& tab,
                                     views::Widget& browser_widget,
                                     const ui::Accelerator& hotkey,
                                     CompletionCallback completion_callback,
                                     base::TimeDelta hold_threshold)
    : hotkey_(hotkey), completion_callback_(std::move(completion_callback)) {
  CHECK(!hotkey_.IsEmpty());
  CHECK(completion_callback_);
  CHECK(browser_widget.GetNativeWindow());

  widget_observation_.Observe(&browser_widget);

  // `base::Unretained(this)` is safe below because `this` owns the
  // subscriptions and `hold_threshold_timer_`.
  browser_inactive_subscription_ = browser.RegisterDidBecomeInactive(
      base::BindRepeating(&HotkeyHoldWatcher::OnBrowserDidBecomeInactive,
                          base::Unretained(this)));
  tab_deactivate_subscription_ = tab.RegisterWillDeactivate(base::BindRepeating(
      &HotkeyHoldWatcher::OnTabWillDeactivate, base::Unretained(this)));
  tab_detach_subscription_ = tab.RegisterWillDetach(base::BindRepeating(
      &HotkeyHoldWatcher::OnTabWillDetach, base::Unretained(this)));

  event_monitor_ = views::EventMonitor::CreateApplicationMonitor(
      this, browser_widget.GetNativeWindow(),
      {ui::EventType::kKeyPressed, ui::EventType::kKeyReleased});

  hold_threshold_timer_.Start(
      FROM_HERE, hold_threshold,
      base::BindOnce(&HotkeyHoldWatcher::OnHoldThresholdReached,
                     base::Unretained(this)));
}

HotkeyHoldWatcher::~HotkeyHoldWatcher() = default;

void HotkeyHoldWatcher::OnHoldThresholdReached() {
  is_press_and_hold_mode_ = true;
}

void HotkeyHoldWatcher::OnEvent(const ui::Event& event) {
  if (!event.IsKeyEvent()) {
    return;
  }
  const ui::KeyEvent& key_event = *event.AsKeyEvent();

  if (key_event.type() == ui::EventType::kKeyReleased) {
    if (IsChordKey(hotkey_, key_event.key_code())) {
      if (is_press_and_hold_mode_) {
        FinishAndDestroyThis(DictationStreamEndTrigger::kHotkeyHoldReleased);
        return;
      }
      // Released before the hold threshold elapsed: stay in toggle mode.
      FinishAndDestroyThis(std::nullopt);
      return;
    }
    return;
  }

  if (key_event.type() == ui::EventType::kKeyPressed) {
    // Auto-repeats of the held hotkey or its modifiers (as well as synthetic
    // IME composition key events) are expected while holding. Any secondary key
    // press outside the chord terminates the hold.
    if (key_event.key_code() != ui::VKEY_PROCESSKEY &&
        !IsChordKey(hotkey_, key_event.key_code())) {
      AbortAndDestroyThis();
      return;
    }
  }
}

void HotkeyHoldWatcher::OnWidgetDestroying(views::Widget* widget) {
  event_monitor_.reset();
  widget_observation_.Reset();
  AbortAndDestroyThis();
}

void HotkeyHoldWatcher::OnBrowserDidBecomeInactive(
    BrowserWindowInterface* browser) {
  AbortAndDestroyThis();
}

void HotkeyHoldWatcher::OnTabWillDeactivate(tabs::TabInterface* tab) {
  AbortAndDestroyThis();
}

void HotkeyHoldWatcher::OnTabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  AbortAndDestroyThis();
}

void HotkeyHoldWatcher::AbortAndDestroyThis() {
  FinishAndDestroyThis(
      is_press_and_hold_mode_
          ? std::optional(DictationStreamEndTrigger::kHotkeyHoldAbort)
          : std::nullopt);
}

void HotkeyHoldWatcher::FinishAndDestroyThis(
    std::optional<DictationStreamEndTrigger> end_trigger) {
  if (!completion_callback_) {
    return;
  }
  hold_threshold_timer_.Stop();
  event_monitor_.reset();
  std::move(completion_callback_).Run(end_trigger);
  // WARNING: `this` is deleted by `completion_callback_`, do not add code below
  // here.
}

}  // namespace dictation
