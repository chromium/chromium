// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/cpu_probe/pressure_test_support.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"

namespace performance_manager::metrics {

FakeCpuProbe::FakeCpuProbe() = default;

FakeCpuProbe::~FakeCpuProbe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FakeCpuProbe::Update(SampleCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock auto_lock(lock_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), last_sample_));
}

base::WeakPtr<CpuProbe> FakeCpuProbe::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void FakeCpuProbe::SetLastSample(absl::optional<PressureSample> sample) {
  base::AutoLock auto_lock(lock_);
  last_sample_ = sample;
}

StreamingCpuProbe::StreamingCpuProbe(std::vector<PressureSample> samples,
                                     base::OnceClosure callback)
    : samples_(std::move(samples)), done_callback_(std::move(callback)) {
  CHECK_GT(samples_.size(), 0u);
}

StreamingCpuProbe::~StreamingCpuProbe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void StreamingCpuProbe::Update(SampleCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ++sample_index_;

  if (sample_index_ < samples_.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), samples_.at(sample_index_)));
    return;
  }

  if (!done_callback_.is_null()) {
    std::move(done_callback_).Run();
  }
}

base::WeakPtr<CpuProbe> StreamingCpuProbe::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace performance_manager::metrics
