// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/model/system_tray_model.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/locale_model.h"
#include "ash/system/model/session_length_limit_model.h"
#include "ash/system/model/tracing_model.h"
#include "ash/system/model/update_model.h"
#include "ash/system/model/virtual_keyboard_model.h"
#include "ash/system/network/active_network_icon.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/logging.h"

namespace ash {

SystemTrayModel::SystemTrayModel()
    : clock_(std::make_unique<ClockModel>()),
      enterprise_domain_(std::make_unique<EnterpriseDomainModel>()),
      locale_(std::make_unique<LocaleModel>()),
      session_length_limit_(std::make_unique<SessionLengthLimitModel>()),
      tracing_(std::make_unique<TracingModel>()),
      update_model_(std::make_unique<UpdateModel>()),
      virtual_keyboard_(std::make_unique<VirtualKeyboardModel>()),
      network_state_model_(std::make_unique<TrayNetworkStateModel>()),
      active_network_icon_(
          std::make_unique<ActiveNetworkIcon>(network_state_model_.get())) {}

SystemTrayModel::~SystemTrayModel() = default;

void SystemTrayModel::SetClient(SystemTrayClient* client) {
  client_ = client;
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

void SystemTrayModel::SetLocaleList(
    std::vector<LocaleInfo> locale_list,
    const std::string& current_locale_iso_code) {
  locale()->SetLocaleList(std::move(locale_list), current_locale_iso_code);
}

void SystemTrayModel::ShowUpdateIcon(UpdateSeverity severity,
                                     bool factory_reset_required,
                                     bool rollback,
                                     UpdateType update_type) {
  update_model()->SetUpdateAvailable(severity, factory_reset_required, rollback,
                                     update_type);
}

void SystemTrayModel::SetUpdateNotificationState(
    NotificationStyle style,
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

void SystemTrayModel::ShowNetworkDetailedViewBubble(bool show_by_click) {
  // Show the bubble on the primary display.
  UnifiedSystemTray* system_tray = Shell::GetPrimaryRootWindowController()
                                       ->GetStatusAreaWidget()
                                       ->unified_system_tray();
  if (system_tray)
    system_tray->ShowNetworkDetailedViewBubble(show_by_click);
}

}  // namespace ash
