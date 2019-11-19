// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/arc_notification_manager_delegate_impl.h"

#include "ash/login_status.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/message_center/message_center_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"

namespace ash {

ArcNotificationManagerDelegateImpl::ArcNotificationManagerDelegateImpl() =
    default;
ArcNotificationManagerDelegateImpl::~ArcNotificationManagerDelegateImpl() =
    default;

bool ArcNotificationManagerDelegateImpl::IsPublicSessionOrKiosk() const {
  const LoginStatus login_status =
      Shell::Get()->session_controller()->login_status();

  return login_status == LoginStatus::PUBLIC ||
         login_status == LoginStatus::KIOSK_APP;
}

void ArcNotificationManagerDelegateImpl::ShowMessageCenter() {
  Shell::Get()
      ->GetPrimaryRootWindowController()
      ->GetStatusAreaWidget()
      ->unified_system_tray()
      ->ShowBubble(false /* show_by_click */);
}

void ArcNotificationManagerDelegateImpl::HideMessageCenter() {
  // Close the message center on all the displays.
  for (auto* root_window_controller :
       RootWindowController::root_window_controllers()) {
    root_window_controller->GetStatusAreaWidget()
        ->unified_system_tray()
        ->CloseBubble();
  }
}

}  // namespace ash
