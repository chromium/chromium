// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/restriction_schedule/device_restriction_schedule_controller.h"

#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/scoped_observation.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/ui/webui/ash/login/device_disabled_screen_handler.h"
#include "chromeos/ash/components/policy/restriction_schedule/device_restriction_schedule_controller_delegate_impl.h"
#include "chromeos/ash/components/policy/weekly_time/test_support.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"

namespace policy {

using ::ash::test::OobeJS;
using ::ash::test::UIPath;

// Empty list.
constexpr const char* kPolicyJsonEmpty = "[]";

constexpr char kDeviceDisabledId[] = "device-disabled";
constexpr UIPath kBannerTitle = {kDeviceDisabledId, "title"};
constexpr UIPath kBannerContents = {kDeviceDisabledId, "subtitle"};

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
        weekly_time::BuildList(clock_->Now(), from_now, duration);
    std::string policy_str;
    ASSERT_TRUE(JSONStringValueSerializer(&policy_str).Serialize(policy_list));
    UpdatePolicy(policy_str);
  }

  DeviceRestrictionScheduleController& controller() {
    return CHECK_DEREF(g_browser_process->platform_part()
                           ->device_restriction_schedule_controller());
  }

  void SetClocks(const base::Clock& clock) {
    clock_ = clock;
    controller().SetClockForTesting(clock);
  }

 protected:
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  ash::LoginManagerMixin login_mixin_{&mixin_host_};
  raw_ref<const base::Clock> clock_{*base::DefaultClock::GetInstance()};
};

class CaptureNotificationWaiter : public message_center::MessageCenterObserver {
 public:
  CaptureNotificationWaiter(base::OnceClosure on_notification_added,
                            const std::string& match_notification_id)
      : on_notification_added_(std::move(on_notification_added)),
        match_notification_id_(match_notification_id) {
    auto* message_center = message_center::MessageCenter::Get();
    if (message_center->FindNotificationById(match_notification_id_)) {
      std::move(on_notification_added_).Run();
      return;
    }
    observation_.Observe(message_center);
  }

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override {
    if (notification_id == match_notification_id_) {
      std::move(on_notification_added_).Run();
    }
  }

 private:
  base::OnceClosure on_notification_added_;
  std::string match_notification_id_;
  base::ScopedObservation<message_center::MessageCenter,
                          message_center::MessageCenterObserver>
      observation_{this};
};

class FakeWallClockTimer : public base::WallClockTimer {
 public:
  void Start(const base::Location& posted_from,
             base::Time desired_run_time,
             base::OnceClosure user_task) override {
    user_task_ = std::move(user_task);
  }

  void FireNow() { std::move(user_task_).Run(); }

 private:
  base::OnceClosure user_task_;
};

IN_PROC_BROWSER_TEST_F(DeviceRestrictionScheduleControllerTest,
                       UpcomingLogoutNotificationShows) {
  LoginUser(login_mixin_.users()[0].account_id);

  // Restriction schedule starts in 20 minutes and lasts for 2 hours.
  SetRestrictionSchedule(base::Minutes(20), base::Hours(2));

  // Verify that upcoming session end notification shows.
  base::test::TestFuture<void> future;
  CaptureNotificationWaiter waiter(
      future.GetCallback(), DeviceRestrictionScheduleControllerDelegateImpl::
                                kUpcomingLogoutNotificationId);
  ASSERT_TRUE(future.Wait());
}

IN_PROC_BROWSER_TEST_F(DeviceRestrictionScheduleControllerTest,
                       PRE_PostLogoutNotificationShows) {
  LoginUser(login_mixin_.users()[0].account_id);

  // Restriction schedule started 20 minutes ago and lasts for 2 hours.
  SetRestrictionSchedule(-base::Minutes(20), base::Hours(2));

  // Logout happens here (Chrome shuts down), and then we start again on the
  // login screen in the next part of the test.
}

IN_PROC_BROWSER_TEST_F(DeviceRestrictionScheduleControllerTest,
                       PostLogoutNotificationShows) {
  // Verify that post-logout notification shows.
  base::test::TestFuture<void> future;
  CaptureNotificationWaiter waiter(
      future.GetCallback(), DeviceRestrictionScheduleControllerDelegateImpl::
                                kPostLogoutNotificationId);
  ASSERT_TRUE(future.Wait());
}

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

IN_PROC_BROWSER_TEST_F(DeviceRestrictionScheduleControllerTest,
                       DeviceDisabledScreenShows) {
  // Restriction schedule started 20 minutes ago and lasts for 2 hours.
  SetRestrictionSchedule(-base::Minutes(20), base::Hours(2));

  ash::OobeScreenWaiter(ash::DeviceDisabledScreenView::kScreenId).Wait();

  // Verify that it's really the restriction schedule banner showing, not the
  // standard device disabled one.
  OobeJS().ExpectElementText(
      l10n_util::GetStringUTF8(
          IDS_DEVICE_DISABLED_HEADING_RESTRICTION_SCHEDULE),
      kBannerTitle);
}

IN_PROC_BROWSER_TEST_F(DeviceRestrictionScheduleControllerTest,
                       RestrictedScheduleEnds) {
  // Restriction schedule started 20 minutes ago and lasts for 2 hours.
  SetRestrictionSchedule(-base::Minutes(20), base::Hours(2));

  // DeviceDisabledScreen is shown.
  ash::OobeScreenWaiter(ash::DeviceDisabledScreenView::kScreenId).Wait();

  // Reset the policy, restriction schedule ends.
  UpdatePolicy(kPolicyJsonEmpty);

  // In order to reset state and stop showing the DeviceDisabledScreen, Chrome
  // is restarted back to the login screen here.
  base::test::TestFuture<void> future;
  auto subscription =
      browser_shutdown::AddAppTerminatingCallback(future.GetCallback());
  ASSERT_TRUE(future.Wait());
}

IN_PROC_BROWSER_TEST_F(DeviceRestrictionScheduleControllerTest,
                       RestrictionScheduleMessageChanged) {
  auto mock_timer_owned = std::make_unique<FakeWallClockTimer>();
  FakeWallClockTimer* mock_timer = mock_timer_owned.get();
  controller().SetMessageUpdateTimerForTesting(std::move(mock_timer_owned));

  base::SimpleTestClock test_clock;
  SetClocks(test_clock);
  test_clock.SetNow(base::Time::Now().LocalMidnight());

  // Set the restriction schedule to start right now, and to last for 1.5 days.
  // Currently the message will contain "Tomorrow", and at next midnight will
  // contain "Today".
  SetRestrictionSchedule(base::TimeDelta(), base::Hours(36));

  ash::OobeScreenWaiter(ash::DeviceDisabledScreenView::kScreenId).Wait();

  OobeJS().ExpectElementContainsText(
      l10n_util::GetStringUTF8(
          IDS_DEVICE_DISABLED_EXPLANATION_RESTRICTION_SCHEDULE_TOMORROW),
      kBannerContents);

  // Update the time to tomorrow and fire the mock timer (+6 hours to avoid any
  // DST issues).
  test_clock.Advance(base::Days(1) + base::Hours(6));
  mock_timer->FireNow();

  OobeJS().ExpectElementContainsText(
      l10n_util::GetStringUTF8(
          IDS_DEVICE_DISABLED_EXPLANATION_RESTRICTION_SCHEDULE_TODAY),
      kBannerContents);

  // Reset the clocks back.
  SetClocks(*base::DefaultClock::GetInstance());
}

}  // namespace policy
