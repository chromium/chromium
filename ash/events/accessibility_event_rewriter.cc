// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/accessibility_event_rewriter.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/accessibility/mouse_keys/mouse_keys_controller.h"
#include "ash/accessibility/switch_access/point_scan_controller.h"
#include "ash/constants/ash_constants.h"
#include "ash/keyboard/keyboard_util.h"
#include "ash/public/cpp/accessibility_event_rewriter_delegate.h"
#include "ash/shell.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/events/ash/event_rewriter_ash.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/types/event_type.h"

namespace ash {

namespace {

// Returns a ui::InputDeviceType given a Switch Access string device type.
ui::InputDeviceType GetInputDeviceType(
    const std::string& switch_access_device_type) {
  if (switch_access_device_type == kSwitchAccessInternalDevice) {
    return ui::INPUT_DEVICE_INTERNAL;
  } else if (switch_access_device_type == kSwitchAccessUsbDevice) {
    return ui::INPUT_DEVICE_USB;
  } else if (switch_access_device_type == kSwitchAccessBluetoothDevice) {
    return ui::INPUT_DEVICE_BLUETOOTH;
  } else {
    return ui::INPUT_DEVICE_UNKNOWN;
  }
}

#if !defined(NDEBUG)
void MaybeLogEventDispatchError(
    const ui::Event* event,
    const ui::EventRewriter::Continuation& continuation,
    const ui::EventDispatchDetails& details) {
  const char* failure_reason = nullptr;
  if (details.dispatcher_destroyed) {
    failure_reason = "destroyed dispatcher";
  } else if (details.target_destroyed) {
    failure_reason = "destroyed target";
  } else if (continuation.WasInvalidated()) {
    failure_reason = "destroyed source";
  } else {
    failure_reason = "no prior rewrite";
  }

  if (failure_reason) {
    VLOG(0) << "Undispatched key " << event->AsKeyEvent()->key_code()
            << " due to " << failure_reason << ".";
  }
}
#endif

}  // namespace

AccessibilityEventRewriter::PendingEventInfo::PendingEventInfo(
    unsigned int id,
    std::unique_ptr<ui::Event> event,
    ui::EventRewriter::Continuation continuation) {
  this->id = id;
  this->event = std::move(event);
  this->continuation = std::move(continuation);
}

AccessibilityEventRewriter::PendingEventInfo::~PendingEventInfo() = default;

AccessibilityEventRewriter::AccessibilityEventRewriter(
    ui::EventRewriterAsh* event_rewriter_ash,
    AccessibilityEventRewriterDelegate* delegate)
    : delegate_(delegate), event_rewriter_ash_(event_rewriter_ash) {
  Shell::Get()->accessibility_controller()->SetAccessibilityEventRewriter(this);
  observation_.Observe(input_method::InputMethodManager::Get());
  // InputMethodManagerImpl::AddObserver calls our InputMethodChanged, so no
  // further initialization needed.
}

AccessibilityEventRewriter::~AccessibilityEventRewriter() {
  Shell::Get()->accessibility_controller()->SetAccessibilityEventRewriter(
      nullptr);
}

void AccessibilityEventRewriter::OnUnhandledSpokenFeedbackEvent(
    std::unique_ptr<ui::Event> event) const {
  DCHECK(event->IsKeyEvent()) << "Unexpected unhandled event type";
  if (::features::IsAccessibilityManifestV3EnabledForChromeVox()) {
    // Unhandled events regularly come back in manifest v3 because DOM key
    // events are only handled if the menu or learn mode are active.
    return;
  }

  // Send the event to the continuation for the most recent event rewritten by
  // ChromeVox, (that is, through its EventSource). Under the assumption that a
  // single AccessibilityEventRewriter is not registered to multiple
  // EventSources, this will be the same as this event's original source.
  if (chromevox_continuation_) {
    SendEventHelper(chromevox_continuation_, event.get());
  }
}

void AccessibilityEventRewriter::ProcessPendingSpokenFeedbackEvent(
    unsigned int id,
    bool propagate) {
  // This method is only allowed for ChromeVox in manifest v3.
  CHECK(Shell::Get()->accessibility_controller()->spoken_feedback().enabled());
  CHECK(::features::IsAccessibilityManifestV3EnabledForChromeVox());
  CHECK(chromevox_mv3_key_handling_enabled_);
  CHECK(!pending_key_events_.empty());

  const auto& pending_event_info = pending_key_events_.front();
  CHECK_EQ(id, pending_event_info.id);
  if (propagate) {
    SendEventHelper(pending_event_info.continuation,
                    pending_event_info.event.get());
  }

  pending_key_events_.pop();
}

void AccessibilityEventRewriter::SendEventHelper(
    const ui::EventRewriter::Continuation continuation,
    const ui::Event* event) const {
#if !defined(NDEBUG)
  ui::EventDispatchDetails details = SendEvent(continuation, event);
  MaybeLogEventDispatchError(event, continuation, details);
#else
  std::ignore = SendEvent(continuation, event);
#endif
}

void AccessibilityEventRewriter::SetKeyCodesForSwitchAccessCommand(
    const std::map<int, std::set<std::string>>& key_codes,
    SwitchAccessCommand command) {
  // Remove all keys for the command.
  for (auto it = key_code_to_switch_access_command_.begin();
       it != key_code_to_switch_access_command_.end();) {
    if (it->second == command) {
      switch_access_key_codes_to_capture_.erase(it->first);
      it = key_code_to_switch_access_command_.erase(it);
    } else {
      it++;
    }
  }

  for (const auto& key_code : key_codes) {
    // Remove any preexisting key.
    switch_access_key_codes_to_capture_.erase(key_code.first);
    key_code_to_switch_access_command_.erase(key_code.first);

    // Map device types from Switch Access's internal representation.
    std::set<ui::InputDeviceType> device_types;
    for (const std::string& switch_access_device : key_code.second)
      device_types.insert(GetInputDeviceType(switch_access_device));

    switch_access_key_codes_to_capture_.insert({key_code.first, device_types});
    key_code_to_switch_access_command_.insert({key_code.first, command});
  }

  // Conflict resolution occurs up the stack (e.g. in the settings pages for
  // Switch Access).
}

void AccessibilityEventRewriter::SetSpokenFeedbackMv3KeyHandlingEnabled(
    bool enabled) {
  CHECK(::features::IsAccessibilityManifestV3EnabledForChromeVox());
  if (chromevox_mv3_key_handling_enabled_ == enabled) {
    return;
  }

  if (enabled) {
    // Ensure we are starting with a clean state.
    CHECK_EQ(0u, next_pending_event_id_);
    CHECK(pending_key_events_.empty());
  } else {
    // Post a task to propagate all pending events. We can't immediately
    // propagate them here because there is a chance that the front-most event
    // is still in-use; this happens if ChromeVox is disabled with the keyboard
    // accelerator.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &AccessibilityEventRewriter::SendAllPendingSpokenFeedbackEvents,
            GetWeakPtr()));
  }
  chromevox_mv3_key_handling_enabled_ = enabled;
}

bool AccessibilityEventRewriter::RewriteEventForChromeVox(
    const ui::Event& event,
    const Continuation continuation) {
  // Save continuation for |OnUnhandledSpokenFeedbackEvent()|.
  chromevox_continuation_ = continuation;

  if (!Shell::Get()->accessibility_controller()->spoken_feedback().enabled() ||
      !event.IsKeyEvent()) {
    return false;
  }

  if (::features::IsAccessibilityManifestV3EnabledForChromeVox() &&
      !chromevox_mv3_key_handling_enabled_) {
    return false;
  }

  const ui::KeyEvent* key_event = event.AsKeyEvent();
  ui::EventRewriterAsh::MutableKeyState state(key_event);

  // Remove the Search modifier before asking for function keys to be
  // rewritten, then restore the flags. This allows ChromeVox to receive keys
  // mappings for raw f1-f12 as e.g. back, but also Search+f1-f12 as
  // Search+back (rather than just f1-f12).
  int original_flags = state.flags;
  state.flags = original_flags & ~ui::EF_COMMAND_DOWN;
  event_rewriter_ash_->RewriteFunctionKeys(*key_event, &state);
  state.flags = original_flags;

  std::unique_ptr<ui::Event> rewritten_event;
  ui::EventRewriterAsh::BuildRewrittenKeyEvent(*key_event, state,
                                               &rewritten_event);
  ui::KeyEvent* rewritten_key_event = rewritten_event.get()->AsKeyEvent();

  // Account for positional keys which we want to remap.
  if (try_rewriting_positional_keys_for_chromevox_) {
    const ui::KeyboardCode remapped_key_code =
        ui::KeycodeConverter::MapPositionalDomCodeToUSShortcutKey(
            key_event->code(), key_event->key_code());
    if (remapped_key_code != ui::VKEY_UNKNOWN) {
      rewritten_key_event->set_key_code(remapped_key_code);
    }
  }

  if (::features::IsAccessibilityManifestV3EnabledForChromeVox() &&
      chromevox_mv3_key_handling_enabled_) {
    if (pending_key_events_.size() >= kMaxPendingEvents) {
      std::ostringstream errorStream;
      errorStream
          << "AccessibilityEventRewriter: dropping key event due to full "
             "queue"
          << rewritten_key_event->ToString();
      LOG(ERROR) << errorStream.str();
      static auto* const crash_key = base::debug::AllocateCrashKeyString(
          "chromevox_mv3_key_events", base::debug::CrashKeySize::Size1024);
      base::debug::SetCrashKeyString(crash_key, errorStream.str());
      base::debug::DumpWithoutCrashing();
      return true;
    }

    pending_key_events_.emplace(next_pending_event_id_,
                                rewritten_key_event->Clone(), continuation);
    // Forward the key event to the ChromeVox service worker.
    delegate_->DispatchKeyEventToChromeVoxMv3(next_pending_event_id_,
                                              rewritten_key_event->Clone());
    // Forward the key event to other ChromeVox extension contexts, like learn
    // mode and the panel.
    delegate_->DispatchKeyEventToChromeVox(rewritten_key_event->Clone(), true);

    ++next_pending_event_id_;

    // Key events in manifest v3 are always captured initially. If the extension
    // decides the key event should propagate, it will be propagated in
    // `ProcessPendingSpokenFeedbackEvent`.
    return true;
  }

  bool capture = chromevox_capture_all_keys_;

  // Always capture the Search key.
  capture |= rewritten_key_event->IsCommandDown() ||
             rewritten_key_event->key_code() == ui::VKEY_LWIN ||
             rewritten_key_event->key_code() == ui::VKEY_RWIN;

  // Don't capture tab as it gets consumed by Blink so never comes back
  // unhandled. In third_party/WebKit/Source/core/input/EventHandler.cpp, a
  // default tab handler consumes tab even when no focusable nodes are found;
  // it sets focus to Chrome and eats the event.
  if (rewritten_key_event->GetDomKey() == ui::DomKey::TAB) {
    capture = false;
  }

  delegate_->DispatchKeyEventToChromeVox(rewritten_key_event->Clone(), capture);
  return capture;
}

bool AccessibilityEventRewriter::RewriteEventForSwitchAccess(
    const ui::Event& event,
    const Continuation continuation) {
  if (!event.IsKeyEvent() || suspend_switch_access_key_handling_)
    return false;

  const ui::KeyEvent* key_event = event.AsKeyEvent();
  ui::EventRewriterAsh::MutableKeyState state(key_event);
  event_rewriter_ash_->RewriteFunctionKeys(*key_event, &state);

  std::unique_ptr<ui::Event> rewritten_event;
  ui::EventRewriterAsh::BuildRewrittenKeyEvent(*key_event, state,
                                               &rewritten_event);
  ui::KeyEvent* rewritten_key_event = rewritten_event.get()->AsKeyEvent();

  const auto& key =
      switch_access_key_codes_to_capture_.find(rewritten_key_event->key_code());
  if (key == switch_access_key_codes_to_capture_.end())
    return false;

  int source_device_id = key_event->source_device_id();
  ui::InputDeviceType keyboard_type = ui::INPUT_DEVICE_UNKNOWN;
  for (const auto& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (source_device_id == keyboard.id) {
      keyboard_type = keyboard.type;
      break;
    }
  }

  // An unknown |source_device_id| needs to pass this check as it's set that way
  // in tests.
  if (source_device_id != ui::ED_UNKNOWN_DEVICE &&
      key->second.count(keyboard_type) == 0) {
    return false;
  }

  if (key_event->type() == ui::EventType::kKeyPressed) {
    AccessibilityController* accessibility_controller =
        Shell::Get()->accessibility_controller();

    if (accessibility_controller->IsPointScanEnabled()) {
      PointScanController* point_scan_controller =
          accessibility_controller->GetPointScanController();
      std::optional<gfx::PointF> point = point_scan_controller->OnPointSelect();
      if (point.has_value()) {
        delegate_->SendPointScanPoint(point.value());
      }
    } else {
      SwitchAccessCommand command =
          key_code_to_switch_access_command_[rewritten_key_event->key_code()];
      delegate_->SendSwitchAccessCommand(command);
    }
  }
  return true;
}

bool AccessibilityEventRewriter::RewriteEventForMagnifier(
    const ui::Event& event,
    const Continuation continuation) {
  if (!event.IsKeyEvent())
    return false;

  const ui::KeyEvent* key_event = event.AsKeyEvent();

  if (!keyboard_util::IsArrowKeyCode(key_event->key_code()) ||
      !key_event->IsControlDown() || !key_event->IsAltDown()) {
    return false;
  }

  if (key_event->type() == ui::EventType::kKeyPressed) {
    // If first time key is pressed (e.g. not repeat), start scrolling.
    if (!(key_event->flags() & ui::EF_IS_REPEAT))
      OnMagnifierKeyPressed(key_event);

    // Either way (first or repeat), capture key press.
    return true;
  }

  if (key_event->type() == ui::EventType::kKeyReleased) {
    OnMagnifierKeyReleased(key_event);
    return true;
  }

  return false;
}

void AccessibilityEventRewriter::OnMagnifierKeyPressed(
    const ui::KeyEvent* event) {
  FullscreenMagnifierController* controller =
      Shell::Get()->fullscreen_magnifier_controller();
  switch (event->key_code()) {
    case ui::VKEY_UP:
      controller->SetScrollDirection(FullscreenMagnifierController::SCROLL_UP);
      delegate_->SendMagnifierCommand(MagnifierCommand::kMoveUp);
      break;
    case ui::VKEY_DOWN:
      controller->SetScrollDirection(
          FullscreenMagnifierController::SCROLL_DOWN);
      delegate_->SendMagnifierCommand(MagnifierCommand::kMoveDown);
      break;
    case ui::VKEY_LEFT:
      controller->SetScrollDirection(
          FullscreenMagnifierController::SCROLL_LEFT);
      delegate_->SendMagnifierCommand(MagnifierCommand::kMoveLeft);
      break;
    case ui::VKEY_RIGHT:
      controller->SetScrollDirection(
          FullscreenMagnifierController::SCROLL_RIGHT);
      delegate_->SendMagnifierCommand(MagnifierCommand::kMoveRight);
      break;
    default:
      NOTREACHED() << "Unexpected keyboard_code:" << event->key_code();
  }
}

void AccessibilityEventRewriter::OnMagnifierKeyReleased(
    const ui::KeyEvent* event) {
  FullscreenMagnifierController* controller =
      Shell::Get()->fullscreen_magnifier_controller();
  controller->SetScrollDirection(FullscreenMagnifierController::SCROLL_NONE);
  delegate_->SendMagnifierCommand(MagnifierCommand::kMoveStop);
}

void AccessibilityEventRewriter::MaybeSendMouseEvent(const ui::Event& event) {
  // Mouse moves are the only pertinent event for accessibility component
  // extensions.
  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();
  if (send_mouse_events_ &&
      (event.type() == ui::EventType::kMouseMoved ||
       event.type() == ui::EventType::kMouseDragged) &&
      (accessibility_controller->fullscreen_magnifier().enabled() ||
       accessibility_controller->docked_magnifier().enabled() ||
       accessibility_controller->spoken_feedback().enabled() ||
       accessibility_controller->face_gaze().enabled())) {
    delegate_->DispatchMouseEvent(event.Clone());
  }
}

ui::EventDispatchDetails AccessibilityEventRewriter::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  bool captured = false;
  if (!delegate_)
    return SendEvent(continuation, &event);

  // TODO(259372916): Switch to using the tray icon visibility.
  if (::features::IsAccessibilityMouseKeysEnabled()) {
    captured = Shell::Get()->mouse_keys_controller()->RewriteEvent(event);
  }

  if (!captured &&
      Shell::Get()->accessibility_controller()->IsSwitchAccessRunning()) {
    captured = RewriteEventForSwitchAccess(event, continuation);
  }

  if (!captured && Shell::Get()
                       ->accessibility_controller()
                       ->fullscreen_magnifier()
                       .enabled()) {
    captured = RewriteEventForMagnifier(event, continuation);
  }

  if (!captured) {
    captured = RewriteEventForChromeVox(event, continuation);
  }

  if (!captured) {
    MaybeSendMouseEvent(event);
  }

  return captured ? DiscardEvent(continuation)
                  : SendEvent(continuation, &event);
}

void AccessibilityEventRewriter::InputMethodChanged(
    input_method::InputMethodManager* manager,
    Profile* profile,
    bool show_message) {
  try_rewriting_positional_keys_for_chromevox_ =
      manager->ArePositionalShortcutsUsedByCurrentInputMethod();
}

void AccessibilityEventRewriter::SendAllPendingSpokenFeedbackEvents() {
  while (!pending_key_events_.empty()) {
    const auto& pending_event_info = pending_key_events_.front();
    SendEventHelper(pending_event_info.continuation,
                    pending_event_info.event.get());
    pending_key_events_.pop();
  }

  next_pending_event_id_ = 0;
}

}  // namespace ash
