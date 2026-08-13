// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/persistent_histogram_allocator.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOGGING_FATAL); }
};

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> data) {
  static Environment env;

  FuzzedDataProvider provider(data.data(), data.size());
  // Fuzz allocator access modes.
  const auto access_mode = provider.PickValueInArray({
      base::PersistentMemoryAllocator::kReadOnly,
      base::PersistentMemoryAllocator::kReadWrite,
      base::PersistentMemoryAllocator::kReadWriteExisting,
  });

  // Keep track of whether the allocator is read-only or read-write to avoid
  // attempting to write to a read-only allocator.
  const bool readonly =
      (access_mode == base::PersistentMemoryAllocator::kReadOnly);
  std::vector<uint8_t> data_copy = provider.ConsumeRemainingBytes<uint8_t>();

  // `PersistentMemoryAllocator` segments must be aligned and an acceptable
  // size.
  if (!base::PersistentMemoryAllocator::IsMemoryAcceptable(
          data_copy.data(), data_copy.size(), /*page_size=*/0, readonly)) {
    return 0;
  }

  // When `access_mode == kReadWrite`, the constructor will write into a new
  // memory segment if `cookie != kGlobalCookie`. In `kReadWrite` mode, ensure
  // memory segment is large enough for `SharedMetadata + BlockHeader`.
  if (access_mode == base::PersistentMemoryAllocator::kReadWrite &&
      data_copy.size() < 80) {
    return 0;
  }

  std::unique_ptr<base::PersistentMemoryAllocator> memory_allocator =
      std::make_unique<base::PersistentMemoryAllocator>(
          data_copy.data(), data_copy.size(), /*page_size=*/0, /*id=*/0,
          /*name=*/"", access_mode);

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

    // Read-only allocators cannot update logged samples in persistent memory,
    // so merge final deltas instead.
    // TODO(crbug.com/489919375): Fuzz the name_override parameter.
    if (readonly) {
      histogram_allocator->MergeHistogramFinalDeltaToStatisticsRecorder(
          histogram.get(), /*name_override=*/"");
    } else {
      histogram_allocator->MergeHistogramDeltaToStatisticsRecorder(
          histogram.get(), /*name_override=*/"");
    }
  }

  return 0;
}
