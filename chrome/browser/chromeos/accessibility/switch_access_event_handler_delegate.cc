// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/switch_access_event_handler_delegate.h"

#include <utility>

#include "ash/public/cpp/accessibility_controller.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"

namespace {

std::string AccessibilityPrivateEnumForCommand(
    ash::SwitchAccessCommand command) {
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

}  // namespace

SwitchAccessEventHandlerDelegate::SwitchAccessEventHandlerDelegate() {
  ash::AccessibilityController::Get()->SetSwitchAccessEventHandlerDelegate(
      this);
}

SwitchAccessEventHandlerDelegate::~SwitchAccessEventHandlerDelegate() {
  if (auto* controller = ash::AccessibilityController::Get())
    controller->SetSwitchAccessEventHandlerDelegate(nullptr);
}

void SwitchAccessEventHandlerDelegate::SendSwitchAccessCommand(
    ash::SwitchAccessCommand command) {
  extensions::EventRouter* event_router = extensions::EventRouter::Get(
      chromeos::AccessibilityManager::Get()->profile());

  auto event_args = std::make_unique<base::ListValue>();
  event_args->AppendString(AccessibilityPrivateEnumForCommand(command));

  auto event = std::make_unique<extensions::Event>(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_SWITCH_ACCESS_COMMAND,
      extensions::api::accessibility_private::OnSwitchAccessCommand::kEventName,
      std::move(event_args));

  event_router->DispatchEventWithLazyListener(
      extension_misc::kSwitchAccessExtensionId, std::move(event));
}
