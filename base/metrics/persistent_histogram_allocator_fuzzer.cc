// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"

struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOGGING_FATAL); }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  // Copy data into a non-const vector.
  std::vector<uint8_t> data_copy(data, data + size);

  // PersistentMemoryAllocator segments must be aligned and an acceptable size.
  if (!base::PersistentMemoryAllocator::IsMemoryAcceptable(
          data_copy.data(), data_copy.size(), /*page_size=*/0,
          /*readonly=*/false)) {
    return 0;
  }

  std::unique_ptr<base::PersistentMemoryAllocator> memory_allocator =
      std::make_unique<base::PersistentMemoryAllocator>(
          data_copy.data(), data_copy.size(), /*page_size=*/0, /*id=*/0,
          /*name=*/"",
          /*access_mode=*/
          base::FilePersistentMemoryAllocator::kReadWriteExisting);

  std::unique_ptr<base::PersistentHistogramAllocator> histogram_allocator =
      std::make_unique<base::PersistentHistogramAllocator>(
          std::move(memory_allocator));

  base::PersistentHistogramAllocator::Iterator hist_iter(
      histogram_allocator.get());
  while (true) {
    std::unique_ptr<base::HistogramBase> histogram = hist_iter.GetNext();
    if (!histogram) {
      break;
    }
    histogram_allocator->MergeHistogramDeltaToStatisticsRecorder(
        histogram.get());
  }

  return 0;
}
