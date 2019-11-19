// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/benchmarks/rasterize_and_record_benchmark.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/timer/lap_timer.h"
#include "base/values.h"
#include "cc/benchmarks/rasterize_and_record_benchmark_impl.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/recording_source.h"
#include "cc/paint/display_item_list.h"
#include "cc/trees/layer_tree_host.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

namespace {

const int kDefaultRecordRepeatCount = 100;

const char* kModeSuffixes[RecordingSource::RECORDING_MODE_COUNT] = {
    "",
    "_painting_disabled",
    "_caching_disabled",
    "_construction_disabled",
    "_subsequence_caching_disabled",
    "_partial_invalidation"};

ContentLayerClient::PaintingControlSetting
RecordingModeToPaintingControlSetting(RecordingSource::RecordingMode mode) {
  switch (mode) {
    case RecordingSource::RECORD_NORMALLY:
      return ContentLayerClient::PAINTING_BEHAVIOR_NORMAL_FOR_TEST;
    case RecordingSource::RECORD_WITH_PAINTING_DISABLED:
      return ContentLayerClient::DISPLAY_LIST_PAINTING_DISABLED;
    case RecordingSource::RECORD_WITH_CACHING_DISABLED:
      return ContentLayerClient::DISPLAY_LIST_CACHING_DISABLED;
    case RecordingSource::RECORD_WITH_CONSTRUCTION_DISABLED:
      return ContentLayerClient::DISPLAY_LIST_CONSTRUCTION_DISABLED;
    case RecordingSource::RECORD_WITH_SUBSEQUENCE_CACHING_DISABLED:
      return ContentLayerClient::SUBSEQUENCE_CACHING_DISABLED;
    case RecordingSource::RECORD_WITH_PARTIAL_INVALIDATION:
      return ContentLayerClient::PARTIAL_INVALIDATION;
    case RecordingSource::RECORDING_MODE_COUNT:
      NOTREACHED();
  }
  return ContentLayerClient::PAINTING_BEHAVIOR_NORMAL_FOR_TEST;
}

}  // namespace

RasterizeAndRecordBenchmark::RasterizeAndRecordBenchmark(
    std::unique_ptr<base::Value> value,
    MicroBenchmark::DoneCallback callback)
    : MicroBenchmark(std::move(callback)),
      record_repeat_count_(kDefaultRecordRepeatCount),
      settings_(std::move(value)),
      main_thread_benchmark_done_(false),
      layer_tree_host_(nullptr) {
  base::DictionaryValue* settings = nullptr;
  settings_->GetAsDictionary(&settings);
  if (!settings)
    return;

  if (settings->HasKey("record_repeat_count"))
    settings->GetInteger("record_repeat_count", &record_repeat_count_);
}

RasterizeAndRecordBenchmark::~RasterizeAndRecordBenchmark() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void RasterizeAndRecordBenchmark::DidUpdateLayers(
    LayerTreeHost* layer_tree_host) {
  layer_tree_host_ = layer_tree_host;
  for (auto* layer : *layer_tree_host)
    layer->RunMicroBenchmark(this);

  DCHECK(!results_.get());
  results_ = base::WrapUnique(new base::DictionaryValue);
  results_->SetInteger("pixels_recorded", record_results_.pixels_recorded);
  results_->SetInteger("painter_memory_usage",
                       static_cast<int>(record_results_.painter_memory_usage));
  results_->SetInteger("paint_op_memory_usage",
                       static_cast<int>(record_results_.paint_op_memory_usage));
  results_->SetInteger("paint_op_count",
                       static_cast<int>(record_results_.paint_op_count));

  for (int i = 0; i < RecordingSource::RECORDING_MODE_COUNT; i++) {
    std::string name = base::StringPrintf("record_time%s_ms", kModeSuffixes[i]);
    results_->SetDouble(name,
                        record_results_.total_best_time[i].InMillisecondsF());
  }
  main_thread_benchmark_done_ = true;
}

void RasterizeAndRecordBenchmark::RecordRasterResults(
    std::unique_ptr<base::Value> results_value) {
  DCHECK(main_thread_benchmark_done_);

  base::DictionaryValue* results = nullptr;
  results_value->GetAsDictionary(&results);
  DCHECK(results);

  results_->MergeDictionary(results);

  NotifyDone(std::move(results_));
}

std::unique_ptr<MicroBenchmarkImpl>
RasterizeAndRecordBenchmark::CreateBenchmarkImpl(
    scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner) {
  return base::WrapUnique(new RasterizeAndRecordBenchmarkImpl(
      origin_task_runner, settings_.get(),
      base::BindOnce(&RasterizeAndRecordBenchmark::RecordRasterResults,
                     weak_ptr_factory_.GetWeakPtr())));
}

void RasterizeAndRecordBenchmark::RunOnLayer(PictureLayer* layer) {
  DCHECK(layer_tree_host_);

  if (!layer->DrawsContent())
    return;

  const int kTimeCheckInterval = 1;
  const int kWarmupRuns = 0;
  const int kTimeLimitMillis = 1;
  ContentLayerClient* painter = layer->client();
  RecordingSource recording_source;

  for (int mode_index = 0; mode_index < RecordingSource::RECORDING_MODE_COUNT;
       mode_index++) {
    ContentLayerClient::PaintingControlSetting painting_control =
        RecordingModeToPaintingControlSetting(
            static_cast<RecordingSource::RecordingMode>(mode_index));
    base::TimeDelta min_time = base::TimeDelta::Max();
    size_t paint_op_memory_usage = 0;
    size_t paint_op_count = 0;

    scoped_refptr<DisplayItemList> display_list;
    for (int i = 0; i < record_repeat_count_; ++i) {
      // Run for a minimum amount of time to avoid problems with timer
      // quantization when the layer is very small.
      base::LapTimer timer(kWarmupRuns,
                           base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
                           kTimeCheckInterval);

      do {
        display_list = painter->PaintContentsToDisplayList(painting_control);
        recording_source.UpdateDisplayItemList(
            display_list, painter->GetApproximateUnsharedMemoryUsage(),
            layer_tree_host_->recording_scale_factor());

        if (paint_op_memory_usage) {
          // Verify we are recording the same thing each time.
          DCHECK_EQ(paint_op_memory_usage, display_list->BytesUsed());
          DCHECK_EQ(paint_op_count, display_list->TotalOpCount());
        } else {
          paint_op_memory_usage = display_list->BytesUsed();
          paint_op_count = display_list->TotalOpCount();
        }

        timer.NextLap();
      } while (!timer.HasTimeLimitExpired());
      base::TimeDelta duration = timer.TimePerLap();
      if (duration < min_time)
        min_time = duration;
    }

    if (mode_index == RecordingSource::RECORD_NORMALLY) {
      record_results_.painter_memory_usage +=
          painter->GetApproximateUnsharedMemoryUsage();
      record_results_.paint_op_memory_usage += paint_op_memory_usage;
      record_results_.paint_op_count += paint_op_count;
      record_results_.pixels_recorded += painter->PaintableRegion().width() *
                                         painter->PaintableRegion().height();
    }
    record_results_.total_best_time[mode_index] += min_time;
  }
}

RasterizeAndRecordBenchmark::RecordResults::RecordResults() = default;
RasterizeAndRecordBenchmark::RecordResults::~RecordResults() = default;

}  // namespace cc
