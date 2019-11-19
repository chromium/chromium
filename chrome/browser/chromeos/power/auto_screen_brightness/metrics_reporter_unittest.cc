// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/metrics_reporter.h"

#include <memory>

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/metrics/daily_event.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

namespace {

constexpr auto kNoAls = MetricsReporter::DeviceClass::kNoAls;
constexpr auto kSupportedAls = MetricsReporter::DeviceClass::kSupportedAls;
constexpr auto kUnsupportedAls = MetricsReporter::DeviceClass::kUnsupportedAls;
constexpr auto kAtlas = MetricsReporter::DeviceClass::kAtlas;
constexpr auto kEve = MetricsReporter::DeviceClass::kEve;
constexpr auto kNocturne = MetricsReporter::DeviceClass::kNocturne;

}  // namespace

class MetricsReporterTest : public testing::Test {
 public:
  MetricsReporterTest() = default;
  ~MetricsReporterTest() override = default;

  void SetUp() override {
    PowerManagerClient::InitializeFake();
    MetricsReporter::RegisterLocalStatePrefs(pref_service_.registry());
  }

  void TearDown() override {
    reporter_.reset();
    PowerManagerClient::Shutdown();
  }

 protected:
  // Reinitialize |reporter_| without resetting underlying prefs. May be called
  // by tests to simulate a Chrome restart.
  void ResetReporter(MetricsReporter::DeviceClass device_class) {
    reporter_ = std::make_unique<MetricsReporter>(PowerManagerClient::Get(),
                                                  &pref_service_);
    reporter_->SetDeviceClass(device_class);
  }

  // Notifies |reporter_| that a user adjustment request is received.
  void SendOnUserBrightnessChangeRequested() {
    reporter_->OnUserBrightnessChangeRequested();
  }

  // Instructs |reporter_| to report daily metrics for reason |type|.
  void TriggerDailyEvent(metrics::DailyEvent::IntervalType type) {
    reporter_->ReportDailyMetricsForTesting(type);
  }

  // Instructs |reporter_| to report daily metrics due to the passage of a day
  // and verifies that it reports one sample with each of the passed values.
  void TriggerDailyEventAndVerifyHistograms(const std::string& histogram_name,
                                            int expected_count) {
    base::HistogramTester histogram_tester;

    TriggerDailyEvent(metrics::DailyEvent::IntervalType::DAY_ELAPSED);
    histogram_tester.ExpectUniqueSample(histogram_name, expected_count, 1);
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<MetricsReporter> reporter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsReporterTest);
};

TEST_F(MetricsReporterTest, CountAndReportEvents) {
  // Three without ALS.
  ResetReporter(kNoAls);
  SendOnUserBrightnessChangeRequested();
  SendOnUserBrightnessChangeRequested();
  SendOnUserBrightnessChangeRequested();
  TriggerDailyEventAndVerifyHistograms(
      MetricsReporter::kNoAlsUserAdjustmentName, 3);

  // Two with unsupported ALS.
  ResetReporter(kUnsupportedAls);
  SendOnUserBrightnessChangeRequested();
  SendOnUserBrightnessChangeRequested();
  TriggerDailyEventAndVerifyHistograms(
      MetricsReporter::kUnsupportedAlsUserAdjustmentName, 2);

  // Two with supported ALS.
  ResetReporter(kSupportedAls);
  SendOnUserBrightnessChangeRequested();
  SendOnUserBrightnessChangeRequested();
  TriggerDailyEventAndVerifyHistograms(
      MetricsReporter::kSupportedAlsUserAdjustmentName, 2);

  // Two with Atlas.
  ResetReporter(kAtlas);
  SendOnUserBrightnessChangeRequested();
  SendOnUserBrightnessChangeRequested();
  TriggerDailyEventAndVerifyHistograms(
      MetricsReporter::kAtlasUserAdjustmentName, 2);

  // Three with Eve.
  ResetReporter(kEve);
  SendOnUserBrightnessChangeRequested();
  SendOnUserBrightnessChangeRequested();
  SendOnUserBrightnessChangeRequested();
  TriggerDailyEventAndVerifyHistograms(MetricsReporter::kEveUserAdjustmentName,
                                       3);

  // The next day, another two with Eve.
  SendOnUserBrightnessChangeRequested();
  SendOnUserBrightnessChangeRequested();
  TriggerDailyEventAndVerifyHistograms(MetricsReporter::kEveUserAdjustmentName,
                                       2);

  // Four with Nocturne.
  ResetReporter(kNocturne);
  SendOnUserBrightnessChangeRequested();
  SendOnUserBrightnessChangeRequested();
  SendOnUserBrightnessChangeRequested();
  SendOnUserBrightnessChangeRequested();
  TriggerDailyEventAndVerifyHistograms(
      MetricsReporter::kNocturneUserAdjustmentName, 4);
}

TEST_F(MetricsReporterTest, LoadInitialCountsFromPrefs) {
  // Create a new reporter and check that it loads its initial event counts from
  // prefs.
  pref_service_.SetInteger(
      prefs::kAutoScreenBrightnessMetricsNoAlsUserAdjustmentCount, 1);
  pref_service_.SetInteger(
      prefs::kAutoScreenBrightnessMetricsSupportedAlsUserAdjustmentCount, 2);
  pref_service_.SetInteger(
      prefs::kAutoScreenBrightnessMetricsAtlasUserAdjustmentCount, 2);
  pref_service_.SetInteger(
      prefs::kAutoScreenBrightnessMetricsEveUserAdjustmentCount, 4);
  pref_service_.SetInteger(
      prefs::kAutoScreenBrightnessMetricsNocturneUserAdjustmentCount, 3);
  ResetReporter(kAtlas);

  TriggerDailyEventAndVerifyHistograms(
      MetricsReporter::kAtlasUserAdjustmentName, 2);

  // The previous report should've cleared the prefs, so a new reporter should
  // start out at zero.
  TriggerDailyEventAndVerifyHistograms(
      MetricsReporter::kAtlasUserAdjustmentName, 0);
}

TEST_F(MetricsReporterTest, IgnoreDailyEventFirstRun) {
  ResetReporter(kAtlas);
  // metrics::DailyEvent notifies observers immediately on first run. Histograms
  // shouldn't be sent in this case.
  base::HistogramTester tester;
  TriggerDailyEvent(metrics::DailyEvent::IntervalType::FIRST_RUN);
  tester.ExpectTotalCount(MetricsReporter::kNoAlsUserAdjustmentName, 0);
  tester.ExpectTotalCount(MetricsReporter::kSupportedAlsUserAdjustmentName, 0);
  tester.ExpectTotalCount(MetricsReporter::kUnsupportedAlsUserAdjustmentName,
                          0);
  tester.ExpectTotalCount(MetricsReporter::kAtlasUserAdjustmentName, 0);
  tester.ExpectTotalCount(MetricsReporter::kEveUserAdjustmentName, 0);
  tester.ExpectTotalCount(MetricsReporter::kNocturneUserAdjustmentName, 0);
}

TEST_F(MetricsReporterTest, IgnoreDailyEventClockChanged) {
  ResetReporter(kNocturne);
  SendOnUserBrightnessChangeRequested();

  // metrics::DailyEvent notifies observers if it sees that the system clock has
  // jumped back. Histograms shouldn't be sent in this case.
  base::HistogramTester tester;
  TriggerDailyEvent(metrics::DailyEvent::IntervalType::CLOCK_CHANGED);
  tester.ExpectTotalCount(MetricsReporter::kNoAlsUserAdjustmentName, 0);
  tester.ExpectTotalCount(MetricsReporter::kSupportedAlsUserAdjustmentName, 0);
  tester.ExpectTotalCount(MetricsReporter::kUnsupportedAlsUserAdjustmentName,
                          0);
  tester.ExpectTotalCount(MetricsReporter::kAtlasUserAdjustmentName, 0);
  tester.ExpectTotalCount(MetricsReporter::kEveUserAdjustmentName, 0);
  tester.ExpectTotalCount(MetricsReporter::kNocturneUserAdjustmentName, 0);

  // The existing stats should be cleared when the clock change notification is
  // received, so the next report should only contain zeros.
  TriggerDailyEventAndVerifyHistograms(
      MetricsReporter::kNocturneUserAdjustmentName, 0);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
