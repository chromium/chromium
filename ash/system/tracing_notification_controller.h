// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRACING_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_TRACING_NOTIFICATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/model/tracing_model.h"
#include "base/memory/raw_ptr.h"

namespace ash {

// Controller class to manage tracing enabled (chrome://slow) notification.
class ASH_EXPORT TracingNotificationController : public TracingObserver {
 public:
  TracingNotificationController();

  TracingNotificationController(const TracingNotificationController&) = delete;
  TracingNotificationController& operator=(
      const TracingNotificationController&) = delete;

  ~TracingNotificationController() override;

  // TracingObserver:
  void OnTracingModeChanged() override;

 private:
  friend class TracingNotificationControllerTest;

  void CreateNotification();
  void RemoveNotification();

  static const char kNotificationId[];

  // True if performance tracing was active on the last time
  // OnTracingModeChanged was called.
  bool was_tracing_ = false;

  const raw_ptr<TracingModel> model_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRACING_NOTIFICATION_CONTROLLER_H_
