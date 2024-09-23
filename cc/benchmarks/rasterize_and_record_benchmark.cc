// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/benchmarks/rasterize_and_record_benchmark.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
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
const int kDefaultRasterizeRepeatCount = 100;

}  // namespace

RasterizeAndRecordBenchmark::RasterizeAndRecordBenchmark(
    base::Value::Dict settings,
    MicroBenchmark::DoneCallback callback)
    : MicroBenchmark(std::move(callback)),
      record_repeat_count_(kDefaultRecordRepeatCount),
      rasterize_repeat_count_(kDefaultRasterizeRepeatCount) {
  auto record_repeat_count = settings.FindInt("record_repeat_count");
  if (record_repeat_count.has_value())
    record_repeat_count_ = *record_repeat_count;

  auto rasterize_repeat_count = settings.FindInt("rasterize_repeat_count");
  if (rasterize_repeat_count.has_value())
    rasterize_repeat_count_ = *rasterize_repeat_count;
}

RasterizeAndRecordBenchmark::~RasterizeAndRecordBenchmark() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void RasterizeAndRecordBenchmark::DidUpdateLayers(
    LayerTreeHost* layer_tree_host) {
  // It is possible that this will be called before NotifyDone is called, in the
  // event that a BeginMainFrame was scheduled before NotifyDone for example.
  // This check prevents the benchmark from being run a second time redundantly.
  if (main_thread_benchmark_done_)
    return;

  layer_tree_host_ = layer_tree_host;
  for (auto* layer : *layer_tree_host)
    layer->RunMicroBenchmark(this);

  PaintBenchmarkResult paint_benchmark_result;
  layer_tree_host->client()->RunPaintBenchmark(record_repeat_count_,
                                               paint_benchmark_result);

  DCHECK(results_.empty());
  results_.Set("pixels_recorded", record_results_.pixels_recorded);
  results_.Set("paint_op_memory_usage",
               static_cast<int>(record_results_.paint_op_memory_usage));
  results_.Set("paint_op_count",
               static_cast<int>(record_results_.paint_op_count));
  results_.Set("record_time_ms", paint_benchmark_result.record_time_ms);
  results_.Set("record_time_caching_disabled_ms",
               paint_benchmark_result.record_time_caching_disabled_ms);
  results_.Set(
      "record_time_subsequence_caching_disabled_ms",
      paint_benchmark_result.record_time_subsequence_caching_disabled_ms);
  results_.Set("raster_invalidation_and_convert_time_ms",
               paint_benchmark_result.raster_invalidation_and_convert_time_ms);
  results_.Set("paint_artifact_compositor_update_time_ms",
               paint_benchmark_result.paint_artifact_compositor_update_time_ms);
  results_.Set("painter_memory_usage",
               static_cast<int>(paint_benchmark_result.painter_memory_usage));
  main_thread_benchmark_done_ = true;
}

void RasterizeAndRecordBenchmark::RecordRasterResults(
    base::Value::Dict results_value) {
  DCHECK(main_thread_benchmark_done_);

  results_.Merge(std::move(results_value));

  NotifyDone(std::move(results_));
}

std::unique_ptr<MicroBenchmarkImpl>
RasterizeAndRecordBenchmark::CreateBenchmarkImpl(
    scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner) {
  return base::WrapUnique(new RasterizeAndRecordBenchmarkImpl(
      origin_task_runner, rasterize_repeat_count_,
      base::BindOnce(&RasterizeAndRecordBenchmark::RecordRasterResults,
                     weak_ptr_factory_.GetWeakPtr())));
}

void RasterizeAndRecordBenchmark::RunOnLayer(PictureLayer* layer) {
  DCHECK(layer_tree_host_);

  if (!layer->draws_content())
    return;

  ContentLayerClient* painter = layer->client();
  scoped_refptr<DisplayItemList> display_list =
      painter->PaintContentsToDisplayList();
  record_results_.paint_op_memory_usage += display_list->BytesUsed();
  record_results_.paint_op_count += display_list->TotalOpCount();
  gfx::Rect bounds =
      display_list->bounds().value_or(gfx::Rect(layer->bounds()));
  record_results_.pixels_recorded += bounds.size().Area64();
}

}  // namespace cc
