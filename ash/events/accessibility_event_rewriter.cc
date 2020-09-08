// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/accessibility_event_rewriter.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/accessibility_event_rewriter_delegate.h"
#include "ash/shell.h"
#include "ui/chromeos/events/event_rewriter_chromeos.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/types/event_type.h"

namespace ash {

AccessibilityEventRewriter::AccessibilityEventRewriter(
    ui::EventRewriterChromeOS* event_rewriter_chromeos,
    AccessibilityEventRewriterDelegate* delegate)
    : delegate_(delegate), event_rewriter_chromeos_(event_rewriter_chromeos) {
  Shell::Get()->accessibility_controller()->SetAccessibilityEventRewriter(this);

  // By default, observe all input device types.
  keyboard_input_device_types_.insert(ui::INPUT_DEVICE_INTERNAL);
  keyboard_input_device_types_.insert(ui::INPUT_DEVICE_USB);
  keyboard_input_device_types_.insert(ui::INPUT_DEVICE_BLUETOOTH);
  keyboard_input_device_types_.insert(ui::INPUT_DEVICE_UNKNOWN);

  UpdateKeyboardDeviceIds();

  observer_.Add(ui::DeviceDataManager::GetInstance());
}

AccessibilityEventRewriter::~AccessibilityEventRewriter() {
  Shell::Get()->accessibility_controller()->SetAccessibilityEventRewriter(
      nullptr);
}

void AccessibilityEventRewriter::OnUnhandledSpokenFeedbackEvent(
    std::unique_ptr<ui::Event> event) const {
  DCHECK(event->IsKeyEvent()) << "Unexpected unhandled event type";
  // Send the event to the continuation for the most recent event rewritten by
  // ChromeVox, (that is, through its EventSource). Under the assumption that a
  // single AccessibilityEventRewriter is not registered to multiple
  // EventSources, this will be the same as this event's original source.
  const char* failure_reason = nullptr;
  if (chromevox_continuation_) {
    ui::EventDispatchDetails details =
        SendEvent(chromevox_continuation_, event.get());
    if (details.dispatcher_destroyed)
      failure_reason = "destroyed dispatcher";
    else if (details.target_destroyed)
      failure_reason = "destroyed target";
  } else if (chromevox_continuation_.WasInvalidated()) {
    failure_reason = "destroyed source";
  } else {
    failure_reason = "no prior rewrite";
  }
  if (failure_reason) {
    VLOG(0) << "Undispatched key " << event->AsKeyEvent()->key_code()
            << " due to " << failure_reason << ".";
  }
}

bool AccessibilityEventRewriter::SetKeyCodesForSwitchAccessCommand(
    std::set<int> new_key_codes,
    SwitchAccessCommand command) {
  bool has_changed = false;
  std::set<int> to_clear;

  // Clear old values that conflict with the new assignment.
  // TODO(anastasi): convert to use iterators directly and remove has_changed as
  // an extra step.
  for (const auto& val : key_code_to_switch_access_command_) {
    int old_key_code = val.first;
    SwitchAccessCommand old_command = val.second;

    if (new_key_codes.count(old_key_code) > 0) {
      if (old_command != command) {
        has_changed = true;
        // Modifying the map while iterating through it causes reference
        // failures.
        to_clear.insert(old_key_code);
      } else {
        new_key_codes.erase(old_key_code);
      }
      continue;
    }

    // This value was previously mapped to the command, but is no longer.
    if (old_command == command) {
      has_changed = true;
      to_clear.insert(old_key_code);
      switch_access_key_codes_to_capture_.erase(old_key_code);
    }
  }
  for (int key_code : to_clear) {
    key_code_to_switch_access_command_.erase(key_code);
  }

  if (new_key_codes.size() == 0)
    return has_changed;

  // Add any new key codes to the map.
  for (int key_code : new_key_codes) {
    switch_access_key_codes_to_capture_.insert(key_code);
    key_code_to_switch_access_command_[key_code] = command;
  }

  return true;
}

void AccessibilityEventRewriter::SetKeyboardInputDeviceTypes(
    const std::set<ui::InputDeviceType>& keyboard_input_device_types) {
  keyboard_input_device_types_ = keyboard_input_device_types;
  UpdateKeyboardDeviceIds();
}

bool AccessibilityEventRewriter::RewriteEventForChromeVox(
    const ui::Event& event,
    const Continuation continuation) {
  // Save continuation for |OnUnhandledSpokenFeedbackEvent()|.
  chromevox_continuation_ = continuation;

  if (!Shell::Get()->accessibility_controller()->spoken_feedback().enabled()) {
    return false;
  }

  if (event.IsKeyEvent()) {
    const ui::KeyEvent* key_event = event.AsKeyEvent();
    ui::EventRewriterChromeOS::MutableKeyState state(key_event);
    event_rewriter_chromeos_->RewriteModifierKeys(*key_event, &state);

    // Remove the Search modifier before asking for function keys to be
    // rewritten, then restore the flags. This allows ChromeVox to receive keys
    // mappings for raw f1-f12 as e.g. back, but also Search+f1-f12 as
    // Search+back (rather than just f1-f12).
    int original_flags = state.flags;
    state.flags = original_flags & ~ui::EF_COMMAND_DOWN;
    event_rewriter_chromeos_->RewriteFunctionKeys(*key_event, &state);
    state.flags = original_flags;

    std::unique_ptr<ui::Event> rewritten_event;
    ui::EventRewriterChromeOS::BuildRewrittenKeyEvent(*key_event, state,
                                                      &rewritten_event);
    const ui::KeyEvent* rewritten_key_event =
        rewritten_event.get()->AsKeyEvent();

    bool capture = chromevox_capture_all_keys_;

    // Always capture the Search key.
    capture |= rewritten_key_event->IsCommandDown() ||
               rewritten_key_event->key_code() == ui::VKEY_LWIN;

    // Don't capture tab as it gets consumed by Blink so never comes back
    // unhandled. In third_party/WebKit/Source/core/input/EventHandler.cpp, a
    // default tab handler consumes tab even when no focusable nodes are found;
    // it sets focus to Chrome and eats the event.
    if (rewritten_key_event->GetDomKey() == ui::DomKey::TAB)
      capture = false;

    delegate_->DispatchKeyEventToChromeVox(
        ui::Event::Clone(*rewritten_key_event), capture);
    return capture;
  }

  if (chromevox_send_mouse_events_ && event.IsMouseEvent())
    delegate_->DispatchMouseEventToChromeVox(ui::Event::Clone(event));

  return false;
}

bool AccessibilityEventRewriter::RewriteEventForSwitchAccess(
    const ui::Event& event,
    const Continuation continuation) {
  if (!event.IsKeyEvent())
    return false;

  const ui::KeyEvent* key_event = event.AsKeyEvent();
  bool capture =
      switch_access_key_codes_to_capture_.count(key_event->key_code()) > 0;

  if (capture && key_event->type() == ui::ET_KEY_PRESSED) {
    SwitchAccessCommand command =
        key_code_to_switch_access_command_[key_event->key_code()];
    delegate_->SendSwitchAccessCommand(command);
  }
  return capture;
}

void AccessibilityEventRewriter::UpdateKeyboardDeviceIds() {
  keyboard_device_ids_.clear();
  for (auto& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (keyboard_input_device_types_.count(keyboard.type))
      keyboard_device_ids_.insert(keyboard.id);
  }
}

ui::EventDispatchDetails AccessibilityEventRewriter::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  if (event.IsKeyEvent() && event.source_device_id() != ui::ED_UNKNOWN_DEVICE &&
      keyboard_device_ids_.count(event.source_device_id()) == 0) {
    return SendEvent(continuation, &event);
  }

  bool captured = false;
  if (!delegate_)
    return SendEvent(continuation, &event);

  if (Shell::Get()->accessibility_controller()->IsSwitchAccessRunning()) {
    captured = RewriteEventForSwitchAccess(event, continuation);
  }

  if (!captured) {
    captured = RewriteEventForChromeVox(event, continuation);
  }

  return captured ? DiscardEvent(continuation)
                  : SendEvent(continuation, &event);
}

void AccessibilityEventRewriter::OnInputDeviceConfigurationChanged(
    uint8_t input_device_types) {
  if (input_device_types & ui::InputDeviceEventObserver::kKeyboard)
    UpdateKeyboardDeviceIds();
}

}  // namespace ash
