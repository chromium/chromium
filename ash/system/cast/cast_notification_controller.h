// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAST_CAST_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_CAST_CAST_NOTIFICATION_CONTROLLER_H_

#include "ash/public/cpp/cast_config_controller.h"

#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class CastNotificationController : public CastConfigController::Observer {
 public:
  CastNotificationController();

  CastNotificationController(const CastNotificationController&) = delete;
  CastNotificationController& operator=(const CastNotificationController&) =
      delete;

  ~CastNotificationController() override;

  // CastConfigControllerObserver:
  void OnDevicesUpdated(const std::vector<SinkAndRoute>& devices) override;

 private:
  void StopCasting(absl::optional<int> button_index);

  // The cast activity id that we are displaying. If the user stops a cast, we
  // send this value to the config delegate so that we stop the right cast.
  std::string displayed_route_id_;

  base::WeakPtrFactory<CastNotificationController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAST_CAST_NOTIFICATION_CONTROLLER_H_
