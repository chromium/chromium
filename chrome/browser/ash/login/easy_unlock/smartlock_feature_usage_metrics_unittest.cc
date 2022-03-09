// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "ash/services/multidevice_setup/public/cpp/multidevice_setup_client_impl.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/login/easy_unlock/smartlock_feature_usage_metrics.h"
#include "chromeos/components/feature_usage/feature_usage_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;

bool IsEligible1() {
  return true;
}

bool IsEligible2() {
  return false;
}

bool IsEnabled1() {
  return true;
}

bool IsEnabled2() {
  return false;
}

class SmartLockFeatureUsageMetricsTest : public ::testing::Test {
 protected:
  SmartLockFeatureUsageMetricsTest() = default;
  ~SmartLockFeatureUsageMetricsTest() override = default;

  multidevice_setup::FakeMultiDeviceSetupClient fake_multidevice_setup_client_;
  std::unique_ptr<feature_usage::FeatureUsageMetrics::Delegate>
      feature_usage_metrics_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SmartLockFeatureUsageMetricsTest, EnabledAndEligibleFeatureStates) {
  feature_usage_metrics_ = std::make_unique<SmartLockFeatureUsageMetrics>(
      base::BindRepeating(&IsEligible1), base::BindRepeating(&IsEnabled1));

  EXPECT_TRUE(feature_usage_metrics_->IsEligible());
  EXPECT_TRUE(feature_usage_metrics_->IsEnabled());

  feature_usage_metrics_ = std::make_unique<SmartLockFeatureUsageMetrics>(
      base::BindRepeating(&IsEligible1), base::BindRepeating(&IsEnabled2));

  EXPECT_TRUE(feature_usage_metrics_->IsEligible());
  EXPECT_FALSE(feature_usage_metrics_->IsEnabled());

  feature_usage_metrics_ = std::make_unique<SmartLockFeatureUsageMetrics>(
      base::BindRepeating(&IsEligible2), base::BindRepeating(&IsEnabled2));

  EXPECT_FALSE(feature_usage_metrics_->IsEligible());
  EXPECT_FALSE(feature_usage_metrics_->IsEnabled());
}

TEST_F(SmartLockFeatureUsageMetricsTest, RecordUsage) {
  SmartLockFeatureUsageMetrics feature_usage_metrics_(
      base::BindRepeating(&IsEligible1), base::BindRepeating(&IsEnabled1));
  base::HistogramTester histograms;

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
