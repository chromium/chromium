// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BENCHMARKS_INVALIDATION_BENCHMARK_H_
#define CC_BENCHMARKS_INVALIDATION_BENCHMARK_H_

#include <stdint.h>

#include "cc/benchmarks/micro_benchmark_controller.h"

namespace cc {

class LayerTreeHost;

// NOTE: this benchmark will not measure or return any results, it will simply
// invalidate a certain area of each layer every frame. It is intended to be
// used in combination with a telemetry benchmark that does the actual
// measurement.
class CC_EXPORT InvalidationBenchmark : public MicroBenchmark {
 public:
  explicit InvalidationBenchmark(base::Value::Dict settings,
                                 MicroBenchmark::DoneCallback callback);
  ~InvalidationBenchmark() override;

  // Implements MicroBenchmark interface.
  void DidUpdateLayers(LayerTreeHost* layer_tree_host) override;
  void RunOnLayer(PictureLayer* layer) override;
  bool ProcessMessage(base::Value::Dict message) override;

 private:
  enum Mode { FIXED_SIZE, LAYER, VIEWPORT, RANDOM };

  float LCGRandom();

  Mode mode_;
  int width_;
  int height_;
  uint32_t seed_ = 0;
};

}  // namespace cc

#endif  // CC_BENCHMARKS_INVALIDATION_BENCHMARK_H_
