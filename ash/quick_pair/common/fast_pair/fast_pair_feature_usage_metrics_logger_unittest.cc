// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/quick_pair/common/fast_pair/fast_pair_feature_usage_metrics_logger.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/quick_pair/common/mock_quick_pair_browser_delegate.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

class FastPairFeatureUsageMetricsLoggerTest : public ::testing::Test {
 protected:
  FastPairFeatureUsageMetricsLoggerTest() = default;
  ~FastPairFeatureUsageMetricsLoggerTest() override = default;

  void SetUp() override {
    browser_delegate_ = std::make_unique<MockQuickPairBrowserDelegate>();
    ON_CALL(*browser_delegate_, GetActivePrefService())
        .WillByDefault(testing::Return(&pref_service_));
    pref_service_.registry()->RegisterBooleanPref(ash::prefs::kFastPairEnabled,
                                                  /*default_value=*/true);

    mock_bluetooth_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_bluetooth_adapter_, IsPresent())
        .WillByDefault(Invoke(
            this, &FastPairFeatureUsageMetricsLoggerTest::IsBluetoothPresent));
    ON_CALL(*mock_bluetooth_adapter_,
            GetLowEnergyScanSessionHardwareOffloadingStatus())
        .WillByDefault(
            Invoke(this, &FastPairFeatureUsageMetricsLoggerTest::
                             GetLowEnergyScanSessionHardwareOffloadingStatus));
    device::BluetoothAdapterFactory::SetAdapterForTesting(
        mock_bluetooth_adapter_);
  }

  bool IsBluetoothPresent() { return is_bluetooth_present_; }

  void SetBluetoothIsPresent(bool present) { is_bluetooth_present_ = present; }

  device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
  GetLowEnergyScanSessionHardwareOffloadingStatus() {
    return hardware_offloading_status_;
  }

  void SetHardwareOffloadingStatus(
      device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
          hardware_offloading_status) {
    hardware_offloading_status_ = hardware_offloading_status;
  }

  void SetEnabled(bool is_enabled) {
    pref_service_.SetBoolean(ash::prefs::kFastPairEnabled, is_enabled);
  }

  void SetManagedEnabled(bool is_enabled) {
    pref_service_.SetManagedPref(ash::prefs::kFastPairEnabled,
                                 std::make_unique<base::Value>(is_enabled));
    ASSERT_TRUE(
        pref_service_.IsManagedPreference(ash::prefs::kFastPairEnabled));
  }

  bool is_bluetooth_present_ = true;
  device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
      hardware_offloading_status_ = device::BluetoothAdapter::
          LowEnergyScanSessionHardwareOffloadingStatus::kUndetermined;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
      mock_bluetooth_adapter_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockQuickPairBrowserDelegate> browser_delegate_;
  TestingPrefServiceSimple pref_service_;
  base::WeakPtrFactory<FastPairFeatureUsageMetricsLoggerTest> weak_ptr_factory_{
      this};
};

TEST_F(FastPairFeatureUsageMetricsLoggerTest, IsEligible_Eligible) {
  FastPairFeatureUsageMetricsLogger feature_usage_metrics;

  SetBluetoothIsPresent(/*present=*/true);
  SetHardwareOffloadingStatus(
      /*hardware_offloading_status=*/device::BluetoothAdapter::
          LowEnergyScanSessionHardwareOffloadingStatus::kSupported);

  EXPECT_TRUE(feature_usage_metrics.IsEligible());
}

TEST_F(FastPairFeatureUsageMetricsLoggerTest, IsEligible_Ineligible) {
  FastPairFeatureUsageMetricsLogger feature_usage_metrics;

  SetBluetoothIsPresent(/*present=*/false);
  SetHardwareOffloadingStatus(
      /*hardware_offloading_status=*/device::BluetoothAdapter::
          LowEnergyScanSessionHardwareOffloadingStatus::kNotSupported);

  EXPECT_FALSE(feature_usage_metrics.IsEligible());
}

TEST_F(FastPairFeatureUsageMetricsLoggerTest, IsEnabled_Enabled) {
  FastPairFeatureUsageMetricsLogger feature_usage_metrics;

  SetBluetoothIsPresent(/*present=*/true);
  SetHardwareOffloadingStatus(
      /*hardware_offloading_status=*/device::BluetoothAdapter::
          LowEnergyScanSessionHardwareOffloadingStatus::kSupported);
  EXPECT_TRUE(feature_usage_metrics.IsEnabled());
}

TEST_F(FastPairFeatureUsageMetricsLoggerTest, IsEnabled_NotEnabled) {
  FastPairFeatureUsageMetricsLogger feature_usage_metrics;

  SetBluetoothIsPresent(/*present=*/true);
  SetHardwareOffloadingStatus(
      /*hardware_offloading_status=*/device::BluetoothAdapter::
          LowEnergyScanSessionHardwareOffloadingStatus::kSupported);
  SetEnabled(/*is_enabled=*/false);
  EXPECT_FALSE(feature_usage_metrics.IsEnabled());
}

TEST_F(FastPairFeatureUsageMetricsLoggerTest, RecordUsage) {
  FastPairFeatureUsageMetricsLogger feature_usage_metrics;

  // Needs to be eligible in order to record usage.
  SetBluetoothIsPresent(/*present=*/true);
  SetHardwareOffloadingStatus(
      /*hardware_offloading_status=*/device::BluetoothAdapter::
          LowEnergyScanSessionHardwareOffloadingStatus::kSupported);

  base::HistogramTester histograms;
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.FastPair",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess, 0);
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.FastPair",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure, 0);

  feature_usage_metrics.RecordUsage(/*success=*/true);
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.FastPair",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess, 1);
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.FastPair",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure, 0);

  feature_usage_metrics.RecordUsage(/*success=*/false);
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.FastPair",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess, 1);
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.FastPair",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure, 1);
}

TEST_F(FastPairFeatureUsageMetricsLoggerTest, IsAccessible_Unmanaged_Enabled) {
  FastPairFeatureUsageMetricsLogger feature_usage_metrics;
  SetBluetoothIsPresent(/*present=*/true);
  SetHardwareOffloadingStatus(
      /*hardware_offloading_status=*/device::BluetoothAdapter::
          LowEnergyScanSessionHardwareOffloadingStatus::kSupported);

  EXPECT_TRUE(feature_usage_metrics.IsAccessible().value());
  EXPECT_TRUE(feature_usage_metrics.IsEnabled());
}

TEST_F(FastPairFeatureUsageMetricsLoggerTest, IsAccessible_Unmanaged_Disabled) {
  FastPairFeatureUsageMetricsLogger feature_usage_metrics;

  SetBluetoothIsPresent(/*present=*/true);
  SetHardwareOffloadingStatus(
      /*hardware_offloading_status=*/device::BluetoothAdapter::
          LowEnergyScanSessionHardwareOffloadingStatus::kSupported);
  SetEnabled(/*is_enabled=*/false);
  EXPECT_TRUE(feature_usage_metrics.IsAccessible().value());
  EXPECT_FALSE(feature_usage_metrics.IsEnabled());
}

TEST_F(FastPairFeatureUsageMetricsLoggerTest, IsAccessible_Managed_Enabled) {
  FastPairFeatureUsageMetricsLogger feature_usage_metrics;

  SetBluetoothIsPresent(/*present=*/true);
  SetHardwareOffloadingStatus(
      /*hardware_offloading_status=*/device::BluetoothAdapter::
          LowEnergyScanSessionHardwareOffloadingStatus::kSupported);
  SetManagedEnabled(/*is_enabled=*/true);
  EXPECT_TRUE(feature_usage_metrics.IsAccessible().value());
  EXPECT_TRUE(feature_usage_metrics.IsEnabled());
}

TEST_F(FastPairFeatureUsageMetricsLoggerTest, IsAccessible_Managed_Disabled) {
  FastPairFeatureUsageMetricsLogger feature_usage_metrics;

  SetBluetoothIsPresent(/*present=*/true);
  SetHardwareOffloadingStatus(
      /*hardware_offloading_status=*/device::BluetoothAdapter::
          LowEnergyScanSessionHardwareOffloadingStatus::kSupported);
  SetManagedEnabled(/*is_enabled=*/false);
  EXPECT_FALSE(feature_usage_metrics.IsAccessible().value());
  EXPECT_FALSE(feature_usage_metrics.IsEnabled());
}

TEST_F(FastPairFeatureUsageMetricsLoggerTest, IsAccessible_Ineligible_Enabled) {
  FastPairFeatureUsageMetricsLogger feature_usage_metrics;

  SetBluetoothIsPresent(/*present=*/false);
  SetHardwareOffloadingStatus(
      /*hardware_offloading_status=*/device::BluetoothAdapter::
          LowEnergyScanSessionHardwareOffloadingStatus::kNotSupported);

  EXPECT_FALSE(feature_usage_metrics.IsEligible());
  SetManagedEnabled(/*is_enabled=*/true);
  EXPECT_FALSE(feature_usage_metrics.IsAccessible().value());
  EXPECT_FALSE(feature_usage_metrics.IsEnabled());
}

}  // namespace quick_pair
}  // namespace ash
