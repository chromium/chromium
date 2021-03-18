// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/family_user_parental_control_metrics.h"

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/chromeos/child_accounts/child_user_service.h"
#include "chrome/browser/chromeos/child_accounts/child_user_service_factory.h"
#include "chrome/browser/chromeos/child_accounts/time_limit_test_utils.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limit_utils.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limits_policy_builder.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_test_utils.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

constexpr base::TimeDelta kOneHour = base::TimeDelta::FromHours(1);
constexpr base::TimeDelta kOneDay = base::TimeDelta::FromDays(1);
constexpr char kStartTime[] = "1 Jan 2020 21:15";

constexpr char kExampleHost0[] = "http://www.example0.com";
constexpr char kExampleURL1[] = "http://www.example1.com/123";
const app_time::AppId kArcApp(apps::mojom::AppType::kArc, "packageName");

arc::mojom::ArcPackageInfoPtr CreateArcAppPackage(
    const std::string& package_name) {
  auto package = arc::mojom::ArcPackageInfo::New();
  package->package_name = package_name;
  package->package_version = 1;
  package->last_backup_android_id = 1;
  package->last_backup_time = 1;
  package->sync = false;
  package->system = false;
  package->permissions = base::flat_map<::arc::mojom::AppPermission, bool>();
  return package;
}

arc::mojom::AppInfo CreateArcAppInfo(const std::string& package_name) {
  arc::mojom::AppInfo app;
  app.package_name = package_name;
  app.name = package_name;
  app.activity = base::StrCat({package_name, ".", "activity"});
  app.sticky = true;
  return app;
}
}  // namespace

namespace utils = time_limit_test_utils;

class FamilyUserParentalControlMetricsTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kWebTimeLimits},
        /*disabled_features=*/{});

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
    profile_builder.SetSupervisedUserId(supervised_users::kChildAccountSUID);
    profile_ = profile_builder.Build();
    EXPECT_TRUE(profile_->IsChild());
    parental_control_metrics_ =
        std::make_unique<FamilyUserParentalControlMetrics>(profile_.get());
    supervised_user_service_ =
        SupervisedUserServiceFactory::GetForProfile(profile_.get());
    supervised_user_service_->Init();
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
    DCHECK(test_api.app_time_controller());
    return test_api.app_time_controller()->app_registry();
  }

  void OnNewDay() { parental_control_metrics_->OnNewDay(); }

  PrefService* GetPrefs() { return profile_->GetPrefs(); }

  std::unique_ptr<TestingProfile> profile_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FamilyUserParentalControlMetrics> parental_control_metrics_;
  SupervisedUserService* supervised_user_service_ = nullptr;
};

TEST_F(FamilyUserParentalControlMetricsTest, BedAndScreenTimeLimitMetrics) {
  base::HistogramTester histogram_tester;

  base::Value policy_content =
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

  GetPrefs()->Set(prefs::kUsageTimeLimit, policy_content);
  // Triggers report:
  OnNewDay();

  histogram_tester.ExpectBucketCount(
      FamilyUserParentalControlMetrics::
          GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*sample=*/
      FamilyUserParentalControlMetrics::TimeLimitPolicyType::kBedTimeLimit,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      FamilyUserParentalControlMetrics::
          GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*sample=*/
      FamilyUserParentalControlMetrics::TimeLimitPolicyType::kScreenTimeLimit,
      /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(
      FamilyUserParentalControlMetrics::
          GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*expected_count=*/2);
}

TEST_F(FamilyUserParentalControlMetricsTest, OverrideTimeLimitMetrics) {
  base::HistogramTester histogram_tester;

  // Adds override time policy created at 1 day ago.
  base::Value policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));

  utils::AddOverrideWithDuration(
      /*policy=*/&policy_content,
      /*action=*/usage_time_limit::TimeLimitOverride::Action::kLock,
      /*created_at=*/base::Time::Now() - kOneDay,
      /*duration=*/base::TimeDelta::FromHours(2));
  GetPrefs()->Set(prefs::kUsageTimeLimit, policy_content);

  // Triggers report.
  OnNewDay();

  // The override time limit policy would not get reported since the difference
  // between reported and created time are greater than 1 day.
  histogram_tester.ExpectBucketCount(
      FamilyUserParentalControlMetrics::
          GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*sample=*/
      FamilyUserParentalControlMetrics::TimeLimitPolicyType::kOverrideTimeLimit,
      /*expected_count=*/0);
  histogram_tester.ExpectUniqueSample(
      FamilyUserParentalControlMetrics::
          GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*sample=*/
      FamilyUserParentalControlMetrics::TimeLimitPolicyType::kNoTimeLimit,
      /*expected_count=*/1);

  // Adds override time policy. Created and reported within 1 day.
  utils::AddOverrideWithDuration(
      /*policy=*/&policy_content,
      /*action=*/usage_time_limit::TimeLimitOverride::Action::kLock,
      /*created_at=*/base::Time::Now() - base::TimeDelta::FromHours(23),
      /*duration=*/base::TimeDelta::FromHours(2));
  GetPrefs()->Set(prefs::kUsageTimeLimit, policy_content);

  // Triggers report.
  OnNewDay();

  // The override time limit policy would get reported since the created
  // time and reported time are within 1 day.
  histogram_tester.ExpectBucketCount(
      FamilyUserParentalControlMetrics::
          GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*sample=*/
      FamilyUserParentalControlMetrics::TimeLimitPolicyType::kOverrideTimeLimit,
      /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(
      FamilyUserParentalControlMetrics::
          GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*expected_count=*/2);
}

TEST_F(FamilyUserParentalControlMetricsTest, AppAndWebTimeLimitMetrics) {
  base::HistogramTester histogram_tester;
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
  EXPECT_EQ(apps::mojom::AppType::kArc, kArcApp.app_type());
  std::string package_name = kArcApp.app_id();
  arc_test_.AddPackage(CreateArcAppPackage(package_name)->Clone());
  const arc::mojom::AppInfo app = CreateArcAppInfo(package_name);
  arc_test_.app_instance()->SendPackageAppListRefreshed(package_name, {app});

  // Add limit policy to the Chrome and the Arc app.
  {
    app_time::AppTimeLimitsPolicyBuilder builder;
    builder.AddAppLimit(
        kArcApp,
        app_time::AppLimit(app_time::AppRestriction::kTimeLimit,
                           base::TimeDelta::FromHours(1), base::Time::Now()));
    builder.AddAppLimit(
        app_time::GetChromeAppId(),
        app_time::AppLimit(app_time::AppRestriction::kTimeLimit,
                           base::TimeDelta::FromHours(1), base::Time::Now()));

    builder.SetResetTime(6, 0);
    DictionaryPrefUpdate update(GetPrefs(), prefs::kPerAppTimeLimitsPolicy);
    base::Value* value = update.Get();
    *value = builder.value().Clone();
  }

  OnNewDay();

  histogram_tester.ExpectBucketCount(
      FamilyUserParentalControlMetrics::
          GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*sample=*/
      FamilyUserParentalControlMetrics::TimeLimitPolicyType::kWebTimeLimit,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      FamilyUserParentalControlMetrics::
          GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*sample=*/
      FamilyUserParentalControlMetrics::TimeLimitPolicyType::kAppTimeLimit,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      FamilyUserParentalControlMetrics::
          GetTimeLimitPolicyTypesHistogramNameForTest(),
      /*expected_count=*/2);
}

TEST_F(FamilyUserParentalControlMetricsTest, WebFilterTypeMetric) {
  base::HistogramTester histogram_tester;

  GetPrefs()->SetInteger(prefs::kDefaultSupervisedUserFilteringBehavior,
                         SupervisedUserURLFilter::ALLOW);
  GetPrefs()->SetBoolean(prefs::kSupervisedUserSafeSites, true);
  OnNewDay();
  histogram_tester.ExpectUniqueSample(
      FamilyUserParentalControlMetrics::FamilyUserParentalControlMetrics::
          GetWebFilterTypeHistogramNameForTest(),
      /*sample=*/
      SupervisedUserURLFilter::WebFilterType::kTryToBlockMatureSites,
      /*expected_count=*/1);

  // Tests filter "allow all sites".
  GetPrefs()->SetBoolean(prefs::kSupervisedUserSafeSites, false);
  OnNewDay();
  histogram_tester.ExpectBucketCount(
      FamilyUserParentalControlMetrics::GetWebFilterTypeHistogramNameForTest(),
      /*sample=*/
      SupervisedUserURLFilter::WebFilterType::kAllowAllSites,
      /*expected_count=*/1);

  // Tests filter "only allow certain sites" on Family Link app.
  GetPrefs()->SetInteger(prefs::kDefaultSupervisedUserFilteringBehavior,
                         SupervisedUserURLFilter::BLOCK);
  OnNewDay();
  histogram_tester.ExpectBucketCount(
      FamilyUserParentalControlMetrics::GetWebFilterTypeHistogramNameForTest(),
      /*sample=*/
      SupervisedUserURLFilter::WebFilterType::kCertainSites,
      /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(
      FamilyUserParentalControlMetrics::GetWebFilterTypeHistogramNameForTest(),
      /*expected_count=*/3);
}

TEST_F(FamilyUserParentalControlMetricsTest, ManagedSiteListTypeMetric) {
  base::HistogramTester histogram_tester;

  GetPrefs()->SetInteger(prefs::kDefaultSupervisedUserFilteringBehavior,
                         SupervisedUserURLFilter::ALLOW);
  GetPrefs()->Set(prefs::kSupervisedUserManualHosts, base::DictionaryValue());
  GetPrefs()->Set(prefs::kSupervisedUserManualURLs, base::DictionaryValue());

  OnNewDay();
  histogram_tester.ExpectBucketCount(
      FamilyUserParentalControlMetrics::
          GetManagedSiteListHistogramNameForTest(),
      /*sample=*/
      SupervisedUserURLFilter::ManagedSiteList::kEmpty,
      /*expected_count=*/1);

  // Blocks `kExampleHost0`.
  {
    DictionaryPrefUpdate hosts_update(GetPrefs(),
                                      prefs::kSupervisedUserManualHosts);
    base::DictionaryValue* hosts = hosts_update.Get();
    hosts->SetKey(kExampleHost0, base::Value(false));
  }
  OnNewDay();
  histogram_tester.ExpectBucketCount(
      FamilyUserParentalControlMetrics::
          GetManagedSiteListHistogramNameForTest(),
      /*sample=*/
      SupervisedUserURLFilter::ManagedSiteList::kBlockedListOnly,
      /*expected_count=*/1);

  // Approves `kExampleHost0`.
  {
    DictionaryPrefUpdate hosts_update(GetPrefs(),
                                      prefs::kSupervisedUserManualHosts);
    base::DictionaryValue* hosts = hosts_update.Get();
    hosts->SetKey(kExampleHost0, base::Value(true));
  }
  OnNewDay();
  histogram_tester.ExpectBucketCount(
      FamilyUserParentalControlMetrics::
          GetManagedSiteListHistogramNameForTest(),
      /*sample=*/
      SupervisedUserURLFilter::ManagedSiteList::kApprovedListOnly,
      /*expected_count=*/1);

  // Blocks `kExampleURL1`.
  {
    DictionaryPrefUpdate urls_update(GetPrefs(),
                                     prefs::kSupervisedUserManualURLs);
    base::DictionaryValue* urls = urls_update.Get();
    urls->SetKey(kExampleURL1, base::Value(false));
  }
  OnNewDay();
  histogram_tester.ExpectBucketCount(
      FamilyUserParentalControlMetrics::
          GetManagedSiteListHistogramNameForTest(),
      /*sample=*/
      SupervisedUserURLFilter::ManagedSiteList::kBoth,
      /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(
      FamilyUserParentalControlMetrics::
          GetManagedSiteListHistogramNameForTest(),
      /*expected_count=*/4);
}

}  // namespace chromeos
