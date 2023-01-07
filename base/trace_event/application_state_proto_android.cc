// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/application_state_proto_android.h"

namespace base {
namespace trace_event {

perfetto::protos::pbzero::ChromeApplicationStateInfo::ChromeApplicationState
ApplicationStateToTraceEnum(base::android::ApplicationState state) {
  using perfetto::protos::pbzero::ChromeApplicationStateInfo;
  switch (state) {
    case base::android::APPLICATION_STATE_UNKNOWN:
      return ChromeApplicationStateInfo::APPLICATION_STATE_UNKNOWN;
    case base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES:
      return ChromeApplicationStateInfo::
          APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES;
    case base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES:
      return ChromeApplicationStateInfo::
          APPLICATION_STATE_HAS_RUNNING_ACTIVITIES;
    case base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES:
      return ChromeApplicationStateInfo::
          APPLICATION_STATE_HAS_PAUSED_ACTIVITIES;
    case base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES:
      return ChromeApplicationStateInfo::
          APPLICATION_STATE_HAS_STOPPED_ACTIVITIES;
  }
}

}  // namespace trace_event
}  // namespace base
