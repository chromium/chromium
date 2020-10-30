// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_FRAME_SORTER_H_
#define CC_METRICS_FRAME_SORTER_H_

#include <stddef.h>

#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "cc/cc_export.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace cc {

// This class is used to process the frames in order of initiation.
// So regardless of which order frames are terminated, the  callback function
// will frames sorter will br called on the frames in the order of initiation
// (e.g. frame_time of that frame).
class CC_EXPORT FrameSorter {
 public:
  using InOrderBeginFramesCallback =
      base::RepeatingCallback<void(const viz::BeginFrameArgs&,
                                   bool /*is_dropped*/)>;
  explicit FrameSorter(InOrderBeginFramesCallback callback);
  ~FrameSorter();

  FrameSorter(const FrameSorter&) = delete;
  FrameSorter& operator=(const FrameSorter&) = delete;

  // The frames must be added in the correct order.
  void AddNewFrame(const viz::BeginFrameArgs& args);

  // The results can be added in any order. However, the frame must have been
  // added by an earlier call to |AddNewFrame()|.
  void AddFrameResult(const viz::BeginFrameArgs& args, bool is_dropped);

  void Reset();

 private:
  void FlushAcks();
  bool RemoveFrameExpectingTwoAcks(const viz::BeginFrameId& frame_id);

  // The callback to run for each flushed frame.
  const InOrderBeginFramesCallback flush_callback_;

  // Frames which are started.
  std::vector<viz::BeginFrameArgs> pending_frames_;

  // Frames that expect acks (all frames in pending_frames_).
  base::flat_set<viz::BeginFrameId> frames_expecting_acks_;

  // Acked frames and their status. (True: dropped, False: not_dropped)
  base::flat_map<viz::BeginFrameId, bool> received_acks_;

  // Frames that expect two acks.
  base::flat_set<viz::BeginFrameId> frames_expecting_two_acks_;

  // Frames that need to be ignored. (They are started prior to a reset)
  base::flat_set<viz::BeginFrameId> frames_to_ignore_acks_;

  // Initial status of a frames that expect two acks.
  base::flat_map<viz::BeginFrameId, bool> received_partial_dropped_acks_;

  base::Optional<uint64_t> current_source_id_;
};

}  // namespace cc

#endif  // CC_METRICS_FRAME_SORTER_H_
