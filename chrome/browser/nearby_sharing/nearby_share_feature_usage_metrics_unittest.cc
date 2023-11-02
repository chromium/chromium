// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/nearby_share_feature_status.h"
#include "chrome/browser/nearby_sharing/nearby_share_feature_usage_metrics.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class NearbyShareFeatureUsageMetricsTest : public ::testing::Test {
 protected:
  NearbyShareFeatureUsageMetricsTest() = default;
  ~NearbyShareFeatureUsageMetricsTest() override = default;

  void SetUp() override {
    RegisterNearbySharingPrefs(pref_service_.registry());
  }

  void SetUnmanagedEnabled(bool is_enabled) {
    pref_service_.SetBoolean(prefs::kNearbySharingEnabledPrefName, is_enabled);
  }
  void SetManagedEnabled(bool is_enabled) {
    pref_service_.SetManagedPref(prefs::kNearbySharingEnabledPrefName,
                                 std::make_unique<base::Value>(is_enabled));
    ASSERT_TRUE(pref_service_.IsManagedPreference(
        prefs::kNearbySharingEnabledPrefName));
  }
  void SetOnboarded(bool is_onboarded) {
    pref_service_.SetBoolean(prefs::kNearbySharingOnboardingCompletePrefName,
                             is_onboarded);
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(NearbyShareFeatureUsageMetricsTest, Enabled_Unmanaged) {
  NearbyShareFeatureUsageMetrics feature_usage_metrics(&pref_service_);
  SetUnmanagedEnabled(false);
  SetOnboarded(false);
  EXPECT_TRUE(feature_usage_metrics.IsEligible());
  EXPECT_FALSE(feature_usage_metrics.IsEnabled());
  EXPECT_EQ(NearbyShareEnabledState::kDisabledAndNotOnboarded,
            GetNearbyShareEnabledState(&pref_service_));

  SetUnmanagedEnabled(false);
  SetOnboarded(true);
  EXPECT_TRUE(feature_usage_metrics.IsEligible());
  EXPECT_FALSE(feature_usage_metrics.IsEnabled());
  EXPECT_EQ(NearbyShareEnabledState::kDisabledAndOnboarded,
            GetNearbyShareEnabledState(&pref_service_));

  // Note: This should never happen in practice.
  SetUnmanagedEnabled(true);
  SetOnboarded(false);
  EXPECT_TRUE(feature_usage_metrics.IsEligible());
  EXPECT_TRUE(feature_usage_metrics.IsEnabled());
  EXPECT_EQ(NearbyShareEnabledState::kEnabledAndNotOnboarded,
            GetNearbyShareEnabledState(&pref_service_));

  SetUnmanagedEnabled(true);
  SetOnboarded(true);
  EXPECT_TRUE(feature_usage_metrics.IsEligible());
  EXPECT_TRUE(feature_usage_metrics.IsEnabled());
  EXPECT_EQ(NearbyShareEnabledState::kEnabledAndOnboarded,
            GetNearbyShareEnabledState(&pref_service_));
}

TEST_F(NearbyShareFeatureUsageMetricsTest, Enabled_Managed) {
  NearbyShareFeatureUsageMetrics feature_usage_metrics(&pref_service_);

  SetManagedEnabled(false);
  SetOnboarded(false);
  EXPECT_TRUE(feature_usage_metrics.IsEligible());
  EXPECT_FALSE(feature_usage_metrics.IsEnabled());
  EXPECT_EQ(NearbyShareEnabledState::kDisallowedByPolicy,
            GetNearbyShareEnabledState(&pref_service_));

  SetManagedEnabled(false);
  SetOnboarded(true);
  EXPECT_TRUE(feature_usage_metrics.IsEligible());
  EXPECT_FALSE(feature_usage_metrics.IsEnabled());
  EXPECT_EQ(NearbyShareEnabledState::kDisallowedByPolicy,
            GetNearbyShareEnabledState(&pref_service_));

  // Note: This should never happen in practice.
  SetManagedEnabled(true);
  SetOnboarded(false);
  EXPECT_TRUE(feature_usage_metrics.IsEligible());
  EXPECT_TRUE(feature_usage_metrics.IsEnabled());
  EXPECT_EQ(NearbyShareEnabledState::kEnabledAndNotOnboarded,
            GetNearbyShareEnabledState(&pref_service_));

  SetManagedEnabled(true);
  SetOnboarded(true);
  EXPECT_TRUE(feature_usage_metrics.IsEligible());
  EXPECT_TRUE(feature_usage_metrics.IsEnabled());
  EXPECT_EQ(NearbyShareEnabledState::kEnabledAndOnboarded,
            GetNearbyShareEnabledState(&pref_service_));
}

TEST_F(NearbyShareFeatureUsageMetricsTest, RecordUsage) {
  // Note: The feature must be enabled to use RecordUsage().
  NearbyShareFeatureUsageMetrics feature_usage_metrics(&pref_service_);
  SetUnmanagedEnabled(true);
  SetOnboarded(true);

  base::HistogramTester histograms;
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.NearbyShare",
      ash::feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess, 0);
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.NearbyShare",
      ash::feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure, 0);

  feature_usage_metrics.RecordUsage(/*success=*/true);
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.NearbyShare",
      ash::feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess, 1);
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.NearbyShare",
      ash::feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure, 0);

  feature_usage_metrics.RecordUsage(/*success=*/false);
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.NearbyShare",
      ash::feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess, 1);
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.NearbyShare",
      ash::feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure, 1);
}
