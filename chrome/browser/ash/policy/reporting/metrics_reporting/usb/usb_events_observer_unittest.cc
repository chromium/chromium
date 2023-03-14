// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/usb/usb_events_observer.h"

#include <sys/types.h>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

using ::ash::cros_healthd::mojom::UsbEventInfo;
using ::ash::cros_healthd::mojom::UsbEventInfoPtr;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::StrEq;

static constexpr int32_t kTestVid = 0xffee;
static constexpr int32_t kTestPid = 0x0;
static constexpr char kTestName[] = "TestName";
static constexpr char kTestVendor[] = "TestVendor";
static constexpr char kTestCategory1[] = "TestCategory1";
static constexpr char kTestCategory2[] = "TestCategory2";
const std::vector<std::string> kTestCategories = {kTestCategory1,
                                                  kTestCategory2};

class UsbEventsObserverTest : public ::testing::Test {
 public:
  UsbEventsObserverTest() = default;

  UsbEventsObserverTest(const UsbEventsObserverTest&) = delete;
  UsbEventsObserverTest& operator=(const UsbEventsObserverTest&) = delete;

  ~UsbEventsObserverTest() override = default;

  void SetUp() override { ::ash::cros_healthd::FakeCrosHealthd::Initialize(); }

  void TearDown() override { ::ash::cros_healthd::FakeCrosHealthd::Shutdown(); }

 private:
  base::test::TaskEnvironment task_environment_;
  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
};

TEST_F(UsbEventsObserverTest, UsbOnRemove) {
  MetricData metric_data;
  UsbEventsObserver usb_observer;
  constexpr int kExpectedUsbTelemetrySize = 1;
  constexpr int kIndexOfUsbTelemetry = 0;

  auto cb = base::BindLambdaForTesting(
      [&](MetricData md) { metric_data = std::move(md); });

  usb_observer.SetOnEventObservedCallback(std::move(cb));
  usb_observer.SetReportingEnabled(true);
  usb_observer.OnEvent(
      ::ash::cros_healthd::mojom::EventInfo::NewUsbEventInfo(UsbEventInfo::New(
          kTestVendor, kTestName, kTestVid, kTestPid, kTestCategories,
          ::ash::cros_healthd::mojom::UsbEventInfo::State::kRemove)));

  UsbTelemetry usb_telemetry =
      metric_data.telemetry_data().peripherals_telemetry().usb_telemetry(
          kIndexOfUsbTelemetry);

  ASSERT_TRUE(metric_data.has_event_data());
  ASSERT_EQ(
      metric_data.telemetry_data().peripherals_telemetry().usb_telemetry_size(),
      kExpectedUsbTelemetrySize);

  EXPECT_TRUE(usb_telemetry.has_name());
  EXPECT_TRUE(usb_telemetry.has_pid());
  EXPECT_TRUE(usb_telemetry.has_vendor());
  EXPECT_TRUE(usb_telemetry.has_vid());
  EXPECT_THAT(usb_telemetry.name(), StrEq(kTestName));
  EXPECT_THAT(usb_telemetry.pid(), Eq(kTestPid));
  EXPECT_THAT(usb_telemetry.vendor(), StrEq(kTestVendor));
  EXPECT_THAT(usb_telemetry.vid(), Eq(kTestVid));
  EXPECT_EQ(metric_data.event_data().type(), MetricEventType::USB_REMOVED);
  ASSERT_EQ(static_cast<size_t>(usb_telemetry.categories().size()),
            kTestCategories.size());

  for (size_t i = 0; i < kTestCategories.size(); ++i) {
    EXPECT_THAT(usb_telemetry.categories()[i], StrEq(kTestCategories[i]));
  }
}

TEST_F(UsbEventsObserverTest, UsbOnAdd) {
  MetricData metric_data;
  UsbEventsObserver usb_observer;
  constexpr int kExpectedUsbTelemetrySize = 1;
  constexpr int kIndexOfUsbTelemetry = 0;

  auto cb = base::BindLambdaForTesting(
      [&](MetricData md) { metric_data = std::move(md); });

  usb_observer.SetOnEventObservedCallback(std::move(cb));
  usb_observer.SetReportingEnabled(true);
  usb_observer.OnEvent(
      ::ash::cros_healthd::mojom::EventInfo::NewUsbEventInfo(UsbEventInfo::New(
          kTestVendor, kTestName, kTestVid, kTestPid, kTestCategories,
          ::ash::cros_healthd::mojom::UsbEventInfo::State::kAdd)));

  UsbTelemetry usb_telemetry =
      metric_data.telemetry_data().peripherals_telemetry().usb_telemetry().at(
          kIndexOfUsbTelemetry);

  ASSERT_TRUE(metric_data.has_event_data());
  ASSERT_EQ(
      metric_data.telemetry_data().peripherals_telemetry().usb_telemetry_size(),
      kExpectedUsbTelemetrySize);

  EXPECT_TRUE(usb_telemetry.has_name());
  EXPECT_TRUE(usb_telemetry.has_pid());
  EXPECT_TRUE(usb_telemetry.has_vendor());
  EXPECT_TRUE(usb_telemetry.has_vid());
  EXPECT_THAT(usb_telemetry.name(), StrEq(kTestName));
  EXPECT_THAT(usb_telemetry.pid(), Eq(kTestPid));
  EXPECT_THAT(usb_telemetry.vendor(), StrEq(kTestVendor));
  EXPECT_THAT(usb_telemetry.vid(), Eq(kTestVid));
  EXPECT_EQ(metric_data.event_data().type(), MetricEventType::USB_ADDED);
  ASSERT_EQ(static_cast<size_t>(usb_telemetry.categories().size()),
            kTestCategories.size());

  for (size_t i = 0; i < kTestCategories.size(); ++i) {
    EXPECT_THAT(usb_telemetry.categories()[i], StrEq(kTestCategories[i]));
  }
}

TEST_F(UsbEventsObserverTest, UsbOnAddUsingFakeCrosHealthd) {
  test::TestEvent<MetricData> result_metric_data;
  UsbEventsObserver usb_observer;
  constexpr int kExpectedUsbTelemetrySize = 1;
  constexpr int kIndexOfUsbTelemetry = 0;

  usb_observer.SetOnEventObservedCallback(result_metric_data.repeating_cb());
  usb_observer.SetReportingEnabled(true);

  ::ash::cros_healthd::mojom::UsbEventInfo info;
  info.state = ::ash::cros_healthd::mojom::UsbEventInfo::State::kAdd;
  ::ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
      ::ash::cros_healthd::mojom::EventCategoryEnum::kUsb,
      ::ash::cros_healthd::mojom::EventInfo::NewUsbEventInfo(info.Clone()));

  const auto metric_data = result_metric_data.result();

  UsbTelemetry usb_telemetry =
      metric_data.telemetry_data().peripherals_telemetry().usb_telemetry(
          kIndexOfUsbTelemetry);

  ASSERT_TRUE(metric_data.has_event_data());
  ASSERT_EQ(
      metric_data.telemetry_data().peripherals_telemetry().usb_telemetry_size(),
      kExpectedUsbTelemetrySize);
  EXPECT_TRUE(usb_telemetry.has_name());
  EXPECT_TRUE(usb_telemetry.has_pid());
  EXPECT_TRUE(usb_telemetry.has_vendor());
  EXPECT_TRUE(usb_telemetry.has_vid());
  EXPECT_THAT(usb_telemetry.categories(), IsEmpty());
  EXPECT_EQ(metric_data.event_data().type(), MetricEventType::USB_ADDED);
}

}  // namespace
}  // namespace reporting
