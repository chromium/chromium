// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_android_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/flags/android/chrome_session_state.h"
#include "components/metrics/android_metrics_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace {

using chrome::android::ActivityType;
using chrome::android::GetActivityType;
using chrome::android::GetCustomTabsVisibleValue;
using chrome::android::GetInitialActivityTypeForTesting;
using chrome::android::SetActivityType;
using chrome::android::SetInitialActivityTypeForTesting;

class ChromeAndroidMetricsProviderTest
    : public testing::TestWithParam<ActivityType> {
 public:
  ChromeAndroidMetricsProviderTest()
      : metrics_provider_(&pref_service_),
        orig_activity_type_(GetInitialActivityTypeForTesting()) {
    ChromeAndroidMetricsProvider::RegisterPrefs(pref_service_.registry());
  }
  ~ChromeAndroidMetricsProviderTest() override {
    // In case the test played with the activity type, restore it to what it
    // was before the test.
    SetInitialActivityTypeForTesting(orig_activity_type_);
    ChromeAndroidMetricsProvider::ResetGlobalStateForTesting();
  }

  ActivityType activity_type() const { return GetParam(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  TestingPrefServiceSimple pref_service_;
  ChromeAndroidMetricsProvider metrics_provider_;
  metrics::ChromeUserMetricsExtension uma_proto_;
  const ActivityType orig_activity_type_;
  base::test::TaskEnvironment task_environment_;
};

}  // namespace

TEST_F(ChromeAndroidMetricsProviderTest,
       ProvideCurrentSessionData_MultiWindowMode) {
  metrics_provider_.ProvideCurrentSessionData(&uma_proto_);
  histogram_tester_.ExpectTotalCount("Android.MultiWindowMode.Active", 1);
}

TEST_F(ChromeAndroidMetricsProviderTest,
       ProvideCurrentSessionData_AppNotifications) {
  metrics_provider_.ProvideCurrentSessionData(&uma_proto_);
  histogram_tester_.ExpectTotalCount("Android.AppNotificationStatus", 1);
}

TEST_F(ChromeAndroidMetricsProviderTest,
       OnDidCreateMetricsLog_HasMultipleUserProfiles) {
  metrics_provider_.OnDidCreateMetricsLog();
  histogram_tester_.ExpectTotalCount("Android.MultipleUserProfilesState", 1);
  // Caches value, test a second time.
  metrics_provider_.OnDidCreateMetricsLog();
  histogram_tester_.ExpectTotalCount("Android.MultipleUserProfilesState", 2);
}

TEST_F(ChromeAndroidMetricsProviderTest,
       ProvidePreviousSessionData_HasMultipleUserProfiles) {
  metrics_provider_.ProvidePreviousSessionData(&uma_proto_);
  histogram_tester_.ExpectTotalCount("Android.MultipleUserProfilesState", 1);
  // Caches value, test a second time.
  metrics_provider_.ProvidePreviousSessionData(&uma_proto_);
  histogram_tester_.ExpectTotalCount("Android.MultipleUserProfilesState", 2);
}

TEST_F(ChromeAndroidMetricsProviderTest,
       OnDidCreateMetricsLog_AndroidMetricsHelper) {
  metrics_provider_.OnDidCreateMetricsLog();
  histogram_tester_.ExpectTotalCount("Android.VersionCode", 1);
  histogram_tester_.ExpectTotalCount("Android.CpuAbiBitnessSupport", 1);
}

TEST_F(ChromeAndroidMetricsProviderTest,
       ProvidePreviousSessionData_AndroidMetricsHelper) {
  metrics_provider_.ProvidePreviousSessionData(&uma_proto_);
  histogram_tester_.ExpectTotalCount("Android.VersionCode", 0);
  histogram_tester_.ExpectTotalCount("Android.CpuAbiBitnessSupport", 1);
}

TEST_F(ChromeAndroidMetricsProviderTest,
       ProvidePreviousSessionDataWithSavedLocalState_AndroidMetricsHelper) {
  metrics::AndroidMetricsHelper::SaveLocalState(&pref_service_, 588700002);
  metrics_provider_.ProvidePreviousSessionData(&uma_proto_);
  histogram_tester_.ExpectTotalCount("Android.VersionCode", 1);
  histogram_tester_.ExpectTotalCount("Android.CpuAbiBitnessSupport", 1);
}

TEST_F(ChromeAndroidMetricsProviderTest,
       ProvideCurrentSessionData_DarkModeState) {
  ASSERT_FALSE(uma_proto_.system_profile().os().has_dark_mode_state());

  metrics_provider_.ProvideCurrentSessionData(&uma_proto_);
  ASSERT_TRUE(uma_proto_.system_profile().os().has_dark_mode_state());
}

TEST_P(ChromeAndroidMetricsProviderTest, OnDidCreateMetricsLog_CustomTabs) {
  // Seed the activity type.
  SetInitialActivityTypeForTesting(activity_type());

  // Call the method under test.
  metrics_provider_.OnDidCreateMetricsLog();

  // Expect 1 sample for all activity types.
  histogram_tester_.ExpectUniqueSample(
      "CustomTabs.Visible", GetCustomTabsVisibleValue(activity_type()), 1);
  histogram_tester_.ExpectUniqueSample("Android.ChromeActivity.Type",
                                       activity_type(), 1);
}

TEST_P(ChromeAndroidMetricsProviderTest, ProvideCurrentSessionData_CustomTabs) {
  // Seed the activity type.
  SetInitialActivityTypeForTesting(activity_type());

  // Call the method under test.
  metrics_provider_.ProvideCurrentSessionData(&uma_proto_);

  // No emission of activity type histograms in ProvideCurrentSessionData.
  histogram_tester_.ExpectTotalCount("CustomTabs.Visible", 0);
  histogram_tester_.ExpectTotalCount("Android.ChromeActivity.Type", 0);
}

// Tests initial transition from kPreFirstTab to !kPreFirstTab.
TEST_P(ChromeAndroidMetricsProviderTest, SetActivityType_CustomTabs) {
  // kPreFirstTab -> kPreFirstTab is not a valid scenario. Early exit.
  if (activity_type() == ActivityType::kPreFirstTab)
    return;

  // Validating startup, so seed the activity type to kPreFirstTab,
  SetInitialActivityTypeForTesting(ActivityType::kPreFirstTab);

  // Set the activity type as if a new tab was created..
  SetActivityType(&pref_service_, activity_type());

  // Emission of Actiity type histograms has been deferred until now. So we
  // should see activity type samples.
  histogram_tester_.ExpectUniqueSample(
      "CustomTabs.Visible", GetCustomTabsVisibleValue(activity_type()), 1);
  histogram_tester_.ExpectUniqueSample("Android.ChromeActivity.Type",
                                       activity_type(), 1);
}

// Tests initial warmup records, no tab becomes visible
TEST_F(ChromeAndroidMetricsProviderTest, NoInitialTab) {
  // Validating startup, so seed the activity type to kPreFirstTab,
  SetInitialActivityTypeForTesting(ActivityType::kPreFirstTab);

  // On startup an initial record is created... but no tab becomes active
  // (example: background warmup or user is still going through FRE), and
  // the initial log record gets closed...
  metrics_provider_.OnDidCreateMetricsLog();
  metrics_provider_.ProvideCurrentSessionData(&uma_proto_);

  // Only one sample issued over lifetime of first record.
  histogram_tester_.ExpectUniqueSample(
      "CustomTabs.Visible",
      GetCustomTabsVisibleValue(ActivityType::kPreFirstTab), 1);
  histogram_tester_.ExpectUniqueSample("Android.ChromeActivity.Type",
                                       ActivityType::kPreFirstTab, 1);

  // ... and a new record opened...  triggering another sample.
  metrics_provider_.OnDidCreateMetricsLog();
  histogram_tester_.ExpectUniqueSample(
      "CustomTabs.Visible",
      GetCustomTabsVisibleValue(ActivityType::kPreFirstTab), 2);

  // ... and subsequently closed...
  metrics_provider_.ProvideCurrentSessionData(&uma_proto_);
  histogram_tester_.ExpectUniqueSample(
      "CustomTabs.Visible",
      GetCustomTabsVisibleValue(ActivityType::kPreFirstTab), 2);
  histogram_tester_.ExpectUniqueSample("Android.ChromeActivity.Type",
                                       ActivityType::kPreFirstTab, 2);
}

// Tests initial transition from kPreFirstTab to !kPreFirstTab.
TEST_P(ChromeAndroidMetricsProviderTest, InitialTab) {
  // kPreFirstTab -> kPreFirstTab is not a valid scenario. Early exit.
  if (activity_type() == ActivityType::kPreFirstTab)
    return;

  // Validating startup, so seed the activity type to kPreFirstTab,
  SetInitialActivityTypeForTesting(ActivityType::kPreFirstTab);

  // On startup an initial record is created... then a tab is resumed which
  // sets the activity type and eventually closes the first log record.
  metrics_provider_.OnDidCreateMetricsLog();
  SetActivityType(&pref_service_, activity_type());
  metrics_provider_.ProvideCurrentSessionData(&uma_proto_);

  // Only two samples issued over lifetime of first record, one undeclared and
  // one for the tested activity type.
  histogram_tester_.ExpectTotalCount("CustomTabs.Visible", 2);
  histogram_tester_.ExpectBucketCount(
      "CustomTabs.Visible",
      GetCustomTabsVisibleValue(ActivityType::kPreFirstTab), 1);
  histogram_tester_.ExpectBucketCount(
      "CustomTabs.Visible", GetCustomTabsVisibleValue(activity_type()), 1);
  histogram_tester_.ExpectTotalCount("Android.ChromeActivity.Type", 2);
  histogram_tester_.ExpectBucketCount("Android.ChromeActivity.Type",
                                      ActivityType::kPreFirstTab, 1);
  histogram_tester_.ExpectBucketCount("Android.ChromeActivity.Type",
                                      activity_type(), 1);

  // ... and a second record is opened/closed
  metrics_provider_.OnDidCreateMetricsLog();
  metrics_provider_.ProvideCurrentSessionData(&uma_proto_);

  // One additional sample issued over lifetime of second record.
  histogram_tester_.ExpectTotalCount("CustomTabs.Visible", 3);
  histogram_tester_.ExpectBucketCount(
      "CustomTabs.Visible",
      GetCustomTabsVisibleValue(ActivityType::kPreFirstTab), 1);
  histogram_tester_.ExpectBucketCount(
      "CustomTabs.Visible", GetCustomTabsVisibleValue(activity_type()), 2);
  histogram_tester_.ExpectTotalCount("Android.ChromeActivity.Type", 3);
  histogram_tester_.ExpectBucketCount("Android.ChromeActivity.Type",
                                      ActivityType::kPreFirstTab, 1);
  histogram_tester_.ExpectBucketCount("Android.ChromeActivity.Type",
                                      activity_type(), 2);
}

// Tests initial transition from kPreFirstTab to !kPreFirstTab.
TEST_P(ChromeAndroidMetricsProviderTest, TabSwitching) {
  const auto first_activity_type = activity_type();
  const auto second_activity_type =
      static_cast<ActivityType>((static_cast<int>(first_activity_type) + 1) %
                                static_cast<int>(ActivityType::kMaxValue));

  // Transition to kPreFirstTab is not a valid scenario. Early exit.
  if (first_activity_type == ActivityType::kPreFirstTab ||
      second_activity_type == ActivityType::kPreFirstTab) {
    return;
  }

  // Validating startup, so seed the activity type to kPreFirstTab,
  SetInitialActivityTypeForTesting(ActivityType::kPreFirstTab);

  // On startup an initial record is created
  metrics_provider_.OnDidCreateMetricsLog();

  // Then a tab is created/resumed which sets the activity type and eventually
  // closes the first log record and starts a second record.
  SetActivityType(&pref_service_, first_activity_type);
  metrics_provider_.ProvideCurrentSessionData(&uma_proto_);
  metrics_provider_.OnDidCreateMetricsLog();

  // A second tab is created/resumed: static data is updated, previous record
  // is closed, new record is created for new activity.
  SetActivityType(&pref_service_, second_activity_type);
  metrics_provider_.ProvideCurrentSessionData(&uma_proto_);
  metrics_provider_.OnDidCreateMetricsLog();

  // Two records were created for the first activity.
  histogram_tester_.ExpectBucketCount("Android.ChromeActivity.Type",
                                      first_activity_type, 2);

  // One additional record created for second activity..
  histogram_tester_.ExpectBucketCount("Android.ChromeActivity.Type",
                                      second_activity_type, 1);

  // The "CustomTabs.Visible" samples may collapse to the same bucket (or not)
  // depending on the actvity pair, so we don't test them here. The coverage
  // from the other tests is sufficient for validating "CustomTabs.Visible".
}

INSTANTIATE_TEST_SUITE_P(All,
                         ChromeAndroidMetricsProviderTest,
                         testing::Values(ActivityType::kTabbed,
                                         ActivityType::kCustomTab,
                                         ActivityType::kTrustedWebActivity,
                                         ActivityType::kWebapp,
                                         ActivityType::kWebApk,
                                         ActivityType::kPreFirstTab,
                                         ActivityType::kAuthTab));
