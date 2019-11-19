// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BENCHMARKS_RASTERIZE_AND_RECORD_BENCHMARK_H_
#define CC_BENCHMARKS_RASTERIZE_AND_RECORD_BENCHMARK_H_

#include <stddef.h>

#include <map>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "cc/benchmarks/micro_benchmark_controller.h"
#include "cc/layers/recording_source.h"

namespace base {
class DictionaryValue;
}

namespace cc {

class LayerTreeHost;

class RasterizeAndRecordBenchmark : public MicroBenchmark {
 public:
  explicit RasterizeAndRecordBenchmark(std::unique_ptr<base::Value> value,
                                       MicroBenchmark::DoneCallback callback);
  ~RasterizeAndRecordBenchmark() override;

  // Implements MicroBenchmark interface.
  void DidUpdateLayers(LayerTreeHost* layer_tree_host) override;
  void RunOnLayer(PictureLayer* layer) override;

  std::unique_ptr<MicroBenchmarkImpl> CreateBenchmarkImpl(
      scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner) override;

 private:
  void RecordRasterResults(std::unique_ptr<base::Value> results);

  struct RecordResults {
    RecordResults();
    ~RecordResults();

    int pixels_recorded = 0;
    size_t painter_memory_usage = 0;
    size_t paint_op_memory_usage = 0;
    size_t paint_op_count = 0;
    base::TimeDelta total_best_time[RecordingSource::RECORDING_MODE_COUNT];
  };

  RecordResults record_results_;
  int record_repeat_count_;
  std::unique_ptr<base::Value> settings_;
  std::unique_ptr<base::DictionaryValue> results_;

  // The following is used in DCHECKs.
  bool main_thread_benchmark_done_;

  LayerTreeHost* layer_tree_host_;

  base::WeakPtrFactory<RasterizeAndRecordBenchmark> weak_ptr_factory_{this};
};

}  // namespace cc

#endif  // CC_BENCHMARKS_RASTERIZE_AND_RECORD_BENCHMARK_H_
