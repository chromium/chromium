// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/restriction_schedule/device_restriction_schedule_controller.h"

#include "ash/constants/ash_switches.h"
#include "base/json/json_string_value_serializer.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chromeos/ash/components/policy/weekly_time/test_support.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "content/public/test/browser_test.h"

namespace policy {

class DeviceRestrictionScheduleControllerTest : public ash::LoginManagerTest {
 public:
  DeviceRestrictionScheduleControllerTest() {
    login_mixin_.AppendRegularUsers(1);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    // Allow failing policy fetch so that we don't shutdown the profile on
    // failure.
    command_line->AppendSwitch(ash::switches::kAllowFailedPolicyFetchForTest);
  }

  void UpdatePolicy(const std::string& policy_str) {
    std::unique_ptr<ash::ScopedDevicePolicyUpdate> device_policy_update =
        device_state_.RequestDevicePolicyUpdate();
    device_policy_update->policy_payload()
        ->mutable_devicerestrictionschedule()
        ->set_value(policy_str.c_str());
  }

  void SetRestrictionSchedule(base::TimeDelta from_now,
                              base::TimeDelta duration) {
    base::Value::List policy_list =
        weekly_time::BuildList(base::Time::Now(), from_now, duration);
    std::string policy_str;
    ASSERT_TRUE(JSONStringValueSerializer(&policy_str).Serialize(policy_list));
    UpdatePolicy(policy_str);
  }

 protected:
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  ash::LoginManagerMixin login_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(DeviceRestrictionScheduleControllerTest,
                       LogoutOnEnteringRestrictedSchedule) {
  LoginUser(login_mixin_.users()[0].account_id);

  // Restriction schedule started 20 minutes ago and lasts for 2 hours.
  SetRestrictionSchedule(-base::Minutes(20), base::Hours(2));

  // Verify that logout happens (Chrome shuts down) upon entering the restricted
  // schedule.
  base::test::TestFuture<void> future;
  auto subscription =
      browser_shutdown::AddAppTerminatingCallback(future.GetCallback());
  ASSERT_TRUE(future.Wait());
}

}  // namespace policy
