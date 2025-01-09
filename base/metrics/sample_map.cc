// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/sample_map.h"

#include <stdint.h>

#include <memory>

#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/sample_map_iterator.h"
#include "base/numerics/wrapping_math.h"

namespace base {

using Count = HistogramBase::Count;
using Sample32 = HistogramBase::Sample32;

SampleMap::SampleMap(uint64_t id)
    : HistogramSamples(id, std::make_unique<LocalMetadata>()) {}

SampleMap::~SampleMap() = default;

void SampleMap::Accumulate(Sample32 value, Count count) {
  // We do not have to do the following atomically -- if the caller needs
  // thread safety, they should use a lock. And since this is in local memory,
  // if a lock is used, we know the value would not be concurrently modified
  // by a different process (in contrast to PersistentSampleMap, where the
  // value in shared memory may be modified concurrently by a subprocess).
  sample_counts_[value] += count;
  IncreaseSumAndCount(strict_cast<int64_t>(count) * value, count);
}

Count SampleMap::GetCount(Sample32 value) const {
  const auto it = sample_counts_.find(value);
  return (it == sample_counts_.end()) ? 0 : it->second;
}

Count SampleMap::TotalCount() const {
  Count count = 0;
  for (const auto& entry : sample_counts_) {
    count += entry.second;
  }
  return count;
}

std::unique_ptr<SampleCountIterator> SampleMap::Iterator() const {
  return std::make_unique<SampleMapIterator<SampleToCountMap, false>>(
      sample_counts_);
}

std::unique_ptr<SampleCountIterator> SampleMap::ExtractingIterator() {
  return std::make_unique<SampleMapIterator<SampleToCountMap, true>>(
      sample_counts_);
}

bool SampleMap::IsDefinitelyEmpty() const {
  // If |sample_counts_| is empty (no entry was ever inserted), then return
  // true. If it does contain some entries, then it may or may not have samples
  // (e.g. it's possible all entries have a bucket count of 0). Just return
  // false in this case. If we are wrong, this will just make the caller perform
  // some extra work thinking that |this| is non-empty.
  return HistogramSamples::IsDefinitelyEmpty() && sample_counts_.empty();
}

bool SampleMap::AddSubtractImpl(SampleCountIterator* iter, Operator op) {
  Sample32 min;
  int64_t max;
  Count count;
  for (; !iter->Done(); iter->Next()) {
    iter->Get(&min, &max, &count);
    if (int64_t{min} + 1 != max) {
      return false;  // SparseHistogram only supports bucket with size 1.
    }

    // Note that we do not need to check that count != 0, since Next() above
    // will skip empty buckets.

    // We do not have to do the following atomically -- if the caller needs
    // thread safety, they should use a lock. And since this is in local memory,
    // if a lock is used, we know the value would not be concurrently modified
    // by a different process (in contrast to PersistentSampleMap, where the
    // value in shared memory may be modified concurrently by a subprocess).
    Count& sample_ref = sample_counts_[min];
    if (op == HistogramSamples::ADD) {
      sample_ref = base::WrappingAdd(sample_ref, count);
    } else {
      sample_ref = base::WrappingSub(sample_ref, count);
    }
  }
  return true;
}

}  // namespace base
