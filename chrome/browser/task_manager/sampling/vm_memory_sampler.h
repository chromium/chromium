// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_SAMPLING_VM_MEMORY_SAMPLER_H_
#define CHROME_BROWSER_TASK_MANAGER_SAMPLING_VM_MEMORY_SAMPLER_H_

#include <stdint.h>

#include <optional>

#include "base/functional/callback.h"
#include "base/process/process_handle.h"

namespace task_manager {

// Samples the memory footprint of processes running inside a VM (e.g. ARC on
// ChromeOS), which cannot be sampled like regular child processes.
// ArcSharedSampler implements it.
class VmMemorySampler {
 public:
  using MemoryFootprintBytes = uint64_t;

  using OnSamplingCompleteCallback =
      base::RepeatingCallback<void(std::optional<MemoryFootprintBytes>)>;

  virtual ~VmMemorySampler() = default;

  // Registers task group specific callback.
  virtual void RegisterCallback(
      base::ProcessId process_id,
      OnSamplingCompleteCallback on_sampling_complete) = 0;

  // Unregisters task group specific callbacks.
  virtual void UnregisterCallback(base::ProcessId process_id) = 0;

  // Triggers a refresh of process stats.
  virtual void Refresh() = 0;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_SAMPLING_VM_MEMORY_SAMPLER_H_
