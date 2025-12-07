// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/user_action.h"

#include "base/metrics/metrics_hashes.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_user_event.pbzero.h"

namespace base::trace_event {

void EmitUserActionEvent(const std::string& action, TimeTicks action_time) {
  constexpr uint64_t kGlobalInstantTrackId = 0;
  TRACE_EVENT_INSTANT(
      TRACE_DISABLED_BY_DEFAULT("user_action_samples"), "UserAction",
      perfetto::NamedTrack("UserAction", 0,
                           perfetto::Track::Global(kGlobalInstantTrackId)),
      action_time, [&](perfetto::EventContext ctx) {
        perfetto::protos::pbzero::ChromeUserEvent* new_sample =
            ctx.event()->set_chrome_user_event();
        if (!ctx.ShouldFilterDebugAnnotations()) {
          new_sample->set_action(action);
        }
        new_sample->set_action_hash(base::HashMetricName(action));
      });
}

}  // namespace base::trace_event
