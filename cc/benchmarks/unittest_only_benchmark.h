// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BENCHMARKS_UNITTEST_ONLY_BENCHMARK_H_
#define CC_BENCHMARKS_UNITTEST_ONLY_BENCHMARK_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/benchmarks/micro_benchmark.h"

namespace cc {

class CC_EXPORT UnittestOnlyBenchmark : public MicroBenchmark {
 public:
  UnittestOnlyBenchmark(base::Value::Dict settings, DoneCallback callback);
  ~UnittestOnlyBenchmark() override;

  void DidUpdateLayers(LayerTreeHost* layer_tree_host) override;
  bool ProcessMessage(base::Value::Dict message) override;

 protected:
  std::unique_ptr<MicroBenchmarkImpl> CreateBenchmarkImpl(
      scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner) override;

 private:
  void RecordImplResults(base::Value::Dict results);

  bool create_impl_benchmark_;
  base::WeakPtrFactory<UnittestOnlyBenchmark> weak_ptr_factory_{this};
};

}  // namespace cc

#endif  // CC_BENCHMARKS_UNITTEST_ONLY_BENCHMARK_H_
