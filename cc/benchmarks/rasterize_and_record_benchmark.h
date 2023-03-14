// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BENCHMARKS_RASTERIZE_AND_RECORD_BENCHMARK_H_
#define CC_BENCHMARKS_RASTERIZE_AND_RECORD_BENCHMARK_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "cc/benchmarks/micro_benchmark_controller.h"
#include "cc/layers/recording_source.h"

namespace cc {

class LayerTreeHost;

class RasterizeAndRecordBenchmark : public MicroBenchmark {
 public:
  explicit RasterizeAndRecordBenchmark(base::Value::Dict settings,
                                       MicroBenchmark::DoneCallback callback);
  ~RasterizeAndRecordBenchmark() override;

  // Implements MicroBenchmark interface.
  void DidUpdateLayers(LayerTreeHost* layer_tree_host) override;
  void RunOnLayer(PictureLayer* layer) override;

  std::unique_ptr<MicroBenchmarkImpl> CreateBenchmarkImpl(
      scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner) override;

 private:
  void RecordRasterResults(base::Value::Dict results);

  struct RecordResults {
    int pixels_recorded = 0;
    size_t paint_op_memory_usage = 0;
    size_t paint_op_count = 0;
  };

  RecordResults record_results_;
  int record_repeat_count_;
  int rasterize_repeat_count_;
  base::Value::Dict results_;

  // The following is used in DCHECKs.
  bool main_thread_benchmark_done_ = false;

  raw_ptr<LayerTreeHost> layer_tree_host_ = nullptr;

  base::WeakPtrFactory<RasterizeAndRecordBenchmark> weak_ptr_factory_{this};
};

}  // namespace cc

#endif  // CC_BENCHMARKS_RASTERIZE_AND_RECORD_BENCHMARK_H_
