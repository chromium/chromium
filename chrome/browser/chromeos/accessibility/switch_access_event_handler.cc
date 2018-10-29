// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/switch_access_event_handler.h"

#include "ash/shell.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/event_handler_common.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_host.h"
#include "ui/events/event.h"

namespace chromeos {

SwitchAccessEventHandler::SwitchAccessEventHandler() {
  if (ash::Shell::HasInstance())
    ash::Shell::Get()->AddPreTargetHandler(this);
}

SwitchAccessEventHandler::~SwitchAccessEventHandler() {
  if (ash::Shell::HasInstance())
    ash::Shell::Get()->RemovePreTargetHandler(this);
}

void SwitchAccessEventHandler::SetKeysToCapture(
    const std::set<int>& key_codes) {
  captured_keys_ = key_codes;
}

void SwitchAccessEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  DCHECK(event);

  ui::KeyboardCode key_code = event->key_code();
  if (captured_keys_.find(key_code) != captured_keys_.end()) {
    CancelEvent(event);
    DispatchKeyEventToSwitchAccess(*event);
  }
}

void SwitchAccessEventHandler::CancelEvent(ui::Event* event) {
  DCHECK(event);
  if (event->cancelable()) {
    event->SetHandled();
    event->StopPropagation();
  }
}

void SwitchAccessEventHandler::DispatchKeyEventToSwitchAccess(
    const ui::KeyEvent& event) {
  extensions::ExtensionHost* host =
      GetAccessibilityExtensionHost(extension_misc::kSwitchAccessExtensionId);
  if (!host)
    return;

  ForwardKeyToExtension(event, host);
}

}  // namespace chromeos
