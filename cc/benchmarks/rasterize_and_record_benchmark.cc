// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/benchmarks/rasterize_and_record_benchmark.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
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

  DCHECK(!results_.get());
  results_ = base::WrapUnique(new base::DictionaryValue);
  results_->SetInteger("pixels_recorded", record_results_.pixels_recorded);
  results_->SetInteger("paint_op_memory_usage",
                       static_cast<int>(record_results_.paint_op_memory_usage));
  results_->SetInteger("paint_op_count",
                       static_cast<int>(record_results_.paint_op_count));
  results_->SetDouble("record_time_ms", paint_benchmark_result.record_time_ms);
  results_->SetDouble("record_time_caching_disabled_ms",
                      paint_benchmark_result.record_time_caching_disabled_ms);
  results_->SetDouble(
      "record_time_subsequence_caching_disabled_ms",
      paint_benchmark_result.record_time_subsequence_caching_disabled_ms);
  results_->SetDouble(
      "record_time_partial_invalidation_ms",
      paint_benchmark_result.record_time_partial_invalidation_ms);
  results_->SetDouble(
      "raster_invalidation_and_convert_time_ms",
      paint_benchmark_result.raster_invalidation_and_convert_time_ms);
  results_->SetDouble(
      "paint_artifact_compositor_update_time_ms",
      paint_benchmark_result.paint_artifact_compositor_update_time_ms);
  results_->SetInteger(
      "painter_memory_usage",
      static_cast<int>(paint_benchmark_result.painter_memory_usage));
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

  ContentLayerClient* painter = layer->client();
  scoped_refptr<DisplayItemList> display_list =
      painter->PaintContentsToDisplayList();
  record_results_.paint_op_memory_usage += display_list->BytesUsed();
  record_results_.paint_op_count += display_list->TotalOpCount();
  record_results_.pixels_recorded +=
      painter->PaintableRegion().width() * painter->PaintableRegion().height();
}

}  // namespace cc
