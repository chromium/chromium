// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/events_api_converters.h"

#include "base/notreached.h"
#include "chrome/common/chromeos/extensions/api/events.h"

namespace chromeos::converters {

namespace unchecked {

api::os_events::AudioJackEventInfo UncheckedConvertPtr(
    crosapi::mojom::TelemetryAudioJackEventInfoPtr ptr) {
  api::os_events::AudioJackEventInfo result;

  result.event_state = Convert(ptr->state);

  return result;
}

}  // namespace unchecked

api::os_events::AudioJackEventState Convert(
    crosapi::mojom::TelemetryAudioJackEventInfo::State state) {
  switch (state) {
    case crosapi::mojom::TelemetryAudioJackEventInfo_State::kUnmappedEnumField:
      return api::os_events::AudioJackEventState::kNone;
    case crosapi::mojom::TelemetryAudioJackEventInfo_State::kAdd:
      return api::os_events::AudioJackEventState::kAdd;
    case crosapi::mojom::TelemetryAudioJackEventInfo_State::kRemove:
      return api::os_events::AudioJackEventState::kRemove;
  }
  NOTREACHED();
}

}  // namespace chromeos::converters
