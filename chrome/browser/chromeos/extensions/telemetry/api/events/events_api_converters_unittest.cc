// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/events_api_converters.h"

#include "chrome/common/chromeos/extensions/api/events.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::converters {

namespace {

namespace cx_events = ::chromeos::api::os_events;
namespace crosapi = ::crosapi::mojom;

}  // namespace

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertAudioJackState) {
  EXPECT_EQ(
      Convert(crosapi::TelemetryAudioJackEventInfo::State::kUnmappedEnumField),
      cx_events::AudioJackEvent::kNone);

  EXPECT_EQ(Convert(crosapi::TelemetryAudioJackEventInfo::State::kAdd),
            cx_events::AudioJackEvent::kConnected);

  EXPECT_EQ(Convert(crosapi::TelemetryAudioJackEventInfo::State::kRemove),
            cx_events::AudioJackEvent::kDisconnected);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest,
     ConvertAudioJackDeviceType) {
  EXPECT_EQ(
      Convert(
          crosapi::TelemetryAudioJackEventInfo::DeviceType::kUnmappedEnumField),
      cx_events::AudioJackDeviceType::kNone);

  EXPECT_EQ(
      Convert(crosapi::TelemetryAudioJackEventInfo::DeviceType::kHeadphone),
      cx_events::AudioJackDeviceType::kHeadphone);

  EXPECT_EQ(
      Convert(crosapi::TelemetryAudioJackEventInfo::DeviceType::kMicrophone),
      cx_events::AudioJackDeviceType::kMicrophone);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertLidState) {
  EXPECT_EQ(Convert(crosapi::TelemetryLidEventInfo::State::kUnmappedEnumField),
            cx_events::LidEvent::kNone);

  EXPECT_EQ(Convert(crosapi::TelemetryLidEventInfo::State::kClosed),
            cx_events::LidEvent::kClosed);

  EXPECT_EQ(Convert(crosapi::TelemetryLidEventInfo::State::kOpened),
            cx_events::LidEvent::kOpened);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertUsbState) {
  EXPECT_EQ(Convert(crosapi::TelemetryUsbEventInfo::State::kUnmappedEnumField),
            cx_events::UsbEvent::kNone);

  EXPECT_EQ(Convert(crosapi::TelemetryUsbEventInfo::State::kAdd),
            cx_events::UsbEvent::kConnected);

  EXPECT_EQ(Convert(crosapi::TelemetryUsbEventInfo::State::kRemove),
            cx_events::UsbEvent::kDisconnected);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertSdCardState) {
  EXPECT_EQ(
      Convert(crosapi::TelemetrySdCardEventInfo::State::kUnmappedEnumField),
      cx_events::SdCardEvent::kNone);

  EXPECT_EQ(Convert(crosapi::TelemetrySdCardEventInfo::State::kAdd),
            cx_events::SdCardEvent::kConnected);

  EXPECT_EQ(Convert(crosapi::TelemetrySdCardEventInfo::State::kRemove),
            cx_events::SdCardEvent::kDisconnected);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertEventCategoryEnum) {
  EXPECT_EQ(Convert(cx_events::EventCategory::kNone),
            crosapi::TelemetryEventCategoryEnum::kUnmappedEnumField);

  EXPECT_EQ(Convert(cx_events::EventCategory::kAudioJack),
            crosapi::TelemetryEventCategoryEnum::kAudioJack);

  EXPECT_EQ(Convert(cx_events::EventCategory::kLid),
            crosapi::TelemetryEventCategoryEnum::kLid);

  EXPECT_EQ(Convert(cx_events::EventCategory::kUsb),
            crosapi::TelemetryEventCategoryEnum::kUsb);

  EXPECT_EQ(Convert(cx_events::EventCategory::kSdCard),
            crosapi::TelemetryEventCategoryEnum::kSdCard);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertAudioJackEventInfo) {
  auto input = crosapi::TelemetryAudioJackEventInfo::New();
  input->state = crosapi::TelemetryAudioJackEventInfo::State::kAdd;
  input->device_type =
      crosapi::TelemetryAudioJackEventInfo::DeviceType::kHeadphone;

  auto result =
      ConvertStructPtr<cx_events::AudioJackEventInfo>(std::move(input));

  EXPECT_EQ(result.event, cx_events::AudioJackEvent::kConnected);
  EXPECT_EQ(result.device_type, cx_events::AudioJackDeviceType::kHeadphone);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertLidEventInfo) {
  auto input = crosapi::TelemetryLidEventInfo::New();
  input->state = crosapi::TelemetryLidEventInfo::State::kOpened;

  auto result = ConvertStructPtr<cx_events::LidEventInfo>(std::move(input));

  EXPECT_EQ(result.event, cx_events::LidEvent::kOpened);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertUsbEventInfo) {
  auto input = crosapi::TelemetryUsbEventInfo::New();
  std::vector<std::string> categories = {"category1", "category2"};
  input->state = crosapi::TelemetryUsbEventInfo::State::kAdd;
  input->vendor = "test_vendor";
  input->name = "test_name";
  input->vid = 1;
  input->pid = 2;
  input->categories = categories;

  auto result = ConvertStructPtr<cx_events::UsbEventInfo>(std::move(input));

  EXPECT_EQ(result.event, cx_events::UsbEvent::kConnected);
  EXPECT_EQ(result.vendor, "test_vendor");
  EXPECT_EQ(result.name, "test_name");
  EXPECT_EQ(result.vid, 1);
  EXPECT_EQ(result.pid, 2);
  EXPECT_EQ(result.categories, categories);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertSdCardEventInfo) {
  auto input = crosapi::TelemetrySdCardEventInfo::New();
  input->state = crosapi::TelemetrySdCardEventInfo::State::kAdd;

  auto result = ConvertStructPtr<cx_events::SdCardEventInfo>(std::move(input));

  EXPECT_EQ(result.event, cx_events::SdCardEvent::kConnected);
}

}  // namespace chromeos::converters
