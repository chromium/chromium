// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_DEVTOOLS_INSTRUMENTATION_H_
#define CC_BASE_DEVTOOLS_INSTRUMENTATION_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/base_export.h"

namespace cc {
namespace devtools_instrumentation {

namespace internal {
CC_BASE_EXPORT extern const char kCategory[];
CC_BASE_EXPORT extern const char kCategoryFrame[];
CC_BASE_EXPORT extern const char kData[];
CC_BASE_EXPORT extern const char kFrameId[];
CC_BASE_EXPORT extern const char kLayerId[];
CC_BASE_EXPORT extern const char kLayerTreeId[];
CC_BASE_EXPORT extern const char kPixelRefId[];

CC_BASE_EXPORT extern const char kImageDecodeTask[];
CC_BASE_EXPORT extern const char kBeginFrame[];
CC_BASE_EXPORT extern const char kNeedsBeginFrameChanged[];
CC_BASE_EXPORT extern const char kActivateLayerTree[];
CC_BASE_EXPORT extern const char kRequestMainThreadFrame[];
CC_BASE_EXPORT extern const char kBeginMainThreadFrame[];
CC_BASE_EXPORT extern const char kDrawFrame[];
CC_BASE_EXPORT extern const char kCompositeLayers[];
}  // namespace internal

extern const char kPaintSetup[];
CC_BASE_EXPORT extern const char kUpdateLayer[];

class CC_BASE_EXPORT ScopedLayerTask {
 public:
  ScopedLayerTask(const char* event_name, int layer_id)
      : event_name_(event_name) {
    TRACE_EVENT_BEGIN1(internal::kCategory, event_name_, internal::kLayerId,
                       layer_id);
  }
  ~ScopedLayerTask() { TRACE_EVENT_END0(internal::kCategory, event_name_); }

 private:
  const char* event_name_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLayerTask);
};

class CC_BASE_EXPORT ScopedImageDecodeTask {
 public:
  enum DecodeType { kSoftware, kGpu };
  enum TaskType { kInRaster, kOutOfRaster };

  ScopedImageDecodeTask(const void* image_ptr,
                        DecodeType decode_type,
                        TaskType task_type);
  ~ScopedImageDecodeTask();

 private:
  const DecodeType decode_type_;
  const TaskType task_type_;
  const base::TimeTicks start_time_;
  DISALLOW_COPY_AND_ASSIGN(ScopedImageDecodeTask);
};

class CC_BASE_EXPORT ScopedLayerTreeTask {
 public:
  ScopedLayerTreeTask(const char* event_name,
                      int layer_id,
                      int layer_tree_host_id)
      : event_name_(event_name) {
    TRACE_EVENT_BEGIN2(internal::kCategory, event_name_, internal::kLayerId,
                       layer_id, internal::kLayerTreeId, layer_tree_host_id);
  }
  ~ScopedLayerTreeTask() { TRACE_EVENT_END0(internal::kCategory, event_name_); }

 private:
  const char* event_name_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLayerTreeTask);
};

struct CC_BASE_EXPORT ScopedCommitTrace {
 public:
  explicit ScopedCommitTrace(int layer_tree_host_id) {
    TRACE_EVENT_BEGIN1(internal::kCategory, internal::kCompositeLayers,
                       internal::kLayerTreeId, layer_tree_host_id);
  }
  ~ScopedCommitTrace() {
    TRACE_EVENT_END0(internal::kCategory, internal::kCompositeLayers);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedCommitTrace);
};

struct CC_BASE_EXPORT ScopedLayerObjectTracker
    : public base::trace_event::TraceScopedTrackableObject<int> {
  explicit ScopedLayerObjectTracker(int layer_id)
      : base::trace_event::TraceScopedTrackableObject<int>(internal::kCategory,
                                                           internal::kLayerId,
                                                           layer_id) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedLayerObjectTracker);
};

inline void CC_BASE_EXPORT DidActivateLayerTree(int layer_tree_host_id,
                                                int frame_id) {
  TRACE_EVENT_INSTANT2(internal::kCategoryFrame, internal::kActivateLayerTree,
                       TRACE_EVENT_SCOPE_THREAD, internal::kLayerTreeId,
                       layer_tree_host_id, internal::kFrameId, frame_id);
}

inline void CC_BASE_EXPORT DidBeginFrame(int layer_tree_host_id) {
  TRACE_EVENT_INSTANT1(internal::kCategoryFrame, internal::kBeginFrame,
                       TRACE_EVENT_SCOPE_THREAD, internal::kLayerTreeId,
                       layer_tree_host_id);
}

inline void CC_BASE_EXPORT DidDrawFrame(int layer_tree_host_id) {
  TRACE_EVENT_INSTANT1(internal::kCategoryFrame, internal::kDrawFrame,
                       TRACE_EVENT_SCOPE_THREAD, internal::kLayerTreeId,
                       layer_tree_host_id);
}

inline void CC_BASE_EXPORT DidRequestMainThreadFrame(int layer_tree_host_id) {
  TRACE_EVENT_INSTANT1(
      internal::kCategoryFrame, internal::kRequestMainThreadFrame,
      TRACE_EVENT_SCOPE_THREAD, internal::kLayerTreeId, layer_tree_host_id);
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
      internal::kCategoryFrame, internal::kBeginMainThreadFrame,
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
      internal::kCategoryFrame, internal::kNeedsBeginFrameChanged,
      TRACE_EVENT_SCOPE_THREAD, internal::kLayerTreeId, layer_tree_host_id,
      internal::kData, NeedsBeginFrameData(new_value));
}

}  // namespace devtools_instrumentation
}  // namespace cc

#endif  // CC_BASE_DEVTOOLS_INSTRUMENTATION_H_
