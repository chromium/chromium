// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_FRAME_SORTER_H_
#define CC_METRICS_FRAME_SORTER_H_

#include <stddef.h>

#include <map>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/containers/ring_buffer.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "cc/cc_export.h"
#include "cc/metrics/frame_info.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace cc {

// FrameSorterObserver class notifies registered
// observers when frames are flushed by the FrameSorter.
class FrameSorterObserver : public base::CheckedObserver {
 public:
  virtual void AddSortedFrame(const viz::BeginFrameArgs&, const FrameInfo&) = 0;
};

// This class is used to process the frames in order of initiation.
// So regardless of which order frames are terminated, the frames
// sorter will be called on the frames in the order of initiation
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

  FrameSorter();
  void AddObserver(FrameSorterObserver* frame_sorter_observer);
  void RemoveObserver(FrameSorterObserver* frame_sorter_observer);
  virtual ~FrameSorter();

  FrameSorter(const FrameSorter&) = delete;
  FrameSorter& operator=(const FrameSorter&) = delete;

  // The frames must be added in the correct order.
  virtual void AddNewFrame(const viz::BeginFrameArgs& args);

  // Called on all frames, including those before FirstContentfulPaint.
  virtual void AddFrameInfoToBuffer(const FrameInfo& frame_info);

  // The results can be added in any order. However, the frame must have been
  // added by an earlier call to |AddNewFrame()|.
  virtual void AddFrameResult(const viz::BeginFrameArgs& args,
                              const FrameInfo& frame_info);

  // Check if a frame has been previously reported as dropped.
  bool IsAlreadyReportedDropped(const viz::BeginFrameId& id) const;

  // Ring buffer which keeps a state shorthand of recently finished frames.
  typedef base::RingBuffer<FrameInfo::FrameFinalState, 180> RingBufferType;
  RingBufferType::Iterator Begin() const { return ring_buffer_.Begin(); }
  // `End()` points to the last `FrameState`, not past it.
  RingBufferType::Iterator End() const { return ring_buffer_.End(); }

  // For requesting recent frame state information.
  size_t frame_history_size() const { return ring_buffer_.BufferSize(); }
  size_t total_frames() const { return total_frames_; }
  size_t total_dropped() const { return total_dropped_; }
  size_t total_partial() const { return total_partial_; }

  uint32_t GetAverageThroughput() const;

  void Reset(bool reset_fcp);

  void OnFirstContentfulPaintReceived();

  bool first_contentful_paint_received() const {
    return first_contentful_paint_received_;
  }

  // Enable dropped frame report for ui::Compositor..
  void EnableReportForUI();

 private:
  void FlushFrames();
  base::TimeDelta ComputeCurrentWindowSize() const;
  void PopSlidingWindow(const viz::BeginFrameArgs& args);
  std::queue<std::pair<const viz::BeginFrameArgs, FrameInfo>> sliding_window_;

  const uint64_t kPendingFramesMaxSize = 300u;

  // The callback to run for each flushed frame.
  base::ObserverList<FrameSorterObserver> observers_;

  // Frames which are started.
  base::circular_deque<viz::BeginFrameArgs> pending_frames_;

  // State of each frame in terms of ack expectation.
  std::map<viz::BeginFrameId, FrameState> frame_states_;
  std::map<viz::BeginFrameId, FrameInfo> frame_infos_;

  // Ring buffer that stores the state of recently completed frames
  // and the associated counters.
  RingBufferType ring_buffer_;
  size_t total_frames_ = 0;
  size_t total_partial_ = 0;
  size_t total_dropped_ = 0;

  std::optional<uint64_t> current_source_id_;
  bool first_contentful_paint_received_ = false;
  bool report_for_ui_ = false;
  std::optional<double> sliding_window_current_percent_dropped_;
  uint32_t dropped_frame_count_in_window_ = 0;
};

}  // namespace cc

#endif  // CC_METRICS_FRAME_SORTER_H_
