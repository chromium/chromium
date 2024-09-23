// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/benchmarks/micro_benchmark_controller_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"
#include "cc/trees/layer_tree_host_impl.h"

namespace cc {

MicroBenchmarkControllerImpl::MicroBenchmarkControllerImpl(
    LayerTreeHostImpl* host)
    : host_(host) {
  DCHECK(host_);
}

MicroBenchmarkControllerImpl::~MicroBenchmarkControllerImpl() = default;

void MicroBenchmarkControllerImpl::ScheduleRun(
    std::unique_ptr<MicroBenchmarkImpl> benchmark) {
  benchmarks_.push_back(std::move(benchmark));
}

void MicroBenchmarkControllerImpl::DidCompleteCommit() {
  for (const auto& benchmark : benchmarks_) {
    DCHECK(!benchmark->IsDone());
    benchmark->DidCompleteCommit(host_);
  }

  CleanUpFinishedBenchmarks();
}

void MicroBenchmarkControllerImpl::CleanUpFinishedBenchmarks() {
  std::erase_if(benchmarks_,
                [](const std::unique_ptr<MicroBenchmarkImpl>& benchmark) {
                  return benchmark->IsDone();
                });
}

}  // namespace cc
