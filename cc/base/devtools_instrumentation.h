// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_DEVTOOLS_INSTRUMENTATION_H_
#define CC_BASE_DEVTOOLS_INSTRUMENTATION_H_

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "base/trace_event/typed_macros.h"
#include "cc/base/base_export.h"

namespace cc {
namespace devtools_instrumentation {

namespace internal {
struct CC_BASE_EXPORT CategoryName {
  // Put these strings into a struct to allow external linkage.
  static constexpr const char kTimeline[] =
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline");
  static constexpr const char kTimelineFrame[] =
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.frame");
};

CC_BASE_EXPORT extern const char kData[];
CC_BASE_EXPORT extern const char kFrameId[];
CC_BASE_EXPORT extern const char kLayerId[];
CC_BASE_EXPORT extern const char kLayerTreeId[];
CC_BASE_EXPORT extern const char kPixelRefId[];
CC_BASE_EXPORT extern const char kFrameSequenceNumber[];
CC_BASE_EXPORT extern const char kHasPartialUpdate[];

CC_BASE_EXPORT extern const char kImageDecodeTask[];
CC_BASE_EXPORT extern const char kBeginFrame[];
CC_BASE_EXPORT extern const char kNeedsBeginFrameChanged[];
CC_BASE_EXPORT extern const char kActivateLayerTree[];
CC_BASE_EXPORT extern const char kRequestMainThreadFrame[];
CC_BASE_EXPORT extern const char kDroppedFrame[];
CC_BASE_EXPORT extern const char kBeginMainThreadFrame[];
CC_BASE_EXPORT extern const char kDrawFrame[];
CC_BASE_EXPORT extern const char kCommit[];
}  // namespace internal

extern const char kPaintSetup[];
CC_BASE_EXPORT extern const char kUpdateLayer[];

class CC_BASE_EXPORT ScopedLayerTask {
 public:
  ScopedLayerTask(const char* event_name, int layer_id)
      : event_name_(event_name) {
    TRACE_EVENT_BEGIN1(internal::CategoryName::kTimeline, event_name_,
                       internal::kLayerId, layer_id);
  }
  ScopedLayerTask(const ScopedLayerTask&) = delete;
  ~ScopedLayerTask() {
    TRACE_EVENT_END0(internal::CategoryName::kTimeline, event_name_);
  }

  ScopedLayerTask& operator=(const ScopedLayerTask&) = delete;

 private:
  const char* event_name_;
};

class CC_BASE_EXPORT ScopedImageTask {
 public:
  enum class ImageType { kAvif, kBmp, kGif, kIco, kJpeg, kPng, kWebP, kOther };

  explicit ScopedImageTask(ImageType image_type)
      : image_type_(image_type), start_time_(base::TimeTicks::Now()) {}
  ScopedImageTask(const ScopedImageTask&) = delete;
  ~ScopedImageTask() = default;
  ScopedImageTask& operator=(const ScopedImageTask&) = delete;

  // Prevents logging duration metrics. Used in cases where a task performed
  // uninteresting work or was terminated early.
  void SuppressMetrics() { suppress_metrics_ = true; }

 protected:
  bool suppress_metrics_ = false;
  const ImageType image_type_;
  const base::TimeTicks start_time_;

  // UMA histogram parameters
  const uint32_t bucket_count_ = 50;
  base::TimeDelta hist_min_ = base::Microseconds(1);
  base::TimeDelta hist_max_ = base::Milliseconds(1000);
};

class CC_BASE_EXPORT ScopedImageUploadTask : public ScopedImageTask {
 public:
  ScopedImageUploadTask(const void* image_ptr, ImageType image_type);
  ScopedImageUploadTask(const ScopedImageUploadTask&) = delete;
  ~ScopedImageUploadTask();

  ScopedImageUploadTask& operator=(const ScopedImageUploadTask&) = delete;
};

class CC_BASE_EXPORT ScopedImageDecodeTask : public ScopedImageTask {
 public:
  enum class TaskType { kInRaster, kOutOfRaster };
  enum class DecodeType { kSoftware, kGpu };

  ScopedImageDecodeTask(const void* image_ptr,
                        DecodeType decode_type,
                        TaskType task_type,
                        ImageType image_type);
  ScopedImageDecodeTask(const ScopedImageDecodeTask&) = delete;
  ~ScopedImageDecodeTask();

  ScopedImageDecodeTask& operator=(const ScopedImageDecodeTask&) = delete;

 private:
  const DecodeType decode_type_;
  const TaskType task_type_;
};

class CC_BASE_EXPORT ScopedLayerTreeTask {
 public:
  ScopedLayerTreeTask(const char* event_name,
                      int layer_id,
                      int layer_tree_host_id)
      : event_name_(event_name) {
    TRACE_EVENT_BEGIN2(internal::CategoryName::kTimeline, event_name_,
                       internal::kLayerId, layer_id, internal::kLayerTreeId,
                       layer_tree_host_id);
  }
  ScopedLayerTreeTask(const ScopedLayerTreeTask&) = delete;
  ~ScopedLayerTreeTask() {
    TRACE_EVENT_END0(internal::CategoryName::kTimeline, event_name_);
  }

  ScopedLayerTreeTask& operator=(const ScopedLayerTreeTask&) = delete;

 private:
  const char* event_name_;
};

struct CC_BASE_EXPORT ScopedCommitTrace {
 public:
  explicit ScopedCommitTrace(int layer_tree_host_id, uint64_t sequence_number) {
    TRACE_EVENT_BEGIN2(internal::CategoryName::kTimeline, internal::kCommit,
                       internal::kLayerTreeId, layer_tree_host_id,
                       internal::kFrameSequenceNumber, sequence_number);
  }
  ScopedCommitTrace(const ScopedCommitTrace&) = delete;
  ~ScopedCommitTrace() {
    TRACE_EVENT_END0(internal::CategoryName::kTimeline, internal::kCommit);
  }

  ScopedCommitTrace& operator=(const ScopedCommitTrace&) = delete;
};

struct CC_BASE_EXPORT ScopedLayerObjectTracker
    : public base::trace_event::
          TraceScopedTrackableObject<int, internal::CategoryName::kTimeline> {
  explicit ScopedLayerObjectTracker(int layer_id)
      : base::trace_event::
            TraceScopedTrackableObject<int, internal::CategoryName::kTimeline>(
                internal::kLayerId,
                layer_id) {}
  ScopedLayerObjectTracker(const ScopedLayerObjectTracker&) = delete;
  ScopedLayerObjectTracker& operator=(const ScopedLayerObjectTracker&) = delete;
};

inline void CC_BASE_EXPORT DidActivateLayerTree(int layer_tree_host_id,
                                                int frame_id) {
  TRACE_EVENT_INSTANT2(internal::CategoryName::kTimelineFrame,
                       internal::kActivateLayerTree, TRACE_EVENT_SCOPE_THREAD,
                       internal::kLayerTreeId, layer_tree_host_id,
                       internal::kFrameId, frame_id);
}

inline void CC_BASE_EXPORT DidBeginFrame(int layer_tree_host_id,
                                         base::TimeTicks begin_frame_timestamp,
                                         uint64_t sequence_number) {
  TRACE_EVENT_INSTANT(internal::CategoryName::kTimelineFrame,
                      perfetto::StaticString(internal::kBeginFrame),
                      begin_frame_timestamp, internal::kLayerTreeId,
                      layer_tree_host_id, internal::kFrameSequenceNumber,
                      sequence_number);
}

inline void CC_BASE_EXPORT DidDrawFrame(int layer_tree_host_id,
                                        uint64_t sequence_number) {
  TRACE_EVENT_INSTANT2(internal::CategoryName::kTimelineFrame,
                       internal::kDrawFrame, TRACE_EVENT_SCOPE_THREAD,
                       internal::kLayerTreeId, layer_tree_host_id,
                       internal::kFrameSequenceNumber, sequence_number);
}

inline void CC_BASE_EXPORT DidRequestMainThreadFrame(int layer_tree_host_id) {
  TRACE_EVENT_INSTANT1(
      internal::CategoryName::kTimelineFrame, internal::kRequestMainThreadFrame,
      TRACE_EVENT_SCOPE_THREAD, internal::kLayerTreeId, layer_tree_host_id);
}

inline void CC_BASE_EXPORT
DidDropSmoothnessFrame(int layer_tree_host_id,
                       base::TimeTicks dropped_frame_timestamp,
                       uint64_t sequence_number,
                       bool has_partial_update) {
  TRACE_EVENT_INSTANT(internal::CategoryName::kTimelineFrame,
                      perfetto::StaticString(internal::kDroppedFrame),
                      dropped_frame_timestamp, internal::kLayerTreeId,
                      layer_tree_host_id, internal::kFrameSequenceNumber,
                      sequence_number, internal::kHasPartialUpdate,
                      has_partial_update);
}

inline std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
BeginMainThreadFrameData(int frame_id) {
  std::unique_ptr<base::trace_event::TracedValue> value(
      new base::trace_event::TracedValue());
  value->SetInteger("frameId", frame_id);
  return std::move(value);
}

inline void CC_BASE_EXPORT WillBeginMainThreadFrame(int layer_tree_host_id,
                                                    int frame_id) {
  TRACE_EVENT_INSTANT2(
      internal::CategoryName::kTimelineFrame, internal::kBeginMainThreadFrame,
      TRACE_EVENT_SCOPE_THREAD, internal::kLayerTreeId, layer_tree_host_id,
      internal::kData, BeginMainThreadFrameData(frame_id));
}

inline std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
NeedsBeginFrameData(bool needs_begin_frame) {
  std::unique_ptr<base::trace_event::TracedValue> value(
      new base::trace_event::TracedValue());
  value->SetInteger("needsBeginFrame", needs_begin_frame);
  return std::move(value);
}

inline void CC_BASE_EXPORT NeedsBeginFrameChanged(int layer_tree_host_id,
                                                  bool new_value) {
  TRACE_EVENT_INSTANT2(
      internal::CategoryName::kTimelineFrame, internal::kNeedsBeginFrameChanged,
      TRACE_EVENT_SCOPE_THREAD, internal::kLayerTreeId, layer_tree_host_id,
      internal::kData, NeedsBeginFrameData(new_value));
}

}  // namespace devtools_instrumentation
}  // namespace cc

#endif  // CC_BASE_DEVTOOLS_INSTRUMENTATION_H_
