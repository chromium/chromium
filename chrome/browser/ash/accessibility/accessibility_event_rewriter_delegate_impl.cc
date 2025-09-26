// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/accessibility_event_rewriter_delegate_impl.h"

#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/event_rewriter_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/event_handler_common.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/common/constants.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace ash {
namespace {

std::string ToString(SwitchAccessCommand command) {
  switch (command) {
    case SwitchAccessCommand::kSelect:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::SwitchAccessCommand::kSelect);
    case SwitchAccessCommand::kNext:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::SwitchAccessCommand::kNext);
    case SwitchAccessCommand::kPrevious:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::SwitchAccessCommand::
              kPrevious);
    case SwitchAccessCommand::kNone:
      NOTREACHED();
  }
}

std::string ToString(MagnifierCommand command) {
  switch (command) {
    case MagnifierCommand::kMoveStop:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::MagnifierCommand::kMoveStop);
    case MagnifierCommand::kMoveUp:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::MagnifierCommand::kMoveUp);
    case MagnifierCommand::kMoveDown:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::MagnifierCommand::kMoveDown);
    case MagnifierCommand::kMoveLeft:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::MagnifierCommand::kMoveLeft);
    case MagnifierCommand::kMoveRight:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::MagnifierCommand::kMoveRight);
  }

  return "";
}

}  // namespace

AccessibilityEventRewriterDelegateImpl::
    AccessibilityEventRewriterDelegateImpl() = default;

AccessibilityEventRewriterDelegateImpl::
    ~AccessibilityEventRewriterDelegateImpl() = default;

void AccessibilityEventRewriterDelegateImpl::DispatchKeyEventToChromeVox(
    std::unique_ptr<ui::Event> event,
    bool capture) {
  extensions::ExtensionHost* host =
      ::features::IsAccessibilityManifestV3EnabledForChromeVox()
          ? GetAccessibilityOffscreenDocumentHost(
                extension_misc::kChromeVoxExtensionId)
          : GetAccessibilityExtensionHost(
                extension_misc::kChromeVoxExtensionId);

  if (!host)
    return;

  // Listen for any unhandled keyboard events from ChromeVox's background page
  // when capturing keys to reinject.
  host->host_contents()->SetDelegate(capture ? this : nullptr);

  // Forward the event to ChromeVox's background page.
  ForwardKeyToExtension(*(event->AsKeyEvent()), host);
}

void AccessibilityEventRewriterDelegateImpl::DispatchKeyEventToChromeVoxMv3(
    unsigned int id,
    std::unique_ptr<ui::Event> event) {
  CHECK(::features::IsAccessibilityManifestV3EnabledForChromeVox());
  CHECK(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  CHECK(event->IsKeyEvent());
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(AccessibilityManager::Get()->profile());
  CHECK(event_router);

  // Transform the ui::KeyEvent into an accessibility_private::KeyboardEvent.
  const ui::KeyEvent* key_event = event->AsKeyEvent();
  extensions::api::accessibility_private::KeyboardEvent keyboard_event;
  keyboard_event.id = id;
  keyboard_event.alt_key = key_event->IsAltDown();
  keyboard_event.code =
      ui::KeycodeConverter::DomCodeToCodeString(key_event->code());
  keyboard_event.ctrl_key = key_event->IsControlDown();
  keyboard_event.key =
      ui::KeycodeConverter::DomKeyToKeyString(key_event->GetDomKey());
  keyboard_event.key_code = key_event->key_code();
  keyboard_event.meta_key = key_event->IsCommandDown();
  keyboard_event.repeat = key_event->is_repeat();
  keyboard_event.shift_key = key_event->IsShiftDown();

  // Build the extension event.
  base::Value::List event_args;
  event_args.Append(keyboard_event.ToValue());
  std::unique_ptr<extensions::Event> extension_event;
  if (key_event->type() == ui::EventType::kKeyPressed) {
    extension_event = std::make_unique<extensions::Event>(
        extensions::events::ACCESSIBILITY_PRIVATE_ON_KEY_DOWN,
        extensions::api::accessibility_private::OnKeyDown::kEventName,
        std::move(event_args));
  } else {
    extension_event = std::make_unique<extensions::Event>(
        extensions::events::ACCESSIBILITY_PRIVATE_ON_KEY_UP,
        extensions::api::accessibility_private::OnKeyUp::kEventName,
        std::move(event_args));
  }

  event_router->DispatchEventWithLazyListener(
      extension_misc::kChromeVoxExtensionId, std::move(extension_event));
}

void AccessibilityEventRewriterDelegateImpl::DispatchMouseEvent(
    std::unique_ptr<ui::Event> event) {
  ax::mojom::Event event_type;

  bool is_synthesized = event->IsSynthesized() ||
                        event->source_device_id() == ui::ED_UNKNOWN_DEVICE;

  switch (event->type()) {
    case ui::EventType::kMouseMoved:
      event_type = ax::mojom::Event::kMouseMoved;
      break;
    case ui::EventType::kMouseDragged:
      event_type = ax::mojom::Event::kMouseDragged;
      break;
    default:
      NOTREACHED();
  }

  AutomationManagerAura::GetInstance()->HandleEvent(
      event_type, /*from_user=*/!is_synthesized);
}

void AccessibilityEventRewriterDelegateImpl::SendSwitchAccessCommand(
    SwitchAccessCommand command) {
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(AccessibilityManager::Get()->profile());

  base::Value::List event_args;
  event_args.Append(ToString(command));

  auto event = std::make_unique<extensions::Event>(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_SWITCH_ACCESS_COMMAND,
      extensions::api::accessibility_private::OnSwitchAccessCommand::kEventName,
      std::move(event_args));

  event_router->DispatchEventWithLazyListener(
      extension_misc::kSwitchAccessExtensionId, std::move(event));
}

void AccessibilityEventRewriterDelegateImpl::SendPointScanPoint(
    const gfx::PointF& point) {
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(AccessibilityManager::Get()->profile());

  base::Value::Dict point_dict;
  point_dict.Set("x", point.x());
  point_dict.Set("y", point.y());

  base::Value::List event_args;
  event_args.Append(std::move(point_dict));

  auto event = std::make_unique<extensions::Event>(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_POINT_SCAN_SET,
      extensions::api::accessibility_private::OnPointScanSet::kEventName,
      std::move(event_args));

  event_router->DispatchEventWithLazyListener(
      extension_misc::kSwitchAccessExtensionId, std::move(event));
}

void AccessibilityEventRewriterDelegateImpl::SendMagnifierCommand(
    MagnifierCommand command) {
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(AccessibilityManager::Get()->profile());

  base::Value::List event_args;
  event_args.Append(ToString(command));

  auto event = std::make_unique<extensions::Event>(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_SWITCH_ACCESS_COMMAND,
      extensions::api::accessibility_private::OnMagnifierCommand::kEventName,
      std::move(event_args));

  event_router->DispatchEventWithLazyListener(
      extension_misc::kAccessibilityCommonExtensionId, std::move(event));
}

void AccessibilityEventRewriterDelegateImpl::OnUnhandledSpokenFeedbackEvent(
    std::unique_ptr<ui::Event> event) const {
  EventRewriterController::Get()->OnUnhandledSpokenFeedbackEvent(
      std::move(event));
}

bool AccessibilityEventRewriterDelegateImpl::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  OnUnhandledSpokenFeedbackEvent(event.os_event->Clone());
  return true;
}

}  // namespace ash
