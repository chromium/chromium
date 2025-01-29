// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_unwind_data.h"

#include <iterator>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/profiler/metadata_recorder.h"
#include "base/profiler/profile_builder.h"
#include "base/profiler/sample_metadata.h"
#include "base/profiler/stack_buffer.h"
#include "base/profiler/stack_copier.h"
#include "base/profiler/suspendable_thread_delegate.h"
#include "base/profiler/unwinder.h"
#include "base/ranges/algorithm.h"

namespace base {

StackUnwindData::StackUnwindData(
    std::unique_ptr<ProfileBuilder> profile_builder)
    : profile_builder_(std::move(profile_builder)),
      module_cache_(profile_builder_->GetModuleCache()) {}

StackUnwindData::~StackUnwindData() = default;

std::vector<UnwinderCapture> StackUnwindData::GetUnwinderSnapshot() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sampling_thread_sequence_checker_);
  std::vector<UnwinderCapture> unwinders;
  for (const auto& unwinder : unwinders_) {
    unwinders.emplace_back(unwinder.get(),
                           unwinder->CreateUnwinderStateCapture());
  }
  return unwinders;
}

void StackUnwindData::Initialize(
    std::vector<std::unique_ptr<Unwinder>> unwinders) {
  DETACH_FROM_SEQUENCE(sampling_thread_sequence_checker_);
  DETACH_FROM_SEQUENCE(worker_sequence_checker_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sampling_thread_sequence_checker_);
  // |unwinders| is iterated backward since |unwinders_factory_| generates
  // unwinders in increasing priority order. |unwinders_| is stored in
  // decreasing priority order for ease of use within the class.
  unwinders_.insert(unwinders_.end(),
                    std::make_move_iterator(unwinders.rbegin()),
                    std::make_move_iterator(unwinders.rend()));

  for (const auto& unwinder : unwinders_) {
    unwinder->Initialize(module_cache_);
  }
}

void StackUnwindData::OnThreadPoolRunning() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sampling_thread_sequence_checker_);
  DETACH_FROM_SEQUENCE(worker_sequence_checker_);
}

void StackUnwindData::AddAuxUnwinder(std::unique_ptr<Unwinder> unwinder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sampling_thread_sequence_checker_);
  // Unwinder would already be initialized on the SamplingThread.
  unwinders_.push_front(std::move(unwinder));
}

}  // namespace base
