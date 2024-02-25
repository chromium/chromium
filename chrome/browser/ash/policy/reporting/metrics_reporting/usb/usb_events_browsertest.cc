// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/constants/ash_switches.h"
#include "base/containers/contains.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/account_id/account_id.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/mock_clock.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;
using testing::StrEq;

namespace reporting {
namespace {

namespace cros_healthd = ::ash::cros_healthd;

// Browser test that validate Usb added/removed events and telemetry collection
// when the`ReportDevicePeripherals policy is set/unset. These tests cases only
// cover USB added events and telemetry collection since FakeCrosHealthd doesn't
// expose a EmitUsbRemovedEventForTesting function.
constexpr char kTestUserEmail[] = "test@example.com";
constexpr char kTestAffiliationId[] = "test_affiliation_id";
constexpr char kDMToken[] = "token";

class UsbEventsBrowserTest : public ::policy::DevicePolicyCrosBrowserTest {
 protected:
  UsbEventsBrowserTest() {
    test::MockClock::Get();
    // Add unaffiliated user for testing purposes.
    login_manager_mixin_.AppendRegularUsers(1);
    ::policy::SetDMTokenForTesting(
        ::policy::DMToken::CreateValidToken(kDMToken));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kLoginManager);
  }

  void SetUpInProcessBrowserTestFixture() override {
    policy::DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    // Set up affiliation for the test user.
    auto device_policy_update = device_state_.RequestDevicePolicyUpdate();
    auto user_policy_update = user_policy_mixin_.RequestPolicyUpdate();

    device_policy_update->policy_data()->add_device_affiliation_ids(
        kTestAffiliationId);
    user_policy_update->policy_data()->add_user_affiliation_ids(
        kTestAffiliationId);
  }

  void EnableUsbPolicy() {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDevicePeripherals, true);
  }

  void DisableUsbPolicy() {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDevicePeripherals, false);
  }

  bool NoUsbEventsEnqueued(const std::vector<Record>& records) {
    return !base::Contains(records, Destination::PERIPHERAL_EVENTS,
                           &Record::destination);
  }

  void LoginAffiliatedUser() {
    const ash::LoginManagerMixin::TestUserInfo user_info(test_account_id_);
    const auto& context =
        ash::LoginManagerMixin::CreateDefaultUserContext(user_info);
    login_manager_mixin_.LoginAsNewRegularUser(context);
    ash::test::WaitForPrimaryUserSessionStart();
  }

  void LoginUnaffiliatedUser() {
    login_manager_mixin_.LoginAsNewRegularUser();
    ash::test::WaitForPrimaryUserSessionStart();
  }

  cros_healthd::mojom::TelemetryInfoPtr CreateUsbTelemetry() {
    constexpr uint8_t kClassId = 255;
    constexpr uint8_t kSubclassId = 1;
    constexpr uint16_t kVendorId = 65535;
    constexpr uint16_t kProductId = 1;
    constexpr char kVendorName[] = "VendorName";
    constexpr char kProductName[] = "ProductName";
    constexpr char kFirmwareVersion[] = "FirmwareVersion";

    cros_healthd::mojom::BusDevicePtr usb_device =
        cros_healthd::mojom::BusDevice::New();
    usb_device->vendor_name = kVendorName;
    usb_device->product_name = kProductName;
    usb_device->bus_info = cros_healthd::mojom::BusInfo::NewUsbBusInfo(
        cros_healthd::mojom::UsbBusInfo::New(
            kClassId, kSubclassId, /*protocol_id=*/0, kVendorId, kProductId,
            /*interfaces = */
            std::vector<cros_healthd::mojom::UsbBusInterfaceInfoPtr>(),
            cros_healthd::mojom::FwupdFirmwareVersionInfo::New(
                kFirmwareVersion,
                cros_healthd::mojom::FwupdVersionFormat::kPlain)));

    std::vector<cros_healthd::mojom::BusDevicePtr> usb_devices;
    usb_devices.push_back(std::move(usb_device));
    auto telemetry_info = cros_healthd::mojom::TelemetryInfo::New();
    telemetry_info->bus_result =
        cros_healthd::mojom::BusResult::NewBusDevices(std::move(usb_devices));
    return telemetry_info;
  }

  void EmitUsbAddEventForTesting() {
    cros_healthd::mojom::UsbEventInfo info;
    info.state = cros_healthd::mojom::UsbEventInfo::State::kAdd;
    cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
        cros_healthd::mojom::EventCategoryEnum::kUsb,
        cros_healthd::mojom::EventInfo::NewUsbEventInfo(info.Clone()));
  }

  const AccountId test_account_id_ = AccountId::FromUserEmailGaiaId(
      kTestUserEmail,
      signin::GetTestGaiaIdForEmail(kTestUserEmail));

  ash::UserPolicyMixin user_policy_mixin_{&mixin_host_, test_account_id_};
  FakeGaiaMixin fake_gaia_mixin_{&mixin_host_};
  ash::LoginManagerMixin login_manager_mixin_{
      &mixin_host_, ash::LoginManagerMixin::UserList(), &fake_gaia_mixin_};
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_F(UsbEventsBrowserTest,
                       UsbEventDrivenTelemetryCollectedOnUsbEvent) {
  chromeos::MissiveClientTestObserver missive_observer(
      Destination::PERIPHERAL_EVENTS);

  EnableUsbPolicy();

  LoginAffiliatedUser();

  // Setup fake telemetry to be collected
  auto usb_telemetry = CreateUsbTelemetry();
  cros_healthd::FakeCrosHealthd::Get()->SetProbeTelemetryInfoResponseForTesting(
      usb_telemetry);

  // Any USB event should trigger event driven telemetry collection
  EmitUsbAddEventForTesting();

  ::content::RunAllTasksUntilIdle();

  Record record = std::get<1>(missive_observer.GetNextEnqueuedRecord());
  ASSERT_TRUE(record.has_source_info());
  EXPECT_THAT(record.source_info().source(), Eq(SourceInfo::ASH));
  MetricData record_data;

  // First record should be the USB added event
  ASSERT_TRUE(record_data.ParseFromString(record.data()));
  EXPECT_THAT(record_data.event_data().type(), Eq(MetricEventType::USB_ADDED));

  // Telemetry should collected be collected with a delay after a USB event
  // occurs.
  test::MockClock::Get().Advance(metrics::kPeripheralCollectionDelay);

  // Second record should be the USB telemetry
  record = std::get<1>(missive_observer.GetNextEnqueuedRecord());
  ASSERT_TRUE(record_data.ParseFromString(record.data()));
  EXPECT_TRUE(record_data.has_telemetry_data());
  EXPECT_TRUE(record_data.telemetry_data().has_peripherals_telemetry());
  // Since telemetry is not an event, it shouldn't have event data or event type
  EXPECT_FALSE(record_data.has_event_data());

  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
}

IN_PROC_BROWSER_TEST_F(
    UsbEventsBrowserTest,
    UsbAddedEventCollectedWhenPolicyEnabledWithAffiliatedUser) {
  chromeos::MissiveClientTestObserver missive_observer(
      Destination::PERIPHERAL_EVENTS);

  EnableUsbPolicy();

  LoginAffiliatedUser();

  EmitUsbAddEventForTesting();

  ::content::RunAllTasksUntilIdle();

  std::tuple<Priority, Record> entry = missive_observer.GetNextEnqueuedRecord();
  Record record = std::get<1>(entry);
  ASSERT_TRUE(record.has_source_info());
  EXPECT_THAT(record.source_info().source(), Eq(SourceInfo::ASH));

  MetricData record_data;
  ASSERT_TRUE(record_data.ParseFromString(record.data()));

  EXPECT_TRUE(record_data.has_telemetry_data());
  EXPECT_TRUE(record_data.telemetry_data().has_peripherals_telemetry());
  EXPECT_THAT(record_data.event_data().type(), Eq(MetricEventType::USB_ADDED));
  EXPECT_THAT(record.destination(), Eq(Destination::PERIPHERAL_EVENTS));
  ASSERT_TRUE(record.has_dm_token());
  EXPECT_THAT(record.dm_token(), StrEq(kDMToken));

  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
}

IN_PROC_BROWSER_TEST_F(
    UsbEventsBrowserTest,
    UsbTelemetryCollectedWhenPolicyEnabledWithAffiliatedUser) {
  chromeos::MissiveClientTestObserver missive_observer(
      Destination::PERIPHERAL_EVENTS);

  EnableUsbPolicy();

  // Setup fake telemetry.
  auto usb_telemetry = CreateUsbTelemetry();
  cros_healthd::FakeCrosHealthd::Get()->SetProbeTelemetryInfoResponseForTesting(
      usb_telemetry);

  // This triggers USB telemetry collection, a.k.a USB status updates
  LoginAffiliatedUser();

  ::content::RunAllTasksUntilIdle();

  std::tuple<Priority, Record> entry = missive_observer.GetNextEnqueuedRecord();
  Record record = std::get<1>(entry);
  ASSERT_TRUE(record.has_source_info());
  EXPECT_THAT(record.source_info().source(), Eq(SourceInfo::ASH));

  MetricData record_data;
  ASSERT_TRUE(record_data.ParseFromString(record.data()));

  EXPECT_TRUE(record_data.has_telemetry_data());
  EXPECT_TRUE(record_data.telemetry_data().has_peripherals_telemetry());
  // Even though USB status updates are triggered by affiliated login, they're
  // technically telemetry, not events, so their event type is
  // EVENT_TYPE_UNSPECIFIED
  EXPECT_THAT(record_data.event_data().type(),
              Eq(MetricEventType::EVENT_TYPE_UNSPECIFIED));
  EXPECT_THAT(record.destination(), Eq(Destination::PERIPHERAL_EVENTS));
  ASSERT_TRUE(record.has_dm_token());
  EXPECT_THAT(record.dm_token(), StrEq(kDMToken));

  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
}

IN_PROC_BROWSER_TEST_F(
    UsbEventsBrowserTest,
    NoUsbEventsOrTelemetryWhenPolicyEnabledWithUnaffiliatedUser) {
  chromeos::MissiveClientTestObserver missive_observer(
      Destination::PERIPHERAL_EVENTS);

  EnableUsbPolicy();

  LoginUnaffiliatedUser();

  EmitUsbAddEventForTesting();

  ::content::RunAllTasksUntilIdle();

  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
}

IN_PROC_BROWSER_TEST_F(
    UsbEventsBrowserTest,
    NoUsbEventsOrTelemetryWhenPolicyDisabledWithAffiliatedUser) {
  chromeos::MissiveClientTestObserver missive_observer(
      Destination::PERIPHERAL_EVENTS);

  // Setup fake telemetry.
  auto usb_telemetry = CreateUsbTelemetry();
  cros_healthd::FakeCrosHealthd::Get()->SetProbeTelemetryInfoResponseForTesting(
      usb_telemetry);

  DisableUsbPolicy();

  LoginAffiliatedUser();

  EmitUsbAddEventForTesting();

  ::content::RunAllTasksUntilIdle();

  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
}

IN_PROC_BROWSER_TEST_F(
    UsbEventsBrowserTest,
    NoUsbEventsOrTelemetryWhenPolicyDisabledWithUnaffiliatedUser) {
  chromeos::MissiveClientTestObserver missive_observer(
      Destination::PERIPHERAL_EVENTS);

  // Setup fake telemetry.
  auto usb_telemetry = CreateUsbTelemetry();
  cros_healthd::FakeCrosHealthd::Get()->SetProbeTelemetryInfoResponseForTesting(
      usb_telemetry);

  DisableUsbPolicy();

  LoginUnaffiliatedUser();

  EmitUsbAddEventForTesting();

  ::content::RunAllTasksUntilIdle();

  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
}

}  // namespace
}  // namespace reporting
