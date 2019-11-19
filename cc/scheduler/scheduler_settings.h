// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_SCHEDULER_SETTINGS_H_
#define CC_SCHEDULER_SCHEDULER_SETTINGS_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/cc_export.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace cc {

class CC_EXPORT SchedulerSettings {
 public:
  SchedulerSettings();
  SchedulerSettings(const SchedulerSettings& other);
  ~SchedulerSettings();

  // Whether a BeginMainFrame should be issued while there is a pending-tree
  // still waiting to be activated. This is disabled by default for the UI
  // compositor, and enabled for renderers (unless there are too few cores).
  bool main_frame_before_activation_enabled = false;

  // Whether commits should happen directly to the active tree, skipping the
  // pending tree. This is turned on only for the UI compositor (and in some
  // tests).
  bool commit_to_active_tree = false;

  // This is enabled for android-webview.
  bool using_synchronous_renderer_compositor = false;

  // This is used to determine whether some begin-frames should be skipped (both
  // in the main-thread and the compositor-thread) if previous frames have had
  // high latency.
  // It is enabled by default on desktop platforms, and has been recently
  // disabled by default on android. It may be disabled on all platforms. See
  // more in https://crbug.com/933846
  bool enable_latency_recovery = true;

  // Turning this on effectively disables pipelining of compositor frame
  // production stages by waiting for each stage to complete before producing
  // the frame. Turning this on also disables latency-recovery. This is enabled
  // for headless-mode and some tests, and disabled elsewhere by default.
  bool wait_for_all_pipeline_stages_before_draw = false;

  int maximum_number_of_failed_draws_before_draw_is_forced = 3;

  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsValue() const;
};

}  // namespace cc

#endif  // CC_SCHEDULER_SCHEDULER_SETTINGS_H_
