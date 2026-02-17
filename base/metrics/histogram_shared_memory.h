// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_HISTOGRAM_SHARED_MEMORY_H_
#define BASE_METRICS_HISTOGRAM_SHARED_MEMORY_H_

#include <optional>
#include <string_view>

#include "base/base_export.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"

#if !BUILDFLAG(USE_BLINK)
#error "This is only intended for platforms that use blink."
#endif

namespace base {

BASE_EXPORT BASE_DECLARE_FEATURE(kPassHistogramSharedMemoryOnLaunch);

// Helper structure to create and return a shared memory region and a histogram
// allocator over top of it. Once returned it is expected that the caller will
// move both the memory regions and the allocator out of the struct and into
// it's own appropriate state variables. Note that the memory region must
// outlive the allocator.
struct BASE_EXPORT HistogramSharedMemory {
  HistogramSharedMemory() = delete;
  ~HistogramSharedMemory() = delete;
  HistogramSharedMemory(HistogramSharedMemory&) = delete;
  HistogramSharedMemory(HistogramSharedMemory&&) = delete;
  HistogramSharedMemory& operator=(HistogramSharedMemory&) = delete;
  HistogramSharedMemory& operator=(HistogramSharedMemory&&) = delete;

  // Configuration with which to create a histogram shared memory region and
  // allocator. Note the expectation that this be initialized with static
  // data for the allocator name (i.e., a string literal or static constant
  // character array).
  struct BASE_EXPORT Config {
    const int process_type;  // See: content/public/common/process_type.h
    const std::string_view allocator_name;
    const size_t memory_size_bytes;
  };

  // Temporary structure used to return the shared memory region and allocator
  // created by the |Create| factory function. The caller is expected to move
  // the returned values out of this struct.
  struct BASE_EXPORT SharedMemory {
    UnsafeSharedMemoryRegion region;
    std::unique_ptr<PersistentMemoryAllocator> allocator;

    SharedMemory(UnsafeSharedMemoryRegion,
                 std::unique_ptr<PersistentMemoryAllocator>);
    ~SharedMemory();

    // Movable
    SharedMemory(SharedMemory&&);
    SharedMemory& operator=(SharedMemory&&);

    // Not copyable
    SharedMemory(SharedMemory&) = delete;
    SharedMemory& operator=(SharedMemory&) = delete;
  };

  // Factory to initialize a shared |memory_region| and |allocator| for
  // |process_id| based on |config|. On success, returns true and updates
  // the values of |memory_region| and |allocator|. On failure, returns false
  // and |memory_region| and |allocator| are unchanged.
  static std::optional<SharedMemory> Create(int process_id,
                                            const Config& config);

  // Returns true if passing the shared memory handle via command-line arguments
  // is enabled. |process_type| values should come from content:::ProcessType.
  static bool PassOnCommandLineIsEnabled(int process_type);

  // Initialize the (global) histogram shared memory from the launch parameters.
  // This should be called in the child process before any histogram samples are
  // recorded.
  static void InitFromLaunchParameters(const CommandLine& command_line);
};

}  // namespace base

#endif  // BASE_METRICS_HISTOGRAM_SHARED_MEMORY_H_
