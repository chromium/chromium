// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MODEL_SYSTEM_TRAY_MODEL_H_
#define ASH_SYSTEM_MODEL_SYSTEM_TRAY_MODEL_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/system_tray.h"
#include "ash/system/time/calendar_model.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class ActiveNetworkIcon;
class ClockModel;
class EnterpriseDomainModel;
class LocaleModel;
struct RelaunchNotificationState;
class SessionLengthLimitModel;
class SystemTrayClient;
class TracingModel;
class TrayNetworkStateModel;
class UpdateModel;
class VirtualKeyboardModel;
class CalendarListModel;
class CalendarModel;
namespace phonehub {
class PhoneHubManager;
}

// Top level model of SystemTray.
class ASH_EXPORT SystemTrayModel : public SystemTray {
 public:
  SystemTrayModel();

  SystemTrayModel(const SystemTrayModel&) = delete;
  SystemTrayModel& operator=(const SystemTrayModel&) = delete;

  ~SystemTrayModel() override;

  // SystemTray:
  void SetClient(SystemTrayClient* client) override;
  void SetPrimaryTrayEnabled(bool enabled) override;
  void SetPrimaryTrayVisible(bool visible) override;
  void SetUse24HourClock(bool use_24_hour) override;
  void SetDeviceEnterpriseInfo(
      const DeviceEnterpriseInfo& device_enterprise_info) override;
  void SetEnterpriseAccountDomainInfo(
      const std::string& account_domain_manager) override;
  void SetPerformanceTracingIconVisible(bool visible) override;
  void SetLocaleList(std::vector<LocaleInfo> locale_list,
                     const std::string& current_locale_iso_code) override;
  void ShowUpdateIcon(UpdateSeverity severity,
                      bool factory_reset_required,
                      bool rollback) override;
  void SetRelaunchNotificationState(
      const RelaunchNotificationState& relaunch_notification_state) override;
  void ResetUpdateState() override;
  void SetUpdateDeferred(DeferredUpdateState state) override;
  void SetUpdateOverCellularAvailableIconVisible(bool visible) override;
  void SetShowEolNotice(bool show) override;
  void SetShowExtendedUpdatesNotice(bool show) override;
  void ShowVolumeSliderBubble() override;
  void ShowNetworkDetailedViewBubble() override;
  void SetPhoneHubManager(
      phonehub::PhoneHubManager* phone_hub_manager) override;

  // This will be set to true in `FakeSystemTrayModel`.
  virtual bool IsFakeModel() const;

  // True if user is in a child session. Virtual for mocking.
  virtual bool IsInUserChildSession() const;

  ClockModel* clock() { return clock_.get(); }
  EnterpriseDomainModel* enterprise_domain() {
    return enterprise_domain_.get();
  }
  LocaleModel* locale() { return locale_.get(); }
  SessionLengthLimitModel* session_length_limit() {
    return session_length_limit_.get();
  }
  TracingModel* tracing() { return tracing_.get(); }
  UpdateModel* update_model() { return update_model_.get(); }
  VirtualKeyboardModel* virtual_keyboard() { return virtual_keyboard_.get(); }
  TrayNetworkStateModel* network_state_model() {
    return network_state_model_.get();
  }
  ActiveNetworkIcon* active_network_icon() {
    return active_network_icon_.get();
  }
  SystemTrayClient* client() { return client_; }
  CalendarListModel* calendar_list_model() {
    return calendar_list_model_.get();
  }
  CalendarModel* calendar_model() { return calendar_model_.get(); }
  phonehub::PhoneHubManager* phone_hub_manager() { return phone_hub_manager_; }

 private:
  std::unique_ptr<ClockModel> clock_;
  std::unique_ptr<EnterpriseDomainModel> enterprise_domain_;
  std::unique_ptr<LocaleModel> locale_;
  std::unique_ptr<SessionLengthLimitModel> session_length_limit_;
  std::unique_ptr<TracingModel> tracing_;
  std::unique_ptr<UpdateModel> update_model_;
  std::unique_ptr<VirtualKeyboardModel> virtual_keyboard_;
  std::unique_ptr<TrayNetworkStateModel> network_state_model_;
  std::unique_ptr<ActiveNetworkIcon> active_network_icon_;
  std::unique_ptr<CalendarListModel> calendar_list_model_;
  std::unique_ptr<CalendarModel> calendar_model_;

  // Client interface in chrome browser. May be null in tests.
  raw_ptr<SystemTrayClient> client_ = nullptr;

  // Unowned.
  raw_ptr<phonehub::PhoneHubManager> phone_hub_manager_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MODEL_SYSTEM_TRAY_MODEL_H_
