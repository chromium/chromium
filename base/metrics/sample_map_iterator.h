// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_SAMPLE_MAP_ITERATOR_H_
#define BASE_METRICS_SAMPLE_MAP_ITERATOR_H_

#include <stdint.h>

#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"

namespace base {

// An iterator for going through a SampleMap. `MapT` is the underlying map type
// that stores the counts. `support_extraction` should be true iff the caller
// wants this iterator to support extracting the values.
// TODO(pkasting): Combine with that for PersistentSampleMap.
template <typename MapT, bool support_extraction>
class SampleMapIterator : public SampleCountIterator {
 private:
  using T = std::conditional_t<support_extraction, MapT, const MapT>;

 public:
  explicit SampleMapIterator(T& sample_counts)
      : iter_(sample_counts.begin()), end_(sample_counts.end()) {
    SkipEmptyBuckets();
  }

  ~SampleMapIterator() override {
    if constexpr (support_extraction) {
      // Ensure that the user has consumed all the samples in order to ensure no
      // samples are lost.
      DCHECK(Done());
    }
  }

  // SampleCountIterator:
  bool Done() const override { return iter_ == end_; }

  void Next() override {
    DCHECK(!Done());
    ++iter_;
    SkipEmptyBuckets();
  }

  void Get(HistogramBase::Sample* min,
           int64_t* max,
           HistogramBase::Count* count) override {
    DCHECK(!Done());
    *min = iter_->first;
    *max = int64_t{iter_->first} + 1;
    // We do not have to do the following atomically -- if the caller needs
    // thread safety, they should use a lock. And since this is in local memory,
    // if a lock is used, we know the value would not be concurrently modified
    // by a different process (in contrast to PersistentSampleMap, where the
    // value in shared memory may be modified concurrently by a subprocess).
    if constexpr (support_extraction) {
      *count = Exchange();
    } else {
      *count = Load();
    }
  }

 private:
  using I = std::conditional_t<support_extraction,
                               typename T::iterator,
                               typename T::const_iterator>;

  void SkipEmptyBuckets() {
    while (!Done() && Load() == 0) {
      ++iter_;
    }
  }

  HistogramBase::Count Load() const { return iter_->second; }

  HistogramBase::Count Exchange() const
    requires support_extraction
  {
    return std::exchange(iter_->second, 0);
  }

  I iter_;
  const I end_;
};

}  // namespace base

#endif  // BASE_METRICS_SAMPLE_MAP_ITERATOR_H_
