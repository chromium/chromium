// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROXY_COMMON_H_
#define CC_TREES_PROXY_COMMON_H_

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/types/id_type.h"
#include "cc/cc_export.h"
#include "cc/metrics/frame_sequence_tracker_collection.h"
#include "cc/trees/begin_main_frame_trace_id.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace cc {

struct CompositorCommitData;
class MutatorEvents;

struct CC_EXPORT BeginMainFrameAndCommitState {
  BeginMainFrameAndCommitState();
  ~BeginMainFrameAndCommitState();

  viz::BeginFrameArgs begin_frame_args;
  std::unique_ptr<CompositorCommitData> commit_data;
  std::vector<std::pair<int, bool>> completed_image_decode_requests;
  std::unique_ptr<MutatorEvents> mutator_events;
  // Bit encoding of the FrameSequenceTrackerType for active trackers
  ActiveFrameSequenceTrackers active_sequence_trackers = 0;
  bool evicted_ui_resources = false;
  BeginMainFrameTraceId trace_id;
};

}  // namespace cc

#endif  // CC_TREES_PROXY_COMMON_H_
