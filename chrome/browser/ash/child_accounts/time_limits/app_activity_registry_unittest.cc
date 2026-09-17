// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limits/app_activity_registry.h"

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_service_wrapper.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limit_utils.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limits_allowlist_policy_test_utils.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limits_allowlist_policy_wrapper.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_notification_delegate.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"
#include "chrome/browser/ash/child_accounts/time_limits/persisted_app_info.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "extensions/common/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"

namespace ash::app_time {

namespace {

class AppTimeNotificationDelegateMock : public AppTimeNotificationDelegate {
 public:
  AppTimeNotificationDelegateMock() = default;
  AppTimeNotificationDelegateMock(const AppTimeNotificationDelegateMock&) =
      delete;
  AppTimeNotificationDelegateMock& operator=(
      const AppTimeNotificationDelegateMock&) = delete;

  ~AppTimeNotificationDelegateMock() override = default;

  MOCK_METHOD3(ShowAppTimeLimitNotification,
               void(const AppId&,
                    const std::optional<base::TimeDelta>&,
                    AppNotification));
};

class AppStateObserverMock : public AppActivityRegistry::AppStateObserver {
 public:
  AppStateObserverMock() = default;
  AppStateObserverMock(const AppStateObserverMock&) = delete;
  AppStateObserverMock& operator=(const AppStateObserverMock&) = delete;

  ~AppStateObserverMock() override = default;

  MOCK_METHOD3(OnAppLimitReached, void(const AppId&, base::TimeDelta, bool));
  MOCK_METHOD1(OnAppLimitRemoved, void(const AppId&));
  MOCK_METHOD1(OnAppInstalled, void(const AppId&));
};

}  // namespace

class AppActivityRegistryTest : public ChromeViewsTestBase {
 protected:
  AppActivityRegistryTest() = default;
  AppActivityRegistryTest(const AppActivityRegistryTest&) = delete;
  AppActivityRegistryTest& operator=(const AppActivityRegistryTest&) = delete;
  ~AppActivityRegistryTest() override = default;

  // ChromeViewsTestBase:
  void SetUp() override;

  void InstallApps();

  base::UnguessableToken CreateInstanceIdForApp(const AppId& app_id);
  base::UnguessableToken GetInstanceIdForApp(const AppId& app_id);

  void SetAppLimit(const AppId& app_id,
                   const std::optional<AppLimit>& app_limit);

  void ReInitializeRegistry();

  AppActivityRegistry& registry() {
    EXPECT_TRUE(registry_.get());
    return *registry_;
  }
  AppActivityRegistry::TestApi& registry_test() {
    EXPECT_TRUE(registry_test_.get());
    return *registry_test_;
  }

  AppTimeNotificationDelegateMock& notification_delegate_mock() {
    return notification_delegate_mock_;
  }
  PrefService* prefs() { return profile_.GetPrefs(); }

  void CreateAppActivityForApp(const AppId& app_id,
                               base::TimeDelta activity_length);

  const AppId app1_{apps::AppType::kArc, "1"};
  const AppId app2_{apps::AppType::kWeb, "3"};
  const AppId google_slides_app_{apps::AppType::kChromeApp,
                                 extension_misc::kGoogleSlidesAppId};

 private:
  TestingProfile profile_;
  AppTimeNotificationDelegateMock notification_delegate_mock_;
  AppServiceWrapper wrapper_{&profile_};
  std::unique_ptr<AppActivityRegistry> registry_;
  std::unique_ptr<AppActivityRegistry::TestApi> registry_test_;

  std::map<AppId, std::vector<base::UnguessableToken>> instance_ids_;
};

void AppActivityRegistryTest::SetUp() {
  ChromeViewsTestBase::SetUp();
  ReInitializeRegistry();
  InstallApps();
}

void AppActivityRegistryTest::InstallApps() {
  registry().OnAppInstalled(GetChromeAppId());
  registry().OnAppInstalled(app1_);
  registry().OnAppInstalled(app2_);
  registry().OnAppAvailable(GetChromeAppId());
  registry().OnAppAvailable(app1_);
  registry().OnAppAvailable(app2_);
}

base::UnguessableToken AppActivityRegistryTest::CreateInstanceIdForApp(
    const AppId& app_id) {
  base::UnguessableToken instance_id(base::UnguessableToken::Create());
  instance_ids_[app_id].push_back(instance_id);
  return instance_id;
}

base::UnguessableToken AppActivityRegistryTest::GetInstanceIdForApp(
    const AppId& app_id) {
  const std::vector<base::UnguessableToken>& app_windows =
      instance_ids_.at(app_id);
  EXPECT_GE(app_windows.size(), 0u);
  return app_windows[app_windows.size() - 1];
}

void AppActivityRegistryTest::SetAppLimit(
    const AppId& app_id,
    const std::optional<AppLimit>& app_limit) {
  registry().SetAppLimit(app_id, app_limit);
  task_environment()->RunUntilIdle();
}

void AppActivityRegistryTest::ReInitializeRegistry() {
  registry_ = std::make_unique<AppActivityRegistry>(
      &wrapper_, &notification_delegate_mock_, prefs());

  registry_test_ =
      std::make_unique<AppActivityRegistry::TestApi>(registry_.get());
}

void AppActivityRegistryTest::CreateAppActivityForApp(
    const AppId& app_id,
    base::TimeDelta activity_length) {
  auto app_instance_id = CreateInstanceIdForApp(app_id);
  registry().OnAppActive(app_id, app_instance_id, base::Time::Now());
  task_environment()->FastForwardBy(activity_length);
  registry().OnAppInactive(app_id, app_instance_id, base::Time::Now());
}

TEST_F(AppActivityRegistryTest, RunningActiveTimeCheck) {
  auto app1_instance_id = CreateInstanceIdForApp(app1_);

  base::Time app1_start_time = base::Time::Now();
  base::TimeDelta active_time = base::Minutes(5);
  registry().OnAppActive(app1_, app1_instance_id, app1_start_time);
  task_environment()->FastForwardBy(active_time / 2);
  EXPECT_EQ(active_time / 2, registry().GetActiveTime(app1_));
  EXPECT_TRUE(registry().IsAppActive(app1_));

  task_environment()->FastForwardBy(active_time / 2);
  base::Time app1_end_time = base::Time::Now();
  registry().OnAppInactive(app1_, app1_instance_id, app1_end_time);
  EXPECT_EQ(active_time, registry().GetActiveTime(app1_));
  EXPECT_FALSE(registry().IsAppActive(app1_));
}

TEST_F(AppActivityRegistryTest, MultipleWindowSameApp) {
  auto app2_instance_id1 = CreateInstanceIdForApp(app2_);
  auto app2_instance_id2 = CreateInstanceIdForApp(app2_);

  base::TimeDelta app2_active_time = base::Minutes(5);

  registry().OnAppActive(app2_, app2_instance_id1, base::Time::Now());
  task_environment()->FastForwardBy(app2_active_time / 2);

  registry().OnAppActive(app2_, app2_instance_id2, base::Time::Now());
  registry().OnAppInactive(app2_, app2_instance_id1, base::Time::Now());
  registry().OnAppInactive(app2_, app2_instance_id1, base::Time::Now());
  EXPECT_TRUE(registry().IsAppActive(app2_));

  task_environment()->FastForwardBy(app2_active_time / 2);

  // Repeated calls to OnAppInactive shouldn't affect the time calculation.
  registry().OnAppInactive(app2_, app2_instance_id1, base::Time::Now());

  // Mark the application inactive.
  registry().OnAppInactive(app2_, app2_instance_id2, base::Time::Now());

  // There was no interruption in active times. Therefore, the app should
  // be active for the whole 5 minutes.
  EXPECT_EQ(app2_active_time, registry().GetActiveTime(app2_));

  base::TimeDelta app2_inactive_time = base::Minutes(1);

  registry().OnAppActive(app2_, app2_instance_id1, base::Time::Now());
  task_environment()->FastForwardBy(app2_active_time / 2);

  registry().OnAppInactive(app2_, app2_instance_id1, base::Time::Now());
  task_environment()->FastForwardBy(app2_inactive_time);
  EXPECT_FALSE(registry().IsAppActive(app2_));

  registry().OnAppActive(app2_, app2_instance_id2, base::Time::Now());
  task_environment()->FastForwardBy(app2_active_time / 2);

  registry().OnAppInactive(app2_, app2_instance_id1, base::Time::Now());
  EXPECT_TRUE(registry().IsAppActive(app2_));

  registry().OnAppInactive(app2_, app2_instance_id2, base::Time::Now());
  EXPECT_FALSE(registry().IsAppActive(app2_));

  EXPECT_EQ(app2_active_time * 2, registry().GetActiveTime(app2_));
}

TEST_F(AppActivityRegistryTest, AppTimeLimitReachedActiveApp) {
  base::Time start = base::Time::Now();

  // Set the time limit for app1_ to be 10 minutes.
  const AppLimit limit(AppRestriction::kTimeLimit, base::Minutes(10), start);
  SetAppLimit(app1_, limit);

  EXPECT_EQ(registry().GetAppState(app1_), AppState::kAvailable);

  auto app1_instance_id = CreateInstanceIdForApp(app1_);

  registry().OnAppActive(app1_, app1_instance_id, start);

  // Expect 5 minute left notification.
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(app1_, testing::_,
                                           AppNotification::kFiveMinutes))
      .Times(1);
  task_environment()->FastForwardBy(base::Minutes(5));
  EXPECT_EQ(base::Minutes(5), registry().GetActiveTime(app1_));
  EXPECT_TRUE(registry().IsAppActive(app1_));

  // Expect One minute left notification.
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(app1_, testing::_,
                                           AppNotification::kOneMinute))
      .Times(1);
  task_environment()->FastForwardBy(base::Minutes(4));
  EXPECT_EQ(base::Minutes(9), registry().GetActiveTime(app1_));

  // Expect time limit reached notification.
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(app1_, testing::_,
                                           AppNotification::kTimeLimitReached))
      .Times(1);
  task_environment()->FastForwardBy(base::Minutes(1));
  EXPECT_EQ(base::Minutes(10), registry().GetActiveTime(app1_));

  EXPECT_EQ(registry().GetAppState(app1_), AppState::kLimitReached);
}

TEST_F(AppActivityRegistryTest, SkippedFiveMinuteNotification) {
  // The application is inactive when the time limit is reached.
  base::Time start = base::Time::Now();

  // Set the time limit for app1_ to be 25 minutes.
  const AppLimit limit(AppRestriction::kTimeLimit, base::Minutes(25), start);
  SetAppLimit(app1_, limit);

  auto app1_instance_id = CreateInstanceIdForApp(app1_);
  base::TimeDelta active_time = base::Minutes(10);
  registry().OnAppActive(app1_, app1_instance_id, start);

  task_environment()->FastForwardBy(active_time);

  const AppLimit new_limit(AppRestriction::kTimeLimit, base::Minutes(14),
                           start + active_time);
  SetAppLimit(app1_, new_limit);

  // Notice that the 5 minute notification is jumped.
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(app1_, testing::_,
                                           AppNotification::kOneMinute))
      .Times(1);
  task_environment()->FastForwardBy(base::Minutes(3));
}

TEST_F(AppActivityRegistryTest, SkippedAllNotifications) {
  // The application is inactive when the time limit is reached.
  base::Time start = base::Time::Now();

  // Set the time limit for app1_ to be 25 minutes.
  const AppLimit limit(AppRestriction::kTimeLimit, base::Minutes(25), start);
  SetAppLimit(app1_, limit);

  auto app1_instance_id = CreateInstanceIdForApp(app1_);
  base::TimeDelta active_time = base::Minutes(10);
  registry().OnAppActive(app1_, app1_instance_id, start);

  task_environment()->FastForwardBy(active_time);

  // Notice that the 5 minute and 1 minute notifications are jumped.
  const AppLimit new_limit(AppRestriction::kTimeLimit, base::Minutes(5),
                           start + active_time);
  SetAppLimit(app1_, new_limit);

  EXPECT_EQ(registry().GetAppState(app1_), AppState::kLimitReached);
}

TEST_F(AppActivityRegistryTest, BlockedAppSetAvailable) {
  base::Time start = base::Time::Now();

  const base::TimeDelta kTenMinutes = base::Minutes(10);
  const AppLimit limit(AppRestriction::kTimeLimit, kTenMinutes, start);
  SetAppLimit(app1_, limit);

  auto app1_instance_id = CreateInstanceIdForApp(app1_);
  registry().OnAppActive(app1_, app1_instance_id, start);

  // There are going to be a bunch of mock notification calls for kFiveMinutes,
  // kOneMinute, and kTimeLimitReached. They have already been tested in the
  // other tests. Let's igonre them.
  task_environment()->FastForwardBy(kTenMinutes);

  EXPECT_EQ(registry().GetAppState(app1_), AppState::kLimitReached);

  const AppLimit new_limit(AppRestriction::kTimeLimit, base::Minutes(20),
                           start + kTenMinutes);
  SetAppLimit(app1_, new_limit);
  EXPECT_EQ(registry().GetAppState(app1_), AppState::kAvailable);
}

TEST_F(AppActivityRegistryTest, ResetTimeReached) {
  base::Time start = base::Time::Now();
  const base::TimeDelta kTenMinutes = base::Minutes(10);

  const AppLimit limit1(AppRestriction::kTimeLimit, kTenMinutes, start);
  const AppLimit limit2(AppRestriction::kTimeLimit, base::Minutes(20), start);
  const std::map<AppId, AppLimit> limits{{app1_, limit1},
                                         {GetChromeAppId(), limit2}};
  registry().UpdateAppLimits(limits);

  auto app1_instance_id = CreateInstanceIdForApp(app1_);
  auto app2_instance_id = CreateInstanceIdForApp(app2_);
  registry().OnAppActive(app1_, app1_instance_id, start);
  registry().OnAppActive(app2_, app2_instance_id, start);

  task_environment()->FastForwardBy(kTenMinutes);

  // App 1's time limit has been reached.
  EXPECT_TRUE(registry().IsAppTimeLimitReached(app1_));
  EXPECT_EQ(kTenMinutes, registry().GetActiveTime(app1_));

  // App 2 is still active.
  EXPECT_FALSE(registry().IsAppTimeLimitReached(app2_));
  EXPECT_EQ(kTenMinutes, registry().GetActiveTime(app2_));

  // Reset time has been reached.
  registry().OnResetTimeReached(start + kTenMinutes);
  EXPECT_FALSE(registry().IsAppTimeLimitReached(app1_));
  EXPECT_EQ(base::Seconds(0), registry().GetActiveTime(app1_));
  EXPECT_FALSE(registry().IsAppTimeLimitReached(app2_));
  EXPECT_EQ(base::Seconds(0), registry().GetActiveTime(app2_));

  // Now make sure that the timers have been scheduled appropriately.
  registry().OnAppActive(app1_, app1_instance_id, start);

  task_environment()->FastForwardBy(kTenMinutes);

  EXPECT_TRUE(registry().IsAppTimeLimitReached(app1_));
  EXPECT_EQ(kTenMinutes, registry().GetActiveTime(app1_));

  // App 2 is still active.
  EXPECT_FALSE(registry().IsAppTimeLimitReached(app2_));
  EXPECT_EQ(kTenMinutes, registry().GetActiveTime(app2_));

  // Now let's make sure
  task_environment()->FastForwardBy(kTenMinutes);
  EXPECT_TRUE(registry().IsAppTimeLimitReached(app2_));
  EXPECT_EQ(*limit2.daily_limit(), registry().GetActiveTime(app2_));
}

TEST_F(AppActivityRegistryTest, SharedTimeLimitForChromeAndWebApps) {
  base::Time start = base::Time::Now();
  const base::TimeDelta kOneHour = base::Hours(1);
  const base::TimeDelta kHalfHour = base::Minutes(30);

  const AppId kChromeAppId = GetChromeAppId();

  const AppLimit limit1(AppRestriction::kTimeLimit, kOneHour, start);
  const std::map<AppId, AppLimit> limits{{kChromeAppId, limit1}};

  registry().UpdateAppLimits(limits);

  auto app2_instance_id = CreateInstanceIdForApp(app2_);

  // Make chrome active for 30 minutes.
  registry().OnChromeAppActivityChanged(ChromeAppActivityState::kActive, start);
  task_environment()->FastForwardBy(kHalfHour);
  registry().OnChromeAppActivityChanged(ChromeAppActivityState::kInactive,
                                        start + kHalfHour);

  // Expect that the active running time for app2 has been updated.
  EXPECT_EQ(kHalfHour, registry().GetActiveTime(kChromeAppId));
  EXPECT_EQ(kHalfHour, registry().GetActiveTime(app2_));

  // Make |app2_| active for 30 minutes. Expect that it reaches its time limit.
  registry().OnAppActive(app2_, app2_instance_id, start + kHalfHour);
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(app2_, testing::_,
                                           AppNotification::kFiveMinutes))
      .Times(1);
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(app2_, testing::_,
                                           AppNotification::kOneMinute))
      .Times(1);
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(app2_, testing::_,
                                           AppNotification::kTimeLimitReached))
      .Times(1);

  task_environment()->FastForwardBy(kHalfHour);

  EXPECT_TRUE(registry().IsAppTimeLimitReached(app2_));
  EXPECT_TRUE(registry().IsAppTimeLimitReached(kChromeAppId));
}

TEST_F(AppActivityRegistryTest, LimitChangedForActiveApp) {
  EXPECT_EQ(registry().GetAppState(app1_), AppState::kAvailable);

  auto app1_instance_id = CreateInstanceIdForApp(app1_);
  base::Time start = base::Time::Now();
  registry().OnAppActive(app1_, app1_instance_id, start);

  EXPECT_TRUE(registry().IsAppActive(app1_));
  EXPECT_EQ(base::Minutes(0), registry().GetActiveTime(app1_));
  EXPECT_EQ(std::nullopt, registry_test().GetAppLimit(app1_));
  EXPECT_EQ(std::nullopt, registry().GetTimeLimit(app1_));
  EXPECT_EQ(std::nullopt, registry_test().GetTimeLeft(app1_));

  task_environment()->FastForwardBy(base::Minutes(5));

  // Limit set for active app.
  const AppLimit limit1(AppRestriction::kTimeLimit, base::Minutes(11),
                        base::Time::Now());
  SetAppLimit(app1_, limit1);

  EXPECT_TRUE(registry().IsAppActive(app1_));
  EXPECT_EQ(base::Minutes(5), registry().GetActiveTime(app1_));
  EXPECT_EQ(base::Minutes(11), *registry().GetTimeLimit(app1_));
  EXPECT_EQ(base::Minutes(6), registry_test().GetTimeLeft(app1_));

  task_environment()->FastForwardBy(base::Minutes(5));

  EXPECT_TRUE(registry().IsAppActive(app1_));
  EXPECT_EQ(base::Minutes(10), registry().GetActiveTime(app1_));
  EXPECT_EQ(base::Minutes(11), *registry().GetTimeLimit(app1_));
  EXPECT_EQ(base::Minutes(1), registry_test().GetTimeLeft(app1_));

  // Increase the limit.
  const AppLimit limit_increase(AppRestriction::kTimeLimit, base::Minutes(20),
                                base::Time::Now());
  SetAppLimit(app1_, limit_increase);
  EXPECT_TRUE(registry().IsAppActive(app1_));
  EXPECT_EQ(base::Minutes(10), registry().GetActiveTime(app1_));
  EXPECT_EQ(base::Minutes(20), *registry().GetTimeLimit(app1_));
  EXPECT_EQ(base::Minutes(10), registry_test().GetTimeLeft(app1_));

  // Decrease the limit.
  const AppLimit limit_decrease(AppRestriction::kTimeLimit, base::Minutes(5),
                                base::Time::Now());
  SetAppLimit(app1_, limit_decrease);
  EXPECT_FALSE(registry().IsAppActive(app1_));
  EXPECT_TRUE(registry().IsAppTimeLimitReached(app1_));
  EXPECT_EQ(base::Minutes(10), registry().GetActiveTime(app1_));
  EXPECT_EQ(base::Minutes(5), *registry().GetTimeLimit(app1_));
  EXPECT_EQ(base::Minutes(0), registry_test().GetTimeLeft(app1_));
}

TEST_F(AppActivityRegistryTest, LimitChangesForInactiveApp) {
  // Set initial limit.
  const AppLimit limit(AppRestriction::kTimeLimit, base::Minutes(5),
                       base::Time::Now());
  SetAppLimit(app1_, limit);

  // Use available limit - app should become paused.
  auto app1_instance_id = CreateInstanceIdForApp(app1_);
  registry().OnAppActive(app1_, app1_instance_id, base::Time::Now());
  task_environment()->FastForwardBy(base::Minutes(5));

  EXPECT_FALSE(registry().IsAppActive(app1_));
  EXPECT_TRUE(registry().IsAppTimeLimitReached(app1_));
  EXPECT_EQ(base::Minutes(5), registry().GetActiveTime(app1_));
  EXPECT_EQ(base::Minutes(5), *registry().GetTimeLimit(app1_));
  EXPECT_EQ(base::Minutes(0), registry_test().GetTimeLeft(app1_));

  // Decrease limit - app should remain paused.
  const AppLimit decreased_limit(AppRestriction::kTimeLimit, base::Minutes(3),
                                 base::Time::Now());
  SetAppLimit(app1_, decreased_limit);

  EXPECT_FALSE(registry().IsAppActive(app1_));
  EXPECT_TRUE(registry().IsAppTimeLimitReached(app1_));
  EXPECT_EQ(base::Minutes(5), registry().GetActiveTime(app1_));
  EXPECT_EQ(base::Minutes(3), *registry().GetTimeLimit(app1_));
  EXPECT_EQ(base::Minutes(0), registry_test().GetTimeLeft(app1_));

  // Increase limit - app should become available, but inactive.
  const AppLimit increased_limit(AppRestriction::kTimeLimit, base::Minutes(10),
                                 base::Time::Now());
  SetAppLimit(app1_, increased_limit);

  EXPECT_FALSE(registry().IsAppActive(app1_));
  EXPECT_TRUE(registry().IsAppAvailable(app1_));
  EXPECT_EQ(base::Minutes(5), registry().GetActiveTime(app1_));
  EXPECT_EQ(base::Minutes(10), *registry().GetTimeLimit(app1_));
  EXPECT_EQ(base::Minutes(5), registry_test().GetTimeLeft(app1_));

  // Decrease limit above used time - app should stay available.
  const AppLimit limit_above_used(AppRestriction::kTimeLimit, base::Minutes(8),
                                  base::Time::Now());
  SetAppLimit(app1_, limit_above_used);

  EXPECT_FALSE(registry().IsAppActive(app1_));
  EXPECT_TRUE(registry().IsAppAvailable(app1_));
  EXPECT_EQ(base::Minutes(5), registry().GetActiveTime(app1_));
  EXPECT_EQ(base::Minutes(8), *registry().GetTimeLimit(app1_));
  EXPECT_EQ(base::Minutes(3), registry_test().GetTimeLeft(app1_));

  // Decrease limit below below time - app should become unavailabe.
  const AppLimit limit_below_used(AppRestriction::kTimeLimit, base::Minutes(4),
                                  base::Time::Now());
  SetAppLimit(app1_, limit_below_used);

  EXPECT_FALSE(registry().IsAppActive(app1_));
  EXPECT_TRUE(registry().IsAppTimeLimitReached(app1_));
  EXPECT_EQ(base::Minutes(5), registry().GetActiveTime(app1_));
  EXPECT_EQ(base::Minutes(4), *registry().GetTimeLimit(app1_));
  EXPECT_EQ(base::Minutes(0), *registry_test().GetTimeLeft(app1_));
}

TEST_F(AppActivityRegistryTest, RemoveLimitsFromAllowlistedApps) {
  // Set initial limit.
  const AppLimit limit(AppRestriction::kTimeLimit, base::Minutes(5),
                       base::Time::Now());
  SetAppLimit(app1_, limit);
  SetAppLimit(app2_, limit);

  AppTimeLimitsAllowlistPolicyBuilder builder;
  builder.SetUp();
  builder.AppendToAllowlistAppList(app1_);

  AppTimeLimitsAllowlistPolicyWrapper wrapper(&builder.dict());
  registry().OnTimeLimitAllowlistChanged(wrapper);

  EXPECT_FALSE(registry_test().GetAppLimit(app1_));
  EXPECT_EQ(limit.daily_limit(), *registry().GetTimeLimit(app2_));
  EXPECT_EQ(registry().GetAppState(app1_), AppState::kAlwaysAvailable);
}

TEST_F(AppActivityRegistryTest, AllowlistedAppsNoLimits) {
  AppTimeLimitsAllowlistPolicyBuilder builder;
  builder.SetUp();
  builder.AppendToAllowlistAppList(app1_);
  AppTimeLimitsAllowlistPolicyWrapper wrapper(&builder.dict());
  registry().OnTimeLimitAllowlistChanged(wrapper);

  // Set initial limit.
  const AppLimit limit(AppRestriction::kTimeLimit, base::Minutes(5),
                       base::Time::Now());
  SetAppLimit(app1_, limit);
  SetAppLimit(app2_, limit);

  EXPECT_FALSE(registry_test().GetAppLimit(app1_));
  EXPECT_EQ(limit.daily_limit(), *registry().GetTimeLimit(app2_));
  EXPECT_EQ(registry().GetAppState(app1_), AppState::kAlwaysAvailable);
}

TEST_F(AppActivityRegistryTest, RestoredApplicationInformation) {
  auto app1_instance_id = CreateInstanceIdForApp(app1_);
  base::TimeDelta active_timedelta = base::Minutes(30);

  const AppLimit limit(AppRestriction::kTimeLimit, active_timedelta,
                       base::Time::Now());
  SetAppLimit(app1_, limit);

  base::Time app1_start_time_1 = base::Time::Now();
  registry().OnAppActive(app1_, app1_instance_id, app1_start_time_1);
  task_environment()->FastForwardBy(active_timedelta / 2);

  // Save app activity.
  registry_test().SaveAppActivity();

  base::Time app1_inactive_time_1 = base::Time::Now();
  registry().OnAppInactive(app1_, app1_instance_id, app1_inactive_time_1);

  // App1 is inactive for 5 minutes.
  task_environment()->FastForwardBy(base::Minutes(5));

  base::Time app1_start_time_2 = base::Time::Now();
  registry().OnAppActive(app1_, app1_instance_id, app1_start_time_2);
  task_environment()->FastForwardBy(active_timedelta / 2);

  // Time limit is reached. App becomes inactive.
  EXPECT_FALSE(registry().IsAppActive(app1_));
  base::Time app1_inactive_time_2 = base::Time::Now();

  // Save app activity.
  registry_test().SaveAppActivity();

  // Now let's recreate AppActivityRegistry. Its state should be restored.
  ReInitializeRegistry();

  EXPECT_TRUE(registry().IsAppInstalled(app1_));
  EXPECT_TRUE(registry().IsAppInstalled(app2_));
  EXPECT_TRUE(registry().IsAppTimeLimitReached(app1_));
  EXPECT_TRUE(registry().IsAppAvailable(app2_));
  EXPECT_EQ(registry().GetActiveTime(app1_), active_timedelta);

  // Now let's test that the app activity are stored appropriately.
  const base::ListValue& list =
      prefs()->GetList(ash::prefs::kPerAppTimeLimitsAppActivities);

  const std::vector<PersistedAppInfo> app_infos =
      PersistedAppInfo::PersistedAppInfosFromList(
          list,
          /* include_app_activity_array */ true);

  // 3 applications. app1_, app2_ and Chrome browser.
  EXPECT_TRUE(app_infos.size() == 3);
  std::vector<AppActivity::ActiveTime> app1_times = {{
      AppActivity::ActiveTime(app1_start_time_1, app1_inactive_time_1),
      AppActivity::ActiveTime(app1_start_time_2, app1_inactive_time_2),
  }};

  for (const auto& app_info : app_infos) {
    if (app_info.app_id() == app1_) {
      const std::vector<AppActivity::ActiveTime> active_time =
          app_info.active_times();
      EXPECT_EQ(app1_times.size(), active_time.size());
      for (size_t i = 0; i < app1_times.size(); i++) {
        EXPECT_EQ(app1_times[i], active_time[i]);
      }

    } else {
      EXPECT_TRUE(app_info.active_times().size() == 0);
    }
  }
}

TEST_F(AppActivityRegistryTest, RemoveUninstalledApplications) {
  CreateAppActivityForApp(app1_, base::Hours(1));
  CreateAppActivityForApp(app2_, base::Hours(1));

  // App1 has been uninstalled.
  registry().OnAppUninstalled(app1_);
  task_environment()->FastForwardBy(base::Minutes(10));

  // Removes app1_ and cleans up ActiveTimes list in user pref.
  registry().OnSuccessfullyReported(base::Time::Now());

  // Now let's test that the app activity are stored appropriately.
  const base::ListValue& list =
      prefs()->GetList(ash::prefs::kPerAppTimeLimitsAppActivities);

  const std::vector<PersistedAppInfo> app_infos =
      PersistedAppInfo::PersistedAppInfosFromList(
          list,
          /* include_app_activity_array */ true);

  EXPECT_EQ(app_infos.size(), 3u);
  for (const auto& entry : app_infos) {
    EXPECT_EQ(entry.active_times().size(), 0u);
  }

  // app1_ will still be present since it still has activity.
  registry().OnResetTimeReached(base::Time::Now());
  registry().SaveAppActivity();
  registry().OnSuccessfullyReported(base::Time::Now());

  const base::ListValue& new_list =
      prefs()->GetList(ash::prefs::kPerAppTimeLimitsAppActivities);

  const std::vector<PersistedAppInfo> final_app_infos =
      PersistedAppInfo::PersistedAppInfosFromList(
          new_list,
          /* include_app_activity_array */ false);

  // Two apps left. They are Chrome, and app2_.
  EXPECT_EQ(final_app_infos.size(), 2u);
  for (const auto& entry : final_app_infos) {
    EXPECT_NE(entry.app_id(), app1_);
  }
}

TEST_F(AppActivityRegistryTest, RemoveOldEntries) {
  base::Time start_time = base::Time::Now();

  CreateAppActivityForApp(app1_, base::Hours(1));
  CreateAppActivityForApp(app2_, base::Hours(1));

  prefs()->SetInt64(ash::prefs::kPerAppTimeLimitsLastSuccessfulReportTime,
                    start_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  task_environment()->AdvanceClock(base::Days(30));
  task_environment()->RunUntilIdle();

  // Now let's recreate AppActivityRegistry. Its state should be restored.
  ReInitializeRegistry();

  // Now let's test that the app activity are stored appropriately.
  const base::ListValue& list =
      prefs()->GetList(ash::prefs::kPerAppTimeLimitsAppActivities);

  const std::vector<PersistedAppInfo> app_infos =
      PersistedAppInfo::PersistedAppInfosFromList(
          list,
          /* include_app_activity_array */ true);

  // The app activities have been cleared.
  for (const auto& app_info : app_infos) {
    const std::vector<AppActivity::ActiveTime>& active_time =
        app_info.active_times();
    EXPECT_EQ(active_time.size(), 0u);
  }
}

TEST_F(AppActivityRegistryTest, ActiveWebAppBlocked) {
  // Create activity for web app.
  CreateAppActivityForApp(app2_, base::Hours(1));

  // Set Chrome as active.
  registry().OnChromeAppActivityChanged(ChromeAppActivityState::kActive,
                                        base::Time::Now());

  // Update the time limits for Chrome.
  AppLimit chrome_limit(AppRestriction::kTimeLimit, base::Minutes(30),
                        base::Time::Now());

  std::map<AppId, AppLimit> app_limits = {{GetChromeAppId(), chrome_limit}};
  registry().UpdateAppLimits(app_limits);

  // Web time limit should be reached.
  EXPECT_EQ(registry().GetAppState(app2_), AppState::kLimitReached);

  EXPECT_EQ(registry().GetAppState(GetChromeAppId()), AppState::kLimitReached);
}

TEST_F(AppActivityRegistryTest, OverrideLimitReachedState) {
  AppStateObserverMock state_observer_mock;
  registry().AddAppStateObserver(&state_observer_mock);
  const base::TimeDelta limit = base::Minutes(30);

  std::map<AppId, AppLimit> app_limits = {
      {app1_, AppLimit(AppRestriction::kTimeLimit, limit, base::Time::Now())},
      {GetChromeAppId(),
       AppLimit(AppRestriction::kTimeLimit, limit, base::Time::Now())}};

  registry().UpdateAppLimits(app_limits);

  // Save app activity and reinitialize.
  EXPECT_CALL(state_observer_mock, OnAppLimitReached(app1_, base::Minutes(30),
                                                     /* was_active */ true))
      .Times(1);
  EXPECT_CALL(state_observer_mock, OnAppLimitReached(app2_, base::Minutes(30),
                                                     /* was_active */ true))
      .Times(1);
  EXPECT_CALL(state_observer_mock,
              OnAppLimitReached(GetChromeAppId(), base::Minutes(30),
                                /* was_active */ false))
      .Times(1);

  // App limits will be reached.
  CreateAppActivityForApp(app1_, 2 * limit);
  CreateAppActivityForApp(app2_, 2 * limit);

  // Save app activity and reinitialize.
  registry().SaveAppActivity();
  ReInitializeRegistry();
  registry().AddAppStateObserver(&state_observer_mock);
  registry().UpdateAppLimits(app_limits);

  // When OnAppInstalled is called for AppActivityRegistry, it will notify its
  // app state observers that the app time limit has been reached.
  EXPECT_CALL(state_observer_mock, OnAppLimitReached(app1_, base::Minutes(30),
                                                     /* was_active */ false))
      .Times(1);
  EXPECT_CALL(state_observer_mock, OnAppLimitReached(app2_, base::Minutes(30),
                                                     /* was_active */ false))
      .Times(1);
  EXPECT_CALL(state_observer_mock,
              OnAppLimitReached(GetChromeAppId(), base::Minutes(30),
                                /* was_active */ false))
      .Times(1);
  InstallApps();

  EXPECT_EQ(registry().GetAppState(app1_), AppState::kLimitReached);
  EXPECT_EQ(registry().GetAppState(app2_), AppState::kLimitReached);
  EXPECT_EQ(registry().GetAppState(GetChromeAppId()), AppState::kLimitReached);

  EXPECT_CALL(state_observer_mock, OnAppLimitReached(app1_, base::Minutes(30),
                                                     /* was_active */ true))
      .Times(1);
  EXPECT_CALL(state_observer_mock, OnAppLimitReached(app2_, base::Minutes(30),
                                                     /* was_active */ true))
      .Times(1);

  registry().OnAppActive(app1_, GetInstanceIdForApp(app1_), base::Time::Now());
  registry().OnAppActive(app2_, GetInstanceIdForApp(app2_), base::Time::Now());
}

TEST_F(AppActivityRegistryTest, AvoidReduntantNotifications) {
  const base::TimeDelta delta = base::Minutes(5);
  AppLimit chrome_limit(AppRestriction::kTimeLimit, base::Minutes(30),
                        base::Time::Now());
  AppLimit app1_limit(AppRestriction::kTimeLimit, base::Minutes(5),
                      base::Time::Now() + delta);
  std::map<AppId, AppLimit> app_limits = {{GetChromeAppId(), chrome_limit},
                                          {app1_, app1_limit}};
  EXPECT_CALL(
      notification_delegate_mock(),
      ShowAppTimeLimitNotification(GetChromeAppId(), chrome_limit.daily_limit(),
                                   AppNotification::kTimeLimitChanged))
      .Times(1);

  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(app1_, app1_limit.daily_limit(),
                                           AppNotification::kTimeLimitChanged))
      .Times(1);

  registry().UpdateAppLimits(app_limits);
  registry().SaveAppActivity();

  // Reinitialized the registry. We don't expect redundant time limit updatese
  // will result in notifications.
  ReInitializeRegistry();
  registry().OnAppInstalled(GetChromeAppId());
  registry().OnAppInstalled(app1_);
  registry().OnAppInstalled(app2_);

  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(GetChromeAppId(), testing::_,
                                           AppNotification::kTimeLimitChanged))
      .Times(0);
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(app1_, testing::_,
                                           AppNotification::kTimeLimitChanged))
      .Times(0);

  registry().UpdateAppLimits(app_limits);

  // Update the limit for Chrome.
  AppLimit new_chrome_limit(AppRestriction::kTimeLimit, base::Minutes(15),
                            base::Time::Now() + 2 * delta);
  app_limits.at(GetChromeAppId()) = new_chrome_limit;

  // Expect that there will be a notification for Chrome but not for app1_.
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(GetChromeAppId(),
                                           new_chrome_limit.daily_limit(),
                                           AppNotification::kTimeLimitChanged))
      .Times(1);
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(app1_, testing::_,
                                           AppNotification::kTimeLimitChanged))
      .Times(0);
  registry().UpdateAppLimits(app_limits);
}

TEST_F(AppActivityRegistryTest, NoNotification) {
  AppLimit app1_limit(AppRestriction::kTimeLimit, base::Minutes(30),
                      base::Time::Now());
  std::map<AppId, AppLimit> app_limits = {{app1_, app1_limit}};

  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(app1_, app1_limit.daily_limit(),
                                           AppNotification::kTimeLimitChanged))
      .Times(0);
  registry().SaveAppActivity();
  ReInitializeRegistry();
  registry().UpdateAppLimits(app_limits);
}

TEST_F(AppActivityRegistryTest, NotificationAfterAppInstall) {
  AppLimit app1_limit(AppRestriction::kTimeLimit, base::Minutes(30),
                      base::Time::Now());
  std::map<AppId, AppLimit> app_limits = {{app1_, app1_limit}};

  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(app1_, app1_limit.daily_limit(),
                                           AppNotification::kTimeLimitChanged))
      .Times(1);

  registry().SaveAppActivity();
  ReInitializeRegistry();
  registry().UpdateAppLimits(app_limits);
  registry().OnAppInstalled(app1_);
}

TEST_F(AppActivityRegistryTest, AvoidRedundantCallsToPauseApp) {
  AppStateObserverMock state_observer_mock;
  registry().AddAppStateObserver(&state_observer_mock);

  const base::TimeDelta kOneHour = base::Hours(1);
  registry().SetAppLimit(
      app1_, AppLimit(AppRestriction::kTimeLimit, kOneHour, base::Time::Now()));

  EXPECT_CALL(state_observer_mock, OnAppLimitReached(app1_, base::Hours(1),
                                                     /* was_active */ true))
      .Times(1);
  CreateAppActivityForApp(app1_, kOneHour);
  EXPECT_TRUE(registry().IsAppTimeLimitReached(app1_));

  auto app1_instance_id = GetInstanceIdForApp(app1_);
  EXPECT_CALL(state_observer_mock, OnAppLimitReached(app1_, base::Hours(1),
                                                     /* was_active */ true))
      .Times(0);
  registry().OnAppActive(app1_, app1_instance_id, base::Time::Now());

  auto new_app1_instance_id = CreateInstanceIdForApp(app1_);
  EXPECT_CALL(state_observer_mock, OnAppLimitReached(app1_, base::Hours(1),
                                                     /* was_active */ true))
      .Times(1);
  registry().OnAppActive(app1_, new_app1_instance_id, base::Time::Now());

  registry().OnAppDestroyed(app1_, new_app1_instance_id, base::Time::Now());
}

TEST_F(AppActivityRegistryTest, AppReinstallations) {
  AppStateObserverMock state_observer_mock;
  registry().AddAppStateObserver(&state_observer_mock);

  AppLimit app1_limit(AppRestriction::kTimeLimit, base::Hours(1),
                      base::Time::Now());

  SetAppLimit(app1_, app1_limit);

  EXPECT_CALL(state_observer_mock,
              OnAppLimitReached(app1_, app1_limit.daily_limit().value(),
                                /* was_active */ true))
      .Times(1);

  // Application will reach its time limits.
  CreateAppActivityForApp(app1_, base::Hours(2));
  registry().OnAppUninstalled(app1_);
  registry().SaveAppActivity();

  // Now let's reinstantiate the registry.
  ReInitializeRegistry();
  registry().AddAppStateObserver(&state_observer_mock);

  EXPECT_CALL(state_observer_mock, OnAppInstalled(app1_)).Times(1);

  // The child user reinstalls the application.
  registry().OnAppInstalled(app1_);
  registry().OnAppAvailable(app1_);

  // Let's set the time limit.
  EXPECT_CALL(state_observer_mock,
              OnAppLimitReached(app1_, app1_limit.daily_limit().value(),
                                /* was_active */ false))
      .Times(1);
  registry().SetAppLimit(app1_, app1_limit);

  // Reinstalled within the same session.
  registry().OnAppUninstalled(app1_);
  EXPECT_CALL(state_observer_mock, OnAppInstalled(app1_)).Times(1);
  EXPECT_CALL(state_observer_mock,
              OnAppLimitReached(app1_, app1_limit.daily_limit().value(),
                                /* was_active */ false))
      .Times(1);

  registry().OnAppAvailable(app1_);
}

TEST_F(AppActivityRegistryTest, LimitSetAfterActivity) {
  AppStateObserverMock state_observer_mock;
  registry().AddAppStateObserver(&state_observer_mock);

  const AppId kApp3(apps::AppType::kWeb, "l");
  registry().OnAppInstalled(kApp3);
  registry().OnAppAvailable(kApp3);

  CreateAppActivityForApp(kApp3, base::Hours(1));

  registry().OnAppActive(kApp3, CreateInstanceIdForApp(kApp3),
                         base::Time::Now());

  const AppLimit web_limit(AppRestriction::kTimeLimit, base::Minutes(20),
                           base::Time::Now());
  EXPECT_CALL(
      state_observer_mock,
      OnAppLimitReached(GetChromeAppId(), web_limit.daily_limit().value(),
                        /* was_active */ false))
      .Times(1);
  EXPECT_CALL(state_observer_mock,
              OnAppLimitReached(app2_, web_limit.daily_limit().value(),
                                /* was_active */ false))
      .Times(1);
  EXPECT_CALL(state_observer_mock,
              OnAppLimitReached(kApp3, web_limit.daily_limit().value(),
                                /* was_active */ true))
      .Times(1);
  const std::map<AppId, AppLimit> limits{{GetChromeAppId(), web_limit}};
  registry().UpdateAppLimits(limits);
}

TEST_F(AppActivityRegistryTest, WebAppInstalled) {
  AppStateObserverMock state_observer_mock;
  registry().AddAppStateObserver(&state_observer_mock);
  const AppLimit web_limit(AppRestriction::kTimeLimit, base::Hours(2),
                           base::Time::Now());
  const std::map<AppId, AppLimit> limits{{GetChromeAppId(), web_limit}};
  registry().UpdateAppLimits(limits);

  registry().OnAppActive(app2_, CreateInstanceIdForApp(app2_),
                         base::Time::Now());
  task_environment()->FastForwardBy(base::Hours(1));

  // Now a new application is installed.
  const AppId kApp3(apps::AppType::kWeb, "l");

  registry().OnAppInstalled(kApp3);
  registry().OnAppAvailable(kApp3);

  EXPECT_CALL(
      state_observer_mock,
      OnAppLimitReached(GetChromeAppId(), web_limit.daily_limit().value(),
                        /* was_active */ false))
      .Times(1);
  EXPECT_CALL(state_observer_mock,
              OnAppLimitReached(app2_, web_limit.daily_limit().value(),
                                /* was_active */ true))
      .Times(1);
  EXPECT_CALL(state_observer_mock,
              OnAppLimitReached(kApp3, web_limit.daily_limit().value(),
                                /* was_active */ false))
      .Times(1);
  task_environment()->FastForwardBy(base::Hours(1));
}

TEST_F(AppActivityRegistryTest, AppBlocked) {
  const AppLimit app1_limit(AppRestriction::kBlocked, std::nullopt,
                            base::Time::Now());
  const std::map<AppId, AppLimit> limits{{app1_, app1_limit}};

  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(app1_, testing::_,
                                           AppNotification::kBlocked))
      .Times(1);
  registry().UpdateAppLimits(limits);

  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(app1_, testing::_,
                                           AppNotification::kAvailable))
      .Times(1);

  registry().UpdateAppLimits(std::map<AppId, AppLimit>());
}

TEST_F(AppActivityRegistryTest, GoogleSlidesPaused) {
  registry().OnAppInstalled(google_slides_app_);
  registry().OnAppAvailable(google_slides_app_);
  AppStateObserverMock state_observer_mock;
  registry().AddAppStateObserver(&state_observer_mock);
  const AppLimit web_limit(AppRestriction::kTimeLimit, base::Hours(2),
                           base::Time::Now());
  const std::map<AppId, AppLimit> limits{{GetChromeAppId(), web_limit}};
  registry().UpdateAppLimits(limits);
  EXPECT_EQ(registry().GetTimeLimit(google_slides_app_),
            web_limit.daily_limit());

  EXPECT_CALL(
      state_observer_mock,
      OnAppLimitReached(GetChromeAppId(), web_limit.daily_limit().value(),
                        /* was_active */ false))
      .Times(1);
  EXPECT_CALL(state_observer_mock,
              OnAppLimitReached(app2_, web_limit.daily_limit().value(),
                                /* was_active */ false))
      .Times(1);
  EXPECT_CALL(
      state_observer_mock,
      OnAppLimitReached(google_slides_app_, web_limit.daily_limit().value(),
                        /* was_active */ true))
      .Times(1);

  CreateAppActivityForApp(app2_, base::Hours(1));
  CreateAppActivityForApp(google_slides_app_, base::Hours(1));
}

}  // namespace ash::app_time
