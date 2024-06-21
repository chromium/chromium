// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_SCHEDULER_SETTINGS_H_
#define CC_SCHEDULER_SCHEDULER_SETTINGS_H_

#include <memory>

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

  // Whether a BeginMainFrame should be issued while there is a previous commit
  // still waiting to be processed.
  bool main_frame_before_commit_enabled = false;

  // Whether commits should happen directly to the active tree, skipping the
  // pending tree. This is turned on only for the UI compositor (and in some
  // tests).
  bool commit_to_active_tree = false;

  // This is enabled for android-webview.
  bool using_synchronous_renderer_compositor = false;

  // Turning this on effectively disables pipelining of compositor frame
  // production stages by waiting for each stage to complete before producing
  // the frame. This is enabled for headless-mode and some tests, and disabled
  // elsewhere by default.
  bool wait_for_all_pipeline_stages_before_draw = false;

  int maximum_number_of_failed_draws_before_draw_is_forced = 3;

  // Whether to disable the limit by which frames are drawn. If this is set,
  // draws are not throttled when pending frames exceed the allowed maximum, as
  // they would be under the default settings.
  bool disable_frame_rate_limit = false;

  // When true BeginImplFrameDeadlineMode::SCROLL will be enabled. This deadline
  // is used to wait for `scroll_deadline_ratio` of `BeginFramrArgs::interval`
  // for input to arrive, before attempting to draw.
  bool scroll_deadline_mode_enabled = false;
  double scroll_deadline_ratio = 0.333;

  // The number of frames to allow slow main commits to delay impl invalidation
  // frames by.
  int delay_impl_invalidation_frames = 0;

  // Whether the tree is pushing updates to a separate display tree via a
  // LayerContext, rather than drawing directly to a compositor frame.
  bool use_layer_context_for_display = false;

  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsValue() const;
};

}  // namespace cc

#endif  // CC_SCHEDULER_SCHEDULER_SETTINGS_H_
