// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/events_api_converters.h"

#include "chrome/common/chromeos/extensions/api/events.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::converters {

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertAudioJackState) {
  EXPECT_EQ(Convert(crosapi::mojom::TelemetryAudioJackEventInfo::State::
                        kUnmappedEnumField),
            api::os_events::AudioJackEvent::kNone);

  EXPECT_EQ(Convert(crosapi::mojom::TelemetryAudioJackEventInfo::State::kAdd),
            api::os_events::AudioJackEvent::kConnected);

  EXPECT_EQ(
      Convert(crosapi::mojom::TelemetryAudioJackEventInfo::State::kRemove),
      api::os_events::AudioJackEvent::kDisconnected);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest,
     ConvertAudioJackDeviceType) {
  EXPECT_EQ(Convert(crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType::
                        kUnmappedEnumField),
            api::os_events::AudioJackDeviceType::kNone);

  EXPECT_EQ(
      Convert(
          crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType::kHeadphone),
      api::os_events::AudioJackDeviceType::kHeadphone);

  EXPECT_EQ(
      Convert(
          crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType::kMicrophone),
      api::os_events::AudioJackDeviceType::kMicrophone);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertLidState) {
  EXPECT_EQ(
      Convert(crosapi::mojom::TelemetryLidEventInfo::State::kUnmappedEnumField),
      api::os_events::LidEvent::kNone);

  EXPECT_EQ(Convert(crosapi::mojom::TelemetryLidEventInfo::State::kClosed),
            api::os_events::LidEvent::kClosed);

  EXPECT_EQ(Convert(crosapi::mojom::TelemetryLidEventInfo::State::kOpened),
            api::os_events::LidEvent::kOpened);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertUsbState) {
  EXPECT_EQ(
      Convert(crosapi::mojom::TelemetryUsbEventInfo::State::kUnmappedEnumField),
      api::os_events::UsbEvent::kNone);

  EXPECT_EQ(Convert(crosapi::mojom::TelemetryUsbEventInfo::State::kAdd),
            api::os_events::UsbEvent::kConnected);

  EXPECT_EQ(Convert(crosapi::mojom::TelemetryUsbEventInfo::State::kRemove),
            api::os_events::UsbEvent::kDisconnected);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertSdCardState) {
  EXPECT_EQ(
      Convert(
          crosapi::mojom::TelemetrySdCardEventInfo::State::kUnmappedEnumField),
      api::os_events::SdCardEvent::kNone);

  EXPECT_EQ(Convert(crosapi::mojom::TelemetrySdCardEventInfo::State::kAdd),
            api::os_events::SdCardEvent::kConnected);

  EXPECT_EQ(Convert(crosapi::mojom::TelemetrySdCardEventInfo::State::kRemove),
            api::os_events::SdCardEvent::kDisconnected);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertEventCategoryEnum) {
  EXPECT_EQ(Convert(api::os_events::EventCategory::kNone),
            crosapi::mojom::TelemetryEventCategoryEnum::kUnmappedEnumField);

  EXPECT_EQ(Convert(api::os_events::EventCategory::kAudioJack),
            crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack);

  EXPECT_EQ(Convert(api::os_events::EventCategory::kLid),
            crosapi::mojom::TelemetryEventCategoryEnum::kLid);

  EXPECT_EQ(Convert(api::os_events::EventCategory::kUsb),
            crosapi::mojom::TelemetryEventCategoryEnum::kUsb);

  EXPECT_EQ(Convert(api::os_events::EventCategory::kSdCard),
            crosapi::mojom::TelemetryEventCategoryEnum::kSdCard);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertAudioJackEventInfo) {
  auto input = crosapi::mojom::TelemetryAudioJackEventInfo::New();
  input->state = crosapi::mojom::TelemetryAudioJackEventInfo::State::kAdd;
  input->device_type =
      crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType::kHeadphone;

  auto result =
      ConvertStructPtr<api::os_events::AudioJackEventInfo>(std::move(input));

  EXPECT_EQ(result.event, api::os_events::AudioJackEvent::kConnected);
  EXPECT_EQ(result.device_type,
            api::os_events::AudioJackDeviceType::kHeadphone);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertLidEventInfo) {
  auto input = crosapi::mojom::TelemetryLidEventInfo::New();
  input->state = crosapi::mojom::TelemetryLidEventInfo::State::kOpened;

  auto result =
      ConvertStructPtr<api::os_events::LidEventInfo>(std::move(input));

  EXPECT_EQ(result.event, api::os_events::LidEvent::kOpened);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertUsbEventInfo) {
  auto input = crosapi::mojom::TelemetryUsbEventInfo::New();
  std::vector<std::string> categories = {"category1", "category2"};
  input->state = crosapi::mojom::TelemetryUsbEventInfo::State::kAdd;
  input->vendor = "test_vendor";
  input->name = "test_name";
  input->vid = 1;
  input->pid = 2;
  input->categories = categories;

  auto result = ConvertStructPtr<api::os_events::UsbEventInfo>(std::move(input));

  EXPECT_EQ(result.event, api::os_events::UsbEvent::kConnected);
  EXPECT_EQ(result.vendor, "test_vendor");
  EXPECT_EQ(result.name, "test_name");
  EXPECT_EQ(result.vid, 1);
  EXPECT_EQ(result.pid, 2);
  EXPECT_EQ(result.categories, categories);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertSdCardEventInfo) {
  auto input = crosapi::mojom::TelemetrySdCardEventInfo::New();
  input->state = crosapi::mojom::TelemetrySdCardEventInfo::State::kAdd;

  auto result =
      ConvertStructPtr<api::os_events::SdCardEventInfo>(std::move(input));

  EXPECT_EQ(result.event, api::os_events::SdCardEvent::kConnected);
}

}  // namespace chromeos::converters
