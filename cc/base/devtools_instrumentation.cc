// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/devtools_instrumentation.h"

namespace cc {
namespace devtools_instrumentation {
namespace {

void RecordMicrosecondTimesUmaByDecodeType(
    const std::string& metric_prefix,
    base::TimeDelta duration,
    base::TimeDelta min,
    base::TimeDelta max,
    uint32_t bucket_count,
    ScopedImageDecodeTask::DecodeType decode_type_) {
  switch (decode_type_) {
    case ScopedImageDecodeTask::kSoftware:
      UmaHistogramCustomMicrosecondsTimes(metric_prefix + ".Software", duration,
                                          min, max, bucket_count);
      break;
    case ScopedImageDecodeTask::kGpu:
      UmaHistogramCustomMicrosecondsTimes(metric_prefix + ".Gpu", duration, min,
                                          max, bucket_count);
      break;
  }
}
}  // namespace

namespace internal {
constexpr const char CategoryName::CategoryName::kTimeline[];
constexpr const char CategoryName::CategoryName::kTimelineFrame[];
const char kData[] = "data";
const char kFrameId[] = "frameId";
const char kLayerId[] = "layerId";
const char kLayerTreeId[] = "layerTreeId";
const char kPixelRefId[] = "pixelRefId";

const char kImageUploadTask[] = "ImageUploadTask";
const char kImageDecodeTask[] = "ImageDecodeTask";
const char kBeginFrame[] = "BeginFrame";
const char kNeedsBeginFrameChanged[] = "NeedsBeginFrameChanged";
const char kActivateLayerTree[] = "ActivateLayerTree";
const char kRequestMainThreadFrame[] = "RequestMainThreadFrame";
const char kBeginMainThreadFrame[] = "BeginMainThreadFrame";
const char kDrawFrame[] = "DrawFrame";
const char kCompositeLayers[] = "CompositeLayers";
}  // namespace internal

const char kPaintSetup[] = "PaintSetup";
const char kUpdateLayer[] = "UpdateLayer";

ScopedImageUploadTask::ScopedImageUploadTask(const void* image_ptr,
                                             ImageType image_type)
    : ScopedImageTask(image_type) {
  TRACE_EVENT_BEGIN1(internal::CategoryName::kTimeline,
                     internal::kImageUploadTask, internal::kPixelRefId,
                     reinterpret_cast<uint64_t>(image_ptr));
}

ScopedImageUploadTask::~ScopedImageUploadTask() {
  TRACE_EVENT_END0(internal::CategoryName::kTimeline,
                   internal::kImageUploadTask);
  if (suppress_metrics_)
    return;

  auto duration = base::TimeTicks::Now() - start_time_;
  switch (image_type_) {
    case ImageType::kWebP:
      UmaHistogramCustomMicrosecondsTimes(
          "Renderer4.ImageUploadTaskDurationUs.WebP", duration, hist_min_,
          hist_max_, bucket_count_);
      break;
    case ImageType::kJpeg:
      UmaHistogramCustomMicrosecondsTimes(
          "Renderer4.ImageUploadTaskDurationUs.Jpeg", duration, hist_min_,
          hist_max_, bucket_count_);
      break;
    case ImageType::kOther:
      UmaHistogramCustomMicrosecondsTimes(
          "Renderer4.ImageUploadTaskDurationUs.Other", duration, hist_min_,
          hist_max_, bucket_count_);
      break;
  }
}

ScopedImageDecodeTask::ScopedImageDecodeTask(const void* image_ptr,
                                             DecodeType decode_type,
                                             TaskType task_type,
                                             ImageType image_type)
    : ScopedImageTask(image_type),
      decode_type_(decode_type),
      task_type_(task_type) {
  TRACE_EVENT_BEGIN1(internal::CategoryName::kTimeline,
                     internal::kImageDecodeTask, internal::kPixelRefId,
                     reinterpret_cast<uint64_t>(image_ptr));
}

ScopedImageDecodeTask::~ScopedImageDecodeTask() {
  TRACE_EVENT_END0(internal::CategoryName::kTimeline,
                   internal::kImageDecodeTask);
  if (suppress_metrics_)
    return;

  auto duration = base::TimeTicks::Now() - start_time_;
  switch (image_type_) {
    case ImageType::kWebP:
      RecordMicrosecondTimesUmaByDecodeType(
          "Renderer4.ImageDecodeTaskDurationUs.WebP", duration, hist_min_,
          hist_max_, bucket_count_, decode_type_);
      break;
    case ImageType::kJpeg:
      RecordMicrosecondTimesUmaByDecodeType(
          "Renderer4.ImageDecodeTaskDurationUs.Jpeg", duration, hist_min_,
          hist_max_, bucket_count_, decode_type_);
      break;
    case ImageType::kOther:
      RecordMicrosecondTimesUmaByDecodeType(
          "Renderer4.ImageDecodeTaskDurationUs.Other", duration, hist_min_,
          hist_max_, bucket_count_, decode_type_);
      break;
  }
  switch (task_type_) {
    case kInRaster:
      RecordMicrosecondTimesUmaByDecodeType(
          "Renderer4.ImageDecodeTaskDurationUs", duration, hist_min_, hist_max_,
          bucket_count_, decode_type_);
      break;
    case kOutOfRaster:
      RecordMicrosecondTimesUmaByDecodeType(
          "Renderer4.ImageDecodeTaskDurationUs.OutOfRaster", duration,
          hist_min_, hist_max_, bucket_count_, decode_type_);
      break;
  }
}

}  // namespace devtools_instrumentation
}  // namespace cc
