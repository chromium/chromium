// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/ranges_manager.h"

namespace base {

RangesManager::RangesManager() = default;

RangesManager::~RangesManager() {
  if (!do_not_release_ranges_on_destroy_for_testing_)
    ReleaseBucketRanges();
}

size_t RangesManager::BucketRangesHash::operator()(
    const BucketRanges* const a) const {
  return a->checksum();
}

bool RangesManager::BucketRangesEqual::operator()(
    const BucketRanges* const a,
    const BucketRanges* const b) const {
  return a->Equals(b);
}

const BucketRanges* RangesManager::GetOrRegisterCanonicalRanges(
    const BucketRanges* ranges) {
  // Note: This code is run in a critical lock path from StatisticsRecorder
  // so we intentionally don't use a CHECK() here.
  DCHECK(ranges->HasValidChecksum());

  // Attempt to insert |ranges| into the set of registered BucketRanges. If an
  // equivalent one already exists (one with the exact same ranges), this
  // fetches the pre-existing one and does not insert the passed |ranges|.
  return *ranges_.insert(ranges).first;
}

std::vector<const BucketRanges*> RangesManager::GetBucketRanges() const {
  std::vector<const BucketRanges*> out;
  out.reserve(ranges_.size());
  out.assign(ranges_.begin(), ranges_.end());
  return out;
}

void RangesManager::ReleaseBucketRanges() {
  for (auto* range : ranges_) {
    delete range;
  }
  ranges_.clear();
}

void RangesManager::DoNotReleaseRangesOnDestroyForTesting() {
  do_not_release_ranges_on_destroy_for_testing_ = true;
}

}  // namespace base
