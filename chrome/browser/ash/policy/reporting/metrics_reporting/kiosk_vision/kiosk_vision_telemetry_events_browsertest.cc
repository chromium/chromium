// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_test_helper.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/prefs/pref_service.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/mock_clock.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::StrEq;

namespace reporting {
namespace {

// Test DM token used to associate reported events.
constexpr char kDMToken[] = "token";

void AssertRecordData(Priority priority, const Record& record) {
  EXPECT_THAT(priority, Eq(Priority::MANUAL_BATCH));
  ASSERT_TRUE(record.has_destination());
  EXPECT_THAT(record.destination(), Eq(Destination::TELEMETRY_METRIC));
  ASSERT_TRUE(record.has_dm_token());
  EXPECT_THAT(record.dm_token(), StrEq(kDMToken));
  ASSERT_TRUE(record.has_source_info());
  EXPECT_THAT(record.source_info().source(), Eq(SourceInfo::ASH));
}

// Returns true if the record includes KioskVisionTelemetry events.
// False otherwise.
bool IsKioskVisionTelemetryEvent(const Record& record) {
  MetricData record_data;
  return record_data.ParseFromString(record.data()) &&
         record_data.has_telemetry_data() &&
         (record_data.telemetry_data().has_kiosk_vision_telemetry() ||
          record_data.telemetry_data().has_kiosk_vision_status());
}

// Returns a test observer that only captures Kiosk vision telemetry events.
::chromeos::MissiveClientTestObserver CreateKioskVisionEventsObserver() {
  return ::chromeos::MissiveClientTestObserver(
      base::BindRepeating(&IsKioskVisionTelemetryEvent));
}

}  // namespace

// Browser test that validates Kiosk Vision telemetry reported by the
// `KioskVisionTelemetrySampler`. Inheriting from `::ash::WebKioskBaseTest` to
// setup a kiosk session and spawn KioskVision's TelemetryProcessor. Only
// available in Ash.
class KioskVisionEventsBrowserTest : public ::ash::WebKioskBaseTest {
 protected:
  KioskVisionEventsBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        reporting::kEnableKioskVisionTelemetry);
  }

  void SetUpOnMainThread() override {
    // Initialize the MockClock.
    test::MockClock::Get();
    ::policy::SetDMTokenForTesting(
        ::policy::DMToken::CreateValidToken(kDMToken));

    WebKioskBaseTest::SetUpOnMainThread();
  }

  ::base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/367691581) Remove kiosk vision.
IN_PROC_BROWSER_TEST_F(KioskVisionEventsBrowserTest,
                       DISABLED_ReportKioskVisions) {
  InitializeRegularOnlineKiosk();

  auto missive_observer = CreateKioskVisionEventsObserver();

  // Fast-forward so that a vision report should be enqueued.
  test::MockClock::Get().Advance(
      reporting::metrics::kInitialCollectionDelay +
      reporting::metrics::kDefaultKioskVisionTelemetryCollectionRate);

  const auto [priority, record] = missive_observer.GetNextEnqueuedRecord();

  AssertRecordData(priority, record);
  MetricData metric_data;
  ASSERT_TRUE(metric_data.ParseFromString(record.data()));
  EXPECT_TRUE(metric_data.has_timestamp_ms());
  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
}

}  // namespace reporting
