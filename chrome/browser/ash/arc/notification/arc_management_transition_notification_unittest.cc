// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/notification/arc_management_transition_notification.h"

#include <memory>
#include <string>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/components/arc/session/arc_management_transition.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

struct TransitionNotificationParams {
  TransitionNotificationParams(ArcManagementTransition arc_transition,
                               const gfx::VectorIcon* notification_icon)
      : arc_transition(arc_transition), notification_icon(notification_icon) {}

  ArcManagementTransition arc_transition;
  raw_ptr<const gfx::VectorIcon> notification_icon;
};

}  // namespace

class ArcManagementTransitionNotificationTest
    : public testing::Test,
      public testing::WithParamInterface<TransitionNotificationParams> {
 public:
  ArcManagementTransitionNotificationTest() = default;
  ~ArcManagementTransitionNotificationTest() override = default;
  ArcManagementTransitionNotificationTest(
      const ArcManagementTransitionNotificationTest&) = delete;
  ArcManagementTransitionNotificationTest& operator=(
      const ArcManagementTransitionNotificationTest&) = delete;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
    arc_app_test_.SetUp(profile());

    feature_list_.InitAndEnableFeature(
        kEnableUnmanagedToManagedTransitionFeature);
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

  const gfx::VectorIcon* expected_notification_icon() {
    return GetParam().notification_icon;
  }

  ArcManagementTransition arc_transition() { return GetParam().arc_transition; }

 private:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  ArcAppTest arc_app_test_;

  content::BrowserTaskEnvironment task_environment_;

  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ArcManagementTransitionNotificationTest,
    ::testing::Values(
        TransitionNotificationParams(ArcManagementTransition::NO_TRANSITION,
                                     nullptr),
        TransitionNotificationParams(ArcManagementTransition::CHILD_TO_REGULAR,
                                     &kNotificationFamilyLinkIcon),
        TransitionNotificationParams(ArcManagementTransition::REGULAR_TO_CHILD,
                                     &kNotificationFamilyLinkIcon),
        TransitionNotificationParams(
            ArcManagementTransition::UNMANAGED_TO_MANAGED,
            &chromeos::kEnterpriseIcon)));

TEST_P(ArcManagementTransitionNotificationTest, BaseFlow) {
  ASSERT_TRUE(arc_app_test()->fake_apps().size());
  arc_app_test()->app_instance()->SendRefreshAppList(
      arc_app_test()->fake_apps());
  const std::string app_id =
      ArcAppTest::GetAppId(*arc_app_test()->fake_apps()[0]);

  profile()->GetPrefs()->SetInteger(prefs::kArcManagementTransition,
                                    static_cast<int>(arc_transition()));

  // Attempt to launch ARC app triggers notification.
  LaunchApp(profile(), app_id, 0 /* event_flags */,
            UserInteractionType::NOT_USER_INITIATED);

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_app_test()->arc_app_list_prefs()->GetApp(app_id);
  ASSERT_TRUE(app_info);

  // In case no management transition in progress notification is not
  // triggered.
  if (arc_transition() == ArcManagementTransition::NO_TRANSITION) {
    EXPECT_FALSE(display_service()->GetNotification(
        kManagementTransitionNotificationId));
    // Last launch is set, indicating that launch attempt was not blocked.
    EXPECT_FALSE(app_info->last_launch_time.is_null());
    return;
  }

  {
    auto notification =
        display_service()->GetNotification(kManagementTransitionNotificationId);

    // Notification is shown.
    ASSERT_TRUE(notification);
    // Notification has expected icon.
    EXPECT_EQ(&notification->vector_small_image(),
              expected_notification_icon());
  }

  // Last launch is not set, indicating that launch attempt was blocked.
  EXPECT_TRUE(app_info->last_launch_time.is_null());

  // Finishing transition automatically dismisses notification.
  profile()->GetPrefs()->SetInteger(
      prefs::kArcManagementTransition,
      static_cast<int>(ArcManagementTransition::NO_TRANSITION));
  EXPECT_FALSE(
      display_service()->GetNotification(kManagementTransitionNotificationId));

  // Re-activate notification and check opt out. On opt-out notification is also
  // automatically dismissed.
  profile()->GetPrefs()->SetInteger(prefs::kArcManagementTransition,
                                    static_cast<int>(arc_transition()));
  ShowManagementTransitionNotification(profile());
  EXPECT_TRUE(
      display_service()->GetNotification(kManagementTransitionNotificationId));
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, false);
  EXPECT_FALSE(
      display_service()->GetNotification(kManagementTransitionNotificationId));
}

}  // namespace arc
