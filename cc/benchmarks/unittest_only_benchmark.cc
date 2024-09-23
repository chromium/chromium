// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/benchmarks/unittest_only_benchmark.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "cc/benchmarks/unittest_only_benchmark_impl.h"

namespace cc {

UnittestOnlyBenchmark::UnittestOnlyBenchmark(base::Value::Dict settings,
                                             DoneCallback callback)
    : MicroBenchmark(std::move(callback)), create_impl_benchmark_(false) {
  auto run_benchmark_impl = settings.FindBool("run_benchmark_impl");
  if (run_benchmark_impl.has_value())
    create_impl_benchmark_ = *run_benchmark_impl;
}

UnittestOnlyBenchmark::~UnittestOnlyBenchmark() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void UnittestOnlyBenchmark::DidUpdateLayers(LayerTreeHost* layer_tree_host) {
  NotifyDone(base::Value::Dict());
}

bool UnittestOnlyBenchmark::ProcessMessage(base::Value::Dict message) {
  return message.FindBool("can_handle").value_or(false);
}

void UnittestOnlyBenchmark::RecordImplResults(base::Value::Dict results) {
  NotifyDone(std::move(results));
}

std::unique_ptr<MicroBenchmarkImpl> UnittestOnlyBenchmark::CreateBenchmarkImpl(
    scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner) {
  if (!create_impl_benchmark_)
    return base::WrapUnique<MicroBenchmarkImpl>(nullptr);

  return base::WrapUnique(new UnittestOnlyBenchmarkImpl(
      origin_task_runner,
      base::BindOnce(&UnittestOnlyBenchmark::RecordImplResults,
                     weak_ptr_factory_.GetWeakPtr())));
}

}  // namespace cc
