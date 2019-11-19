// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MODEL_SYSTEM_TRAY_MODEL_H_
#define ASH_SYSTEM_MODEL_SYSTEM_TRAY_MODEL_H_

#include <memory>

#include "ash/public/cpp/system_tray.h"
#include "base/macros.h"

namespace ash {

class ActiveNetworkIcon;
class ClockModel;
class EnterpriseDomainModel;
class LocaleModel;
class SessionLengthLimitModel;
class SystemTrayClient;
class TracingModel;
class TrayNetworkStateModel;
class UpdateModel;
class VirtualKeyboardModel;

// Top level model of SystemTray.
class SystemTrayModel : public SystemTray {
 public:
  SystemTrayModel();
  ~SystemTrayModel() override;

  // SystemTray:
  void SetClient(SystemTrayClient* client) override;
  void SetPrimaryTrayEnabled(bool enabled) override;
  void SetPrimaryTrayVisible(bool visible) override;
  void SetUse24HourClock(bool use_24_hour) override;
  void SetEnterpriseDisplayDomain(const std::string& enterprise_display_domain,
                                  bool active_directory_managed) override;
  void SetPerformanceTracingIconVisible(bool visible) override;
  void SetLocaleList(std::vector<LocaleInfo> locale_list,
                     const std::string& current_locale_iso_code) override;
  void ShowUpdateIcon(UpdateSeverity severity,
                      bool factory_reset_required,
                      bool rollback,
                      UpdateType update_type) override;
  void SetUpdateNotificationState(
      NotificationStyle style,
      const base::string16& notification_title,
      const base::string16& notification_body) override;
  void SetUpdateOverCellularAvailableIconVisible(bool visible) override;
  void ShowVolumeSliderBubble() override;
  void ShowNetworkDetailedViewBubble(bool show_by_click) override;

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

  // TODO(tetsui): Add following as a sub-model of SystemTrayModel:
  // * BluetoothModel

  // Client interface in chrome browser. May be null in tests.
  SystemTrayClient* client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayModel);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MODEL_SYSTEM_TRAY_MODEL_H_
