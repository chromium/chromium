// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/family_user_parental_control_metrics.h"

#include <memory>
#include <string>

#include "ash/components/arc/test/fake_app_instance.h"
#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/child_accounts/apps/app_test_utils.h"
#include "chrome/browser/ash/child_accounts/child_user_service.h"
#include "chrome/browser/ash/child_accounts/child_user_service_factory.h"
#include "chrome/browser/ash/child_accounts/time_limit_test_utils.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limit_utils.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limits_policy_builder.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr base::TimeDelta kOneHour = base::Hours(1);
constexpr base::TimeDelta kOneDay = base::Days(1);
constexpr char kStartTime[] = "1 Jan 2020 21:15";

const app_time::AppId kArcApp(apps::AppType::kArc, "packageName");

}  // namespace

namespace utils = time_limit_test_utils;

class FamilyUserParentalControlMetricsTest : public testing::Test {
 public:
  void SetUp() override {
    base::Time start_time;
    EXPECT_TRUE(base::Time::FromString(kStartTime, &start_time));
    base::TimeDelta forward_by = start_time - base::Time::Now();
    EXPECT_LT(base::TimeDelta(), forward_by);
    task_environment_.AdvanceClock(forward_by);

    // Build a child profile.
    std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    TestingProfile::Builder profile_builder;
    profile_builder.SetPrefService(std::move(prefs));
    profile_builder.SetIsSupervisedProfile();
    profile_ = profile_builder.Build();
    EXPECT_TRUE(profile_->IsChild());
    parental_control_metrics_ =
        std::make_unique<FamilyUserParentalControlMetrics>(profile_.get());
  }

  void TearDown() override {
    parental_control_metrics_.reset();
    profile_.reset();
  }

 protected:
  app_time::AppActivityRegistry* GetAppActivityRegistry() {
    ChildUserService* service =
        ChildUserServiceFactory::GetForBrowserContext(profile_.get());
    ChildUserService::TestApi test_api = ChildUserService::TestApi(service);
    EXPECT_TRUE(test_api.app_time_controller());
    return test_api.app_time_controller()->app_registry();
  }

  void OnNewDay() { parental_control_metrics_->OnNewDay(); }

  PrefService* GetPrefs() { return profile_->GetPrefs(); }

  std::unique_ptr<TestingProfile> profile_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<FamilyUserParentalControlMetrics> parental_control_metrics_;
};

TEST_F(FamilyUserParentalControlMetricsTest, BedAndScreenTimeLimitMetrics) {
  ASSERT_TRUE(ChildUserServiceFactory::GetForBrowserContext(profile_.get()));

  base::Value::Dict policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));

  // Adds bedtime policy:
  utils::AddTimeWindowLimit(
      /*policy=*/&policy_content, /*day=*/utils::kFriday,
      /*start_time=*/utils::CreateTime(21, 0),
      /*end_time=*/utils::CreateTime(7, 0),
      /*last_updated=*/base::Time::Now());
  // Adds usage time policy:
  utils::AddTimeUsageLimit(
      /*policy=*/&policy_content, /*day=*/utils::kMonday,
      /*quota*/ kOneHour,
      /*last_updated=*/base::Time::Now());

  GetPrefs()->SetDict(prefs::kUsageTimeLimit, policy_content.Clone());

  histogram_tester_.ExpectBucketCount(
      ChildUserService::GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*sample=*/
      ChildUserService::TimeLimitPolicyType::kBedTimeLimit,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      ChildUserService::GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*sample=*/
      ChildUserService::TimeLimitPolicyType::kScreenTimeLimit,
      /*expected_count=*/1);

  histogram_tester_.ExpectTotalCount(
      ChildUserService::GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*expected_count=*/2);

  // Tests daily report.
  OnNewDay();

  histogram_tester_.ExpectBucketCount(
      ChildUserService::GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*sample=*/
      ChildUserService::TimeLimitPolicyType::kBedTimeLimit,
      /*expected_count=*/2);
  histogram_tester_.ExpectBucketCount(
      ChildUserService::GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*sample=*/
      ChildUserService::TimeLimitPolicyType::kScreenTimeLimit,
      /*expected_count=*/2);

  histogram_tester_.ExpectTotalCount(
      ChildUserService::GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*expected_count=*/4);
}

TEST_F(FamilyUserParentalControlMetricsTest, OverrideTimeLimitMetrics) {
  ASSERT_TRUE(ChildUserServiceFactory::GetForBrowserContext(profile_.get()));

  // Adds override time policy created at 1 day ago.
  base::Value::Dict policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));

  utils::AddOverrideWithDuration(
      /*policy=*/&policy_content,
      /*action=*/usage_time_limit::TimeLimitOverride::Action::kLock,
      /*created_at=*/base::Time::Now() - kOneDay,
      /*duration=*/base::Hours(2));
  GetPrefs()->SetDict(prefs::kUsageTimeLimit, policy_content.Clone());

  // The override time limit policy would not get reported since the difference
  // between reported and created time are greater than 1 day.
  histogram_tester_.ExpectBucketCount(
      ChildUserService::GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*sample=*/
      ChildUserService::TimeLimitPolicyType::kOverrideTimeLimit,
      /*expected_count=*/0);
  histogram_tester_.ExpectUniqueSample(
      ChildUserService::GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*sample=*/
      ChildUserService::TimeLimitPolicyType::kNoTimeLimit,
      /*expected_count=*/1);

  // Adds override time policy. Created and reported within 1 day.
  utils::AddOverrideWithDuration(
      /*policy=*/&policy_content,
      /*action=*/usage_time_limit::TimeLimitOverride::Action::kLock,
      /*created_at=*/base::Time::Now() - base::Hours(23),
      /*duration=*/base::Hours(2));
  GetPrefs()->SetDict(prefs::kUsageTimeLimit, policy_content.Clone());

  // The override time limit policy would get reported since the created
  // time and reported time are within 1 day.
  histogram_tester_.ExpectBucketCount(
      ChildUserService::GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*sample=*/
      ChildUserService::TimeLimitPolicyType::kOverrideTimeLimit,
      /*expected_count=*/1);

  // Tests daily report.
  OnNewDay();

  histogram_tester_.ExpectBucketCount(
      ChildUserService::GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*sample=*/
      ChildUserService::TimeLimitPolicyType::kOverrideTimeLimit,
      /*expected_count=*/2);
  histogram_tester_.ExpectTotalCount(
      ChildUserService::GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*expected_count=*/3);
}

TEST_F(FamilyUserParentalControlMetricsTest, AppTimeLimitMetrics) {
  apps::AppServiceTest app_service_test_;
  ArcAppTest arc_test_;

  // During tests, AppService doesn't notify AppActivityRegistry that chrome
  // app is installed. Mark chrome as installed here.
  GetAppActivityRegistry()->OnAppInstalled(app_time::GetChromeAppId());
  GetAppActivityRegistry()->OnAppAvailable(app_time::GetChromeAppId());

  // Install and set up Arc app.
  app_service_test_.SetUp(profile_.get());
  arc_test_.SetUp(profile_.get());
  arc_test_.app_instance()->set_icon_response_type(
      arc::FakeAppInstance::IconResponseType::ICON_RESPONSE_SKIP);
  EXPECT_EQ(apps::AppType::kArc, kArcApp.app_type());
  std::string package_name = kArcApp.app_id();
  arc_test_.AddPackage(CreateArcAppPackage(package_name)->Clone());
  std::vector<arc::mojom::AppInfoPtr> apps;
  apps.emplace_back(CreateArcAppInfo(package_name, package_name));
  arc_test_.app_instance()->SendPackageAppListRefreshed(package_name, apps);

  // Add limit policy to the Chrome and the Arc app.
  {
    app_time::AppTimeLimitsPolicyBuilder builder;
    builder.AddAppLimit(kArcApp,
                        app_time::AppLimit(app_time::AppRestriction::kTimeLimit,
                                           base::Hours(1), base::Time::Now()));
    builder.AddAppLimit(app_time::GetChromeAppId(),
                        app_time::AppLimit(app_time::AppRestriction::kTimeLimit,
                                           base::Hours(1), base::Time::Now()));

    builder.SetResetTime(6, 0);
    GetPrefs()->SetDict(prefs::kPerAppTimeLimitsPolicy,
                        builder.value().Clone());
  }

  histogram_tester_.ExpectBucketCount(
      ChildUserService::GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*sample=*/
      ChildUserService::TimeLimitPolicyType::kAppTimeLimit,
      /*expected_count=*/1);

  // Tests daily report.
  OnNewDay();

  histogram_tester_.ExpectBucketCount(
      ChildUserService::GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*sample=*/
      ChildUserService::TimeLimitPolicyType::kAppTimeLimit,
      /*expected_count=*/2);

  histogram_tester_.ExpectTotalCount(
      ChildUserService::GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*expected_count=*/2);
}

}  // namespace ash
