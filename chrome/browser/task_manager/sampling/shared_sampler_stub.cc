// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/sampling/shared_sampler.h"

#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/browser_thread.h"

namespace task_manager {

SharedSampler::SharedSampler(
    const scoped_refptr<base::SequencedTaskRunner>& blocking_pool_runner) {}

SharedSampler::~SharedSampler() {}

int64_t SharedSampler::GetSupportedFlags() const {
  return 0;
}

void SharedSampler::RegisterCallback(
    base::ProcessId process_id,
    OnSamplingCompleteCallback on_sampling_complete) {}

void SharedSampler::UnregisterCallback(base::ProcessId process_id) {}

void SharedSampler::Refresh(base::ProcessId process_id, int64_t refresh_flags) {
}

}  // namespace task_manager
