// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/restriction_schedule/device_restriction_schedule_controller.h"

#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "content/public/test/browser_test.h"

namespace policy {

class DeviceRestrictionScheduleControllerTest : public ash::LoginManagerTest {};

IN_PROC_BROWSER_TEST_F(DeviceRestrictionScheduleControllerTest,
                       ControllerExists) {
  EXPECT_TRUE(g_browser_process->platform_part()
                  ->device_restriction_schedule_controller());
}

}  // namespace policy
