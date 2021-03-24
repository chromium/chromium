// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/notification/arc_supervision_transition_notification.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "components/arc/session/arc_supervision_transition.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcSupervisionTransitionNotificationTest
    : public testing::Test,
      public testing::WithParamInterface<ArcSupervisionTransition> {
 public:
  ArcSupervisionTransitionNotificationTest() = default;
  ~ArcSupervisionTransitionNotificationTest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
    arc_app_test_.SetUp(profile());
  }

  void TearDown() override {
    arc_app_test_.TearDown();
    display_service_.reset();
    profile_.reset();
  }

  Profile* profile() { return profile_.get(); }
  NotificationDisplayServiceTester* display_service() {
    return display_service_.get();
  }
  ArcAppTest* arc_app_test() { return &arc_app_test_; }

 private:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  ArcAppTest arc_app_test_;

  content::BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ArcSupervisionTransitionNotificationTest);
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ArcSupervisionTransitionNotificationTest,
    ::testing::Values(ArcSupervisionTransition::NO_TRANSITION,
                      ArcSupervisionTransition::CHILD_TO_REGULAR,
                      ArcSupervisionTransition::REGULAR_TO_CHILD));

TEST_P(ArcSupervisionTransitionNotificationTest, BaseFlow) {
  ASSERT_TRUE(arc_app_test()->fake_apps().size());
  arc_app_test()->app_instance()->SendRefreshAppList(
      arc_app_test()->fake_apps());
  const std::string app_id =
      ArcAppTest::GetAppId(arc_app_test()->fake_apps()[0]);

  profile()->GetPrefs()->SetInteger(prefs::kArcSupervisionTransition,
                                    static_cast<int>(GetParam()));

  // Attempt to launch ARC++ app triggers notification.
  LaunchApp(profile(), app_id, 0 /* event_flags */,
            UserInteractionType::NOT_USER_INITIATED);

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_app_test()->arc_app_list_prefs()->GetApp(app_id);
  ASSERT_TRUE(app_info);

  // In case no supervision transition in progress notification is not
  // triggered.
  if (GetParam() == ArcSupervisionTransition::NO_TRANSITION) {
    EXPECT_FALSE(display_service()->GetNotification(
        kSupervisionTransitionNotificationId));
    // Last launch is set, indicating that launch attempt was not blocked.
    EXPECT_FALSE(app_info->last_launch_time.is_null());
    return;
  }

  EXPECT_TRUE(
      display_service()->GetNotification(kSupervisionTransitionNotificationId));
  // Last launch is not set, indicating that launch attempt was blocked.
  EXPECT_TRUE(app_info->last_launch_time.is_null());

  // Finishing transition automatically dismisses notification.
  profile()->GetPrefs()->SetInteger(
      prefs::kArcSupervisionTransition,
      static_cast<int>(ArcSupervisionTransition::NO_TRANSITION));
  EXPECT_FALSE(
      display_service()->GetNotification(kSupervisionTransitionNotificationId));

  // Re-activate notification and check opt out. On opt-out notification is also
  // automatially dismissed.
  profile()->GetPrefs()->SetInteger(prefs::kArcSupervisionTransition,
                                    static_cast<int>(GetParam()));
  ShowSupervisionTransitionNotification(profile());
  EXPECT_TRUE(
      display_service()->GetNotification(kSupervisionTransitionNotificationId));
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, false);
  EXPECT_FALSE(
      display_service()->GetNotification(kSupervisionTransitionNotificationId));
}

}  // namespace arc
