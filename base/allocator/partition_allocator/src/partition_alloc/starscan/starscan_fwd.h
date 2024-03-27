// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_STARSCAN_STARSCAN_FWD_H_
#define PARTITION_ALLOC_STARSCAN_STARSCAN_FWD_H_

#include <cstdint>

namespace partition_alloc::internal {

// Defines what thread executes a StarScan task.
enum class Context {
  // For tasks executed from mutator threads (safepoints).
  kMutator,
  // For concurrent scanner tasks.
  kScanner
};

// Defines ISA extension for scanning.
enum class SimdSupport : uint8_t {
  kUnvectorized,
  kSSE41,
  kAVX2,
  kNEON,
};

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_STARSCAN_STARSCAN_FWD_H_
