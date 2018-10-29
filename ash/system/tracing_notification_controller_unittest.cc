// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tracing_notification_controller.h"

#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "ui/message_center/message_center.h"

namespace ash {

class TracingNotificationControllerTest : public AshTestBase {
 public:
  TracingNotificationControllerTest() = default;
  ~TracingNotificationControllerTest() override = default;

 protected:
  bool HasNotification() {
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
        TracingNotificationController::kNotificationId);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TracingNotificationControllerTest);
};

// Tests that the notification becomes visible when the tray model toggles
// it.
TEST_F(TracingNotificationControllerTest, Visibility) {
  // The system starts with tracing off, so the notification isn't visible.
  EXPECT_FALSE(HasNotification());

  // Simulate turning on tracing.
  SystemTrayModel* model = Shell::Get()->system_tray_model();
  model->SetPerformanceTracingIconVisible(true);
  EXPECT_TRUE(HasNotification());

  // Simulate turning off tracing.
  model->SetPerformanceTracingIconVisible(false);
  EXPECT_FALSE(HasNotification());
}

}  // namespace ash
