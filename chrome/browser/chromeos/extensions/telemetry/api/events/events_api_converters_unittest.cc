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

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertEventCategoryEnum) {
  EXPECT_EQ(Convert(api::os_events::EventCategory::kNone),
            crosapi::mojom::TelemetryEventCategoryEnum::kUnmappedEnumField);

  EXPECT_EQ(Convert(api::os_events::EventCategory::kAudioJack),
            crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack);

  EXPECT_EQ(Convert(api::os_events::EventCategory::kLid),
            crosapi::mojom::TelemetryEventCategoryEnum::kLid);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertAudioJackEventInfo) {
  auto input = crosapi::mojom::TelemetryAudioJackEventInfo::New();
  input->state = crosapi::mojom::TelemetryAudioJackEventInfo::State::kAdd;
  input->device_type =
      crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType::kHeadphone;

  auto result =
      ConvertEventPtr<api::os_events::AudioJackEventInfo>(std::move(input));

  EXPECT_EQ(result.event, api::os_events::AudioJackEvent::kConnected);
  EXPECT_EQ(result.device_type,
            api::os_events::AudioJackDeviceType::kHeadphone);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertLidEventInfo) {
  auto input = crosapi::mojom::TelemetryLidEventInfo::New();
  input->state = crosapi::mojom::TelemetryLidEventInfo::State::kOpened;

  auto result = ConvertEventPtr<api::os_events::LidEventInfo>(std::move(input));

  EXPECT_EQ(result.event, api::os_events::LidEvent::kOpened);
}

}  // namespace chromeos::converters
