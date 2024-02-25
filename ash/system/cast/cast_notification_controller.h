// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAST_CAST_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_CAST_CAST_NOTIFICATION_CONTROLLER_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/public/cpp/cast_config_controller.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

class ASH_EXPORT CastNotificationController
    : public CastConfigController::Observer,
      public views::WidgetObserver {
 public:
  CastNotificationController();

  CastNotificationController(const CastNotificationController&) = delete;
  CastNotificationController& operator=(const CastNotificationController&) =
      delete;

  ~CastNotificationController() override;

  // CastConfigControllerObserver:
  void OnDevicesUpdated(const std::vector<SinkAndRoute>& devices) override;

  // views::WidgetObserver overrides:
  void OnWidgetDestroyed(views::Widget* widget) override;

 private:
  // The callback that is triggered when the cast notification is pressed,
  // either on the body or buttons.
  void PressedCallback(std::optional<int> button_index);

  // Stops casting to the current displayed_route_id_
  void StopCasting();

  // If the current displayed_route_id_ can be frozen / unfrozen (displayed as
  // pause / resume to users), then this function freezes or unfreezes the
  // route given by displayed_route_id_ depending on its current state.
  void FreezePressed();

  // The cast activity id that we are displaying. If the user stops a cast, we
  // send this value to the config delegate so that we stop the right cast.
  std::string displayed_route_id_;

  // Freeze info for the route we are currently displaying. If the user
  // interacts with a cast, we use these values.
  std::optional<int> freeze_button_index_;
  bool displayed_route_is_frozen_ = false;
  bool freeze_on_tray_widget_destroyed_ = false;

  base::WeakPtrFactory<CastNotificationController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAST_CAST_NOTIFICATION_CONTROLLER_H_
