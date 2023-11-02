// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BENCHMARKS_RASTERIZE_AND_RECORD_BENCHMARK_IMPL_H_
#define CC_BENCHMARKS_RASTERIZE_AND_RECORD_BENCHMARK_IMPL_H_

#include <stddef.h>

#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "cc/benchmarks/micro_benchmark_impl.h"
#include "cc/raster/lcd_text_disallowed_reason.h"
#include "cc/raster/task_graph_runner.h"

namespace cc {

class LayerTreeHostImpl;
class PictureLayerImpl;

class RasterizeAndRecordBenchmarkImpl : public MicroBenchmarkImpl {
 public:
  explicit RasterizeAndRecordBenchmarkImpl(
      scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner,
      int rasterize_repeat_count,
      MicroBenchmarkImpl::DoneCallback callback);
  ~RasterizeAndRecordBenchmarkImpl() override;

  // Implements MicroBenchmark interface.
  void DidCompleteCommit(LayerTreeHostImpl* host) override;
  void RunOnLayer(PictureLayerImpl* layer) override;

 private:
  struct RasterizeResults {
    RasterizeResults();
    ~RasterizeResults();

    int pixels_rasterized;
    int pixels_rasterized_with_non_solid_color;
    int pixels_rasterized_as_opaque;
    int visible_pixels_by_lcd_text_disallowed_reason
        [kLCDTextDisallowedReasonCount];
    base::TimeDelta total_best_time;
    int total_layers;
    int total_picture_layers;
    int total_picture_layers_with_no_content;
    int total_picture_layers_off_screen;
  };

  RasterizeResults rasterize_results_;
  int rasterize_repeat_count_;
};

}  // namespace cc

#endif  // CC_BENCHMARKS_RASTERIZE_AND_RECORD_BENCHMARK_IMPL_H_
