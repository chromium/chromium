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
            api::os_events::AudioJackEventState::kNone);

  EXPECT_EQ(Convert(crosapi::mojom::TelemetryAudioJackEventInfo::State::kAdd),
            api::os_events::AudioJackEventState::kAdd);

  EXPECT_EQ(
      Convert(crosapi::mojom::TelemetryAudioJackEventInfo::State::kRemove),
      api::os_events::AudioJackEventState::kRemove);
}

TEST(TelemetryExtensionEventsApiConvertersUnitTest, ConvertAudioJackEventInfo) {
  auto input = crosapi::mojom::TelemetryAudioJackEventInfo::New();
  input->state = crosapi::mojom::TelemetryAudioJackEventInfo::State::kAdd;

  auto result =
      ConvertEventPtr<api::os_events::AudioJackEventInfo>(std::move(input));

  EXPECT_EQ(result.event_state, api::os_events::AudioJackEventState::kAdd);
}

}  // namespace chromeos::converters
