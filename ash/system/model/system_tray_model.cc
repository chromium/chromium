// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/model/system_tray_model.h"

#include <memory>

#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/update_types.h"
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
#include "ash/system/notification_center/message_center_controller.h"
#include "ash/system/phonehub/phone_hub_notification_controller.h"
#include "ash/system/phonehub/phone_hub_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/time/calendar_list_model.h"
#include "ash/system/time/calendar_model.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/unified/unified_system_tray.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"

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
          std::make_unique<ActiveNetworkIcon>(network_state_model_.get())),
      calendar_list_model_(std::make_unique<CalendarListModel>()),
      calendar_model_(std::make_unique<CalendarModel>()) {}

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

void SystemTrayModel::SetDeviceEnterpriseInfo(
    const DeviceEnterpriseInfo& device_enterprise_info) {
  enterprise_domain()->SetDeviceEnterpriseInfo(device_enterprise_info);
}

void SystemTrayModel::SetEnterpriseAccountDomainInfo(
    const std::string& account_domain_manager) {
  enterprise_domain()->SetEnterpriseAccountDomainInfo(account_domain_manager);
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
                                     bool rollback) {
  update_model()->SetUpdateAvailable(severity, factory_reset_required,
                                     rollback);
}

void SystemTrayModel::SetRelaunchNotificationState(
    const RelaunchNotificationState& relaunch_notification_state) {
  update_model()->SetRelaunchNotificationState(relaunch_notification_state);
}

void SystemTrayModel::ResetUpdateState() {
  update_model()->ResetUpdateAvailable();
}

void SystemTrayModel::SetUpdateDeferred(DeferredUpdateState state) {
  update_model()->SetUpdateDeferred(state);
}

void SystemTrayModel::SetUpdateOverCellularAvailableIconVisible(bool visible) {
  update_model()->SetUpdateOverCellularAvailable(visible);
}

void SystemTrayModel::SetShowEolNotice(bool show) {
  update_model()->SetShowEolNotice(show);
}

void SystemTrayModel::SetShowExtendedUpdatesNotice(bool show) {
  update_model()->SetShowExtendedUpdatesNotice(show);
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

void SystemTrayModel::ShowNetworkDetailedViewBubble() {
  // Show the bubble on the primary display.
  UnifiedSystemTray* system_tray = Shell::GetPrimaryRootWindowController()
                                       ->GetStatusAreaWidget()
                                       ->unified_system_tray();
  if (system_tray)
    system_tray->ShowNetworkDetailedViewBubble();
}

void SystemTrayModel::SetPhoneHubManager(
    phonehub::PhoneHubManager* phone_hub_manager) {
  for (RootWindowController* root_window_controller :
       Shell::GetAllRootWindowControllers()) {
    auto* phone_hub_tray =
        root_window_controller->GetStatusAreaWidget()->phone_hub_tray();
    phone_hub_tray->SetPhoneHubManager(phone_hub_manager);
  }

  Shell::Get()
      ->message_center_controller()
      ->phone_hub_notification_controller()
      ->SetManager(phone_hub_manager);

  phone_hub_manager_ = phone_hub_manager;
}

bool SystemTrayModel::IsFakeModel() const {
  return false;
}

bool SystemTrayModel::IsInUserChildSession() const {
  return Shell::Get()->session_controller()->IsUserChild();
}

}  // namespace ash
