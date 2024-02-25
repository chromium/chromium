// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/devtools_instrumentation.h"

#include <string>

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
    case ScopedImageDecodeTask::DecodeType::kSoftware:
      UmaHistogramCustomMicrosecondsTimes(metric_prefix + ".Software", duration,
                                          min, max, bucket_count);
      break;
    case ScopedImageDecodeTask::DecodeType::kGpu:
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
const char kFrameSequenceNumber[] = "frameSeqId";
const char kHasPartialUpdate[] = "hasPartialUpdate";

const char kImageUploadTask[] = "ImageUploadTask";
const char kImageDecodeTask[] = "ImageDecodeTask";
const char kBeginFrame[] = "BeginFrame";
const char kNeedsBeginFrameChanged[] = "NeedsBeginFrameChanged";
const char kActivateLayerTree[] = "ActivateLayerTree";
const char kRequestMainThreadFrame[] = "RequestMainThreadFrame";
const char kDroppedFrame[] = "DroppedFrame";
const char kBeginMainThreadFrame[] = "BeginMainThreadFrame";
const char kDrawFrame[] = "DrawFrame";
const char kCommit[] = "Commit";
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
  const char* histogram_name = nullptr;
  switch (image_type_) {
    case ImageType::kAvif:
      histogram_name = "Renderer4.ImageDecodeTaskDurationUs.Avif";
      break;
    case ImageType::kBmp:
      histogram_name = "Renderer4.ImageDecodeTaskDurationUs.Bmp";
      break;
    case ImageType::kGif:
      histogram_name = "Renderer4.ImageDecodeTaskDurationUs.Gif";
      break;
    case ImageType::kIco:
      histogram_name = "Renderer4.ImageDecodeTaskDurationUs.Ico";
      break;
    case ImageType::kJpeg:
      histogram_name = "Renderer4.ImageDecodeTaskDurationUs.Jpeg";
      break;
    case ImageType::kPng:
      histogram_name = "Renderer4.ImageDecodeTaskDurationUs.Png";
      break;
    case ImageType::kWebP:
      histogram_name = "Renderer4.ImageDecodeTaskDurationUs.WebP";
      break;
    case ImageType::kOther:
      histogram_name = "Renderer4.ImageDecodeTaskDurationUs.Other";
      break;
  }
  RecordMicrosecondTimesUmaByDecodeType(histogram_name, duration, hist_min_,
                                        hist_max_, bucket_count_, decode_type_);

  switch (task_type_) {
    case TaskType::kInRaster:
      RecordMicrosecondTimesUmaByDecodeType(
          "Renderer4.ImageDecodeTaskDurationUs", duration, hist_min_, hist_max_,
          bucket_count_, decode_type_);
      break;
    case TaskType::kOutOfRaster:
      RecordMicrosecondTimesUmaByDecodeType(
          "Renderer4.ImageDecodeTaskDurationUs.OutOfRaster", duration,
          hist_min_, hist_max_, bucket_count_, decode_type_);
      break;
  }
}

}  // namespace devtools_instrumentation
}  // namespace cc
