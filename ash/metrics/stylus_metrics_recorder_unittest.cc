// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/stylus_metrics_recorder.h"

#include <memory>
#include <set>
#include <string>

#include "ash/system/power/peripheral_battery_listener.h"
#include "ash/system/power/peripheral_battery_tests.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/contains.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

using BatteryInfo = ash::PeripheralBatteryListener::BatteryInfo;

namespace ash {
namespace {

const char kHistogramGarageSessionMetric[] =
    "ChromeOS.FeatureUsage.StylusDetachedFromGarageSession";
const char kHistogramGarageSessionUsetimeMetric[] =
    "ChromeOS.FeatureUsage.StylusDetachedFromGarageSession.Usetime";
const char kHistogramDockSessionMetric[] =
    "ChromeOS.FeatureUsage.StylusDetachedFromDockSession";
const char kHistogramDockSessionUsetimeMetric[] =
    "ChromeOS.FeatureUsage.StylusDetachedFromDockSession.Usetime";
const char kHistogramGarageOrDockSessionMetric[] =
    "ChromeOS.FeatureUsage.StylusDetachedFromGarageOrDockSession";
const char kHistogramGarageOrDockSessionUsetimeMetric[] =
    "ChromeOS.FeatureUsage.StylusDetachedFromGarageOrDockSession.Usetime";

enum class StylusChargingStyle { kDock, kGarage };

std::string BatteryKey(StylusChargingStyle style) {
  return (style == StylusChargingStyle::kGarage) ? "garaged-stylus-charger"
                                                 : "docked-stylus-charger";
}

// Test fixture for the StylusMetricsRecorder class.
class StylusMetricsRecorderTest : public AshTestBase {
 public:
  StylusMetricsRecorderTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  StylusMetricsRecorderTest(const StylusMetricsRecorderTest&) = delete;
  StylusMetricsRecorderTest& operator=(const StylusMetricsRecorderTest&) =
      delete;
  ~StylusMetricsRecorderTest() override = default;

  // AshTestBase:

  void SetUp() override {
    AshTestBase::SetUp();
    stylus_metrics_recorder_ = std::make_unique<ash::StylusMetricsRecorder>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    stylus_metrics_recorder_.reset();
    AshTestBase::TearDown();
  }

  base::TimeTicks NowTicks() { return task_environment()->NowTicks(); }

  void AdvanceClock(base::TimeDelta delta) {
    task_environment()->AdvanceClock(delta);
  }

  BatteryInfo ConstructBatteryInfo(
      StylusChargingStyle style,
      BatteryInfo::ChargeStatus charge_status,
      bool battery_report_eligible = true,
      BatteryInfo::PeripheralType type =
          BatteryInfo::PeripheralType::kStylusViaCharger) {
    const std::string key = BatteryKey(style);
    const int level = 50;
    const std::u16string name = base::ASCIIToUTF16("name:" + key);
    const std::string btaddr = "";

    return BatteryInfo(key, name, level, battery_report_eligible, NowTicks(),
                       type, charge_status, btaddr);
  }

  void SetChargerState(StylusChargingStyle style,
                       BatteryInfo::ChargeStatus charge_status,
                       bool battery_report_eligible = true,
                       BatteryInfo::PeripheralType type =
                           BatteryInfo::PeripheralType::kStylusViaCharger) {
    const BatteryInfo info = ConstructBatteryInfo(
        style, charge_status, battery_report_eligible, type);
    if (!base::Contains(known_batteries_, info.key)) {
      stylus_metrics_recorder_->OnAddingBattery(info);
      known_batteries_.insert(info.key);
    }
    stylus_metrics_recorder_->OnUpdatedBatteryLevel(info);
  }

  void RemoveCharger(StylusChargingStyle style,
                     BatteryInfo::ChargeStatus charge_status,
                     bool battery_report_eligible = true,
                     BatteryInfo::PeripheralType type =
                         BatteryInfo::PeripheralType::kStylusViaCharger) {
    const BatteryInfo info = ConstructBatteryInfo(
        style, charge_status, battery_report_eligible, type);
    if (base::Contains(known_batteries_, info.key)) {
      stylus_metrics_recorder_->OnRemovingBattery(info);
      known_batteries_.erase(info.key);
    }
  }

 protected:
  // The test target.
  std::unique_ptr<StylusMetricsRecorder> stylus_metrics_recorder_;

  // Used to verify recorded data.
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  // Track whether batteries are known or need to be added.
  std::set<std::string> known_batteries_;
};

}  // namespace

// Verifies that histogram is not recorded when no events are received.
TEST_F(StylusMetricsRecorderTest, Baseline) {
  histogram_tester_->ExpectTotalCount(kHistogramGarageSessionMetric, 0);
  histogram_tester_->ExpectTotalCount(kHistogramDockSessionMetric, 0);
  histogram_tester_->ExpectTotalCount(
      kHistogramGarageOrDockSessionUsetimeMetric, 0);
  histogram_tester_->ExpectTotalCount(kHistogramGarageSessionUsetimeMetric, 0);
  histogram_tester_->ExpectTotalCount(kHistogramDockSessionUsetimeMetric, 0);
  histogram_tester_->ExpectTotalCount(
      kHistogramGarageOrDockSessionUsetimeMetric, 0);
}

TEST_F(StylusMetricsRecorderTest, BaselineStayInGarage) {
  const base::TimeDelta kTimeSpentCharging = base::Minutes(5);

  SetChargerState(StylusChargingStyle::kGarage,
                  BatteryInfo::ChargeStatus::kCharging);
  AdvanceClock(kTimeSpentCharging);
  // By removing the battery, we force the stylus_metrics_recorder to close out
  // the session.
  RemoveCharger(StylusChargingStyle::kGarage,
                BatteryInfo::ChargeStatus::kCharging);

  histogram_tester_->ExpectTotalCount(kHistogramGarageSessionMetric, 0);
  histogram_tester_->ExpectTotalCount(kHistogramDockSessionMetric, 0);
  histogram_tester_->ExpectTotalCount(kHistogramGarageOrDockSessionMetric, 0);

  histogram_tester_->ExpectTotalCount(kHistogramGarageSessionUsetimeMetric, 0);
  histogram_tester_->ExpectTotalCount(kHistogramDockSessionUsetimeMetric, 0);
  histogram_tester_->ExpectTotalCount(
      kHistogramGarageOrDockSessionUsetimeMetric, 0);
}

TEST_F(StylusMetricsRecorderTest, BaselineStayInDock) {
  const base::TimeDelta kTimeSpentCharging = base::Minutes(5);

  SetChargerState(StylusChargingStyle::kDock,
                  BatteryInfo::ChargeStatus::kCharging);
  AdvanceClock(kTimeSpentCharging);
  // By removing the battery, we force the stylus_metrics_recorder to close out
  // the session.
  RemoveCharger(StylusChargingStyle::kDock,
                BatteryInfo::ChargeStatus::kCharging);

  histogram_tester_->ExpectTotalCount(kHistogramGarageSessionMetric, 0);
  histogram_tester_->ExpectTotalCount(kHistogramDockSessionMetric, 0);
  histogram_tester_->ExpectTotalCount(kHistogramGarageOrDockSessionMetric, 0);

  histogram_tester_->ExpectTotalCount(kHistogramGarageSessionUsetimeMetric, 0);
  histogram_tester_->ExpectTotalCount(kHistogramDockSessionUsetimeMetric, 0);
  histogram_tester_->ExpectTotalCount(
      kHistogramGarageOrDockSessionUsetimeMetric, 0);
}

TEST_F(StylusMetricsRecorderTest, RemovedFromGarage) {
  const base::TimeDelta kTimeSpentInUse = base::Minutes(5);
  const base::TimeDelta kTimeSpentCharging = base::Minutes(1);

  SetChargerState(StylusChargingStyle::kGarage,
                  BatteryInfo::ChargeStatus::kDischarging);
  AdvanceClock(kTimeSpentInUse);
  // By removing the battery, we force the stylus_metrics_recorder to close out
  // the session.
  SetChargerState(StylusChargingStyle::kGarage,
                  BatteryInfo::ChargeStatus::kCharging);
  // Step time further when we're back on charge, to make sure this time is not
  // counted
  AdvanceClock(kTimeSpentCharging);
  RemoveCharger(StylusChargingStyle::kGarage,
                BatteryInfo::ChargeStatus::kCharging);

  histogram_tester_->ExpectTotalCount(kHistogramDockSessionMetric, 0);

  histogram_tester_->ExpectBucketCount(
      kHistogramGarageSessionMetric,
      static_cast<int>(
          feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess),
      1);
  histogram_tester_->ExpectBucketCount(
      kHistogramGarageSessionMetric,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEnabled), 1);
  histogram_tester_->ExpectBucketCount(
      kHistogramGarageSessionMetric,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEligible),
      1);

  histogram_tester_->ExpectBucketCount(
      kHistogramGarageOrDockSessionMetric,
      static_cast<int>(
          feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess),
      1);
  histogram_tester_->ExpectBucketCount(
      kHistogramGarageOrDockSessionMetric,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEnabled), 1);
  histogram_tester_->ExpectBucketCount(
      kHistogramGarageOrDockSessionMetric,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEligible),
      1);

  histogram_tester_->ExpectTimeBucketCount(kHistogramGarageSessionUsetimeMetric,
                                           kTimeSpentInUse, 1);
  histogram_tester_->ExpectTimeBucketCount(
      kHistogramGarageOrDockSessionUsetimeMetric, kTimeSpentInUse, 1);
}

TEST_F(StylusMetricsRecorderTest, RemovedFromDock) {
  const base::TimeDelta kTimeSpentInUse = base::Minutes(5);
  const base::TimeDelta kTimeSpentCharging = base::Minutes(1);

  SetChargerState(StylusChargingStyle::kDock,
                  BatteryInfo::ChargeStatus::kDischarging);
  AdvanceClock(kTimeSpentInUse);
  // By removing the battery, we force the stylus_metrics_recorder to close out
  // the session.
  SetChargerState(StylusChargingStyle::kDock,
                  BatteryInfo::ChargeStatus::kCharging);
  // Step time further when we're back on charge, to make sure this time is not
  // counted
  AdvanceClock(kTimeSpentCharging);
  RemoveCharger(StylusChargingStyle::kDock,
                BatteryInfo::ChargeStatus::kCharging);

  histogram_tester_->ExpectTotalCount(kHistogramGarageSessionMetric, 0);

  histogram_tester_->ExpectBucketCount(
      kHistogramDockSessionMetric,
      static_cast<int>(
          feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess),
      1);
  histogram_tester_->ExpectBucketCount(
      kHistogramDockSessionMetric,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEnabled), 1);
  histogram_tester_->ExpectBucketCount(
      kHistogramDockSessionMetric,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEligible),
      1);

  histogram_tester_->ExpectBucketCount(
      kHistogramGarageOrDockSessionMetric,
      static_cast<int>(
          feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess),
      1);
  histogram_tester_->ExpectBucketCount(
      kHistogramGarageOrDockSessionMetric,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEnabled), 1);
  histogram_tester_->ExpectBucketCount(
      kHistogramGarageOrDockSessionMetric,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEligible),
      1);

  histogram_tester_->ExpectTimeBucketCount(kHistogramDockSessionUsetimeMetric,
                                           kTimeSpentInUse, 1);
  histogram_tester_->ExpectTimeBucketCount(
      kHistogramGarageOrDockSessionUsetimeMetric, kTimeSpentInUse, 1);
}

TEST_F(StylusMetricsRecorderTest, ShutdownWhileStylusRemoved) {
  const base::TimeDelta kTimeSpentInUse = base::Minutes(5);

  SetChargerState(StylusChargingStyle::kGarage,
                  BatteryInfo::ChargeStatus::kDischarging);
  AdvanceClock(kTimeSpentInUse);
  // By removing the battery, we force the stylus_metrics_recorder to close out
  // the session.
  SetChargerState(StylusChargingStyle::kGarage,
                  BatteryInfo::ChargeStatus::kCharging);
  // Just destroy the recorder, without replacing the stylus; we should still
  // see the time recorded
  stylus_metrics_recorder_.reset();

  histogram_tester_->ExpectBucketCount(
      kHistogramGarageSessionMetric,
      static_cast<int>(
          feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess),
      1);

  histogram_tester_->ExpectTimeBucketCount(kHistogramGarageSessionUsetimeMetric,
                                           kTimeSpentInUse, 1);
}

TEST_F(StylusMetricsRecorderTest, StylusUsageOverMultipleDays) {
  const base::TimeDelta kTimeSpentInUse = base::Hours(48);

  SetChargerState(StylusChargingStyle::kGarage,
                  BatteryInfo::ChargeStatus::kDischarging);
  AdvanceClock(kTimeSpentInUse);
  RemoveCharger(StylusChargingStyle::kGarage,
                BatteryInfo::ChargeStatus::kCharging);

  histogram_tester_->ExpectBucketCount(
      kHistogramGarageSessionMetric,
      static_cast<int>(
          feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess),
      1);

  histogram_tester_->ExpectTimeBucketCount(kHistogramGarageSessionUsetimeMetric,
                                           kTimeSpentInUse, 1);
}

TEST_F(StylusMetricsRecorderTest, StylusChargeSequencing) {
  const base::TimeDelta kTimeSpentTrickleCharging = base::Minutes(1);
  const base::TimeDelta kTimeSpentCharging = base::Minutes(60);
  const base::TimeDelta kTimeSpentFull = base::Minutes(5);
  const base::TimeDelta kTimeSpentDischarging = base::Minutes(60);
  const int kCycles = 2;

  // Initial state, stylus is garage, charging, not in use
  SetChargerState(StylusChargingStyle::kGarage,
                  BatteryInfo::ChargeStatus::kCharging);

  for (int cycle = 0; cycle < kCycles; cycle++) {
    SetChargerState(StylusChargingStyle::kGarage,
                    BatteryInfo::ChargeStatus::kCharging);
    AdvanceClock(kTimeSpentTrickleCharging);
    SetChargerState(StylusChargingStyle::kGarage,
                    BatteryInfo::ChargeStatus::kCharging);
    AdvanceClock(kTimeSpentCharging);
    SetChargerState(StylusChargingStyle::kGarage,
                    BatteryInfo::ChargeStatus::kFull);
    AdvanceClock(kTimeSpentFull);
    // Stylus is removed from garage when it starts discharging
    SetChargerState(StylusChargingStyle::kGarage,
                    BatteryInfo::ChargeStatus::kDischarging);
    AdvanceClock(kTimeSpentDischarging);
  }

  // Final state, same as initial
  SetChargerState(StylusChargingStyle::kGarage,
                  BatteryInfo::ChargeStatus::kCharging);

  histogram_tester_->ExpectBucketCount(
      kHistogramGarageSessionMetric,
      static_cast<int>(
          feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess),
      kCycles);

  histogram_tester_->ExpectTimeBucketCount(kHistogramGarageSessionUsetimeMetric,
                                           kTimeSpentDischarging, kCycles);
}

}  // namespace ash
