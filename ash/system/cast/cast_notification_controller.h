// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAST_CAST_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_CAST_CAST_NOTIFICATION_CONTROLLER_H_

#include "ash/public/cpp/cast_config_controller.h"

#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class CastNotificationController : public CastConfigController::Observer {
 public:
  CastNotificationController();
  ~CastNotificationController() override;

  // CastConfigControllerObserver:
  void OnDevicesUpdated(const std::vector<SinkAndRoute>& devices) override;

 private:
  void StopCasting();

  // The cast activity id that we are displaying. If the user stops a cast, we
  // send this value to the config delegate so that we stop the right cast.
  std::string displayed_route_id_;

  base::WeakPtrFactory<CastNotificationController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CastNotificationController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAST_CAST_NOTIFICATION_CONTROLLER_H_
