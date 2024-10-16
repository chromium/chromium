// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_activity_types_service.h"

#include "base/json/values_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {

using ActivityType =
    PrivacySandboxActivityTypesService::PrivacySandboxStorageActivityType;
using UserSegment = PrivacySandboxActivityTypesService::
    PrivacySandboxStorageUserSegmentByRecentActivity;

class PrivacySandboxActivityTypesServiceTest : public testing::Test {
 public:
  PrivacySandboxActivityTypesServiceTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    InitializeFeaturesBeforeStart();
    RegisterProfilePrefs(prefs()->registry());
    privacy_sandbox_activity_types_service_ =
        std::make_unique<PrivacySandboxActivityTypesService>(prefs());
  }

  virtual void InitializeFeaturesBeforeStart() {}

  base::test::ScopedFeatureList* feature_list() { return &inner_feature_list_; }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

  content::BrowserTaskEnvironment* browser_task_environment() {
    return &browser_task_environment_;
  }

  PrivacySandboxActivityTypesService* privacy_sandbox_activity_types_service() {
    return privacy_sandbox_activity_types_service_.get();
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  TestingPrefServiceSimple prefs_;
  base::test::ScopedFeatureList inner_feature_list_;
  std::unique_ptr<PrivacySandboxActivityTypesService>
      privacy_sandbox_activity_types_service_;
};

class PrivacySandboxActivityTypeStorageTests
    : public PrivacySandboxActivityTypesServiceTest {
 public:
  PrivacySandboxActivityTypeStorageTests()
      : local_state_(std::make_unique<ScopedTestingLocalState>(
            TestingBrowserProcess::GetGlobal())) {}

  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxActivityTypeStorage,
        {{"last-n-launches", "5"},
         {"within-x-days", "2"},
         {"skip-pre-first-tab", "false"}});
  }

 protected:
  base::HistogramTester histogram_tester;
  ScopedTestingLocalState* local_state() { return local_state_.get(); }

 private:
  std::unique_ptr<ScopedTestingLocalState> local_state_;
};

TEST_F(PrivacySandboxActivityTypeStorageTests, VerifyListOverflow) {
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kAGSACustomTab);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            1u);
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kNonAGSACustomTab);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            2u);
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTabbed);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            3u);
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kWebApk);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            4u);
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kWebapp);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            5u);
  //   Since we are already at a size of 5, and last-n-launches is set to 5, the
  //   next call of another launch will remove the first element in the list
  //   before adding the newly created one. The size should still be 5.
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTrustedWebActivity);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            5u);
}

// This test is ensuring that the start of the list is represented as the newest
// records and the end is the oldest records.
TEST_F(PrivacySandboxActivityTypeStorageTests, VerifyListOrder) {
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kAGSACustomTab);
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[0]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kAGSACustomTab));

  browser_task_environment()->FastForwardBy(base::Minutes(5));
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kNonAGSACustomTab);
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[0]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kNonAGSACustomTab));

  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTrustedWebActivity);
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[0]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kTrustedWebActivity));

  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTabbed);
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kWebapp);
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[0]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kWebapp));

  browser_task_environment()->FastForwardBy(base::Minutes(5));
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kWebApk);
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kAGSACustomTab);
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[0]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kAGSACustomTab));
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[1]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kWebApk));
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[2]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kWebapp));
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[3]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kTabbed));
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[4]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kTrustedWebActivity));
}

TEST_F(PrivacySandboxActivityTypeStorageTests, VerifyListExpiration) {
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kOther);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            1u);
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kNonAGSACustomTab);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            2u);
  // Even though within-x-days is set to 2 days, we still include records that
  // are inclusive of the time boundary. When we fast forward by 2 days and add
  // a third record, all three entries are still in the record list.
  browser_task_environment()->FastForwardBy(base::Days(2));
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kPreFirstTab);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            3u);
  // Now by fast forwarding by 1 more day, we have exceeded the within-x-days of
  // 2 days, so the first two entries should be removed and the size should
  // be 2.
  browser_task_environment()->FastForwardBy(base::Days(1));
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kWebApk);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            2u);
}

TEST_F(PrivacySandboxActivityTypeStorageTests, VerifyTimeBackwards) {
  // Initializing the activity type record list with entries that have
  // timestamps set for future dates (e.g., 5 and 7 days from now).
  base::Value::List old_records;
  base::Value::Dict first_record;
  base::Value::Dict second_record;

  first_record.Set("timestamp",
                   base::TimeToValue(base::Time::Now() + base::Days(5)));
  first_record.Set("activity_type",
                   static_cast<int>(ActivityType::kAGSACustomTab));

  second_record.Set("timestamp",
                    base::TimeToValue(base::Time::Now() + base::Days(7)));
  second_record.Set("activity_type", static_cast<int>(ActivityType::kTabbed));

  old_records.Append(std::move(first_record));
  old_records.Append(std::move(second_record));

  prefs()->SetList(prefs::kPrivacySandboxActivityTypeRecord2,
                   std::move(old_records));

  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            2u);

  // After recording a new activity, any previous records with timestamps in the
  // future (greater than the current timestamp) are not added to the updated
  // list.
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTrustedWebActivity);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            1u);
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[0]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kTrustedWebActivity));
}

class PrivacySandboxActivityTypeStorageMetricsTests
    : public PrivacySandboxActivityTypesServiceTest {
 public:
  PrivacySandboxActivityTypeStorageMetricsTests()
      : local_state_(std::make_unique<ScopedTestingLocalState>(
            TestingBrowserProcess::GetGlobal())) {}

  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxActivityTypeStorage,
        {{"last-n-launches", "100"},
         {"within-x-days", "60"},
         {"skip-pre-first-tab", "false"}});
  }

  struct PercentageMetricValues {
    int AGSACCTPercent = 0;
    int AGSACCTBucketCount = 1;
    int BrAppPercent = 0;
    int BrAppBucketCount = 1;
    int NonAGSACCTPercent = 0;
    int NonAGSACCTBucketCount = 1;
    int TWAPercent = 0;
    int TWABucketCount = 1;
    int WebappPercent = 0;
    int WebappBucketCount = 1;
    int WebAPKPercent = 0;
    int WebAPKBucketCount = 1;
    int OtherPercent = 0;
    int OtherBucketCount = 1;
    int PreFirstTabPercent = 0;
    int PreFirstTabCount = 1;
  };

  void TestMetricValues(PercentageMetricValues values) {
    histogram_tester.ExpectBucketCount(
        "PrivacySandbox.ActivityTypeStorage.Percentage.AGSACCT2",
        values.AGSACCTPercent, values.AGSACCTBucketCount);
    histogram_tester.ExpectBucketCount(
        "PrivacySandbox.ActivityTypeStorage.Percentage.BrApp2",
        values.BrAppPercent, values.BrAppBucketCount);
    histogram_tester.ExpectBucketCount(
        "PrivacySandbox.ActivityTypeStorage.Percentage.NonAGSACCT2",
        values.NonAGSACCTPercent, values.NonAGSACCTBucketCount);
    histogram_tester.ExpectBucketCount(
        "PrivacySandbox.ActivityTypeStorage.Percentage.TWA2", values.TWAPercent,
        values.TWABucketCount);
    histogram_tester.ExpectBucketCount(
        "PrivacySandbox.ActivityTypeStorage.Percentage.WebApp2",
        values.WebappPercent, values.WebappBucketCount);
    histogram_tester.ExpectBucketCount(
        "PrivacySandbox.ActivityTypeStorage.Percentage.WebApk2",
        values.WebAPKPercent, values.WebAPKBucketCount);
    histogram_tester.ExpectBucketCount(
        "PrivacySandbox.ActivityTypeStorage.Percentage.Other2",
        values.OtherPercent, values.OtherBucketCount);
    histogram_tester.ExpectBucketCount(
        "PrivacySandbox.ActivityTypeStorage.Percentage.PreFirstTab2",
        values.PreFirstTabPercent, values.PreFirstTabCount);
  }

 protected:
  base::HistogramTester histogram_tester;
  ScopedTestingLocalState* local_state() { return local_state_.get(); }

 private:
  std::unique_ptr<ScopedTestingLocalState> local_state_;
};

TEST_F(PrivacySandboxActivityTypeStorageMetricsTests,
       VerifyMetricsRecordsLength) {
  local_state()->Get()->SetInt64(
      metrics::prefs::kMetricsReportingEnabledTimestamp,
      (base::Time::Now() - base::Days(10)).ToTimeT());
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.RecordsLength", 1, 1);
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kWebapp);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.RecordsLength", 2, 1);
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kWebApk);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.RecordsLength", 3, 1);
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTrustedWebActivity);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.RecordsLength", 4, 1);
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kAGSACustomTab);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.RecordsLength", 5, 1);
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kOther);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.RecordsLength", 6, 1);
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kPreFirstTab);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.RecordsLength", 7, 1);
}

TEST_F(PrivacySandboxActivityTypeStorageMetricsTests,
       VerifyMetricsPercentages) {
  local_state()->Get()->SetInt64(
      metrics::prefs::kMetricsReportingEnabledTimestamp,
      (base::Time::Now() - base::Days(10)).ToTimeT());
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kAGSACustomTab);
  TestMetricValues({.AGSACCTPercent = 100});

  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kNonAGSACustomTab);
  TestMetricValues({.AGSACCTPercent = 50,
                    .BrAppBucketCount = 2,
                    .NonAGSACCTPercent = 50,
                    .TWABucketCount = 2,
                    .WebappBucketCount = 2,
                    .WebAPKBucketCount = 2,
                    .OtherBucketCount = 2,
                    .PreFirstTabCount = 2});

  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTrustedWebActivity);
  TestMetricValues({.AGSACCTPercent = 33,
                    .BrAppBucketCount = 3,
                    .NonAGSACCTPercent = 33,
                    .TWAPercent = 33,
                    .WebappBucketCount = 3,
                    .WebAPKBucketCount = 3,
                    .OtherBucketCount = 3,
                    .PreFirstTabCount = 3});

  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kAGSACustomTab);
  TestMetricValues({.AGSACCTPercent = 50,
                    .AGSACCTBucketCount = 2,
                    .BrAppBucketCount = 4,
                    .NonAGSACCTPercent = 25,
                    .TWAPercent = 25,
                    .WebappBucketCount = 4,
                    .WebAPKBucketCount = 4,
                    .OtherBucketCount = 4,
                    .PreFirstTabCount = 4});

  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kWebApk);
  TestMetricValues({.AGSACCTPercent = 40,
                    .BrAppBucketCount = 5,
                    .NonAGSACCTPercent = 20,
                    .TWAPercent = 20,
                    .WebappBucketCount = 5,
                    .WebAPKPercent = 20,
                    .OtherBucketCount = 5,
                    .PreFirstTabCount = 5});

  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTrustedWebActivity);
  TestMetricValues({.AGSACCTPercent = 33,
                    .AGSACCTBucketCount = 2,
                    .BrAppBucketCount = 6,
                    .NonAGSACCTPercent = 17,
                    .TWAPercent = 33,
                    .TWABucketCount = 2,
                    .WebappBucketCount = 6,
                    .WebAPKPercent = 17,
                    .OtherBucketCount = 6,
                    .PreFirstTabCount = 6});

  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kWebapp);
  TestMetricValues({.AGSACCTPercent = 29,
                    .BrAppBucketCount = 7,
                    .NonAGSACCTPercent = 14,
                    .TWAPercent = 29,
                    .WebappPercent = 14,
                    .WebAPKPercent = 14,
                    .OtherBucketCount = 7,
                    .PreFirstTabCount = 7});

  browser_task_environment()->FastForwardBy(base::Days(61));
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTabbed);
  // Since 61 days have passed, the activity log gets cleared because it is
  // passed our within-x-days feature param.
  TestMetricValues({.BrAppPercent = 100,
                    .NonAGSACCTBucketCount = 2,
                    .TWABucketCount = 3,
                    .WebappBucketCount = 7,
                    .WebAPKBucketCount = 5,
                    .OtherBucketCount = 8,
                    .PreFirstTabCount = 8});
}

TEST_F(PrivacySandboxActivityTypeStorageMetricsTests,
       VerifyUserSegmentMetrics) {
  local_state()->Get()->SetInt64(
      metrics::prefs::kMetricsReportingEnabledTimestamp,
      (base::Time::Now() - base::Days(10)).ToTimeT());
  for (int i = 0; i < 10; ++i) {
    privacy_sandbox_activity_types_service()->RecordActivityType(
        ActivityType::kWebapp);
  }
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2", 1);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasWebapp, 1);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.20MostRecentRecordsUserSegment2", 0);

  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTrustedWebActivity);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasTWA, 1);
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kWebapp);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasTWA, 2);

  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kWebApk);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasPWA, 1);
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTrustedWebActivity);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasPWA, 2);

  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kNonAGSACustomTab);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasNonAGSACCT, 1);
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kWebApk);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasNonAGSACCT, 2);

  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kAGSACustomTab);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasAGSACCT, 1);
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kNonAGSACustomTab);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasAGSACCT, 2);

  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasBrowserApp, 1);
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kAGSACustomTab);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasBrowserApp, 2);

  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.20MostRecentRecordsUserSegment2", 1);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.20MostRecentRecordsUserSegment2",
      UserSegment::kHasBrowserApp, 1);

  for (int i = 0; i < 9; ++i) {
    privacy_sandbox_activity_types_service()->RecordActivityType(
        ActivityType::kAGSACustomTab);
  }
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasBrowserApp, 10);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasAGSACCT, 3);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.20MostRecentRecordsUserSegment2",
      UserSegment::kHasBrowserApp, 10);

  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasOther, 0);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.20MostRecentRecordsUserSegment2",
      UserSegment::kHasOther, 0);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasPreFirstTab, 0);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.20MostRecentRecordsUserSegment2",
      UserSegment::kHasPreFirstTab, 0);
}

TEST_F(PrivacySandboxActivityTypeStorageMetricsTests, VerifyNoMetrics) {
  // Set the kMetricsReportingEnabledTimestamp of UMA opt in to 10 days in the
  // future and we should receive no metrics on any of the data in the Activity
  // Type storage list. The list should still be populated to a size of 10
  // records.
  local_state()->Get()->SetInt64(
      metrics::prefs::kMetricsReportingEnabledTimestamp,
      (base::Time::Now() + base::Days(10)).ToTimeT());
  for (int i = 0; i < 10; ++i) {
    privacy_sandbox_activity_types_service()->RecordActivityType(
        ActivityType::kTabbed);
  }
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2", 0);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.20MostRecentRecordsUserSegment2", 0);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.AGSACCT2", 0);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.BrApp2", 0);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.NonAGSACCT2", 0);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.TWA2", 0);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.WebApp2", 0);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.Other2", 0);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.PreFirstTab2", 0);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.RecordsLength2", 0);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            10u);
}

TEST_F(PrivacySandboxActivityTypeStorageMetricsTests,
       VerifyDurationSinceOldestRecordMetrics) {
  local_state()->Get()->SetInt64(
      metrics::prefs::kMetricsReportingEnabledTimestamp,
      (base::Time::Now() - base::Days(10)).ToTimeT());
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord", 0, 1);
  browser_task_environment()->FastForwardBy(base::Days(5));
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord", 5, 1);
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord", 5, 2);
  browser_task_environment()->FastForwardBy(base::Days(10));
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord", 15, 1);
  browser_task_environment()->FastForwardBy(base::Days(10));
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord", 25, 1);
  browser_task_environment()->FastForwardBy(base::Days(10));
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord", 35, 1);
  browser_task_environment()->FastForwardBy(base::Days(10));
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord", 45, 1);
  browser_task_environment()->FastForwardBy(base::Days(10));
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord", 55, 1);
  browser_task_environment()->FastForwardBy(base::Days(10));
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord", 60, 1);
  browser_task_environment()->FastForwardBy(base::Days(10));
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord", 60, 2);
}

class PrivacySandboxActivityTypeStorageMetricsTypeReceivedTests
    : public PrivacySandboxActivityTypeStorageMetricsTests,
      public testing::WithParamInterface<int> {};

TEST_P(PrivacySandboxActivityTypeStorageMetricsTypeReceivedTests,
       VerifyTypeReceivedMetric) {
  privacy_sandbox_activity_types_service()->RecordActivityType(
      static_cast<ActivityType>(GetParam()));
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.TypeReceived",
      static_cast<ActivityType>(GetParam()), 1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PrivacySandboxActivityTypeStorageMetricsTypeReceivedTests,
    testing::Range(static_cast<int>(ActivityType::kOther),
                   static_cast<int>(ActivityType::kMaxValue) + 1));

class PrivacySandboxActivityTypeStorageSkipPreFirstTabTests
    : public PrivacySandboxActivityTypeStorageTests {
  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxActivityTypeStorage,
        {{"last-n-launches", "100"},
         {"within-x-days", "60"},
         {"skip-pre-first-tab", "true"}});
  }
};

TEST_F(PrivacySandboxActivityTypeStorageSkipPreFirstTabTests,
       RecordsOnlyTabbedActivity) {
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kTabbed);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            1u);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.TypeReceived", ActivityType::kTabbed,
      1);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.BrApp2", 100, 1);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.PreFirstTab2", 0, 1);
  privacy_sandbox_activity_types_service()->RecordActivityType(
      ActivityType::kPreFirstTab);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            1u);
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[0]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kTabbed));
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.TypeReceived",
      ActivityType::kPreFirstTab, 1);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.BrApp2", 100, 1);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.PreFirstTab2", 0, 1);
}
}  // namespace privacy_sandbox
