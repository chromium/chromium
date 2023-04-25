// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/telemetry_extension/telemetry_event_service_converters.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_exception.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::converters {

// Tests that `ConvertEventPtr` function returns nullptr if input is
// nullptr. `ConvertEventPtr` is a template, so we can test this function
// with any valid type.
TEST(TelemetryEventServiceConvertersTest, ConvertEventPtrTakesNullPtr) {
  EXPECT_TRUE(ConvertEventPtr(cros_healthd::mojom::EventInfoPtr()).is_null());
}

TEST(TelemetryEventServiceConvertersTest,
     ConvertTelemetryAudioJackEventInfo_State) {
  EXPECT_EQ(
      Convert(
          cros_healthd::mojom::AudioJackEventInfo::State::kUnmappedEnumField),
      crosapi::mojom::TelemetryAudioJackEventInfo::State::kUnmappedEnumField);

  EXPECT_EQ(Convert(cros_healthd::mojom::AudioJackEventInfo::State::kAdd),
            crosapi::mojom::TelemetryAudioJackEventInfo::State::kAdd);

  EXPECT_EQ(Convert(cros_healthd::mojom::AudioJackEventInfo::State::kRemove),
            crosapi::mojom::TelemetryAudioJackEventInfo::State::kRemove);
}

TEST(TelemetryEventServiceConvertersTest,
     ConvertTelemetryAudioJackEventInfo_DeviceType) {
  EXPECT_EQ(Convert(cros_healthd::mojom::AudioJackEventInfo::DeviceType::
                        kUnmappedEnumField),
            crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType::
                kUnmappedEnumField);

  EXPECT_EQ(
      Convert(cros_healthd::mojom::AudioJackEventInfo::DeviceType::kHeadphone),
      crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType::kHeadphone);

  EXPECT_EQ(
      Convert(cros_healthd::mojom::AudioJackEventInfo::DeviceType::kMicrophone),
      crosapi::mojom::TelemetryAudioJackEventInfo::DeviceType::kMicrophone);
}

TEST(TelemetryEventServiceConvertersTest, ConvertTelemetryLidEventInfo_State) {
  EXPECT_EQ(
      Convert(cros_healthd::mojom::LidEventInfo::State::kUnmappedEnumField),
      crosapi::mojom::TelemetryLidEventInfo::State::kUnmappedEnumField);

  EXPECT_EQ(Convert(cros_healthd::mojom::LidEventInfo::State::kClosed),
            crosapi::mojom::TelemetryLidEventInfo::State::kClosed);

  EXPECT_EQ(Convert(cros_healthd::mojom::LidEventInfo::State::kOpened),
            crosapi::mojom::TelemetryLidEventInfo::State::kOpened);
}

TEST(TelemetryEventServiceConvertersTest,
     ConvertTelemetryExtensionExceptionReason) {
  EXPECT_EQ(
      Convert(cros_healthd::mojom::Exception_Reason::kUnmappedEnumField),
      crosapi::mojom::TelemetryExtensionException::Reason::kUnmappedEnumField);

  EXPECT_EQ(
      Convert(
          cros_healthd::mojom::Exception_Reason::kMojoDisconnectWithoutReason),
      crosapi::mojom::TelemetryExtensionException::Reason::
          kMojoDisconnectWithoutReason);

  EXPECT_EQ(Convert(cros_healthd::mojom::Exception_Reason::kUnexpected),
            crosapi::mojom::TelemetryExtensionException::Reason::kUnexpected);

  EXPECT_EQ(Convert(cros_healthd::mojom::Exception_Reason::kUnsupported),
            crosapi::mojom::TelemetryExtensionException::Reason::kUnsupported);
}

TEST(TelemetryEventServiceConvertersTest, ConvertTelemetryEventCategoryEnum) {
  EXPECT_EQ(
      Convert(crosapi::mojom::TelemetryEventCategoryEnum::kUnmappedEnumField),
      cros_healthd::mojom::EventCategoryEnum::kUnmappedEnumField);

  EXPECT_EQ(Convert(crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack),
            cros_healthd::mojom::EventCategoryEnum::kAudioJack);

  EXPECT_EQ(Convert(crosapi::mojom::TelemetryEventCategoryEnum::kLid),
            cros_healthd::mojom::EventCategoryEnum::kLid);
}

TEST(TelemetryEventServiceConvertersTest,
     ConvertTelemetryAudioJackEventInfoPtr) {
  auto input = cros_healthd::mojom::AudioJackEventInfo::New();
  input->state = cros_healthd::mojom::AudioJackEventInfo::State::kAdd;

  EXPECT_EQ(ConvertEventPtr(std::move(input)),
            crosapi::mojom::TelemetryAudioJackEventInfo::New(
                crosapi::mojom::TelemetryAudioJackEventInfo::State::kAdd));
}

TEST(TelemetryEventServiceConvertersTest, ConvertTelemetryLidEventInfoPtr) {
  auto input = cros_healthd::mojom::LidEventInfo::New();
  input->state = cros_healthd::mojom::LidEventInfo::State::kClosed;

  EXPECT_EQ(ConvertEventPtr(std::move(input)),
            crosapi::mojom::TelemetryLidEventInfo::New(
                crosapi::mojom::TelemetryLidEventInfo::State::kClosed));
}

TEST(TelemetryEventServiceConvertersTest, ConvertTelemetryExtensionException) {
  constexpr char kDebugMessage[] = "TestMessage";

  auto input = cros_healthd::mojom::Exception::New();
  input->reason = cros_healthd::mojom::Exception::Reason::kUnexpected;
  input->debug_message = kDebugMessage;

  auto result = ConvertEventPtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->reason,
            crosapi::mojom::TelemetryExtensionException::Reason::kUnexpected);
  EXPECT_EQ(result->debug_message, kDebugMessage);
}

TEST(TelemetryEventServiceConvertersTest,
     ConvertTelemetryExtensionSupportedPtr) {
  EXPECT_EQ(ConvertEventPtr(cros_healthd::mojom::Supported::New()),
            crosapi::mojom::TelemetryExtensionSupported::New());
}

TEST(TelemetryEventServiceConvertersTest,
     ConvertTelemetryExtensionUnsupportedReasonPtr) {
  EXPECT_EQ(
      ConvertEventPtr(
          cros_healthd::mojom::UnsupportedReason::NewUnmappedUnionField(9)),
      crosapi::mojom::TelemetryExtensionUnsupportedReason::
          NewUnmappedUnionField(9));
}

TEST(TelemetryEventServiceConvertersTest,
     ConvertTelemetryExtensionUnsupportedPtr) {
  constexpr char kDebugMsg[] = "Test";
  constexpr uint8_t kUnmappedUnionField = 4;

  auto input = cros_healthd::mojom::Unsupported::New();
  input->debug_message = kDebugMsg;
  input->reason = cros_healthd::mojom::UnsupportedReason::NewUnmappedUnionField(
      kUnmappedUnionField);

  auto result = ConvertEventPtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->debug_message, kDebugMsg);
  EXPECT_EQ(result->reason,
            crosapi::mojom::TelemetryExtensionUnsupportedReason::
                NewUnmappedUnionField(kUnmappedUnionField));
}

TEST(TelemetryEventServiceConvertersTest,
     ConvertTelemetryExtensionSupportStatusPtr) {
  constexpr char kDebugMsg[] = "Test";
  constexpr uint8_t kUnmappedUnionField = 4;

  EXPECT_EQ(ConvertEventPtr(cros_healthd::mojom::SupportStatus::NewSupported(
                cros_healthd::mojom::Supported::New())),
            crosapi::mojom::TelemetryExtensionSupportStatus::NewSupported(
                crosapi::mojom::TelemetryExtensionSupported::New()));

  EXPECT_EQ(
      ConvertEventPtr(cros_healthd::mojom::SupportStatus::NewUnmappedUnionField(
          kUnmappedUnionField)),
      crosapi::mojom::TelemetryExtensionSupportStatus::NewUnmappedUnionField(
          kUnmappedUnionField));

  auto unsupported = cros_healthd::mojom::Unsupported::New();
  unsupported->debug_message = kDebugMsg;
  unsupported->reason =
      cros_healthd::mojom::UnsupportedReason::NewUnmappedUnionField(
          kUnmappedUnionField);

  auto unsupported_result =
      ConvertEventPtr(cros_healthd::mojom::SupportStatus::NewUnsupported(
          std::move(unsupported)));

  ASSERT_TRUE(unsupported_result->is_unsupported());
  EXPECT_EQ(unsupported_result->get_unsupported()->debug_message, kDebugMsg);
  EXPECT_EQ(unsupported_result->get_unsupported()->reason,
            crosapi::mojom::TelemetryExtensionUnsupportedReason::
                NewUnmappedUnionField(kUnmappedUnionField));

  auto exception = cros_healthd::mojom::Exception::New();
  exception->reason = cros_healthd::mojom::Exception::Reason::kUnexpected;
  exception->debug_message = kDebugMsg;

  auto exception_result = ConvertEventPtr(
      cros_healthd::mojom::SupportStatus::NewException(std::move(exception)));

  ASSERT_TRUE(exception_result->is_exception());
  EXPECT_EQ(exception_result->get_exception()->reason,
            crosapi::mojom::TelemetryExtensionException::Reason::kUnexpected);
  EXPECT_EQ(exception_result->get_exception()->debug_message, kDebugMsg);
}

TEST(TelemetryEventServiceConvertersTest, ConvertTelemetryEventInfoPtr) {
  auto audio_jack_info = cros_healthd::mojom::AudioJackEventInfo::New();
  audio_jack_info->state = cros_healthd::mojom::AudioJackEventInfo::State::kAdd;

  auto input = cros_healthd::mojom::EventInfo::NewAudioJackEventInfo(
      std::move(audio_jack_info));

  EXPECT_EQ(ConvertEventPtr(std::move(input)),
            crosapi::mojom::TelemetryEventInfo::NewAudioJackEventInfo(
                crosapi::mojom::TelemetryAudioJackEventInfo::New(
                    crosapi::mojom::TelemetryAudioJackEventInfo::State::kAdd)));

  auto illegal_info = cros_healthd::mojom::ThunderboltEventInfo::New();
  auto illegal_input = cros_healthd::mojom::EventInfo::NewThunderboltEventInfo(
      std::move(illegal_info));

  EXPECT_TRUE(ConvertEventPtr(std::move(illegal_input)).is_null());
}

}  // namespace ash::converters
