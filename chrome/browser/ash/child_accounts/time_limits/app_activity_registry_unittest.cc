// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limits/app_activity_registry.h"

#include <map>
#include <memory>
#include <optional>
#include <vector>

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
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "extensions/common/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"

namespace ash {
namespace app_time {

namespace {

const AppId kApp1(apps::AppType::kArc, "1");
const AppId kApp2(apps::AppType::kWeb, "3");
const AppId kGoogleSlidesApp(apps::AppType::kChromeApp,
                             extension_misc::kGoogleSlidesAppId);

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
  registry().OnAppInstalled(kApp1);
  registry().OnAppInstalled(kApp2);
  registry().OnAppAvailable(GetChromeAppId());
  registry().OnAppAvailable(kApp1);
  registry().OnAppAvailable(kApp2);
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
  auto app1_instance_id = CreateInstanceIdForApp(kApp1);

  base::Time app1_start_time = base::Time::Now();
  base::TimeDelta active_time = base::Minutes(5);
  registry().OnAppActive(kApp1, app1_instance_id, app1_start_time);
  task_environment()->FastForwardBy(active_time / 2);
  EXPECT_EQ(active_time / 2, registry().GetActiveTime(kApp1));
  EXPECT_TRUE(registry().IsAppActive(kApp1));

  task_environment()->FastForwardBy(active_time / 2);
  base::Time app1_end_time = base::Time::Now();
  registry().OnAppInactive(kApp1, app1_instance_id, app1_end_time);
  EXPECT_EQ(active_time, registry().GetActiveTime(kApp1));
  EXPECT_FALSE(registry().IsAppActive(kApp1));
}

TEST_F(AppActivityRegistryTest, MultipleWindowSameApp) {
  auto app2_instance_id1 = CreateInstanceIdForApp(kApp2);
  auto app2_instance_id2 = CreateInstanceIdForApp(kApp2);

  base::TimeDelta app2_active_time = base::Minutes(5);

  registry().OnAppActive(kApp2, app2_instance_id1, base::Time::Now());
  task_environment()->FastForwardBy(app2_active_time / 2);

  registry().OnAppActive(kApp2, app2_instance_id2, base::Time::Now());
  registry().OnAppInactive(kApp2, app2_instance_id1, base::Time::Now());
  registry().OnAppInactive(kApp2, app2_instance_id1, base::Time::Now());
  EXPECT_TRUE(registry().IsAppActive(kApp2));

  task_environment()->FastForwardBy(app2_active_time / 2);

  // Repeated calls to OnAppInactive shouldn't affect the time calculation.
  registry().OnAppInactive(kApp2, app2_instance_id1, base::Time::Now());

  // Mark the application inactive.
  registry().OnAppInactive(kApp2, app2_instance_id2, base::Time::Now());

  // There was no interruption in active times. Therefore, the app should
  // be active for the whole 5 minutes.
  EXPECT_EQ(app2_active_time, registry().GetActiveTime(kApp2));

  base::TimeDelta app2_inactive_time = base::Minutes(1);

  registry().OnAppActive(kApp2, app2_instance_id1, base::Time::Now());
  task_environment()->FastForwardBy(app2_active_time / 2);

  registry().OnAppInactive(kApp2, app2_instance_id1, base::Time::Now());
  task_environment()->FastForwardBy(app2_inactive_time);
  EXPECT_FALSE(registry().IsAppActive(kApp2));

  registry().OnAppActive(kApp2, app2_instance_id2, base::Time::Now());
  task_environment()->FastForwardBy(app2_active_time / 2);

  registry().OnAppInactive(kApp2, app2_instance_id1, base::Time::Now());
  EXPECT_TRUE(registry().IsAppActive(kApp2));

  registry().OnAppInactive(kApp2, app2_instance_id2, base::Time::Now());
  EXPECT_FALSE(registry().IsAppActive(kApp2));

  EXPECT_EQ(app2_active_time * 2, registry().GetActiveTime(kApp2));
}

TEST_F(AppActivityRegistryTest, AppTimeLimitReachedActiveApp) {
  base::Time start = base::Time::Now();

  // Set the time limit for kApp1 to be 10 minutes.
  const AppLimit limit(AppRestriction::kTimeLimit, base::Minutes(10), start);
  SetAppLimit(kApp1, limit);

  EXPECT_EQ(registry().GetAppState(kApp1), AppState::kAvailable);

  auto app1_instance_id = CreateInstanceIdForApp(kApp1);

  registry().OnAppActive(kApp1, app1_instance_id, start);

  // Expect 5 minute left notification.
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(kApp1, testing::_,
                                           AppNotification::kFiveMinutes))
      .Times(1);
  task_environment()->FastForwardBy(base::Minutes(5));
  EXPECT_EQ(base::Minutes(5), registry().GetActiveTime(kApp1));
  EXPECT_TRUE(registry().IsAppActive(kApp1));

  // Expect One minute left notification.
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(kApp1, testing::_,
                                           AppNotification::kOneMinute))
      .Times(1);
  task_environment()->FastForwardBy(base::Minutes(4));
  EXPECT_EQ(base::Minutes(9), registry().GetActiveTime(kApp1));

  // Expect time limit reached notification.
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(kApp1, testing::_,
                                           AppNotification::kTimeLimitReached))
      .Times(1);
  task_environment()->FastForwardBy(base::Minutes(1));
  EXPECT_EQ(base::Minutes(10), registry().GetActiveTime(kApp1));

  EXPECT_EQ(registry().GetAppState(kApp1), AppState::kLimitReached);
}

TEST_F(AppActivityRegistryTest, SkippedFiveMinuteNotification) {
  // The application is inactive when the time limit is reached.
  base::Time start = base::Time::Now();

  // Set the time limit for kApp1 to be 25 minutes.
  const AppLimit limit(AppRestriction::kTimeLimit, base::Minutes(25), start);
  SetAppLimit(kApp1, limit);

  auto app1_instance_id = CreateInstanceIdForApp(kApp1);
  base::TimeDelta active_time = base::Minutes(10);
  registry().OnAppActive(kApp1, app1_instance_id, start);

  task_environment()->FastForwardBy(active_time);

  const AppLimit new_limit(AppRestriction::kTimeLimit, base::Minutes(14),
                           start + active_time);
  SetAppLimit(kApp1, new_limit);

  // Notice that the 5 minute notification is jumped.
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(kApp1, testing::_,
                                           AppNotification::kOneMinute))
      .Times(1);
  task_environment()->FastForwardBy(base::Minutes(3));
}

TEST_F(AppActivityRegistryTest, SkippedAllNotifications) {
  // The application is inactive when the time limit is reached.
  base::Time start = base::Time::Now();

  // Set the time limit for kApp1 to be 25 minutes.
  const AppLimit limit(AppRestriction::kTimeLimit, base::Minutes(25), start);
  SetAppLimit(kApp1, limit);

  auto app1_instance_id = CreateInstanceIdForApp(kApp1);
  base::TimeDelta active_time = base::Minutes(10);
  registry().OnAppActive(kApp1, app1_instance_id, start);

  task_environment()->FastForwardBy(active_time);

  // Notice that the 5 minute and 1 minute notifications are jumped.
  const AppLimit new_limit(AppRestriction::kTimeLimit, base::Minutes(5),
                           start + active_time);
  SetAppLimit(kApp1, new_limit);

  EXPECT_EQ(registry().GetAppState(kApp1), AppState::kLimitReached);
}

TEST_F(AppActivityRegistryTest, BlockedAppSetAvailable) {
  base::Time start = base::Time::Now();

  const base::TimeDelta kTenMinutes = base::Minutes(10);
  const AppLimit limit(AppRestriction::kTimeLimit, kTenMinutes, start);
  SetAppLimit(kApp1, limit);

  auto app1_instance_id = CreateInstanceIdForApp(kApp1);
  registry().OnAppActive(kApp1, app1_instance_id, start);

  // There are going to be a bunch of mock notification calls for kFiveMinutes,
  // kOneMinute, and kTimeLimitReached. They have already been tested in the
  // other tests. Let's igonre them.
  task_environment()->FastForwardBy(kTenMinutes);

  EXPECT_EQ(registry().GetAppState(kApp1), AppState::kLimitReached);

  const AppLimit new_limit(AppRestriction::kTimeLimit, base::Minutes(20),
                           start + kTenMinutes);
  SetAppLimit(kApp1, new_limit);
  EXPECT_EQ(registry().GetAppState(kApp1), AppState::kAvailable);
}

TEST_F(AppActivityRegistryTest, ResetTimeReached) {
  base::Time start = base::Time::Now();
  const base::TimeDelta kTenMinutes = base::Minutes(10);

  const AppLimit limit1(AppRestriction::kTimeLimit, kTenMinutes, start);
  const AppLimit limit2(AppRestriction::kTimeLimit, base::Minutes(20), start);
  const std::map<AppId, AppLimit> limits{{kApp1, limit1},
                                         {GetChromeAppId(), limit2}};
  registry().UpdateAppLimits(limits);

  auto app1_instance_id = CreateInstanceIdForApp(kApp1);
  auto app2_instance_id = CreateInstanceIdForApp(kApp2);
  registry().OnAppActive(kApp1, app1_instance_id, start);
  registry().OnAppActive(kApp2, app2_instance_id, start);

  task_environment()->FastForwardBy(kTenMinutes);

  // App 1's time limit has been reached.
  EXPECT_TRUE(registry().IsAppTimeLimitReached(kApp1));
  EXPECT_EQ(kTenMinutes, registry().GetActiveTime(kApp1));

  // App 2 is still active.
  EXPECT_FALSE(registry().IsAppTimeLimitReached(kApp2));
  EXPECT_EQ(kTenMinutes, registry().GetActiveTime(kApp2));

  // Reset time has been reached.
  registry().OnResetTimeReached(start + kTenMinutes);
  EXPECT_FALSE(registry().IsAppTimeLimitReached(kApp1));
  EXPECT_EQ(base::Seconds(0), registry().GetActiveTime(kApp1));
  EXPECT_FALSE(registry().IsAppTimeLimitReached(kApp2));
  EXPECT_EQ(base::Seconds(0), registry().GetActiveTime(kApp2));

  // Now make sure that the timers have been scheduled appropriately.
  registry().OnAppActive(kApp1, app1_instance_id, start);

  task_environment()->FastForwardBy(kTenMinutes);

  EXPECT_TRUE(registry().IsAppTimeLimitReached(kApp1));
  EXPECT_EQ(kTenMinutes, registry().GetActiveTime(kApp1));

  // App 2 is still active.
  EXPECT_FALSE(registry().IsAppTimeLimitReached(kApp2));
  EXPECT_EQ(kTenMinutes, registry().GetActiveTime(kApp2));

  // Now let's make sure
  task_environment()->FastForwardBy(kTenMinutes);
  EXPECT_TRUE(registry().IsAppTimeLimitReached(kApp2));
  EXPECT_EQ(*limit2.daily_limit(), registry().GetActiveTime(kApp2));
}

TEST_F(AppActivityRegistryTest, SharedTimeLimitForChromeAndWebApps) {
  base::Time start = base::Time::Now();
  const base::TimeDelta kOneHour = base::Hours(1);
  const base::TimeDelta kHalfHour = base::Minutes(30);

  const AppId kChromeAppId = GetChromeAppId();

  const AppLimit limit1(AppRestriction::kTimeLimit, kOneHour, start);
  const std::map<AppId, AppLimit> limits{{kChromeAppId, limit1}};

  registry().UpdateAppLimits(limits);

  auto app2_instance_id = CreateInstanceIdForApp(kApp2);

  // Make chrome active for 30 minutes.
  registry().OnChromeAppActivityChanged(ChromeAppActivityState::kActive, start);
  task_environment()->FastForwardBy(kHalfHour);
  registry().OnChromeAppActivityChanged(ChromeAppActivityState::kInactive,
                                        start + kHalfHour);

  // Expect that the active running time for app2 has been updated.
  EXPECT_EQ(kHalfHour, registry().GetActiveTime(kChromeAppId));
  EXPECT_EQ(kHalfHour, registry().GetActiveTime(kApp2));

  // Make |kApp2| active for 30 minutes. Expect that it reaches its time limit.
  registry().OnAppActive(kApp2, app2_instance_id, start + kHalfHour);
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(kApp2, testing::_,
                                           AppNotification::kFiveMinutes))
      .Times(1);
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(kApp2, testing::_,
                                           AppNotification::kOneMinute))
      .Times(1);
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(kApp2, testing::_,
                                           AppNotification::kTimeLimitReached))
      .Times(1);

  task_environment()->FastForwardBy(kHalfHour);

  EXPECT_TRUE(registry().IsAppTimeLimitReached(kApp2));
  EXPECT_TRUE(registry().IsAppTimeLimitReached(kChromeAppId));
}

TEST_F(AppActivityRegistryTest, LimitChangedForActiveApp) {
  EXPECT_EQ(registry().GetAppState(kApp1), AppState::kAvailable);

  auto app1_instance_id = CreateInstanceIdForApp(kApp1);
  base::Time start = base::Time::Now();
  registry().OnAppActive(kApp1, app1_instance_id, start);

  EXPECT_TRUE(registry().IsAppActive(kApp1));
  EXPECT_EQ(base::Minutes(0), registry().GetActiveTime(kApp1));
  EXPECT_EQ(std::nullopt, registry_test().GetAppLimit(kApp1));
  EXPECT_EQ(std::nullopt, registry().GetTimeLimit(kApp1));
  EXPECT_EQ(std::nullopt, registry_test().GetTimeLeft(kApp1));

  task_environment()->FastForwardBy(base::Minutes(5));

  // Limit set for active app.
  const AppLimit limit1(AppRestriction::kTimeLimit, base::Minutes(11),
                        base::Time::Now());
  SetAppLimit(kApp1, limit1);

  EXPECT_TRUE(registry().IsAppActive(kApp1));
  EXPECT_EQ(base::Minutes(5), registry().GetActiveTime(kApp1));
  EXPECT_EQ(base::Minutes(11), *registry().GetTimeLimit(kApp1));
  EXPECT_EQ(base::Minutes(6), registry_test().GetTimeLeft(kApp1));

  task_environment()->FastForwardBy(base::Minutes(5));

  EXPECT_TRUE(registry().IsAppActive(kApp1));
  EXPECT_EQ(base::Minutes(10), registry().GetActiveTime(kApp1));
  EXPECT_EQ(base::Minutes(11), *registry().GetTimeLimit(kApp1));
  EXPECT_EQ(base::Minutes(1), registry_test().GetTimeLeft(kApp1));

  // Increase the limit.
  const AppLimit limit_increase(AppRestriction::kTimeLimit, base::Minutes(20),
                                base::Time::Now());
  SetAppLimit(kApp1, limit_increase);
  EXPECT_TRUE(registry().IsAppActive(kApp1));
  EXPECT_EQ(base::Minutes(10), registry().GetActiveTime(kApp1));
  EXPECT_EQ(base::Minutes(20), *registry().GetTimeLimit(kApp1));
  EXPECT_EQ(base::Minutes(10), registry_test().GetTimeLeft(kApp1));

  // Decrease the limit.
  const AppLimit limit_decrease(AppRestriction::kTimeLimit, base::Minutes(5),
                                base::Time::Now());
  SetAppLimit(kApp1, limit_decrease);
  EXPECT_FALSE(registry().IsAppActive(kApp1));
  EXPECT_TRUE(registry().IsAppTimeLimitReached(kApp1));
  EXPECT_EQ(base::Minutes(10), registry().GetActiveTime(kApp1));
  EXPECT_EQ(base::Minutes(5), *registry().GetTimeLimit(kApp1));
  EXPECT_EQ(base::Minutes(0), registry_test().GetTimeLeft(kApp1));
}

TEST_F(AppActivityRegistryTest, LimitChangesForInactiveApp) {
  // Set initial limit.
  const AppLimit limit(AppRestriction::kTimeLimit, base::Minutes(5),
                       base::Time::Now());
  SetAppLimit(kApp1, limit);

  // Use available limit - app should become paused.
  auto app1_instance_id = CreateInstanceIdForApp(kApp1);
  registry().OnAppActive(kApp1, app1_instance_id, base::Time::Now());
  task_environment()->FastForwardBy(base::Minutes(5));

  EXPECT_FALSE(registry().IsAppActive(kApp1));
  EXPECT_TRUE(registry().IsAppTimeLimitReached(kApp1));
  EXPECT_EQ(base::Minutes(5), registry().GetActiveTime(kApp1));
  EXPECT_EQ(base::Minutes(5), *registry().GetTimeLimit(kApp1));
  EXPECT_EQ(base::Minutes(0), registry_test().GetTimeLeft(kApp1));

  // Decrease limit - app should remain paused.
  const AppLimit decreased_limit(AppRestriction::kTimeLimit, base::Minutes(3),
                                 base::Time::Now());
  SetAppLimit(kApp1, decreased_limit);

  EXPECT_FALSE(registry().IsAppActive(kApp1));
  EXPECT_TRUE(registry().IsAppTimeLimitReached(kApp1));
  EXPECT_EQ(base::Minutes(5), registry().GetActiveTime(kApp1));
  EXPECT_EQ(base::Minutes(3), *registry().GetTimeLimit(kApp1));
  EXPECT_EQ(base::Minutes(0), registry_test().GetTimeLeft(kApp1));

  // Increase limit - app should become available, but inactive.
  const AppLimit increased_limit(AppRestriction::kTimeLimit, base::Minutes(10),
                                 base::Time::Now());
  SetAppLimit(kApp1, increased_limit);

  EXPECT_FALSE(registry().IsAppActive(kApp1));
  EXPECT_TRUE(registry().IsAppAvailable(kApp1));
  EXPECT_EQ(base::Minutes(5), registry().GetActiveTime(kApp1));
  EXPECT_EQ(base::Minutes(10), *registry().GetTimeLimit(kApp1));
  EXPECT_EQ(base::Minutes(5), registry_test().GetTimeLeft(kApp1));

  // Decrease limit above used time - app should stay available.
  const AppLimit limit_above_used(AppRestriction::kTimeLimit, base::Minutes(8),
                                  base::Time::Now());
  SetAppLimit(kApp1, limit_above_used);

  EXPECT_FALSE(registry().IsAppActive(kApp1));
  EXPECT_TRUE(registry().IsAppAvailable(kApp1));
  EXPECT_EQ(base::Minutes(5), registry().GetActiveTime(kApp1));
  EXPECT_EQ(base::Minutes(8), *registry().GetTimeLimit(kApp1));
  EXPECT_EQ(base::Minutes(3), registry_test().GetTimeLeft(kApp1));

  // Decrease limit below below time - app should become unavailabe.
  const AppLimit limit_below_used(AppRestriction::kTimeLimit, base::Minutes(4),
                                  base::Time::Now());
  SetAppLimit(kApp1, limit_below_used);

  EXPECT_FALSE(registry().IsAppActive(kApp1));
  EXPECT_TRUE(registry().IsAppTimeLimitReached(kApp1));
  EXPECT_EQ(base::Minutes(5), registry().GetActiveTime(kApp1));
  EXPECT_EQ(base::Minutes(4), *registry().GetTimeLimit(kApp1));
  EXPECT_EQ(base::Minutes(0), *registry_test().GetTimeLeft(kApp1));
}

TEST_F(AppActivityRegistryTest, RemoveLimitsFromAllowlistedApps) {
  // Set initial limit.
  const AppLimit limit(AppRestriction::kTimeLimit, base::Minutes(5),
                       base::Time::Now());
  SetAppLimit(kApp1, limit);
  SetAppLimit(kApp2, limit);

  AppTimeLimitsAllowlistPolicyBuilder builder;
  builder.SetUp();
  builder.AppendToAllowlistAppList(kApp1);

  AppTimeLimitsAllowlistPolicyWrapper wrapper(&builder.dict());
  registry().OnTimeLimitAllowlistChanged(wrapper);

  EXPECT_FALSE(registry_test().GetAppLimit(kApp1));
  EXPECT_EQ(limit.daily_limit(), *registry().GetTimeLimit(kApp2));
  EXPECT_EQ(registry().GetAppState(kApp1), AppState::kAlwaysAvailable);
}

TEST_F(AppActivityRegistryTest, AllowlistedAppsNoLimits) {
  AppTimeLimitsAllowlistPolicyBuilder builder;
  builder.SetUp();
  builder.AppendToAllowlistAppList(kApp1);
  AppTimeLimitsAllowlistPolicyWrapper wrapper(&builder.dict());
  registry().OnTimeLimitAllowlistChanged(wrapper);

  // Set initial limit.
  const AppLimit limit(AppRestriction::kTimeLimit, base::Minutes(5),
                       base::Time::Now());
  SetAppLimit(kApp1, limit);
  SetAppLimit(kApp2, limit);

  EXPECT_FALSE(registry_test().GetAppLimit(kApp1));
  EXPECT_EQ(limit.daily_limit(), *registry().GetTimeLimit(kApp2));
  EXPECT_EQ(registry().GetAppState(kApp1), AppState::kAlwaysAvailable);
}

TEST_F(AppActivityRegistryTest, RestoredApplicationInformation) {
  auto app1_instance_id = CreateInstanceIdForApp(kApp1);
  base::TimeDelta active_timedelta = base::Minutes(30);

  const AppLimit limit(AppRestriction::kTimeLimit, active_timedelta,
                       base::Time::Now());
  SetAppLimit(kApp1, limit);

  base::Time app1_start_time_1 = base::Time::Now();
  registry().OnAppActive(kApp1, app1_instance_id, app1_start_time_1);
  task_environment()->FastForwardBy(active_timedelta / 2);

  // Save app activity.
  registry_test().SaveAppActivity();

  base::Time app1_inactive_time_1 = base::Time::Now();
  registry().OnAppInactive(kApp1, app1_instance_id, app1_inactive_time_1);

  // App1 is inactive for 5 minutes.
  task_environment()->FastForwardBy(base::Minutes(5));

  base::Time app1_start_time_2 = base::Time::Now();
  registry().OnAppActive(kApp1, app1_instance_id, app1_start_time_2);
  task_environment()->FastForwardBy(active_timedelta / 2);

  // Time limit is reached. App becomes inactive.
  EXPECT_FALSE(registry().IsAppActive(kApp1));
  base::Time app1_inactive_time_2 = base::Time::Now();

  // Save app activity.
  registry_test().SaveAppActivity();

  // Now let's recreate AppActivityRegistry. Its state should be restored.
  ReInitializeRegistry();

  EXPECT_TRUE(registry().IsAppInstalled(kApp1));
  EXPECT_TRUE(registry().IsAppInstalled(kApp2));
  EXPECT_TRUE(registry().IsAppTimeLimitReached(kApp1));
  EXPECT_TRUE(registry().IsAppAvailable(kApp2));
  EXPECT_EQ(registry().GetActiveTime(kApp1), active_timedelta);

  // Now let's test that the app activity are stored appropriately.
  const base::Value::List& list =
      prefs()->GetList(prefs::kPerAppTimeLimitsAppActivities);

  const std::vector<PersistedAppInfo> app_infos =
      PersistedAppInfo::PersistedAppInfosFromList(
          list,
          /* include_app_activity_array */ true);

  // 3 applications. kApp1, kApp2 and Chrome browser.
  EXPECT_TRUE(app_infos.size() == 3);
  std::vector<AppActivity::ActiveTime> app1_times = {{
      AppActivity::ActiveTime(app1_start_time_1, app1_inactive_time_1),
      AppActivity::ActiveTime(app1_start_time_2, app1_inactive_time_2),
  }};

  for (const auto& app_info : app_infos) {
    if (app_info.app_id() == kApp1) {
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
  CreateAppActivityForApp(kApp1, base::Hours(1));
  CreateAppActivityForApp(kApp2, base::Hours(1));

  // App1 has been uninstalled.
  registry().OnAppUninstalled(kApp1);
  task_environment()->FastForwardBy(base::Minutes(10));

  // Removes kApp1 and cleans up ActiveTimes list in user pref.
  registry().OnSuccessfullyReported(base::Time::Now());

  // Now let's test that the app activity are stored appropriately.
  const base::Value::List& list =
      prefs()->GetList(prefs::kPerAppTimeLimitsAppActivities);

  const std::vector<PersistedAppInfo> app_infos =
      PersistedAppInfo::PersistedAppInfosFromList(
          list,
          /* include_app_activity_array */ true);

  EXPECT_EQ(app_infos.size(), 3u);
  for (const auto& entry : app_infos)
    EXPECT_EQ(entry.active_times().size(), 0u);

  // kApp1 will still be present since it still has activity.
  registry().OnResetTimeReached(base::Time::Now());
  registry().SaveAppActivity();
  registry().OnSuccessfullyReported(base::Time::Now());

  const base::Value::List& new_list =
      prefs()->GetList(prefs::kPerAppTimeLimitsAppActivities);

  const std::vector<PersistedAppInfo> final_app_infos =
      PersistedAppInfo::PersistedAppInfosFromList(
          new_list,
          /* include_app_activity_array */ false);

  // Two apps left. They are Chrome, and kApp2.
  EXPECT_EQ(final_app_infos.size(), 2u);
  for (const auto& entry : final_app_infos) {
    EXPECT_NE(entry.app_id(), kApp1);
  }
}

TEST_F(AppActivityRegistryTest, RemoveOldEntries) {
  base::Time start_time = base::Time::Now();

  CreateAppActivityForApp(kApp1, base::Hours(1));
  CreateAppActivityForApp(kApp2, base::Hours(1));

  prefs()->SetInt64(prefs::kPerAppTimeLimitsLastSuccessfulReportTime,
                    start_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  task_environment()->AdvanceClock(base::Days(30));
  task_environment()->RunUntilIdle();

  // Now let's recreate AppActivityRegistry. Its state should be restored.
  ReInitializeRegistry();

  // Now let's test that the app activity are stored appropriately.
  const base::Value::List& list =
      prefs()->GetList(prefs::kPerAppTimeLimitsAppActivities);

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
  CreateAppActivityForApp(kApp2, base::Hours(1));

  // Set Chrome as active.
  registry().OnChromeAppActivityChanged(ChromeAppActivityState::kActive,
                                        base::Time::Now());

  // Update the time limits for Chrome.
  AppLimit chrome_limit(AppRestriction::kTimeLimit, base::Minutes(30),
                        base::Time::Now());

  std::map<AppId, AppLimit> app_limits = {{GetChromeAppId(), chrome_limit}};
  registry().UpdateAppLimits(app_limits);

  // Web time limit should be reached.
  EXPECT_EQ(registry().GetAppState(kApp2), AppState::kLimitReached);

  EXPECT_EQ(registry().GetAppState(GetChromeAppId()), AppState::kLimitReached);
}

TEST_F(AppActivityRegistryTest, OverrideLimitReachedState) {
  AppStateObserverMock state_observer_mock;
  registry().AddAppStateObserver(&state_observer_mock);
  const base::TimeDelta limit = base::Minutes(30);

  std::map<AppId, AppLimit> app_limits = {
      {kApp1, AppLimit(AppRestriction::kTimeLimit, limit, base::Time::Now())},
      {GetChromeAppId(),
       AppLimit(AppRestriction::kTimeLimit, limit, base::Time::Now())}};

  registry().UpdateAppLimits(app_limits);

  // Save app activity and reinitialize.
  EXPECT_CALL(state_observer_mock, OnAppLimitReached(kApp1, base::Minutes(30),
                                                     /* was_active */ true))
      .Times(1);
  EXPECT_CALL(state_observer_mock, OnAppLimitReached(kApp2, base::Minutes(30),
                                                     /* was_active */ true))
      .Times(1);
  EXPECT_CALL(state_observer_mock,
              OnAppLimitReached(GetChromeAppId(), base::Minutes(30),
                                /* was_active */ false))
      .Times(1);

  // App limits will be reached.
  CreateAppActivityForApp(kApp1, 2 * limit);
  CreateAppActivityForApp(kApp2, 2 * limit);

  // Save app activity and reinitialize.
  registry().SaveAppActivity();
  ReInitializeRegistry();
  registry().AddAppStateObserver(&state_observer_mock);
  registry().UpdateAppLimits(app_limits);

  // When OnAppInstalled is called for AppActivityRegistry, it will notify its
  // app state observers that the app time limit has been reached.
  EXPECT_CALL(state_observer_mock, OnAppLimitReached(kApp1, base::Minutes(30),
                                                     /* was_active */ false))
      .Times(1);
  EXPECT_CALL(state_observer_mock, OnAppLimitReached(kApp2, base::Minutes(30),
                                                     /* was_active */ false))
      .Times(1);
  EXPECT_CALL(state_observer_mock,
              OnAppLimitReached(GetChromeAppId(), base::Minutes(30),
                                /* was_active */ false))
      .Times(1);
  InstallApps();

  EXPECT_EQ(registry().GetAppState(kApp1), AppState::kLimitReached);
  EXPECT_EQ(registry().GetAppState(kApp2), AppState::kLimitReached);
  EXPECT_EQ(registry().GetAppState(GetChromeAppId()), AppState::kLimitReached);

  EXPECT_CALL(state_observer_mock, OnAppLimitReached(kApp1, base::Minutes(30),
                                                     /* was_active */ true))
      .Times(1);
  EXPECT_CALL(state_observer_mock, OnAppLimitReached(kApp2, base::Minutes(30),
                                                     /* was_active */ true))
      .Times(1);

  registry().OnAppActive(kApp1, GetInstanceIdForApp(kApp1), base::Time::Now());
  registry().OnAppActive(kApp2, GetInstanceIdForApp(kApp2), base::Time::Now());
}

TEST_F(AppActivityRegistryTest, AvoidReduntantNotifications) {
  const base::TimeDelta delta = base::Minutes(5);
  AppLimit chrome_limit(AppRestriction::kTimeLimit, base::Minutes(30),
                        base::Time::Now());
  AppLimit app1_limit(AppRestriction::kTimeLimit, base::Minutes(5),
                      base::Time::Now() + delta);
  std::map<AppId, AppLimit> app_limits = {{GetChromeAppId(), chrome_limit},
                                          {kApp1, app1_limit}};
  EXPECT_CALL(
      notification_delegate_mock(),
      ShowAppTimeLimitNotification(GetChromeAppId(), chrome_limit.daily_limit(),
                                   AppNotification::kTimeLimitChanged))
      .Times(1);

  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(kApp1, app1_limit.daily_limit(),
                                           AppNotification::kTimeLimitChanged))
      .Times(1);

  registry().UpdateAppLimits(app_limits);
  registry().SaveAppActivity();

  // Reinitialized the registry. We don't expect redundant time limit updatese
  // will result in notifications.
  ReInitializeRegistry();
  registry().OnAppInstalled(GetChromeAppId());
  registry().OnAppInstalled(kApp1);
  registry().OnAppInstalled(kApp2);

  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(GetChromeAppId(), testing::_,
                                           AppNotification::kTimeLimitChanged))
      .Times(0);
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(kApp1, testing::_,
                                           AppNotification::kTimeLimitChanged))
      .Times(0);

  registry().UpdateAppLimits(app_limits);

  // Update the limit for Chrome.
  AppLimit new_chrome_limit(AppRestriction::kTimeLimit, base::Minutes(15),
                            base::Time::Now() + 2 * delta);
  app_limits.at(GetChromeAppId()) = new_chrome_limit;

  // Expect that there will be a notification for Chrome but not for kApp1.
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(GetChromeAppId(),
                                           new_chrome_limit.daily_limit(),
                                           AppNotification::kTimeLimitChanged))
      .Times(1);
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(kApp1, testing::_,
                                           AppNotification::kTimeLimitChanged))
      .Times(0);
  registry().UpdateAppLimits(app_limits);
}

TEST_F(AppActivityRegistryTest, NoNotification) {
  AppLimit app1_limit(AppRestriction::kTimeLimit, base::Minutes(30),
                      base::Time::Now());
  std::map<AppId, AppLimit> app_limits = {{kApp1, app1_limit}};

  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(kApp1, app1_limit.daily_limit(),
                                           AppNotification::kTimeLimitChanged))
      .Times(0);
  registry().SaveAppActivity();
  ReInitializeRegistry();
  registry().UpdateAppLimits(app_limits);
}

TEST_F(AppActivityRegistryTest, NotificationAfterAppInstall) {
  AppLimit app1_limit(AppRestriction::kTimeLimit, base::Minutes(30),
                      base::Time::Now());
  std::map<AppId, AppLimit> app_limits = {{kApp1, app1_limit}};

  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(kApp1, app1_limit.daily_limit(),
                                           AppNotification::kTimeLimitChanged))
      .Times(1);

  registry().SaveAppActivity();
  ReInitializeRegistry();
  registry().UpdateAppLimits(app_limits);
  registry().OnAppInstalled(kApp1);
}

TEST_F(AppActivityRegistryTest, AvoidRedundantCallsToPauseApp) {
  AppStateObserverMock state_observer_mock;
  registry().AddAppStateObserver(&state_observer_mock);

  const base::TimeDelta kOneHour = base::Hours(1);
  registry().SetAppLimit(
      kApp1, AppLimit(AppRestriction::kTimeLimit, kOneHour, base::Time::Now()));

  EXPECT_CALL(state_observer_mock, OnAppLimitReached(kApp1, base::Hours(1),
                                                     /* was_active */ true))
      .Times(1);
  CreateAppActivityForApp(kApp1, kOneHour);
  EXPECT_TRUE(registry().IsAppTimeLimitReached(kApp1));

  auto app1_instance_id = GetInstanceIdForApp(kApp1);
  EXPECT_CALL(state_observer_mock, OnAppLimitReached(kApp1, base::Hours(1),
                                                     /* was_active */ true))
      .Times(0);
  registry().OnAppActive(kApp1, app1_instance_id, base::Time::Now());

  auto new_app1_instance_id = CreateInstanceIdForApp(kApp1);
  EXPECT_CALL(state_observer_mock, OnAppLimitReached(kApp1, base::Hours(1),
                                                     /* was_active */ true))
      .Times(1);
  registry().OnAppActive(kApp1, new_app1_instance_id, base::Time::Now());

  registry().OnAppDestroyed(kApp1, new_app1_instance_id, base::Time::Now());
}

TEST_F(AppActivityRegistryTest, AppReinstallations) {
  AppStateObserverMock state_observer_mock;
  registry().AddAppStateObserver(&state_observer_mock);

  AppLimit app1_limit(AppRestriction::kTimeLimit, base::Hours(1),
                      base::Time::Now());

  SetAppLimit(kApp1, app1_limit);

  EXPECT_CALL(state_observer_mock,
              OnAppLimitReached(kApp1, app1_limit.daily_limit().value(),
                                /* was_active */ true))
      .Times(1);

  // Application will reach its time limits.
  CreateAppActivityForApp(kApp1, base::Hours(2));
  registry().OnAppUninstalled(kApp1);
  registry().SaveAppActivity();

  // Now let's reinstantiate the registry.
  ReInitializeRegistry();
  registry().AddAppStateObserver(&state_observer_mock);

  EXPECT_CALL(state_observer_mock, OnAppInstalled(kApp1)).Times(1);

  // The child user reinstalls the application.
  registry().OnAppInstalled(kApp1);
  registry().OnAppAvailable(kApp1);

  // Let's set the time limit.
  EXPECT_CALL(state_observer_mock,
              OnAppLimitReached(kApp1, app1_limit.daily_limit().value(),
                                /* was_active */ false))
      .Times(1);
  registry().SetAppLimit(kApp1, app1_limit);

  // Reinstalled within the same session.
  registry().OnAppUninstalled(kApp1);
  EXPECT_CALL(state_observer_mock, OnAppInstalled(kApp1)).Times(1);
  EXPECT_CALL(state_observer_mock,
              OnAppLimitReached(kApp1, app1_limit.daily_limit().value(),
                                /* was_active */ false))
      .Times(1);

  registry().OnAppAvailable(kApp1);
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
              OnAppLimitReached(kApp2, web_limit.daily_limit().value(),
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

  registry().OnAppActive(kApp2, CreateInstanceIdForApp(kApp2),
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
              OnAppLimitReached(kApp2, web_limit.daily_limit().value(),
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
  const std::map<AppId, AppLimit> limits{{kApp1, app1_limit}};

  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(kApp1, testing::_,
                                           AppNotification::kBlocked))
      .Times(1);
  registry().UpdateAppLimits(limits);

  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(kApp1, testing::_,
                                           AppNotification::kAvailable))
      .Times(1);

  registry().UpdateAppLimits(std::map<AppId, AppLimit>());
}

TEST_F(AppActivityRegistryTest, GoogleSlidesPaused) {
  registry().OnAppInstalled(kGoogleSlidesApp);
  registry().OnAppAvailable(kGoogleSlidesApp);
  AppStateObserverMock state_observer_mock;
  registry().AddAppStateObserver(&state_observer_mock);
  const AppLimit web_limit(AppRestriction::kTimeLimit, base::Hours(2),
                           base::Time::Now());
  const std::map<AppId, AppLimit> limits{{GetChromeAppId(), web_limit}};
  registry().UpdateAppLimits(limits);
  EXPECT_EQ(registry().GetTimeLimit(kGoogleSlidesApp), web_limit.daily_limit());

  EXPECT_CALL(
      state_observer_mock,
      OnAppLimitReached(GetChromeAppId(), web_limit.daily_limit().value(),
                        /* was_active */ false))
      .Times(1);
  EXPECT_CALL(state_observer_mock,
              OnAppLimitReached(kApp2, web_limit.daily_limit().value(),
                                /* was_active */ false))
      .Times(1);
  EXPECT_CALL(
      state_observer_mock,
      OnAppLimitReached(kGoogleSlidesApp, web_limit.daily_limit().value(),
                        /* was_active */ true))
      .Times(1);

  CreateAppActivityForApp(kApp2, base::Hours(1));
  CreateAppActivityForApp(kGoogleSlidesApp, base::Hours(1));
}

}  // namespace app_time
}  // namespace ash
