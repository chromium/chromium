// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/components/settings/cros_settings_names.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using ::testing::Eq;

namespace reporting {
namespace {

constexpr int kSignalStrength = 50;
constexpr char kWifiGuid[] = "wifi-guid";
constexpr char kWifiServicePath[] = "/service/wlan";
const auto network_connection_state =
    chromeos::network_health::mojom::NetworkState::kOnline;

Record GetNextRecord(::chromeos::MissiveClientTestObserver* observer) {
  const std::tuple<Priority, Record>& enqueued_record =
      observer->GetNextEnqueuedRecord();
  Priority priority = std::get<0>(enqueued_record);
  Record record = std::get<1>(enqueued_record);
  EXPECT_THAT(priority, Eq(Priority::SLOW_BATCH));
  return record;
}

class NetworkEventsBrowserTest : public ::policy::DevicePolicyCrosBrowserTest {
 protected:
  NetworkEventsBrowserTest() {
    crypto_home_mixin_.MarkUserAsExisting(affiliation_mixin_.account_id());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ::policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
        command_line);
    ::policy::DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ::policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    if (content::IsPreTest()) {
      // Preliminary setup - set up affiliated user
      ::policy::AffiliationTestHelper::PreLoginUser(
          affiliation_mixin_.account_id());

      return;
    }
    ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  }

  void EnablePolicy() {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDeviceNetworkStatus, true);
  }

  void DisablePolicy() {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDeviceNetworkStatus, false);
  }

  // Create user.
  ::policy::DevicePolicyCrosTestHelper test_helper_;
  ::policy::AffiliationMixin affiliation_mixin_{&mixin_host_, &test_helper_};
  ash::CryptohomeMixin crypto_home_mixin_{&mixin_host_};
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_F(NetworkEventsBrowserTest,
                       PRE_ConnectionStateAffiliatedUserAndPolicyEnabled) {
  // dummy case to register user.
}

IN_PROC_BROWSER_TEST_F(NetworkEventsBrowserTest,
                       ConnectionStateAffiliatedUserAndPolicyEnabled) {
  chromeos::MissiveClientTestObserver missive_observer_(
      ::reporting::Destination::EVENT_METRIC);

  ::ash::NetworkHandlerTestHelper network_handler_test_helper_;
  network_handler_test_helper_.AddDefaultProfiles();
  network_handler_test_helper_.ResetDevicesAndServices();
  auto* const service_client = network_handler_test_helper_.service_test();

  service_client->AddService(kWifiServicePath, kWifiGuid, "wifi-name",
                             shill::kTypeWifi, shill::kStateReady, true);

  EnablePolicy();

  ash::cros_healthd::FakeCrosHealthd::Get()
      ->EmitConnectionStateChangedEventForTesting(kWifiGuid,
                                                  network_connection_state);

  const Record& record = GetNextRecord(&missive_observer_);
  MetricData record_data;

  ASSERT_TRUE(record_data.ParseFromString(record.data()));

  // Testing event found successfully.
  EXPECT_THAT(
      record_data.event_data().type(),
      Eq(::reporting::MetricEventType::NETWORK_CONNECTION_STATE_CHANGE));
}

IN_PROC_BROWSER_TEST_F(NetworkEventsBrowserTest,
                       PRE_SignalStrengthAffiliatedUserAndPolicyEnabled) {
  // dummy case to register user.
}

IN_PROC_BROWSER_TEST_F(NetworkEventsBrowserTest,
                       SignalStrengthAffiliatedUserAndPolicyEnabled) {
  chromeos::MissiveClientTestObserver missive_observer_(
      ::reporting::Destination::EVENT_METRIC);

  ::ash::NetworkHandlerTestHelper network_handler_test_helper_;
  network_handler_test_helper_.AddDefaultProfiles();
  network_handler_test_helper_.ResetDevicesAndServices();
  auto* const service_client = network_handler_test_helper_.service_test();

  service_client->AddService(kWifiServicePath, kWifiGuid, "wifi-name",
                             shill::kTypeWifi, shill::kStateReady, true);

  EnablePolicy();
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->EmitSignalStrengthChangedEventForTesting(
          kWifiGuid,
          chromeos::network_health::mojom::UInt32Value::New(kSignalStrength));

  const Record& record = GetNextRecord(&missive_observer_);
  MetricData record_data;

  ASSERT_TRUE(record_data.ParseFromString(record.data()));

  // Testing event found successfully.
  EXPECT_THAT(record_data.event_data().type(),
              Eq(::reporting::MetricEventType::NETWORK_SIGNAL_STRENGTH_CHANGE));
}

}  // namespace
}  // namespace reporting
