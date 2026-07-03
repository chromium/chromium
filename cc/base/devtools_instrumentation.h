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
#include "third_party/perfetto/include/perfetto/tracing/track_event_args.h"

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

inline constexpr char kData[] = "data";
inline constexpr char kFrameId[] = "frameId";
inline constexpr char kLayerId[] = "layerId";
inline constexpr char kLayerTreeId[] = "layerTreeId";
inline constexpr char kPixelRefId[] = "pixelRefId";
inline constexpr char kFrameSequenceNumber[] = "frameSeqId";
inline constexpr char kHasPartialUpdate[] = "hasPartialUpdate";

inline constexpr char kImageUploadTask[] = "ImageUploadTask";
inline constexpr char kImageDecodeTask[] = "ImageDecodeTask";
inline constexpr char kBeginFrame[] = "BeginFrame";
inline constexpr char kNeedsBeginFrameChanged[] = "NeedsBeginFrameChanged";
inline constexpr char kActivateLayerTree[] = "ActivateLayerTree";
inline constexpr char kRequestMainThreadFrame[] = "RequestMainThreadFrame";
inline constexpr char kDroppedFrame[] = "DroppedFrame";
inline constexpr char kBeginMainThreadFrame[] = "BeginMainThreadFrame";
inline constexpr char kDrawFrame[] = "DrawFrame";
inline constexpr char kCommit[] = "Commit";
}  // namespace internal

inline constexpr char kPaintSetup[] = "PaintSetup";
inline constexpr char kUpdateLayer[] = "UpdateLayer";

class CC_BASE_EXPORT ScopedImageTask {
 public:
  enum class ImageType {
    kAvif,
    kBmp,
    kGif,
    kIco,
    kJpeg,
    kJxl,
    kPng,
    kWebP,
    kOther
  };

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
  ScopedLayerTreeTask(perfetto::StaticString event_name,
                      int layer_id,
                      int layer_tree_host_id) {
    TRACE_EVENT_BEGIN(internal::CategoryName::kTimeline, event_name,
                      internal::kLayerId, layer_id, internal::kLayerTreeId,
                      layer_tree_host_id);
  }
  ScopedLayerTreeTask(const ScopedLayerTreeTask&) = delete;
  ~ScopedLayerTreeTask() { TRACE_EVENT_END(internal::CategoryName::kTimeline); }

  ScopedLayerTreeTask& operator=(const ScopedLayerTreeTask&) = delete;
};

struct CC_BASE_EXPORT ScopedCommitTrace {
 public:
  explicit ScopedCommitTrace(int layer_tree_host_id, uint64_t sequence_number) {
    TRACE_EVENT_BEGIN(internal::CategoryName::kTimeline, internal::kCommit,
                      internal::kLayerTreeId, layer_tree_host_id,
                      internal::kFrameSequenceNumber, sequence_number);
  }
  ScopedCommitTrace(const ScopedCommitTrace&) = delete;
  ~ScopedCommitTrace() { TRACE_EVENT_END(internal::CategoryName::kTimeline); }

  ScopedCommitTrace& operator=(const ScopedCommitTrace&) = delete;
};

class CC_BASE_EXPORT ScopedLayerObjectTracker {
 public:
  explicit ScopedLayerObjectTracker(int layer_id) : layer_id_(layer_id) {
    TRACE_EVENT_INSTANT(internal::CategoryName::kTimeline, "Layer:created",
                        perfetto::Flow::ProcessScoped(
                            static_cast<uint64_t>(layer_id_), "Layer"));
  }
  ScopedLayerObjectTracker(const ScopedLayerObjectTracker&) = delete;
  ~ScopedLayerObjectTracker() {
    TRACE_EVENT_INSTANT(internal::CategoryName::kTimeline, "Layer:deleted",
                        perfetto::TerminatingFlow::ProcessScoped(
                            static_cast<uint64_t>(layer_id_), "Layer"));
  }

  ScopedLayerObjectTracker& operator=(const ScopedLayerObjectTracker&) = delete;

 private:
  int layer_id_;
};

inline void CC_BASE_EXPORT DidActivateLayerTree(int layer_tree_host_id,
                                                int frame_id) {
  TRACE_EVENT_INSTANT(internal::CategoryName::kTimelineFrame,
                      perfetto::StaticString(internal::kActivateLayerTree),
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
  TRACE_EVENT_INSTANT(internal::CategoryName::kTimelineFrame,
                      perfetto::StaticString(internal::kDrawFrame),
                      internal::kLayerTreeId, layer_tree_host_id,
                      internal::kFrameSequenceNumber, sequence_number);
}

inline void CC_BASE_EXPORT DidRequestMainThreadFrame(int layer_tree_host_id) {
  TRACE_EVENT_INSTANT(internal::CategoryName::kTimelineFrame,
                      perfetto::StaticString(internal::kRequestMainThreadFrame),
                      internal::kLayerTreeId, layer_tree_host_id);
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
  TRACE_EVENT_INSTANT(internal::CategoryName::kTimelineFrame,
                      perfetto::StaticString(internal::kBeginMainThreadFrame),
                      internal::kLayerTreeId, layer_tree_host_id,
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
  TRACE_EVENT_INSTANT(internal::CategoryName::kTimelineFrame,
                      perfetto::StaticString(internal::kNeedsBeginFrameChanged),
                      internal::kLayerTreeId, layer_tree_host_id,
                      internal::kData, NeedsBeginFrameData(new_value));
}

}  // namespace devtools_instrumentation
}  // namespace cc

#endif  // CC_BASE_DEVTOOLS_INSTRUMENTATION_H_
