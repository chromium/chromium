// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/power_metrics_reporter.h"

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

class PowerMetricsReporterTest : public testing::Test {
 public:
  PowerMetricsReporterTest() = default;
  ~PowerMetricsReporterTest() override = default;

  void SetUp() override {
    PowerManagerClient::InitializeFake();
    PowerMetricsReporter::RegisterLocalStatePrefs(pref_service_.registry());
    ResetReporter();
  }

  void TearDown() override {
    reporter_.reset();
    PowerManagerClient::Shutdown();
  }

 protected:
  // Reinitialize |reporter_| without resetting underlying prefs. May be called
  // by tests to simulate a Chrome restart.
  void ResetReporter() {
    reporter_ = std::make_unique<PowerMetricsReporter>(
        chromeos::FakePowerManagerClient::Get(), &pref_service_);
  }

  // Notifies |reporter_| that the screen is undimmed and on.
  void SendNormalScreenIdleState() {
    chromeos::FakePowerManagerClient::Get()->SendScreenIdleStateChanged(
        power_manager::ScreenIdleState());
  }

  // Notifies |reporter_| that the screen is dimmed and on.
  void SendDimmedScreenIdleState() {
    power_manager::ScreenIdleState state;
    state.set_dimmed(true);
    chromeos::FakePowerManagerClient::Get()->SendScreenIdleStateChanged(state);
  }

  // Notifies |reporter_| that the screen is dimmed and off.
  void SendDimmedAndOffScreenIdleState() {
    power_manager::ScreenIdleState state;
    state.set_dimmed(true);
    state.set_off(true);
    chromeos::FakePowerManagerClient::Get()->SendScreenIdleStateChanged(state);
  }

  // Instructs |reporter_| to report daily metrics for reason |type|.
  void TriggerDailyEvent(metrics::DailyEvent::IntervalType type) {
    reporter_->ReportDailyMetrics(type);
  }

  // Instructs |reporter_| to report daily metrics due to the passage of a day
  // and verifies that it reports one sample with each of the passed values.
  void TriggerDailyEventAndVerifyHistograms(int idle_dim_count,
                                            int idle_off_count,
                                            int idle_suspend_count,
                                            int lid_closed_suspend_count) {
    base::HistogramTester histogram_tester;
    TriggerDailyEvent(metrics::DailyEvent::IntervalType::DAY_ELAPSED);
    histogram_tester.ExpectUniqueSample(
        PowerMetricsReporter::kIdleScreenDimCountName, idle_dim_count, 1);
    histogram_tester.ExpectUniqueSample(
        PowerMetricsReporter::kIdleScreenOffCountName, idle_off_count, 1);
    histogram_tester.ExpectUniqueSample(
        PowerMetricsReporter::kIdleSuspendCountName, idle_suspend_count, 1);
    histogram_tester.ExpectUniqueSample(
        PowerMetricsReporter::kLidClosedSuspendCountName,
        lid_closed_suspend_count, 1);
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<PowerMetricsReporter> reporter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerMetricsReporterTest);
};

TEST_F(PowerMetricsReporterTest, CountAndReportEvents) {
  // Report two screen dims, one screen off event, and no suspends.
  SendNormalScreenIdleState();
  SendDimmedScreenIdleState();
  SendDimmedAndOffScreenIdleState();
  SendNormalScreenIdleState();
  SendDimmedScreenIdleState();
  SendNormalScreenIdleState();
  TriggerDailyEventAndVerifyHistograms(2, 1, 0, 0);

  // The next day, report three dims, two screen-offs, and one idle suspend.
  SendDimmedScreenIdleState();
  SendDimmedAndOffScreenIdleState();
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_IDLE);
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  SendNormalScreenIdleState();
  SendDimmedScreenIdleState();
  SendDimmedAndOffScreenIdleState();
  SendNormalScreenIdleState();
  SendDimmedScreenIdleState();
  SendNormalScreenIdleState();
  TriggerDailyEventAndVerifyHistograms(3, 2, 1, 0);

  // The next day, report a single lid-closed suspend.
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_LID_CLOSED);
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  TriggerDailyEventAndVerifyHistograms(0, 0, 0, 1);

  // We should report zeros if a day passes without any events.
  TriggerDailyEventAndVerifyHistograms(0, 0, 0, 0);
}

TEST_F(PowerMetricsReporterTest, LoadInitialCountsFromPrefs) {
  // Create a new reporter and check that it loads its initial event counts from
  // prefs.
  pref_service_.SetInteger(prefs::kPowerMetricsIdleScreenDimCount, 5);
  pref_service_.SetInteger(prefs::kPowerMetricsIdleScreenOffCount, 3);
  pref_service_.SetInteger(prefs::kPowerMetricsIdleSuspendCount, 2);
  ResetReporter();
  TriggerDailyEventAndVerifyHistograms(5, 3, 2, 0);

  // The previous report should've cleared the prefs, so a new reporter should
  // start out at zero.
  ResetReporter();
  TriggerDailyEventAndVerifyHistograms(0, 0, 0, 0);
}

TEST_F(PowerMetricsReporterTest, IgnoreUnchangedScreenIdleState) {
  // Only report transitions from undimmed to dimmed and from on to off.
  SendDimmedAndOffScreenIdleState();
  SendDimmedAndOffScreenIdleState();
  SendDimmedScreenIdleState();
  SendDimmedScreenIdleState();
  TriggerDailyEventAndVerifyHistograms(1, 1, 0, 0);
}

TEST_F(PowerMetricsReporterTest, IgnoreOtherSuspendReasons) {
  // Suspends triggered for other reasons shouldn't be reported.
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  TriggerDailyEventAndVerifyHistograms(0, 0, 0, 0);
}

TEST_F(PowerMetricsReporterTest, IgnoreDailyEventFirstRun) {
  // metrics::DailyEvent notifies observers immediately on first run. Histograms
  // shouldn't be sent in this case.
  base::HistogramTester tester;
  TriggerDailyEvent(metrics::DailyEvent::IntervalType::FIRST_RUN);
  tester.ExpectTotalCount(PowerMetricsReporter::kIdleScreenDimCountName, 0);
  tester.ExpectTotalCount(PowerMetricsReporter::kIdleScreenOffCountName, 0);
  tester.ExpectTotalCount(PowerMetricsReporter::kIdleSuspendCountName, 0);
  tester.ExpectTotalCount(PowerMetricsReporter::kLidClosedSuspendCountName, 0);
}

TEST_F(PowerMetricsReporterTest, IgnoreDailyEventClockChanged) {
  SendDimmedScreenIdleState();
  SendNormalScreenIdleState();

  // metrics::DailyEvent notifies observers if it sees that the system clock has
  // jumped back. Histograms shouldn't be sent in this case.
  base::HistogramTester tester;
  TriggerDailyEvent(metrics::DailyEvent::IntervalType::CLOCK_CHANGED);
  tester.ExpectTotalCount(PowerMetricsReporter::kIdleScreenDimCountName, 0);
  tester.ExpectTotalCount(PowerMetricsReporter::kIdleScreenOffCountName, 0);
  tester.ExpectTotalCount(PowerMetricsReporter::kIdleSuspendCountName, 0);
  tester.ExpectTotalCount(PowerMetricsReporter::kLidClosedSuspendCountName, 0);

  // The existing stats should be cleared when the clock change notification is
  // received, so the next report should only contain zeros.
  TriggerDailyEventAndVerifyHistograms(0, 0, 0, 0);
}

}  // namespace chromeos
