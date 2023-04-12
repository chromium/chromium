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

  result.event = Convert(ptr->state);

  return result;
}

}  // namespace unchecked

api::os_events::AudioJackEvent Convert(
    crosapi::mojom::TelemetryAudioJackEventInfo::State state) {
  switch (state) {
    case crosapi::mojom::TelemetryAudioJackEventInfo_State::kUnmappedEnumField:
      return api::os_events::AudioJackEvent::kNone;
    case crosapi::mojom::TelemetryAudioJackEventInfo_State::kAdd:
      return api::os_events::AudioJackEvent::kConnected;
    case crosapi::mojom::TelemetryAudioJackEventInfo_State::kRemove:
      return api::os_events::AudioJackEvent::kDisconnected;
  }
  NOTREACHED();
}

}  // namespace chromeos::converters
