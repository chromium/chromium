// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MODEL_SYSTEM_TRAY_MODEL_H_
#define ASH_SYSTEM_MODEL_SYSTEM_TRAY_MODEL_H_

#include <memory>

#include "ash/public/interfaces/system_tray.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace ash {

class ClockModel;
class EnterpriseDomainModel;
class SessionLengthLimitModel;
class TracingModel;
class UpdateModel;
class VirtualKeyboardModel;

// Top level model of SystemTray.
class SystemTrayModel : public mojom::SystemTray {
 public:
  SystemTrayModel();
  ~SystemTrayModel() override;

  // Binds the mojom::SystemTray interface to this object.
  void BindRequest(mojom::SystemTrayRequest request);

  // mojom::SystemTray:
  void SetClient(mojom::SystemTrayClientPtr client) override;
  void SetPrimaryTrayEnabled(bool enabled) override;
  void SetPrimaryTrayVisible(bool visible) override;
  void SetUse24HourClock(bool use_24_hour) override;
  void SetEnterpriseDisplayDomain(const std::string& enterprise_display_domain,
                                  bool active_directory_managed) override;
  void SetPerformanceTracingIconVisible(bool visible) override;
  void ShowUpdateIcon(mojom::UpdateSeverity severity,
                      bool factory_reset_required,
                      bool rollback,
                      mojom::UpdateType update_type) override;
  void SetUpdateNotificationState(
      mojom::NotificationStyle style,
      const base::string16& notification_title,
      const base::string16& notification_body) override;
  void SetUpdateOverCellularAvailableIconVisible(bool visible) override;
  void ShowVolumeSliderBubble() override;

  ClockModel* clock() { return clock_.get(); }
  EnterpriseDomainModel* enterprise_domain() {
    return enterprise_domain_.get();
  }
  SessionLengthLimitModel* session_length_limit() {
    return session_length_limit_.get();
  }
  TracingModel* tracing() { return tracing_.get(); }
  UpdateModel* update_model() { return update_model_.get(); }
  VirtualKeyboardModel* virtual_keyboard() { return virtual_keyboard_.get(); }

  const mojom::SystemTrayClientPtr& client_ptr() { return client_ptr_; }

 private:
  std::unique_ptr<ClockModel> clock_;
  std::unique_ptr<EnterpriseDomainModel> enterprise_domain_;
  std::unique_ptr<SessionLengthLimitModel> session_length_limit_;
  std::unique_ptr<TracingModel> tracing_;
  std::unique_ptr<UpdateModel> update_model_;
  std::unique_ptr<VirtualKeyboardModel> virtual_keyboard_;

  // TODO(tetsui): Add following as a sub-model of SystemTrayModel:
  // * BluetoothModel

  // Bindings for users of the mojo interface.
  mojo::BindingSet<mojom::SystemTray> bindings_;

  // Client interface in chrome browser. May be null in tests.
  mojom::SystemTrayClientPtr client_ptr_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayModel);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MODEL_SYSTEM_TRAY_MODEL_H_
