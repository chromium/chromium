// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/benchmarks/micro_benchmark_controller.h"

#include <limits>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "cc/benchmarks/invalidation_benchmark.h"
#include "cc/benchmarks/rasterize_and_record_benchmark.h"
#include "cc/benchmarks/unittest_only_benchmark.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_impl.h"

namespace cc {

int MicroBenchmarkController::next_id_ = 1;

namespace {

std::unique_ptr<MicroBenchmark> CreateBenchmark(
    const std::string& name,
    base::Value::Dict settings,
    MicroBenchmark::DoneCallback callback) {
  if (name == "invalidation_benchmark") {
    return std::make_unique<InvalidationBenchmark>(std::move(settings),
                                                   std::move(callback));
  } else if (name == "rasterize_and_record_benchmark") {
    return std::make_unique<RasterizeAndRecordBenchmark>(std::move(settings),
                                                         std::move(callback));
  } else if (name == "unittest_only_benchmark") {
    return std::make_unique<UnittestOnlyBenchmark>(std::move(settings),
                                                   std::move(callback));
  }
  return nullptr;
}

}  // namespace

MicroBenchmarkController::MicroBenchmarkController(LayerTreeHost* host)
    : host_(host),
      main_controller_task_runner_(
          base::SingleThreadTaskRunner::HasCurrentDefault()
              ? base::SingleThreadTaskRunner::GetCurrentDefault()
              : nullptr) {
  DCHECK(host_);
}

MicroBenchmarkController::~MicroBenchmarkController() = default;

int MicroBenchmarkController::ScheduleRun(
    const std::string& micro_benchmark_name,
    base::Value::Dict settings,
    MicroBenchmark::DoneCallback callback) {
  std::unique_ptr<MicroBenchmark> benchmark = CreateBenchmark(
      micro_benchmark_name, std::move(settings), std::move(callback));
  if (benchmark.get()) {
    int id = GetNextIdAndIncrement();
    benchmark->set_id(id);
    benchmarks_.push_back(std::move(benchmark));
    host_->SetNeedsCommit();
    return id;
  }
  return 0;
}

int MicroBenchmarkController::GetNextIdAndIncrement() {
  int id = next_id_++;
  // Wrap around to 1 if we overflow (very unlikely).
  if (next_id_ == std::numeric_limits<int>::max())
    next_id_ = 1;
  return id;
}

bool MicroBenchmarkController::SendMessage(int id, base::Value::Dict message) {
  auto it = base::ranges::find(benchmarks_, id, &MicroBenchmark::id);
  if (it == benchmarks_.end())
    return false;
  return (*it)->ProcessMessage(std::move(message));
}

std::vector<std::unique_ptr<MicroBenchmarkImpl>>
MicroBenchmarkController::CreateImplBenchmarks() const {
  std::vector<std::unique_ptr<MicroBenchmarkImpl>> result;
  for (const auto& benchmark : benchmarks_) {
    std::unique_ptr<MicroBenchmarkImpl> benchmark_impl;
    if (!benchmark->ProcessedForBenchmarkImpl()) {
      benchmark_impl =
          benchmark->GetBenchmarkImpl(main_controller_task_runner_);
    }

    if (benchmark_impl.get())
      result.push_back(std::move(benchmark_impl));
  }
  return result;
}

void MicroBenchmarkController::DidUpdateLayers() {
  for (const auto& benchmark : benchmarks_) {
    if (!benchmark->IsDone())
      benchmark->DidUpdateLayers(host_);
  }

  CleanUpFinishedBenchmarks();
}

void MicroBenchmarkController::CleanUpFinishedBenchmarks() {
  std::erase_if(benchmarks_,
                [](const std::unique_ptr<MicroBenchmark>& benchmark) {
                  return benchmark->IsDone();
                });
}

}  // namespace cc
