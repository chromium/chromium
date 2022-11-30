// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/memory_ablation_study.h"

#include <algorithm>
#include <cstring>

#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/system/sys_info.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "crypto/random.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace memory {

const char kUXStudy1Switch[] = "ux-study-1";
const char kUXStudy1A[] = "A";
const char kUXStudy1B[] = "B";
const char kUXStudy1C[] = "C";
const char kUXStudy1D[] = "D";

namespace {

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with "UXStudy1Arm" in
// src/tools/metrics/histograms/enums.xml.
enum class UXStudy1Arm {
  A = 0,
  B = 1,
  C = 2,
  D = 3,
  kMaxValue = D,
};

// The name of the Finch study that turns on the experiment.
BASE_FEATURE(kMemoryAblationStudy,
             "MemoryAblationStudy",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The total amount of memory to ablate in MB.
const char kAblationSizeMb[] = "ablation-size-mb";

// Number of seconds to wait between allocation periods.
constexpr int kAllocateTimerIntervalSeconds = 10;

// Maximum number of MB to allocate at a time.
constexpr int kAllocateAmountMb = 10;

// Numbers of seconds to wait between reading the next region.
constexpr int kReadTimerIntervalSeconds = 30;

// Size in bytes of the uncompressible region.
constexpr int kUncompressibleRegionSize = 4096;

// Theres some variance on exact ram values so we use values slightly under 2GB
// and 4GB.
#if BUILDFLAG(IS_ANDROID)
constexpr int kMinimumRamMB = 1700;
#else
constexpr int kMinimumRamMB = 3700;
#endif

// The forced amount to ablate in MB, set by command line.
absl::optional<int32_t> ForcedAblationMB() {
  base::CommandLine* line = base::CommandLine::ForCurrentProcess();
  std::string value = line->GetSwitchValueASCII(kUXStudy1Switch);

  int32_t ablation = 0;
  UXStudy1Arm study = UXStudy1Arm::A;
  if (value == kUXStudy1A) {
    ablation = 0;
    study = UXStudy1Arm::A;
  } else if (value == kUXStudy1B) {
    ablation = 300;
    study = UXStudy1Arm::B;
  } else if (value == kUXStudy1C) {
    ablation = 700;
    study = UXStudy1Arm::C;
  } else if (value == kUXStudy1D) {
    ablation = 1000;
    study = UXStudy1Arm::D;
  } else {
    return absl::nullopt;
  }
  UMA_HISTOGRAM_ENUMERATION("Memory.UX.Study.1", study);
  return ablation;
}

}  // namespace

MemoryAblationStudy::MemoryAblationStudy() {
  absl::optional<int32_t> forced_ablation = ForcedAblationMB();
  if (forced_ablation) {
    remaining_allocation_mb_ = forced_ablation.value();
  } else {
    // On Android we restrict to 2GB+ devices.
    // On Desktop we restrict to 4GB+ devices.
    if (base::SysInfo::AmountOfPhysicalMemoryMB() < kMinimumRamMB)
      return;

    // This class does nothing if the study is disabled.
    if (!base::FeatureList::IsEnabled(kMemoryAblationStudy)) {
      return;
    }

    remaining_allocation_mb_ = base::GetFieldTrialParamByFeatureAsInt(
        kMemoryAblationStudy, kAblationSizeMb, /*default_value=*/0);
  }
  if (remaining_allocation_mb_ <= 0)
    return;

  allocate_timer_.Start(FROM_HERE, base::Seconds(kAllocateTimerIntervalSeconds),
                        base::BindRepeating(&MemoryAblationStudy::Allocate,
                                            base::Unretained(this)));
  read_timer_.Start(
      FROM_HERE, base::Seconds(kReadTimerIntervalSeconds),
      base::BindRepeating(&MemoryAblationStudy::Read, base::Unretained(this)));
}

MemoryAblationStudy::~MemoryAblationStudy() {
  // Avoid compiler optimizations by aliasing |dummy_read_|.
  uint8_t dummy = dummy_read_;
  base::debug::Alias(&dummy);
}

void MemoryAblationStudy::Allocate() {
  CHECK_GT(remaining_allocation_mb_, 0);
  int32_t amount_to_allocate_mb =
      std::min(remaining_allocation_mb_, kAllocateAmountMb);

  // Do accounting first. Stop the timer if this is the last allocation.
  remaining_allocation_mb_ -= amount_to_allocate_mb;
  if (remaining_allocation_mb_ <= 0) {
    allocate_timer_.Stop();
  }

  // Generate the initial uncompressible region if necessary.
  if (uncompressible_region_.empty()) {
    uncompressible_region_.resize(kUncompressibleRegionSize);
    crypto::RandBytes(uncompressible_region_.data(), kUncompressibleRegionSize);
  }

  // Allocate the new region.
  size_t amount_to_allocate_bytes = amount_to_allocate_mb * 1024 * 1024;
  Region region;
  region.resize(amount_to_allocate_bytes);

  // Fill it with uncompressible bytes.
  DCHECK_EQ(amount_to_allocate_bytes % kUncompressibleRegionSize, 0u);
  uint8_t* ptr = region.data();
  for (size_t offset = 0; offset < amount_to_allocate_bytes;
       offset += kUncompressibleRegionSize) {
    memcpy(ptr + offset, uncompressible_region_.data(),
           kUncompressibleRegionSize);
  }

  regions_.push_back(std::move(region));
}

void MemoryAblationStudy::Read() {
  if (regions_.empty())
    return;

  last_region_read_ = ((last_region_read_ + 1) % regions_.size());
  Region& region = regions_[last_region_read_];
  uint8_t* ptr = region.data();
  for (size_t offset = 0; offset < region.size();
       offset += kUncompressibleRegionSize) {
    dummy_read_ += ptr[offset];
  }
}

}  // namespace memory
