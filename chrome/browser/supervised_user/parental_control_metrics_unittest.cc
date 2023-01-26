// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/parental_control_metrics.h"

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kStartTime[] = "1 Jan 2020 21:15";
constexpr char kExampleHost0[] = "http://www.example0.com";
constexpr char kExampleURL1[] = "http://www.example1.com/123";

}  // namespace

class ParentalControlMetricsTest : public testing::Test {
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
    profile_builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                                      SyncServiceFactory::GetDefaultFactory());

    profile_ = profile_builder.Build();
    EXPECT_TRUE(profile_->IsChild());
    supervised_user_service_ =
        SupervisedUserServiceFactory::GetForProfile(profile_.get());
    supervised_user_service_->Init();
    parental_control_metrics_ =
        std::make_unique<ParentalControlMetrics>(supervised_user_service_);
  }

  void TearDown() override {
    parental_control_metrics_.reset();
    profile_.reset();
  }

 protected:
  void OnNewDay() { parental_control_metrics_->OnNewDay(); }

  PrefService* GetPrefs() { return profile_->GetPrefs(); }

  std::unique_ptr<TestingProfile> profile_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<ParentalControlMetrics> parental_control_metrics_;
  raw_ptr<SupervisedUserService> supervised_user_service_ = nullptr;
};

TEST_F(ParentalControlMetricsTest, WebFilterTypeMetric) {
  // Overriding the value of prefs::kSupervisedUserSafeSites and
  // prefs::kDefaultSupervisedUserFilteringBehavior in default storage is
  // needed, otherwise no report could be triggered policies change or
  // OnNewDay(). Since the default values are the same of override values, the
  // WebFilterType doesn't change and no report here.
  GetPrefs()->SetInteger(prefs::kDefaultSupervisedUserFilteringBehavior,
                         SupervisedUserURLFilter::ALLOW);
  GetPrefs()->SetBoolean(prefs::kSupervisedUserSafeSites, true);

  // Tests daily report.
  OnNewDay();
  histogram_tester_.ExpectUniqueSample(
      SupervisedUserURLFilter::GetWebFilterTypeHistogramNameForTest(),
      /*sample=*/
      SupervisedUserURLFilter::WebFilterType::kTryToBlockMatureSites,
      /*expected_count=*/1);

  // Tests filter "allow all sites".
  GetPrefs()->SetBoolean(prefs::kSupervisedUserSafeSites, false);

  histogram_tester_.ExpectBucketCount(
      SupervisedUserURLFilter::GetWebFilterTypeHistogramNameForTest(),
      /*sample=*/
      SupervisedUserURLFilter::WebFilterType::kAllowAllSites,
      /*expected_count=*/1);

  // Tests filter "only allow certain sites" on Family Link app.
  GetPrefs()->SetInteger(prefs::kDefaultSupervisedUserFilteringBehavior,
                         SupervisedUserURLFilter::BLOCK);

  histogram_tester_.ExpectBucketCount(
      SupervisedUserURLFilter::GetWebFilterTypeHistogramNameForTest(),
      /*sample=*/
      SupervisedUserURLFilter::WebFilterType::kCertainSites,
      /*expected_count=*/1);

  histogram_tester_.ExpectTotalCount(
      SupervisedUserURLFilter::GetWebFilterTypeHistogramNameForTest(),
      /*expected_count=*/3);
}

TEST_F(ParentalControlMetricsTest, ManagedSiteListTypeMetric) {
  // Overriding the value of prefs::kSupervisedUserSafeSites and
  // prefs::kDefaultSupervisedUserFilteringBehavior in default storage is
  // needed, otherwise no report could be triggered by policies change or
  // OnNewDay(). Since the default values are the same of override values, the
  // WebFilterType doesn't change and no report here.
  GetPrefs()->SetInteger(prefs::kDefaultSupervisedUserFilteringBehavior,
                         SupervisedUserURLFilter::ALLOW);
  GetPrefs()->SetBoolean(prefs::kSupervisedUserSafeSites, true);

  // Tests daily report.
  OnNewDay();
  histogram_tester_.ExpectUniqueSample(
      SupervisedUserURLFilter::GetManagedSiteListHistogramNameForTest(),
      /*sample=*/
      SupervisedUserURLFilter::ManagedSiteList::kEmpty,
      /*expected_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      SupervisedUserURLFilter::GetApprovedSitesCountHistogramNameForTest(),
      /*sample=*/0, /*expected_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      SupervisedUserURLFilter::GetBlockedSitesCountHistogramNameForTest(),
      /*sample=*/0, /*expected_count=*/1);

  // Blocks `kExampleHost0`.
  {
    ScopedDictPrefUpdate hosts_update(GetPrefs(),
                                      prefs::kSupervisedUserManualHosts);
    base::Value::Dict& hosts = hosts_update.Get();
    hosts.Set(kExampleHost0, false);
  }

  histogram_tester_.ExpectBucketCount(
      SupervisedUserURLFilter::GetManagedSiteListHistogramNameForTest(),
      /*sample=*/
      SupervisedUserURLFilter::ManagedSiteList::kBlockedListOnly,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      SupervisedUserURLFilter::GetApprovedSitesCountHistogramNameForTest(),
      /*sample=*/0, /*expected_count=*/2);
  histogram_tester_.ExpectBucketCount(
      SupervisedUserURLFilter::GetBlockedSitesCountHistogramNameForTest(),
      /*sample=*/1, /*expected_count=*/1);

  // Approves `kExampleHost0`.
  {
    ScopedDictPrefUpdate hosts_update(GetPrefs(),
                                      prefs::kSupervisedUserManualHosts);
    base::Value::Dict& hosts = hosts_update.Get();
    hosts.Set(kExampleHost0, true);
  }

  histogram_tester_.ExpectBucketCount(
      SupervisedUserURLFilter::GetManagedSiteListHistogramNameForTest(),
      /*sample=*/
      SupervisedUserURLFilter::ManagedSiteList::kApprovedListOnly,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      SupervisedUserURLFilter::GetApprovedSitesCountHistogramNameForTest(),
      /*sample=*/1, /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      SupervisedUserURLFilter::GetBlockedSitesCountHistogramNameForTest(),
      /*sample=*/0, /*expected_count=*/2);

  // Blocks `kExampleURL1`.
  {
    ScopedDictPrefUpdate urls_update(GetPrefs(),
                                     prefs::kSupervisedUserManualURLs);
    base::Value::Dict& urls = urls_update.Get();
    urls.Set(kExampleURL1, false);
  }

  histogram_tester_.ExpectBucketCount(
      SupervisedUserURLFilter::GetManagedSiteListHistogramNameForTest(),
      /*sample=*/
      SupervisedUserURLFilter::ManagedSiteList::kBoth,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      SupervisedUserURLFilter::GetApprovedSitesCountHistogramNameForTest(),
      /*sample=*/1, /*expected_count=*/2);
  histogram_tester_.ExpectBucketCount(
      SupervisedUserURLFilter::GetBlockedSitesCountHistogramNameForTest(),
      /*sample=*/1, /*expected_count=*/2);

  histogram_tester_.ExpectTotalCount(
      SupervisedUserURLFilter::GetManagedSiteListHistogramNameForTest(),
      /*expected_count=*/4);
  histogram_tester_.ExpectTotalCount(
      SupervisedUserURLFilter::GetApprovedSitesCountHistogramNameForTest(),
      /*expected_count=*/4);
  histogram_tester_.ExpectTotalCount(
      SupervisedUserURLFilter::GetBlockedSitesCountHistogramNameForTest(),
      /*expected_count=*/4);
}
