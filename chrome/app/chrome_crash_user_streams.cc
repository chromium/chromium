// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_crash_user_streams.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/sequence_checker.h"
#include "components/crash/core/common/shared_memory_user_stream_writer.h"
#include "components/metrics/system_profile_user_stream.h"

ChromeCrashUserStreams::ChromeCrashUserStreams()
    : system_profile_stream_(metrics::kSystemProfileUserStreamType,
                             metrics::kSystemProfileUserStreamCapacityBytes) {}

ChromeCrashUserStreams::~ChromeCrashUserStreams() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::vector<base::ReadOnlySharedMemoryRegion>
ChromeCrashUserStreams::GetSharedMemoryRegions() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<base::ReadOnlySharedMemoryRegion> regions;
  base::ReadOnlySharedMemoryRegion region =
      system_profile_stream_.DuplicateRegion();
  // `DuplicateRegion()` can fail if the OS exhausts its handles, file
  // descriptors, or Mach ports. In that case, gracefully degrade by dropping
  // the user stream rather than causing an unnecessary crash.
  if (region.IsValid()) {
    regions.push_back(std::move(region));
  }

  return regions;
}

bool ChromeCrashUserStreams::Write(uint32_t user_stream_type,
                                   base::span<const uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (user_stream_type != metrics::kSystemProfileUserStreamType) {
    return false;
  }

  // Callers must not exceed the stream's capacity: `copy_prefix_from()` can't
  // write a partial payload, and a truncated protobuf would be unusable anyway.
  return system_profile_stream_.WriteData(
      [&data](base::span<uint8_t> slot_span) -> std::optional<size_t> {
        slot_span.copy_prefix_from(data);
        return data.size();
      });
}
