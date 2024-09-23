// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limits/app_time_controller.h"

#include <optional>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/child_accounts/apps/app_test_utils.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limit_utils.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limits_policy_builder.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_client.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_loader.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {
namespace app_time {

namespace {

constexpr char kStartTime[] = "1 Jan 2020 00:00:00 GMT";
constexpr base::TimeDelta kDay = base::Hours(24);
constexpr base::TimeDelta kSixHours = base::Hours(6);
constexpr base::TimeDelta kOneHour = base::Hours(1);
constexpr base::TimeDelta kZeroTime = base::Seconds(0);
constexpr char kApp1Name[] = "App1";
constexpr char kApp2Name[] = "App2";
const AppId kApp1(apps::AppType::kArc, "1");
const AppId kApp2(apps::AppType::kArc, "2");

// Calculate the previous reset time.
base::Time GetLastResetTime(base::Time timestamp) {
  base::Time nearest_midnight = timestamp.LocalMidnight();
  base::Time prev_midnight;
  if (timestamp > nearest_midnight)
    prev_midnight = nearest_midnight;
  else
    prev_midnight = nearest_midnight - base::Hours(24);

  // Reset time is at 6 am for the tests.
  base::Time reset_time = prev_midnight + base::Hours(6);
  if (reset_time <= timestamp)
    return reset_time;
  else
    return reset_time - base::Hours(24);
}

}  // namespace

class AppTimeControllerTest : public testing::Test {
 protected:
  class FakeIconLoader : public apps::IconLoader {
   public:
    FakeIconLoader() = default;
    FakeIconLoader(const FakeIconLoader&) = delete;
    FakeIconLoader& operator=(const FakeIconLoader&) = delete;
    ~FakeIconLoader() override = default;

    std::unique_ptr<apps::IconLoader::Releaser> LoadIconFromIconKey(
        const std::string& id,
        const apps::IconKey& icon_key,
        apps::IconType icon_type,
        int32_t size_hint_in_dip,
        bool allow_placeholder_icon,
        apps::LoadIconCallback callback) override {
      auto expected_icon_type = apps::IconType::kStandard;
      EXPECT_EQ(icon_type, expected_icon_type);
      auto iv = std::make_unique<apps::IconValue>();
      iv->icon_type = icon_type;
      iv->uncompressed =
          gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(1, 1), 1.0f));
      iv->is_placeholder_icon = false;

      std::move(callback).Run(std::move(iv));
      return nullptr;
    }
  };

  AppTimeControllerTest() = default;
  AppTimeControllerTest(const AppTimeControllerTest&) = delete;
  AppTimeControllerTest& operator=(const AppTimeControllerTest&) = delete;
  ~AppTimeControllerTest() override = default;

  void SetUp() override;
  void TearDown() override;

  void CreateActivityForApp(const AppId& app_id,
                            base::TimeDelta active_time,
                            base::TimeDelta time_limit);

  void SimulateInstallArcApp(const AppId& app_id, const std::string& app_name);
  bool HasNotificationFor(const std::string& app_name,
                          AppNotification notification) const;
  size_t GetNotificationsCount();
  void DismissNotifications();

  void DeleteController();
  void InstantiateController();

  AppTimeController::TestApi* test_api() { return test_api_.get(); }
  AppTimeController* controller() { return controller_.get(); }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

  SystemClockClient::TestInterface* system_clock_client_test() {
    return SystemClockClient::Get()->GetTestInterface();
  }

  NotificationDisplayServiceTester& notification_tester() {
    return notification_tester_;
  }

  apps::AppServiceTest& app_service_test() { return app_service_test_; }

  Profile& profile() { return profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
  NotificationDisplayServiceTester notification_tester_{&profile_};
  FakeIconLoader icon_loader_;
  apps::AppServiceTest app_service_test_;
  ArcAppTest arc_test_;

  std::unique_ptr<AppTimeController> controller_;
  std::unique_ptr<AppTimeController::TestApi> test_api_;
};

void AppTimeControllerTest::SetUp() {
  SystemClockClient::InitializeFake();
  testing::Test::SetUp();

  // The tests are going to start at local midnight on January 1.
  base::Time time;
  ASSERT_TRUE(base::Time::FromString(kStartTime, &time));
  base::Time local_midnight = time.LocalMidnight();
  base::TimeDelta forward_by = local_midnight - base::Time::Now();
  task_environment_.FastForwardBy(forward_by);

  app_service_test_.SetUp(&profile_);
  apps::AppServiceProxyFactory::GetForProfile(&profile_)
      ->OverrideInnerIconLoaderForTesting(&icon_loader_);

  arc_test_.SetUp(&profile_);
  arc_test_.app_instance()->set_icon_response_type(
      arc::FakeAppInstance::IconResponseType::ICON_RESPONSE_SKIP);
  task_environment_.RunUntilIdle();

  InstantiateController();
  SimulateInstallArcApp(kApp1, kApp1Name);
  SimulateInstallArcApp(kApp2, kApp2Name);
}

void AppTimeControllerTest::TearDown() {
  test_api_.reset();
  controller_.reset();
  arc_test_.TearDown();
  SystemClockClient::Shutdown();
  testing::Test::TearDown();
}

void AppTimeControllerTest::CreateActivityForApp(const AppId& app_id,
                                                 base::TimeDelta time_active,
                                                 base::TimeDelta time_limit) {
  AppActivityRegistry* registry = controller_->app_registry();
  const AppLimit limit(AppRestriction::kTimeLimit, time_limit,
                       base::Time::Now());
  registry->SetAppLimit(app_id, limit);
  task_environment_.RunUntilIdle();

  // AppActivityRegistry uses `instance_id` to uniquely identify between
  // different instances of the same active application.
  auto instance_id = base::UnguessableToken::Create();
  registry->OnAppActive(app_id, instance_id, base::Time::Now());
  task_environment_.FastForwardBy(time_active);
  if (time_active < time_limit) {
    registry->OnAppInactive(app_id, instance_id, base::Time::Now());
  }
}

void AppTimeControllerTest::SimulateInstallArcApp(const AppId& app_id,
                                                  const std::string& app_name) {
  std::string package_name = app_id.app_id();
  arc_test_.AddPackage(CreateArcAppPackage(package_name)->Clone());
  std::vector<arc::mojom::AppInfoPtr> apps;
  apps.emplace_back(CreateArcAppInfo(package_name, app_name));
  arc_test_.app_instance()->SendPackageAppListRefreshed(package_name, apps);
  task_environment_.RunUntilIdle();
  return;
}

bool AppTimeControllerTest::HasNotificationFor(
    const std::string& app_name,
    AppNotification notification) const {
  std::string notification_id;
  switch (notification) {
    case AppNotification::kFiveMinutes:
    case AppNotification::kOneMinute:
      notification_id = "time-limit-reaching-id-";
      break;
    case AppNotification::kTimeLimitChanged:
      notification_id = "time-limit-updated-id-";
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  notification_id = base::StrCat({notification_id, app_name});

  std::optional<message_center::Notification> message_center_notification =
      notification_tester_.GetNotification(notification_id);
  return message_center_notification.has_value();
}

size_t AppTimeControllerTest::GetNotificationsCount() {
  return notification_tester_
      .GetDisplayedNotificationsForType(NotificationHandler::Type::TRANSIENT)
      .size();
}

void AppTimeControllerTest::DismissNotifications() {
  notification_tester_.RemoveAllNotifications(
      NotificationHandler::Type::TRANSIENT, true /* by_user */);
}

void AppTimeControllerTest::DeleteController() {
  controller_.reset();
  test_api_.reset();
}

void AppTimeControllerTest::InstantiateController() {
  controller_ =
      std::make_unique<AppTimeController>(&profile_, base::DoNothing());
  controller_->Init();
  test_api_ = std::make_unique<AppTimeController::TestApi>(controller_.get());
}

TEST_F(AppTimeControllerTest, GetNextResetTime) {
  base::Time start_time = base::Time::Now();

  base::Time next_reset_time = test_api()->GetNextResetTime();
  base::Time local_midnight = next_reset_time.LocalMidnight();
  EXPECT_EQ(kSixHours, next_reset_time - local_midnight);

  EXPECT_TRUE(next_reset_time >= start_time);
  EXPECT_TRUE(next_reset_time <= start_time + kDay);
}

TEST_F(AppTimeControllerTest, ResetTimeReached) {
  base::Time start_time = base::Time::Now();

  // Assert that we start at midnight.
  ASSERT_EQ(start_time, start_time.LocalMidnight());

  // This App will not reach its time limit. Advances time by 1 hour.
  CreateActivityForApp(kApp1, kOneHour, kOneHour * 2);

  // This app will reach its time limit. Advances time by 1 hour.
  CreateActivityForApp(kApp2, kOneHour, kOneHour / 2);

  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp1), kOneHour);
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp2), kOneHour / 2);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp1),
            AppState::kAvailable);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp2),
            AppState::kLimitReached);

  // The default reset time is 6 hours after local midnight. Fast forward by 4
  // hours to reach it. FastForwardBy triggers the reset timer.
  task_environment().FastForwardBy(base::Hours(4));

  // Make sure that there is no activity
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp1), kZeroTime);
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp2), kZeroTime);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp1),
            AppState::kAvailable);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp2),
            AppState::kAvailable);
}

TEST_F(AppTimeControllerTest, SystemTimeChangedFastForwardByTwoDays) {
  CreateActivityForApp(kApp1, kOneHour, kOneHour * 2);
  CreateActivityForApp(kApp2, kOneHour, kOneHour / 2);

  // Advance system time with two days. TaskEnvironment::AdvanceClock doesn't
  // run the tasks that have been posted. This allows us to simulate the system
  // time changing to two days ahead without triggering the reset timer.
  task_environment().AdvanceClock(2 * kDay);

  // Since the reset timer has not been triggered the application activities are
  // instact.
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp1), kOneHour);
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp2), kOneHour / 2);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp1),
            AppState::kAvailable);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp2),
            AppState::kLimitReached);

  // Notify AppTimeController that system time has changed. This triggers reset.
  system_clock_client_test()->NotifyObserversSystemClockUpdated();

  // Make sure that there is no activity
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp1), kZeroTime);
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp2), kZeroTime);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp1),
            AppState::kAvailable);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp2),
            AppState::kAvailable);
}

TEST_F(AppTimeControllerTest, SystemTimeChangedGoingBackwards) {
  CreateActivityForApp(kApp1, kOneHour, kOneHour * 2);
  CreateActivityForApp(kApp2, kOneHour, kOneHour / 2);

  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp1), kOneHour);
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp2), kOneHour / 2);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp1),
            AppState::kAvailable);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp2),
            AppState::kLimitReached);

  // Simulate time has gone back by setting the last reset time to be in the
  // future.
  base::Time last_reset_time = test_api()->GetLastResetTime();
  test_api()->SetLastResetTime(last_reset_time + 2 * kDay);
  system_clock_client_test()->NotifyObserversSystemClockUpdated();

  // Make sure that there is no activity
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp1), kZeroTime);
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp2), kZeroTime);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp1),
            AppState::kAvailable);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp2),
            AppState::kAvailable);
}

TEST_F(AppTimeControllerTest, TimeLimitNotification) {
  AppActivityRegistry* registry = controller()->app_registry();

  const AppLimit limit1(AppRestriction::kTimeLimit, base::Minutes(35),
                        base::Time::Now());
  const AppLimit limit2(AppRestriction::kTimeLimit, base::Minutes(30),
                        base::Time::Now());
  const std::map<AppId, AppLimit> limits{{kApp1, limit1}, {kApp2, limit2}};
  registry->UpdateAppLimits(limits);
  task_environment().RunUntilIdle();

  auto instance_id = base::UnguessableToken::Create();
  registry->OnAppActive(kApp1, instance_id, base::Time::Now());
  registry->OnAppActive(kApp2, instance_id, base::Time::Now());

  task_environment().FastForwardBy(base::Minutes(25));

  // Expect that there is a 5 minute notification for kApp2.
  EXPECT_TRUE(HasNotificationFor(kApp2Name, AppNotification::kFiveMinutes));

  // One minute left notification will be shown and then the app will reach its
  // time limit.
  task_environment().FastForwardBy(base::Minutes(5));

  EXPECT_TRUE(HasNotificationFor(kApp2Name, AppNotification::kOneMinute));
  EXPECT_TRUE(HasNotificationFor(kApp1Name, AppNotification::kFiveMinutes));

  task_environment().FastForwardBy(base::Minutes(5));

  EXPECT_TRUE(HasNotificationFor(kApp1Name, AppNotification::kOneMinute));
}

TEST_F(AppTimeControllerTest, TimeLimitUpdatedNotification) {
  AppActivityRegistry* registry = controller()->app_registry();

  // Set new time limits.
  const AppLimit limit1(AppRestriction::kTimeLimit, base::Minutes(35),
                        base::Time::Now());
  const AppLimit limit2(AppRestriction::kTimeLimit, base::Minutes(30),
                        base::Time::Now());
  registry->UpdateAppLimits({{kApp1, limit1}, {kApp2, limit2}});
  task_environment().RunUntilIdle();

  // Expect time limit changed notification for both apps.
  EXPECT_EQ(2u, GetNotificationsCount());
  EXPECT_TRUE(
      HasNotificationFor(kApp1Name, AppNotification::kTimeLimitChanged));
  EXPECT_TRUE(
      HasNotificationFor(kApp2Name, AppNotification::kTimeLimitChanged));

  DismissNotifications();

  // Only update one time limit.
  const base::TimeDelta delta = base::Minutes(1);
  const AppLimit limit3(AppRestriction::kTimeLimit, base::Minutes(10),
                        base::Time::Now() + delta);
  registry->UpdateAppLimits({{kApp1, limit1}, {kApp2, limit3}});
  task_environment().RunUntilIdle();
  EXPECT_EQ(1u, GetNotificationsCount());
  EXPECT_TRUE(
      HasNotificationFor(kApp2Name, AppNotification::kTimeLimitChanged));

  DismissNotifications();

  // Remove one time limit.
  registry->UpdateAppLimits({{kApp2, limit3}});
  task_environment().RunUntilIdle();
  EXPECT_EQ(1u, GetNotificationsCount());
  EXPECT_TRUE(
      HasNotificationFor(kApp1Name, AppNotification::kTimeLimitChanged));

  DismissNotifications();
}

TEST_F(AppTimeControllerTest, RestoreLastResetTime) {
  {
    AppTimeLimitsPolicyBuilder builder;
    builder.AddAppLimit(kApp1, AppLimit(AppRestriction::kTimeLimit,
                                        kOneHour * 2, base::Time::Now()));
    builder.AddAppLimit(kApp2, AppLimit(AppRestriction::kTimeLimit,
                                        kOneHour / 2, base::Time::Now()));
    builder.SetResetTime(6, 0);
    profile().GetPrefs()->SetDict(prefs::kPerAppTimeLimitsPolicy,
                                  builder.value().Clone());
  }

  // If there was no valid last reset time stored in user pref,
  // AppTimeController sets it to the expected last reset time based on
  // base::Time::Now().
  base::Time last_reset_time = GetLastResetTime(base::Time::Now());
  EXPECT_EQ(test_api()->GetLastResetTime(), last_reset_time);

  auto instance_id = base::UnguessableToken::Create();
  controller()->app_registry()->OnAppActive(kApp1, instance_id,
                                            last_reset_time);
  controller()->app_registry()->OnAppActive(kApp2, instance_id,
                                            last_reset_time);
  task_environment().FastForwardBy(kOneHour);

  controller()->app_registry()->OnAppInactive(kApp1, instance_id,
                                              base::Time::Now());
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp1),
            AppState::kAvailable);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp2),
            AppState::kLimitReached);

  AppActivityRegistry::TestApi(controller()->app_registry()).SaveAppActivity();

  DeleteController();

  // Don't change last reset time. Ensure that application state's are not
  // cleared.
  InstantiateController();

  // Make sure that AppTimeController doesn't always take base::Time::Now() for
  // its last reset time.
  EXPECT_EQ(test_api()->GetLastResetTime(), last_reset_time);

  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp1),
            AppState::kAvailable);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp2),
            AppState::kLimitReached);
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp1), kOneHour);
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp2), kOneHour / 2);

  DeleteController();

  // Now let's update the last reset time so that it is 24 hours before
  // |last_reset_time|.
  last_reset_time = last_reset_time - kDay;
  profile().GetPrefs()->SetInt64(
      prefs::kPerAppTimeLimitsLastResetTime,
      last_reset_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  InstantiateController();

  // AppTimeController will realize that the reset boundary has been crossed.
  // Therefore, it will trigger reset and update the last reset time to now.
  EXPECT_EQ(test_api()->GetLastResetTime(),
            GetLastResetTime(base::Time::Now()));

  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp1),
            AppState::kAvailable);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp2),
            AppState::kAvailable);
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp1), kZeroTime);
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp2), kZeroTime);
}

TEST_F(AppTimeControllerTest, MetricsTest) {
  base::HistogramTester histogram_tester;
  DeleteController();
  InstantiateController();

  {
    AppTimeLimitsPolicyBuilder builder;
    AppId absent_app(apps::AppType::kArc, "absent_app");
    AppLimit app_limit(AppRestriction::kTimeLimit, kOneHour, base::Time::Now());
    AppLimit blocked_app(AppRestriction::kBlocked, std::nullopt,
                         base::Time::Now());
    builder.AddAppLimit(kApp1, app_limit);
    builder.AddAppLimit(absent_app, app_limit);
    builder.AddAppLimit(kApp2, blocked_app);
    builder.SetResetTime(6, 0);
    profile().GetPrefs()->SetDict(prefs::kPerAppTimeLimitsPolicy,
                                  builder.value().Clone());
  }

  // Enagagement is recorded at the beginning of the session when
  // AppTimeController is instantiated. There was no policy set at the beginning
  // of this session. Therefore engagement is 0.
  histogram_tester.ExpectBucketCount(kEngagementMetric, 0, 1);

  // Even though the policy has 2 apps with time limit set, one of them is not
  // installed/present in AppActivityRegistry. Therefore bucket size is one.
  histogram_tester.ExpectBucketCount(kAppsWithTimeLimitMetric, 1, 1);
  histogram_tester.ExpectBucketCount(kBlockedAppsCountMetric, 1, 1);

  controller()->app_registry()->SaveAppActivity();
  controller()->RecordMetricsOnShutdown();
  DeleteController();
  histogram_tester.ExpectBucketCount(kPolicyChangeCountMetric, 1, 1);

  InstantiateController();

  // Session 2 starts with PerAppTimeLimits policy already set with one
  // application which has time limit set.
  histogram_tester.ExpectBucketCount(kEngagementMetric, 1, 1);

  // There was no update to policy. Therefore, no change in the following
  // metrics.
  histogram_tester.ExpectBucketCount(kBlockedAppsCountMetric, 1, 1);
  histogram_tester.ExpectBucketCount(kAppsWithTimeLimitMetric, 1, 1);
  histogram_tester.ExpectBucketCount(kPolicyChangeCountMetric, 1, 1);

  controller()->RecordMetricsOnShutdown();
  DeleteController();
  // There was actually no policy update when the controller was reinstantiated.
  histogram_tester.ExpectBucketCount(kPolicyChangeCountMetric, 0, 1);
}

TEST_F(AppTimeControllerTest, SetLastResetTimeTest) {
  base::Time now = base::Time::Now();
  base::Time nearest_midnight = now.LocalMidnight();
  base::Time prev_midnight;
  if (now > nearest_midnight)
    prev_midnight = nearest_midnight;
  else
    prev_midnight = nearest_midnight - kDay;

  base::Time reset_time = prev_midnight + kSixHours;

  test_api()->SetLastResetTime(prev_midnight);
  EXPECT_EQ(test_api()->GetLastResetTime(), reset_time - kDay);

  test_api()->SetLastResetTime(prev_midnight + 3 * kOneHour);
  EXPECT_EQ(test_api()->GetLastResetTime(), reset_time - kDay);

  test_api()->SetLastResetTime(prev_midnight + kSixHours);
  EXPECT_EQ(test_api()->GetLastResetTime(), reset_time);

  test_api()->SetLastResetTime(prev_midnight + 2 * kSixHours);
  EXPECT_EQ(test_api()->GetLastResetTime(), reset_time);

  test_api()->SetLastResetTime(prev_midnight + kDay);
  EXPECT_EQ(test_api()->GetLastResetTime(), reset_time);
}

}  // namespace app_time
}  // namespace ash
