// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/accessibility_event_rewriter_delegate.h"

#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/event_rewriter_controller.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/event_handler_common.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/exo/wm_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/common/constants.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/events/event.h"

namespace {

std::string ToString(ash::SwitchAccessCommand command) {
  switch (command) {
    case ash::SwitchAccessCommand::kSelect:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::SWITCH_ACCESS_COMMAND_SELECT);
    case ash::SwitchAccessCommand::kNext:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::SWITCH_ACCESS_COMMAND_NEXT);
    case ash::SwitchAccessCommand::kPrevious:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::
              SWITCH_ACCESS_COMMAND_PREVIOUS);
    case ash::SwitchAccessCommand::kNone:
      NOTREACHED();
      return "";
  }
}

std::string ToString(ash::MagnifierCommand command) {
  switch (command) {
    case ash::MagnifierCommand::kMoveStop:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::MAGNIFIER_COMMAND_MOVESTOP);
    case ash::MagnifierCommand::kMoveUp:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::MAGNIFIER_COMMAND_MOVEUP);
    case ash::MagnifierCommand::kMoveDown:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::MAGNIFIER_COMMAND_MOVEDOWN);
    case ash::MagnifierCommand::kMoveLeft:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::MAGNIFIER_COMMAND_MOVELEFT);
    case ash::MagnifierCommand::kMoveRight:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::MAGNIFIER_COMMAND_MOVERIGHT);
  }

  return "";
}

}  // namespace

AccessibilityEventRewriterDelegate::AccessibilityEventRewriterDelegate() {
  // If WMHelper doesn't exist, do nothing. This occurs in tests.
  if (exo::WMHelper::HasInstance())
    exo::WMHelper::GetInstance()->AddActivationObserver(this);
}

AccessibilityEventRewriterDelegate::~AccessibilityEventRewriterDelegate() {
  // If WMHelper is already destroyed, do nothing.
  // TODO(crbug.com/748380): Fix shutdown order.
  if (exo::WMHelper::HasInstance())
    exo::WMHelper::GetInstance()->RemoveActivationObserver(this);
}

void AccessibilityEventRewriterDelegate::DispatchKeyEventToChromeVox(
    std::unique_ptr<ui::Event> event,
    bool capture) {
  extensions::ExtensionHost* host = chromeos::GetAccessibilityExtensionHost(
      extension_misc::kChromeVoxExtensionId);
  if (!host)
    return;

  // Listen for any unhandled keyboard events from ChromeVox's background page
  // when capturing keys to reinject.
  host->host_contents()->SetDelegate(capture ? this : nullptr);

  // Forward the event to ChromeVox's background page.
  chromeos::ForwardKeyToExtension(*(event->AsKeyEvent()), host);
}

void AccessibilityEventRewriterDelegate::DispatchMouseEvent(
    std::unique_ptr<ui::Event> event) {
  if (is_arc_window_active_)
    return;

  AutomationManagerAura::GetInstance()->HandleEvent(
      ax::mojom::Event::kMouseMoved);
}

void AccessibilityEventRewriterDelegate::SendSwitchAccessCommand(
    ash::SwitchAccessCommand command) {
  extensions::EventRouter* event_router = extensions::EventRouter::Get(
      chromeos::AccessibilityManager::Get()->profile());

  auto event_args = std::make_unique<base::ListValue>();
  event_args->AppendString(ToString(command));

  auto event = std::make_unique<extensions::Event>(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_SWITCH_ACCESS_COMMAND,
      extensions::api::accessibility_private::OnSwitchAccessCommand::kEventName,
      std::move(event_args));

  event_router->DispatchEventWithLazyListener(
      extension_misc::kSwitchAccessExtensionId, std::move(event));
}

void AccessibilityEventRewriterDelegate::SendPointScanPoint(
    const gfx::PointF& point) {
  extensions::EventRouter* event_router = extensions::EventRouter::Get(
      chromeos::AccessibilityManager::Get()->profile());

  auto event_args = std::make_unique<base::ListValue>();
  auto point_dict = std::make_unique<base::DictionaryValue>();

  point_dict->SetDouble("x", point.x());
  point_dict->SetDouble("y", point.y());

  event_args->Append(std::move(point_dict));

  auto event = std::make_unique<extensions::Event>(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_POINT_SCAN_SET,
      extensions::api::accessibility_private::OnPointScanSet::kEventName,
      std::move(event_args));

  event_router->DispatchEventWithLazyListener(
      extension_misc::kSwitchAccessExtensionId, std::move(event));
}

void AccessibilityEventRewriterDelegate::SendMagnifierCommand(
    ash::MagnifierCommand command) {
  extensions::EventRouter* event_router = extensions::EventRouter::Get(
      chromeos::AccessibilityManager::Get()->profile());

  auto event_args = std::make_unique<base::ListValue>();
  event_args->AppendString(ToString(command));

  auto event = std::make_unique<extensions::Event>(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_SWITCH_ACCESS_COMMAND,
      extensions::api::accessibility_private::OnMagnifierCommand::kEventName,
      std::move(event_args));

  event_router->DispatchEventWithLazyListener(
      extension_misc::kAccessibilityCommonExtensionId, std::move(event));
}

void AccessibilityEventRewriterDelegate::OnUnhandledSpokenFeedbackEvent(
    std::unique_ptr<ui::Event> event) const {
  ash::EventRewriterController::Get()->OnUnhandledSpokenFeedbackEvent(
      std::move(event));
}

bool AccessibilityEventRewriterDelegate::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  OnUnhandledSpokenFeedbackEvent(ui::Event::Clone(*event.os_event));
  return true;
}

void AccessibilityEventRewriterDelegate::OnWindowActivated(
    ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (gained_active == lost_active)
    return;

  is_arc_window_active_ = ash::IsArcWindow(gained_active);
}
