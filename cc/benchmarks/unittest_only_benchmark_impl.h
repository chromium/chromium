// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BENCHMARKS_UNITTEST_ONLY_BENCHMARK_IMPL_H_
#define CC_BENCHMARKS_UNITTEST_ONLY_BENCHMARK_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "cc/benchmarks/micro_benchmark_impl.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {

class LayerTreeHostImpl;
class CC_EXPORT UnittestOnlyBenchmarkImpl : public MicroBenchmarkImpl {
 public:
  UnittestOnlyBenchmarkImpl(
      scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner,
      DoneCallback callback);
  ~UnittestOnlyBenchmarkImpl() override;

  void DidCompleteCommit(LayerTreeHostImpl* host) override;
};

}  // namespace cc

#endif  // CC_BENCHMARKS_UNITTEST_ONLY_BENCHMARK_IMPL_H_
