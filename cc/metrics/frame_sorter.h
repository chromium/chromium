// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_FRAME_SORTER_H_
#define CC_METRICS_FRAME_SORTER_H_

#include <stddef.h>

#include <map>
#include <optional>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "cc/cc_export.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace cc {

struct FrameInfo;

// This class is used to process the frames in order of initiation.
// So regardless of which order frames are terminated, the  callback function
// will frames sorter will br called on the frames in the order of initiation
// (e.g. frame_time of that frame).
class CC_EXPORT FrameSorter {
 public:
  class FrameState {
   public:
    void OnBegin();
    void OnAck(bool frame_is_dropped);
    void OnReset();
    bool IsComplete() const;  // Checks if all acks are received.
    bool is_dropped() const { return is_dropped_; }
    bool should_ignore() const { return should_ignore_; }

   private:
    uint16_t on_begin_counter = 0;  // Counts the number of AddNewFrame calls.
    uint16_t ack_counter = 0;       // Counts the number of acks received.
    bool is_dropped_ = false;       // Flags if any of the acks were dropped.
    bool should_ignore_ = false;    // Flags if there was a reset prior to acks.
  };

  using InOrderBeginFramesCallback =
      base::RepeatingCallback<void(const viz::BeginFrameArgs&,
                                   const FrameInfo&)>;
  explicit FrameSorter(InOrderBeginFramesCallback callback);
  ~FrameSorter();

  FrameSorter(const FrameSorter&) = delete;
  FrameSorter& operator=(const FrameSorter&) = delete;

  // The frames must be added in the correct order.
  void AddNewFrame(const viz::BeginFrameArgs& args);

  // The results can be added in any order. However, the frame must have been
  // added by an earlier call to |AddNewFrame()|.
  void AddFrameResult(const viz::BeginFrameArgs& args,
                      const FrameInfo& frame_info);

  // Check if a frame has been previously reported as dropped.
  bool IsAlreadyReportedDropped(const viz::BeginFrameId& id) const;

  void Reset();

 private:
  void FlushFrames();

  const uint64_t kPendingFramesMaxSize = 300u;

  // The callback to run for each flushed frame.
  const InOrderBeginFramesCallback flush_callback_;

  // Frames which are started.
  base::circular_deque<viz::BeginFrameArgs> pending_frames_;

  // State of each frame in terms of ack expectation.
  std::map<viz::BeginFrameId, FrameState> frame_states_;
  std::map<viz::BeginFrameId, FrameInfo> frame_infos_;

  std::optional<uint64_t> current_source_id_;
};

}  // namespace cc

#endif  // CC_METRICS_FRAME_SORTER_H_
