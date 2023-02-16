// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/activity_analyzer.h"
#include "base/logging.h"
#include "base/metrics/persistent_memory_allocator.h"

struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOG_FATAL); }
};

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  if (size < 64u) {  // sizeof(base::PersistentMemoryAllocator::SharedMetadata)
    return 0;
  }

  std::unique_ptr<base::PersistentMemoryAllocator> allocator =
      std::make_unique<base::PersistentMemoryAllocator>(
          const_cast<uint8_t*>(data), size, /*page_size=*/0, /*id=*/0,
          /*name=*/"",
          /*read_only=*/true);

  base::debug::GlobalActivityAnalyzer gaa(std::move(allocator));
  std::ignore = gaa.GetFirstProcess();

  return 0;
}
