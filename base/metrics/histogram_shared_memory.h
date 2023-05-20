// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_HISTOGRAM_SHARED_MEMORY_H_
#define BASE_METRICS_HISTOGRAM_SHARED_MEMORY_H_

#include "base/base_export.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

// Configuration with which to create a histogram shared memory region and
// allocator.
struct BASE_EXPORT HistogramSharedMemoryConfig {
  base::StringPiece allocator_name;
  size_t memory_size_bytes;
};

// Helper structure to create and return a shared memory region and a histogram
// allocator over top of it. Once returned it is expected that the caller will
// move both the memory regions and the allocator out of the struct and into
// it's own appropriate state variables. Note that the memory region must
// outlive the allocator.
class BASE_EXPORT HistogramSharedMemory {
 public:
  HistogramSharedMemory();
  ~HistogramSharedMemory();

  // Move operations are supported.
  HistogramSharedMemory(HistogramSharedMemory&& other);
  HistogramSharedMemory& operator=(HistogramSharedMemory&& other);

  // Copy operations are NOT supported.
  HistogramSharedMemory(const HistogramSharedMemory&) = delete;
  HistogramSharedMemory& operator=(const HistogramSharedMemory&) = delete;

  // Factory to initialize a shared memory region for |unique_process_id|
  // based on |config|.
  static absl::optional<HistogramSharedMemory> Create(
      int unique_process_id,
      const HistogramSharedMemoryConfig& config);

  // Returns true if the memory region and allocator are valid.
  bool IsValid() const;

  // Returns, and transfers ownership of, the memory region to the caller.
  base::WritableSharedMemoryRegion TakeRegion();

  // Returns, and transfers ownership of, the memory allocator to the caller.
  std::unique_ptr<base::WritableSharedPersistentMemoryAllocator>
  TakeAllocator();

 private:
  // Internal constructor.
  HistogramSharedMemory(
      base::WritableSharedMemoryRegion region,
      std::unique_ptr<base::WritableSharedPersistentMemoryAllocator> allocator);

  // The shared memory region.
  base::WritableSharedMemoryRegion region_;

  // The shared memory allocator.
  std::unique_ptr<base::WritableSharedPersistentMemoryAllocator> allocator_;
};

}  // namespace base
#endif  // BASE_METRICS_HISTOGRAM_SHARED_MEMORY_H_
