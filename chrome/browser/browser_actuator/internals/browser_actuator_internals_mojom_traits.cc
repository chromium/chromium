// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/internals/browser_actuator_internals_mojom_traits.h"

#include "base/numerics/safe_conversions.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

using browser_actuator::SessionEventSnapshot;
using browser_actuator::SessionSnapshot;
using browser_actuator_internals::mojom::MessageDirection;

// static
MessageDirection
StructTraits<browser_actuator_internals::mojom::RecordedEventDataView,
             SessionEventSnapshot>::direction(const SessionEventSnapshot& e) {
  return e.is_downstream ? MessageDirection::kDownstream
                         : MessageDirection::kUpstream;
}

// static
bool StructTraits<browser_actuator_internals::mojom::RecordedEventDataView,
                  SessionEventSnapshot>::
    Read(browser_actuator_internals::mojom::RecordedEventDataView data,
         SessionEventSnapshot* out) {
  MessageDirection direction;
  if (!data.ReadTimestamp(&out->timestamp) || !data.ReadDirection(&direction) ||
      !data.ReadPayloadTypes(&out->payload_types) ||
      !data.ReadMessage(&out->message)) {
    return false;
  }
  out->is_downstream = direction == MessageDirection::kDownstream;
  out->message_truncated = data.message_truncated();
  return true;
}

// static
uint32_t StructTraits<
    browser_actuator_internals::mojom::SessionSummaryDataView,
    SessionSnapshot>::total_downstream_messages(const SessionSnapshot& s) {
  return base::saturated_cast<uint32_t>(s.total_downstream_messages);
}

// static
uint32_t StructTraits<
    browser_actuator_internals::mojom::SessionSummaryDataView,
    SessionSnapshot>::total_upstream_messages(const SessionSnapshot& s) {
  return base::saturated_cast<uint32_t>(s.total_upstream_messages);
}

// static
uint32_t StructTraits<browser_actuator_internals::mojom::SessionSummaryDataView,
                      SessionSnapshot>::total_events(const SessionSnapshot& s) {
  return base::saturated_cast<uint32_t>(s.total_events);
}

// static
bool StructTraits<browser_actuator_internals::mojom::SessionSummaryDataView,
                  SessionSnapshot>::
    Read(browser_actuator_internals::mojom::SessionSummaryDataView data,
         SessionSnapshot* out) {
  if (!data.ReadSessionId(&out->session_id) ||
      !data.ReadStartWallTime(&out->start_wall_time) ||
      !data.ReadEndWallTime(&out->end_wall_time) ||
      !data.ReadEvents(&out->events)) {
    return false;
  }
  out->total_downstream_messages = data.total_downstream_messages();
  out->total_upstream_messages = data.total_upstream_messages();
  out->total_events = data.total_events();
  return true;
}

}  // namespace mojo
