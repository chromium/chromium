// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/model/system_tray_model.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/session_length_limit_model.h"
#include "ash/system/model/tracing_model.h"
#include "ash/system/model/update_model.h"
#include "ash/system/model/virtual_keyboard_model.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/logging.h"

namespace ash {

SystemTrayModel::SystemTrayModel()
    : clock_(std::make_unique<ClockModel>()),
      enterprise_domain_(std::make_unique<EnterpriseDomainModel>()),
      session_length_limit_(std::make_unique<SessionLengthLimitModel>()),
      tracing_(std::make_unique<TracingModel>()),
      update_model_(std::make_unique<UpdateModel>()),
      virtual_keyboard_(std::make_unique<VirtualKeyboardModel>()) {}

SystemTrayModel::~SystemTrayModel() = default;

void SystemTrayModel::BindRequest(mojom::SystemTrayRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void SystemTrayModel::SetClient(mojom::SystemTrayClientPtr client_ptr) {
  client_ptr_ = std::move(client_ptr);
}

void SystemTrayModel::SetPrimaryTrayEnabled(bool enabled) {
  UnifiedSystemTray* tray = Shell::GetPrimaryRootWindowController()
                                ->GetStatusAreaWidget()
                                ->unified_system_tray();
  if (!tray)
    return;
  tray->SetTrayEnabled(enabled);
}

void SystemTrayModel::SetPrimaryTrayVisible(bool visible) {
  auto* status_area =
      Shell::GetPrimaryRootWindowController()->GetStatusAreaWidget();
  if (status_area)
    status_area->SetSystemTrayVisibility(visible);
}

void SystemTrayModel::SetUse24HourClock(bool use_24_hour) {
  clock()->SetUse24HourClock(use_24_hour);
}

void SystemTrayModel::SetEnterpriseDisplayDomain(
    const std::string& enterprise_display_domain,
    bool active_directory_managed) {
  enterprise_domain()->SetEnterpriseDisplayDomain(enterprise_display_domain,
                                                  active_directory_managed);
}

void SystemTrayModel::SetPerformanceTracingIconVisible(bool visible) {
  tracing()->SetIsTracing(visible);
}

void SystemTrayModel::ShowUpdateIcon(mojom::UpdateSeverity severity,
                                     bool factory_reset_required,
                                     bool rollback,
                                     mojom::UpdateType update_type) {
  update_model()->SetUpdateAvailable(severity, factory_reset_required, rollback,
                                     update_type);
}

void SystemTrayModel::SetUpdateNotificationState(
    mojom::NotificationStyle style,
    const base::string16& notification_title,
    const base::string16& notification_body) {
  update_model()->SetUpdateNotificationState(style, notification_title,
                                             notification_body);
}

void SystemTrayModel::SetUpdateOverCellularAvailableIconVisible(bool visible) {
  update_model()->SetUpdateOverCellularAvailable(visible);
}

void SystemTrayModel::ShowVolumeSliderBubble() {
  // Show the bubble on all monitors with a system tray.
  for (RootWindowController* root : Shell::GetAllRootWindowControllers()) {
    UnifiedSystemTray* system_tray =
        root->GetStatusAreaWidget()->unified_system_tray();
    if (!system_tray)
      continue;
    system_tray->ShowVolumeSliderBubble();
  }
}

}  // namespace ash
