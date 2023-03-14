// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/benchmarks/micro_benchmark.h"

#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "cc/benchmarks/micro_benchmark_impl.h"

namespace cc {

MicroBenchmark::MicroBenchmark(DoneCallback callback)
    : callback_(std::move(callback)) {}

MicroBenchmark::~MicroBenchmark() = default;

bool MicroBenchmark::IsDone() const {
  return is_done_;
}

void MicroBenchmark::DidUpdateLayers(LayerTreeHost* layer_tree_host) {}

void MicroBenchmark::NotifyDone(base::Value::Dict result) {
  std::move(callback_).Run(std::move(result));
  is_done_ = true;
}

void MicroBenchmark::RunOnLayer(PictureLayer* layer) {}

bool MicroBenchmark::ProcessMessage(base::Value::Dict message) {
  return false;
}

bool MicroBenchmark::ProcessedForBenchmarkImpl() const {
  return processed_for_benchmark_impl_;
}

std::unique_ptr<MicroBenchmarkImpl> MicroBenchmark::GetBenchmarkImpl(
    scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner) {
  DCHECK(!processed_for_benchmark_impl_);
  processed_for_benchmark_impl_ = true;
  return CreateBenchmarkImpl(origin_task_runner);
}

std::unique_ptr<MicroBenchmarkImpl> MicroBenchmark::CreateBenchmarkImpl(
    scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner) {
  return base::WrapUnique<MicroBenchmarkImpl>(nullptr);
}

}  // namespace cc
