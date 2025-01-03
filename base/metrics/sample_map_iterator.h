// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_SAMPLE_MAP_ITERATOR_H_
#define BASE_METRICS_SAMPLE_MAP_ITERATOR_H_

#include <stdint.h>

#include "base/check.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"

namespace base {

// An iterator for going through a SampleMap.
// TODO(pkasting): Combine with that for PersistentSampleMap.
template <typename T, typename I>
class SampleMapIterator : public SampleCountIterator {
 public:
  explicit SampleMapIterator(T& sample_counts)
      : iter_(sample_counts.begin()), end_(sample_counts.end()) {
    SkipEmptyBuckets();
  }

  ~SampleMapIterator() override;

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

}  // namespace base

#endif  // BASE_METRICS_SAMPLE_MAP_ITERATOR_H_
