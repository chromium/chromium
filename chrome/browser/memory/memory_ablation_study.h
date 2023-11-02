// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_MEMORY_ABLATION_STUDY_H_
#define CHROME_BROWSER_MEMORY_MEMORY_ABLATION_STUDY_H_

#include <cstdint>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/timer/timer.h"

namespace memory {

// A command-line switch that can also be set from chrome://flags that chooses
// an ablation group.
extern const char kUXStudy1Switch[];
extern const char kUXStudy1A[];
extern const char kUXStudy1B[];
extern const char kUXStudy1C[];
extern const char kUXStudy1D[];

// This class is the implementation of a memory ablation study. It artificially
// increases memory usage for different experiment arms.
class MemoryAblationStudy {
 public:
  MemoryAblationStudy();
  MemoryAblationStudy(const MemoryAblationStudy&) = delete;
  MemoryAblationStudy& operator=(const MemoryAblationStudy&) = delete;
  ~MemoryAblationStudy();

 private:
  FRIEND_TEST_ALL_PREFIXES(MemoryAblationStudyTest, Basic);

  using Region = std::vector<uint8_t>;

  // Allocates more memory if necessary. "remaining_allocation_mb_" is
  // guaranteed to be greater than 0.
  void Allocate();

  // Reads from all allocated pages.
  void Read();

  // The remaining amount of memory this class still needs to allocate, in
  // megabytes.
  int32_t remaining_allocation_mb_ = 0;

  // When this timer fires, more memory is allocated. This timer is stopped once
  // the desired allocation amount is reached.
  base::RepeatingTimer allocate_timer_;

  // When this timer fires, the class reads one byte from each allocated page to
  // ensure that the pages are resident.
  base::RepeatingTimer read_timer_;

  // A dummy variable used to help force pages to become resident.
  uint8_t dummy_read_ = 0;

  // The index of the last region that was read/paged-in.
  size_t last_region_read_ = 0;

  // Holds all allocated regions.
  std::vector<Region> regions_;

  // This region is exactly 4096 bytes in size and contains uncompressible data.
  Region uncompressible_region_;
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_MEMORY_ABLATION_STUDY_H_
