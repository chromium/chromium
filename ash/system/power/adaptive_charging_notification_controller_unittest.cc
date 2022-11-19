// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/adaptive_charging_notification_controller.h"

#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/i18n/rtl.h"
#include "base/run_loop.h"
#include "base/test/icu_test_util.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

const base::Time::Exploded kTestDateTimeExploded = {
    2022, 4,  5, 29,  // Fri, Apr 29, 2022
    2,    42, 7, 0    // 2:42:07.000 in UTC = 12:42:07 in Australia AEST.
};

// Enables or disables the user pref for the entire feature.
void SetAdaptiveChargingPref(bool enabled) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kPowerAdaptiveChargingEnabled, enabled);
  base::RunLoop().RunUntilIdle();
}

// Returns the number of (popup or non-popup) notifications that are currently
// visible in the message center queue.
size_t VisibleNotificationCount() {
  return message_center::MessageCenter::Get()->GetVisibleNotifications().size();
}

}  // namespace

class AdaptiveChargingNotificationControllerTest : public AshTestBase {
 public:
  AdaptiveChargingNotificationControllerTest() = default;
  AdaptiveChargingNotificationControllerTest(
      const AdaptiveChargingNotificationControllerTest&) = delete;
  AdaptiveChargingNotificationControllerTest& operator=(
      const AdaptiveChargingNotificationControllerTest&) = delete;
  ~AdaptiveChargingNotificationControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = std::make_unique<AdaptiveChargingNotificationController>();
  }

  AdaptiveChargingNotificationController* GetController() {
    return controller_.get();
  }

  void SimulateClick(int button_index) {
    message_center::Notification* notification =
        message_center::MessageCenter::Get()->FindVisibleNotificationById(
            /*id=*/"adaptive-charging-notify-info");
    notification->delegate()->Click(button_index, absl::nullopt);
  }

 private:
  std::unique_ptr<AdaptiveChargingNotificationController> controller_;
};

TEST_F(AdaptiveChargingNotificationControllerTest, ShouldntShowNotification) {
  SetAdaptiveChargingPref(false);

  GetController()->ShowAdaptiveChargingNotification();
  GetController()->ShowAdaptiveChargingNotification(5);

  EXPECT_EQ(VisibleNotificationCount(), 0u);
}

TEST_F(AdaptiveChargingNotificationControllerTest, ShowNotificationWithHour) {
  SetAdaptiveChargingPref(true);
  GetController()->ShowAdaptiveChargingNotification(5);

  EXPECT_EQ(VisibleNotificationCount(), 1u);
}

TEST_F(AdaptiveChargingNotificationControllerTest,
       ShowNotificationWithoutHour) {
  SetAdaptiveChargingPref(true);
  GetController()->ShowAdaptiveChargingNotification();

  EXPECT_EQ(VisibleNotificationCount(), 1u);
}

TEST_F(AdaptiveChargingNotificationControllerTest, HaveTimeInNotification) {
  // Set default locale.
  base::test::ScopedRestoreICUDefaultLocale restore_locale;
  base::i18n::SetICUDefaultLocale("en_AU");
  base::test::ScopedRestoreDefaultTimezone sydney_time("Australia/Sydney");

  // Override time for testing.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() {
        base::Time time;
        EXPECT_TRUE(base::Time::FromUTCExploded(kTestDateTimeExploded, &time));
        return time;
      },
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  SetAdaptiveChargingPref(true);
  GetController()->ShowAdaptiveChargingNotification(5);

  const message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindPopupNotificationById(
          "adaptive-charging-notify-info");

  ASSERT_TRUE(notification);

  // Current local time is 12:42 pm, so 5 hours after should be 5:30pm (rounding
  // from 5:42pm).
  EXPECT_NE(notification->message().find(u"5:30\u202fpm"),
            std::u16string::npos);
}

TEST_F(AdaptiveChargingNotificationControllerTest, TimeRoundingUpTest) {
  // Set default locale.
  base::test::ScopedRestoreICUDefaultLocale restore_locale;
  base::i18n::SetICUDefaultLocale("en_AU");
  base::test::ScopedRestoreDefaultTimezone sydney_time("Australia/Sydney");

  // Override time for testing.
  base::subtle::ScopedTimeClockOverrides time_override(
      []() {
        base::Time time;
        EXPECT_TRUE(base::Time::FromUTCExploded(kTestDateTimeExploded, &time));
        return time + base::Minutes(3);  // Local time is 12:45pm.
      },
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  SetAdaptiveChargingPref(true);
  GetController()->ShowAdaptiveChargingNotification(5);

  const message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindPopupNotificationById(
          "adaptive-charging-notify-info");

  ASSERT_TRUE(notification);

  // Current local time is 12:45 pm, so 5 hours after should be 6:00pm (rounding
  // from 5:45pm).
  EXPECT_NE(notification->message().find(u"6:00\u202fpm"),
            std::u16string::npos);
}

TEST_F(AdaptiveChargingNotificationControllerTest,
       ClickButtonMakesNotificationDisappear) {
  SetAdaptiveChargingPref(true);
  GetController()->ShowAdaptiveChargingNotification(5);
  EXPECT_EQ(VisibleNotificationCount(), 1u);

  // Notification should disappear after click.
  SimulateClick(/*button_index=*/0);
  EXPECT_EQ(VisibleNotificationCount(), 0u);
}

}  // namespace ash
