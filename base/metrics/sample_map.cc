// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/sample_map.h"

#include <type_traits>

#include "base/check.h"
#include "base/numerics/safe_conversions.h"

namespace base {

typedef HistogramBase::Count Count;
typedef HistogramBase::Sample Sample;

namespace {

// An iterator for going through a SampleMap. The logic here is identical
// to that of the iterator for PersistentSampleMap but with different data
// structures. Changes here likely need to be duplicated there.
template <typename T, typename I>
class IteratorTemplate : public SampleCountIterator {
 public:
  explicit IteratorTemplate(T& sample_counts)
      : iter_(sample_counts.begin()), end_(sample_counts.end()) {
    SkipEmptyBuckets();
  }

  ~IteratorTemplate() override;

  // SampleCountIterator:
  bool Done() const override { return iter_ == end_; }
  void Next() override {
    DCHECK(!Done());
    ++iter_;
    SkipEmptyBuckets();
  }
  void Get(HistogramBase::Sample* min,
           int64_t* max,
           HistogramBase::Count* count) override;

 private:
  void SkipEmptyBuckets() {
    while (!Done() && iter_->second == 0) {
      ++iter_;
    }
  }

  I iter_;
  const I end_;
};

typedef std::map<HistogramBase::Sample, HistogramBase::Count> SampleToCountMap;
typedef IteratorTemplate<const SampleToCountMap,
                         SampleToCountMap::const_iterator>
    SampleMapIterator;

template <>
SampleMapIterator::~IteratorTemplate() = default;

// Get() for an iterator of a SampleMap.
template <>
void SampleMapIterator::Get(Sample* min, int64_t* max, Count* count) {
  DCHECK(!Done());
  *min = iter_->first;
  *max = strict_cast<int64_t>(iter_->first) + 1;
  // We do not have to do the following atomically -- if the caller needs thread
  // safety, they should use a lock. And since this is in local memory, if a
  // lock is used, we know the value would not be concurrently modified by a
  // different process (in contrast to PersistentSampleMap, where the value in
  // shared memory may be modified concurrently by a subprocess).
  *count = iter_->second;
}

typedef IteratorTemplate<SampleToCountMap, SampleToCountMap::iterator>
    ExtractingSampleMapIterator;

template <>
ExtractingSampleMapIterator::~IteratorTemplate() {
  // Ensure that the user has consumed all the samples in order to ensure no
  // samples are lost.
  DCHECK(Done());
}

// Get() for an extracting iterator of a SampleMap.
template <>
void ExtractingSampleMapIterator::Get(Sample* min, int64_t* max, Count* count) {
  DCHECK(!Done());
  *min = iter_->first;
  *max = strict_cast<int64_t>(iter_->first) + 1;
  // We do not have to do the following atomically -- if the caller needs thread
  // safety, they should use a lock. And since this is in local memory, if a
  // lock is used, we know the value would not be concurrently modified by a
  // different process (in contrast to PersistentSampleMap, where the value in
  // shared memory may be modified concurrently by a subprocess).
  *count = iter_->second;
  iter_->second = 0;
}

}  // namespace

SampleMap::SampleMap() : SampleMap(0) {}

SampleMap::SampleMap(uint64_t id)
    : HistogramSamples(id, std::make_unique<LocalMetadata>()) {}

SampleMap::~SampleMap() = default;

void SampleMap::Accumulate(Sample value, Count count) {
  // We do not have to do the following atomically -- if the caller needs
  // thread safety, they should use a lock. And since this is in local memory,
  // if a lock is used, we know the value would not be concurrently modified
  // by a different process (in contrast to PersistentSampleMap, where the
  // value in shared memory may be modified concurrently by a subprocess).
  sample_counts_[value] += count;
  IncreaseSumAndCount(strict_cast<int64_t>(count) * value, count);
}

Count SampleMap::GetCount(Sample value) const {
  auto it = sample_counts_.find(value);
  if (it == sample_counts_.end())
    return 0;
  return it->second;
}

Count SampleMap::TotalCount() const {
  Count count = 0;
  for (const auto& entry : sample_counts_) {
    count += entry.second;
  }
  return count;
}

std::unique_ptr<SampleCountIterator> SampleMap::Iterator() const {
  return std::make_unique<SampleMapIterator>(sample_counts_);
}

std::unique_ptr<SampleCountIterator> SampleMap::ExtractingIterator() {
  return std::make_unique<ExtractingSampleMapIterator>(sample_counts_);
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
  Sample min;
  int64_t max;
  Count count;
  for (; !iter->Done(); iter->Next()) {
    iter->Get(&min, &max, &count);
    if (strict_cast<int64_t>(min) + 1 != max) {
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
