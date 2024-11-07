// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/prefs/pref_service.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/mock_clock.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
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
  EXPECT_THAT(priority, Eq(Priority::IMMEDIATE));
  ASSERT_TRUE(record.has_destination());
  EXPECT_THAT(record.destination(), Eq(Destination::KIOSK_HEARTBEAT_EVENTS));
  ASSERT_TRUE(record.has_dm_token());
  EXPECT_THAT(record.dm_token(), StrEq(kDMToken));
  ASSERT_TRUE(record.has_source_info());
  EXPECT_THAT(record.source_info().source(), Eq(SourceInfo::ASH));
}

// Returns true if the record includes KioskHeartbeatTelemetry events.
// False otherwise.
bool IsKioskHeartbeatTelemetryEvent(const Record& record) {
  MetricData record_data;
  return record_data.ParseFromString(record.data()) &&
         record_data.has_telemetry_data() &&
         record_data.telemetry_data().has_heartbeat_telemetry();
}

// Browser test that validates Kiosk Heartbeat telemetry reported by the
// `KioskHeartbeatTelemetrySampler`. Inheriting from
// `DevicePolicyCrosBrowserTest` enables use of `AffiliationMixin` for setting
// up profile/device affiliation. Only available in Ash.
class KioskHeartbeatEventsBrowserTest
    : public ::policy::DevicePolicyCrosBrowserTest {
 protected:
  KioskHeartbeatEventsBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kKioskHeartbeatsViaERP);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ::policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
        command_line);
    ::policy::DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    // Initialize the MockClock.
    test::MockClock::Get();
    crypto_home_mixin_.MarkUserAsExisting(affiliation_mixin_.account_id());
    crypto_home_mixin_.ApplyAuthConfig(
        affiliation_mixin_.account_id(),
        ash::test::UserAuthConfig::Create(ash::test::kDefaultAuthSetup));
    ::policy::SetDMTokenForTesting(
        ::policy::DMToken::CreateValidToken(kDMToken));
    ::policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();

    if (::content::IsPreTest()) {
      // Preliminary setup - set up affiliated user.
      ::policy::AffiliationTestHelper::PreLoginUser(
          affiliation_mixin_.account_id());
      return;
    }

    // Login as affiliated user otherwise and set up test environment.
    ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  }

  void SetKioskHeartbeatEnabled() {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ::ash::kHeartbeatEnabled, true);
    PrefService* local_state = g_browser_process->local_state();
    local_state->SetBoolean(::ash::kHeartbeatEnabled, true);
  }

  ::ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  ::base::test::ScopedFeatureList scoped_feature_list_;
  ::policy::DevicePolicyCrosTestHelper test_helper_;
  ::policy::AffiliationMixin affiliation_mixin_{&mixin_host_, &test_helper_};
  ::ash::CryptohomeMixin crypto_home_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(KioskHeartbeatEventsBrowserTest,
                       PRE_ReportKioskHeartbeats) {
  // Simple case that sets up the affiliated user through SetUpOnMainThread
  // PRE-condition.
}

IN_PROC_BROWSER_TEST_F(KioskHeartbeatEventsBrowserTest, ReportKioskHeartbeats) {
  SetKioskHeartbeatEnabled();
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsKioskHeartbeatTelemetryEvent));

  // Consume all queued tasks so that policy is synced and collector started.
  base::RunLoop().RunUntilIdle();

  // Fail if no heartbeat is queued immediately.
  ASSERT_TRUE(missive_observer.HasNewEnqueuedRecord())
      << "No new KioskHeartbeat record enqueued to ERP. Failing";

  const auto [priority, record] = missive_observer.GetNextEnqueuedRecord();

  AssertRecordData(priority, record);
  MetricData metric_data;
  ASSERT_TRUE(metric_data.ParseFromString(record.data()));
  EXPECT_TRUE(metric_data.has_timestamp_ms());
  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());

  // Fast-forward so that another heartbeat should be enqueued.
  test::MockClock::Get().Advance(
      metrics::kDefaultHeartbeatTelemetryCollectionRate);

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(missive_observer.HasNewEnqueuedRecord())
      << "No new KioskHeartbeat record enqueued to ERP. Failing";
}
}  // namespace
}  // namespace reporting
