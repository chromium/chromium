// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/benchmarks/unittest_only_benchmark_impl.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "base/values.h"

namespace cc {

UnittestOnlyBenchmarkImpl::UnittestOnlyBenchmarkImpl(
    scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner,
    DoneCallback callback)
    : MicroBenchmarkImpl(std::move(callback), origin_task_runner) {}

UnittestOnlyBenchmarkImpl::~UnittestOnlyBenchmarkImpl() = default;

void UnittestOnlyBenchmarkImpl::DidCompleteCommit(LayerTreeHostImpl* host) {
  NotifyDone(base::Value::Dict());
}

}  // namespace cc
