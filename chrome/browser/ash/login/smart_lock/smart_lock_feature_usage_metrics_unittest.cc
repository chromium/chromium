// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/login/smart_lock/smart_lock_feature_usage_metrics.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;

class SmartLockFeatureUsageMetricsTest : public ::testing::Test {
 protected:
  SmartLockFeatureUsageMetricsTest() = default;
  ~SmartLockFeatureUsageMetricsTest() override = default;

  void TestFeatureState(FeatureState feature_state,
                        bool expected_eligible_value,
                        bool expected_enabled_value) {
    fake_multidevice_setup_client_.SetFeatureState(Feature::kSmartLock,
                                                   feature_state);
    EXPECT_EQ(expected_eligible_value, feature_usage_metrics_->IsEligible());
    EXPECT_EQ(expected_enabled_value, feature_usage_metrics_->IsEnabled());
  }

  multidevice_setup::FakeMultiDeviceSetupClient fake_multidevice_setup_client_;
  std::unique_ptr<feature_usage::FeatureUsageMetrics::Delegate>
      feature_usage_metrics_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SmartLockFeatureUsageMetricsTest, EnabledAndEligibleFeatureStates) {
  feature_usage_metrics_ = std::make_unique<SmartLockFeatureUsageMetrics>(
      &fake_multidevice_setup_client_);

  TestFeatureState(FeatureState::kProhibitedByPolicy,
                   /*expected_eligible_value=*/true,
                   /*expected_enabled_value=*/false);
  TestFeatureState(FeatureState::kDisabledByUser,
                   /*expected_eligible_value=*/true,
                   /*expected_enabled_value=*/false);
  TestFeatureState(FeatureState::kEnabledByUser,
                   /*expected_eligible_value=*/true,
                   /*expected_enabled_value=*/true);
  TestFeatureState(FeatureState::kNotSupportedByChromebook,
                   /*expected_eligible_value=*/false,
                   /*expected_enabled_value=*/false);
  TestFeatureState(FeatureState::kNotSupportedByPhone,
                   /*expected_eligible_value=*/false,
                   /*expected_enabled_value=*/false);
  TestFeatureState(
      FeatureState::kUnavailableNoVerifiedHost_HostExistsButNotSetAndVerified,
      /*expected_eligible_value=*/true,
      /*expected_enabled_value=*/false);
  TestFeatureState(FeatureState::kUnavailableInsufficientSecurity,
                   /*expected_eligible_value=*/true,
                   /*expected_enabled_value=*/false);
  TestFeatureState(FeatureState::kUnavailableSuiteDisabled,
                   /*expected_eligible_value=*/true,
                   /*expected_enabled_value=*/false);
  TestFeatureState(FeatureState::kUnavailableTopLevelFeatureDisabled,
                   /*expected_eligible_value=*/true,
                   /*expected_enabled_value=*/false);
  TestFeatureState(FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts,
                   /*expected_eligible_value=*/false,
                   /*expected_enabled_value=*/false);
}

TEST_F(SmartLockFeatureUsageMetricsTest, RecordUsage) {
  SmartLockFeatureUsageMetrics feature_usage_metrics_(
      &fake_multidevice_setup_client_);
  base::HistogramTester histograms;

  // feature_usage_metrics_ must be both eligible and enabled in order for
  // RecordUsage method to be used.
  fake_multidevice_setup_client_.SetFeatureState(Feature::kSmartLock,
                                                 FeatureState::kEnabledByUser);

  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.SmartLock",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess, 0);
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.SmartLock",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure, 0);

  feature_usage_metrics_.RecordUsage(/*success=*/true);
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.SmartLock",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess, 1);
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.SmartLock",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure, 0);

  feature_usage_metrics_.RecordUsage(/*success=*/false);
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.SmartLock",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess, 1);
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.SmartLock",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure, 1);
}

}  // namespace
}  // namespace ash
