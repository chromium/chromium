// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/external_display/display_events_observer.h"

#include <sys/types.h>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_info_metric_sampler_test_utils.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/nullable_primitives.mojom.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

using ::ash::cros_healthd::mojom::EventCategoryEnum;
using ::ash::cros_healthd::mojom::EventInfo;
using ::ash::cros_healthd::mojom::ExternalDisplayEventInfo;
using ::ash::cros_healthd::mojom::ExternalDisplayInfoPtr;
using ::testing::StrEq;

static constexpr int32_t kTestDisplayWidth = 1920;
static constexpr int32_t kTestDisplayHeight = 1080;
static constexpr int32_t kTestModelId = 12345;
static constexpr char kTestManufacturer[] = "TestManufacturer";
static constexpr char kTestDisplayName[] = "TestDisplayName";
static constexpr uint32_t kTestSerialNumber = 2048123;
static constexpr char kTestEdidVersion[] = "v2.0";
static constexpr int32_t kTestManufactureYear = 2020;

static constexpr int kExpectedDisplayDevicesSize = 1;
static constexpr int kIndexOfDisplayDevice = 0;

class DisplayEventsObserverTest : public ::testing::Test {
 public:
  DisplayEventsObserverTest() = default;
  DisplayEventsObserverTest(const DisplayEventsObserverTest&) = delete;
  DisplayEventsObserverTest& operator=(const DisplayEventsObserverTest&) =
      delete;
  ~DisplayEventsObserverTest() override = default;

  // ::testing::Test:
  void SetUp() override { ::ash::cros_healthd::FakeCrosHealthd::Initialize(); }
  void TearDown() override { ::ash::cros_healthd::FakeCrosHealthd::Shutdown(); }

 protected:
  ExternalDisplayInfoPtr CreateTestDisplayInfo() {
    return reporting::test::CreateExternalDisplay(
        kTestDisplayWidth, kTestDisplayHeight, /*resolution_horizontal=*/500,
        /*resolution_vertical=*/500,
        /*refresh_rate=*/60.0, kTestManufacturer, kTestModelId,
        kTestManufactureYear, kTestDisplayName, kTestEdidVersion,
        kTestSerialNumber);
  }

  void VerifyDisplayDevice(const DisplayDevice& display_device) {
    EXPECT_FALSE(display_device.is_internal());
    EXPECT_EQ(display_device.display_width(), kTestDisplayWidth);
    EXPECT_EQ(display_device.display_height(), kTestDisplayHeight);
    EXPECT_EQ(display_device.model_id(), kTestModelId);
    EXPECT_THAT(display_device.manufacturer(), StrEq(kTestManufacturer));
    EXPECT_THAT(display_device.display_name(), StrEq(kTestDisplayName));
    EXPECT_EQ(display_device.serial_number(), kTestSerialNumber);
    EXPECT_EQ(display_device.edid_version(), kTestEdidVersion);
    EXPECT_EQ(display_device.manufacture_year(), kTestManufactureYear);
  }

  void RunExternalDisplayOnEventCommon(
      ExternalDisplayEventInfo::State display_event_state,
      MetricEventType expected_metric_event_type) {
    MetricData metric_data;
    DisplayEventsObserver display_observer;

    auto cb = base::BindLambdaForTesting(
        [&](MetricData md) { metric_data = std::move(md); });

    display_observer.SetOnEventObservedCallback(std::move(cb));
    display_observer.SetReportingEnabled(true);

    display_observer.OnEvent(
        EventInfo::NewExternalDisplayEventInfo(ExternalDisplayEventInfo::New(
            display_event_state, CreateTestDisplayInfo())));

    ASSERT_TRUE(metric_data.has_event_data());
    EXPECT_EQ(metric_data.event_data().type(), expected_metric_event_type);

    ASSERT_TRUE(metric_data.has_info_data());
    ASSERT_TRUE(metric_data.info_data().has_display_info());
    ASSERT_EQ(metric_data.info_data().display_info().display_device_size(),
              kExpectedDisplayDevicesSize);

    const DisplayDevice& display_device =
        metric_data.info_data().display_info().display_device(
            kIndexOfDisplayDevice);
    VerifyDisplayDevice(display_device);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
};

TEST_F(DisplayEventsObserverTest, ExternalDisplayOnAdd) {
  RunExternalDisplayOnEventCommon(ExternalDisplayEventInfo::State::kAdd,
                                  MetricEventType::EXTERNAL_DISPLAY_CONNECTED);
}

TEST_F(DisplayEventsObserverTest, ExternalDisplayOnRemove) {
  RunExternalDisplayOnEventCommon(
      ExternalDisplayEventInfo::State::kRemove,
      MetricEventType::EXTERNAL_DISPLAY_DISCONNECTED);
}

TEST_F(DisplayEventsObserverTest, ExternalDisplayOnAddUsingFakeCrosHealthd) {
  test::TestEvent<MetricData> result_metric_data;
  DisplayEventsObserver display_observer;

  display_observer.SetOnEventObservedCallback(
      result_metric_data.repeating_cb());
  display_observer.SetReportingEnabled(true);

  ::ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
      EventCategoryEnum::kExternalDisplay,
      EventInfo::NewExternalDisplayEventInfo(ExternalDisplayEventInfo::New(
          ExternalDisplayEventInfo::State::kAdd, CreateTestDisplayInfo())));

  const auto metric_data = result_metric_data.result();
  ASSERT_TRUE(metric_data.has_event_data());
  EXPECT_EQ(metric_data.event_data().type(),
            MetricEventType::EXTERNAL_DISPLAY_CONNECTED);
  ASSERT_TRUE(metric_data.has_info_data());
  ASSERT_TRUE(metric_data.info_data().has_display_info());
  ASSERT_EQ(metric_data.info_data().display_info().display_device_size(),
            kExpectedDisplayDevicesSize);
  const DisplayDevice& display_device =
      metric_data.info_data().display_info().display_device(
          kIndexOfDisplayDevice);
  VerifyDisplayDevice(display_device);
}

// Tests that non-display events are ignored.
TEST_F(DisplayEventsObserverTest, IgnoreNonDisplayEvents) {
  bool callback_called = false;
  DisplayEventsObserver display_observer;

  auto cb = base::BindLambdaForTesting(
      [&](MetricData md) { callback_called = true; });
  display_observer.SetOnEventObservedCallback(std::move(cb));
  display_observer.SetReportingEnabled(true);
  display_observer.OnEvent(EventInfo::NewBluetoothEventInfo(nullptr));

  EXPECT_FALSE(callback_called);
}

// Tests that display events with null display_info are ignored.
TEST_F(DisplayEventsObserverTest, IgnoreDisplayEventWithNullDisplayInfo) {
  bool callback_called = false;
  DisplayEventsObserver display_observer;

  auto cb = base::BindLambdaForTesting(
      [&](MetricData md) { callback_called = true; });

  display_observer.SetOnEventObservedCallback(std::move(cb));
  display_observer.SetReportingEnabled(true);
  display_observer.OnEvent(
      EventInfo::NewExternalDisplayEventInfo(ExternalDisplayEventInfo::New(
          ExternalDisplayEventInfo::State::kAdd, /*display_info=*/nullptr)));

  EXPECT_FALSE(callback_called);
}

}  // namespace
}  // namespace reporting
