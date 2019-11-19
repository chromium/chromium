// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BENCHMARKS_UNITTEST_ONLY_BENCHMARK_H_
#define CC_BENCHMARKS_UNITTEST_ONLY_BENCHMARK_H_

#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "cc/benchmarks/micro_benchmark.h"

namespace cc {

class CC_EXPORT UnittestOnlyBenchmark : public MicroBenchmark {
 public:
  UnittestOnlyBenchmark(std::unique_ptr<base::Value> value,
                        DoneCallback callback);
  ~UnittestOnlyBenchmark() override;

  void DidUpdateLayers(LayerTreeHost* layer_tree_host) override;
  bool ProcessMessage(std::unique_ptr<base::Value> value) override;

 protected:
  std::unique_ptr<MicroBenchmarkImpl> CreateBenchmarkImpl(
      scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner) override;

 private:
  void RecordImplResults(std::unique_ptr<base::Value> results);

  bool create_impl_benchmark_;
  base::WeakPtrFactory<UnittestOnlyBenchmark> weak_ptr_factory_{this};
};

}  // namespace cc

#endif  // CC_BENCHMARKS_UNITTEST_ONLY_BENCHMARK_H_
