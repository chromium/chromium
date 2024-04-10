// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/fast_initiation/fast_initiation_scanner_feature_usage_metrics.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/fast_initiation/fast_initiation_scanner.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "components/prefs/testing_pref_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;

namespace {
const char kHistogramName[] =
    "ChromeOS.FeatureUsage.NearbyShareBackgroundScanning";

class FakeFastInitiationScannerFactory : public FastInitiationScanner::Factory {
 public:
  std::unique_ptr<FastInitiationScanner> CreateInstance(
      scoped_refptr<device::BluetoothAdapter> adapter) override {
    return nullptr;
  }

  bool IsHardwareSupportAvailable() override {
    return is_hardware_support_available_;
  }

  void SetHardwareSupportAvailable(bool is_hardware_support_available) {
    is_hardware_support_available_ = is_hardware_support_available;
  }

 private:
  bool is_hardware_support_available_ = true;
};
}  // namespace

class FastInitiationScannerFeatureUsageMetricsTest : public ::testing::Test {
 protected:
  FastInitiationScannerFeatureUsageMetricsTest() = default;
  ~FastInitiationScannerFeatureUsageMetricsTest() override = default;

  void SetUp() override {
    RegisterNearbySharingPrefs(pref_service_.registry());

    mock_bluetooth_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(
        mock_bluetooth_adapter_);

    fast_initiation_scanner_factory_ =
        std::make_unique<FakeFastInitiationScannerFactory>();
    FastInitiationScanner::Factory::SetFactoryForTesting(
        fast_initiation_scanner_factory_.get());
  }

  void TearDown() override {
    FastInitiationScanner::Factory::SetFactoryForTesting(nullptr);
  }

  void ExpectBucketCounts(bool is_eligible, bool is_enabled) {
    // Allow time for the histogram to be logged.
    base::HistogramTester histograms;
    task_environment_.FastForwardBy(base::Minutes(1));

    histograms.ExpectBucketCount(
        kHistogramName,
        ash::feature_usage::FeatureUsageMetrics::Event::kEligible,
        is_eligible ? 1 : 0);
    histograms.ExpectBucketCount(
        kHistogramName,
        ash::feature_usage::FeatureUsageMetrics::Event::kEnabled,
        is_enabled ? 1 : 0);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_bluetooth_adapter_;
  std::unique_ptr<FakeFastInitiationScannerFactory>
      fast_initiation_scanner_factory_;
};

TEST_F(FastInitiationScannerFeatureUsageMetricsTest, EligibleEnabled) {
  FastInitiationScannerFeatureUsageMetrics feature_usage_metrics(
      &pref_service_);
  feature_usage_metrics.SetBluetoothAdapter(mock_bluetooth_adapter_);
  EXPECT_TRUE(feature_usage_metrics.IsEligible());
  EXPECT_TRUE(feature_usage_metrics.IsEnabled());
  ExpectBucketCounts(/*is_eligible=*/true, /*is_enabled=*/true);
}

TEST_F(FastInitiationScannerFeatureUsageMetricsTest, NotEligible_NoAdapter) {
  FastInitiationScannerFeatureUsageMetrics feature_usage_metrics(
      &pref_service_);
  EXPECT_FALSE(feature_usage_metrics.IsEligible());
  EXPECT_FALSE(feature_usage_metrics.IsEnabled());
  ExpectBucketCounts(/*is_eligible=*/false, /*is_enabled=*/false);
}

TEST_F(FastInitiationScannerFeatureUsageMetricsTest,
       NotEligible_NoHardwareSupport) {
  fast_initiation_scanner_factory_->SetHardwareSupportAvailable(false);
  FastInitiationScannerFeatureUsageMetrics feature_usage_metrics(
      &pref_service_);
  feature_usage_metrics.SetBluetoothAdapter(mock_bluetooth_adapter_);
  EXPECT_FALSE(feature_usage_metrics.IsEligible());
  EXPECT_FALSE(feature_usage_metrics.IsEnabled());
  ExpectBucketCounts(/*is_eligible=*/false, /*is_enabled=*/false);
}

TEST_F(FastInitiationScannerFeatureUsageMetricsTest, NotEnabled_Pref) {
  pref_service_.SetInteger(
      prefs::kNearbySharingFastInitiationNotificationStatePrefName,
      static_cast<int>(nearby_share::mojom::FastInitiationNotificationState::
                           kDisabledByUser));
  FastInitiationScannerFeatureUsageMetrics feature_usage_metrics(
      &pref_service_);
  feature_usage_metrics.SetBluetoothAdapter(mock_bluetooth_adapter_);
  EXPECT_TRUE(feature_usage_metrics.IsEligible());
  EXPECT_FALSE(feature_usage_metrics.IsEnabled());
  ExpectBucketCounts(/*is_eligible=*/true, /*is_enabled=*/false);
}

TEST_F(FastInitiationScannerFeatureUsageMetricsTest, NotEnabled_Managed) {
  pref_service_.SetManagedPref(prefs::kNearbySharingEnabledPrefName,
                               std::make_unique<base::Value>(false));
  FastInitiationScannerFeatureUsageMetrics feature_usage_metrics(
      &pref_service_);
  feature_usage_metrics.SetBluetoothAdapter(mock_bluetooth_adapter_);
  EXPECT_TRUE(feature_usage_metrics.IsEligible());
  EXPECT_FALSE(feature_usage_metrics.IsEnabled());
  ExpectBucketCounts(/*is_eligible=*/true, /*is_enabled=*/false);
}

TEST_F(FastInitiationScannerFeatureUsageMetricsTest, RecordUsage) {
  FastInitiationScannerFeatureUsageMetrics feature_usage_metrics(
      &pref_service_);
  feature_usage_metrics.SetBluetoothAdapter(mock_bluetooth_adapter_);

  base::HistogramTester histograms;
  histograms.ExpectBucketCount(
      kHistogramName,
      ash::feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess, 0);
  histograms.ExpectBucketCount(
      kHistogramName,
      ash::feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure, 0);

  feature_usage_metrics.RecordUsage(/*success=*/true);
  histograms.ExpectBucketCount(
      kHistogramName,
      ash::feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess, 1);
  histograms.ExpectBucketCount(
      kHistogramName,
      ash::feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure, 0);

  feature_usage_metrics.RecordUsage(/*success=*/false);
  histograms.ExpectBucketCount(
      kHistogramName,
      ash::feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess, 1);
  histograms.ExpectBucketCount(
      kHistogramName,
      ash::feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure, 1);
}
