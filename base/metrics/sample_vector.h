// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// SampleVector implements HistogramSamples interface. It is used by all
// Histogram based classes to store samples.

#ifndef BASE_METRICS_SAMPLE_VECTOR_H_
#define BASE_METRICS_SAMPLE_VECTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/bucket_ranges.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/persistent_memory_allocator.h"

namespace base {

class BucketRanges;

class BASE_EXPORT SampleVectorBase : public HistogramSamples {
 public:
  SampleVectorBase(const SampleVectorBase&) = delete;
  SampleVectorBase& operator=(const SampleVectorBase&) = delete;
  ~SampleVectorBase() override;

  // HistogramSamples:
  void Accumulate(HistogramBase::Sample value,
                  HistogramBase::Count count) override;
  HistogramBase::Count GetCount(HistogramBase::Sample value) const override;
  HistogramBase::Count TotalCount() const override;
  std::unique_ptr<SampleCountIterator> Iterator() const override;
  std::unique_ptr<SampleCountIterator> ExtractingIterator() override;

  // Get count of a specific bucket.
  HistogramBase::Count GetCountAtIndex(size_t bucket_index) const;

  // Access the bucket ranges held externally.
  const BucketRanges* bucket_ranges() const { return bucket_ranges_; }

 protected:
  SampleVectorBase(uint64_t id,
                   Metadata* meta,
                   const BucketRanges* bucket_ranges);
  SampleVectorBase(uint64_t id,
                   std::unique_ptr<Metadata> meta,
                   const BucketRanges* bucket_ranges);

  bool AddSubtractImpl(
      SampleCountIterator* iter,
      HistogramSamples::Operator op) override;  // |op| is ADD or SUBTRACT.

  virtual size_t GetBucketIndex(HistogramBase::Sample value) const;

  // Moves the single-sample value to a mounted "counts" array.
  void MoveSingleSampleToCounts();

  // Mounts (creating if necessary) an array of "counts" for multi-value
  // storage.
  void MountCountsStorageAndMoveSingleSample();

  // Mounts "counts" storage that already exists. This does not attempt to move
  // any single-sample information to that storage as that would violate the
  // "const" restriction that is often used to indicate read-only memory.
  virtual bool MountExistingCountsStorage() const = 0;

  // Creates "counts" storage and returns a pointer to it. Ownership of the
  // array remains with the called method but will never change. This must be
  // called while some sort of lock is held to prevent reentry.
  virtual HistogramBase::Count* CreateCountsStorageWhileLocked() = 0;

  HistogramBase::AtomicCount* counts() {
    return counts_.load(std::memory_order_acquire);
  }

  const HistogramBase::AtomicCount* counts() const {
    return counts_.load(std::memory_order_acquire);
  }

  void set_counts(HistogramBase::AtomicCount* counts) const {
    counts_.store(counts, std::memory_order_release);
  }

  size_t counts_size() const { return bucket_ranges_->bucket_count(); }

 private:
  friend class SampleVectorTest;
  FRIEND_TEST_ALL_PREFIXES(HistogramTest, CorruptSampleCounts);
  FRIEND_TEST_ALL_PREFIXES(SharedHistogramTest, CorruptSampleCounts);

  // |counts_| is actually a pointer to a HistogramBase::AtomicCount array but
  // is held as an atomic pointer for concurrency reasons. When combined with
  // the single_sample held in the metadata, there are four possible states:
  //   1) single_sample == zero, counts_ == null
  //   2) single_sample != zero, counts_ == null
  //   3) single_sample != zero, counts_ != null BUT IS EMPTY
  //   4) single_sample == zero, counts_ != null and may have data
  // Once |counts_| is set, it can never revert and any existing single-sample
  // must be moved to this storage. It is mutable because changing it doesn't
  // change the (const) data but must adapt if a non-const object causes the
  // storage to be allocated and updated.
  mutable std::atomic<HistogramBase::AtomicCount*> counts_{nullptr};

  // Shares the same BucketRanges with Histogram object.
  const raw_ptr<const BucketRanges> bucket_ranges_;
};

// A sample vector that uses local memory for the counts array.
class BASE_EXPORT SampleVector : public SampleVectorBase {
 public:
  explicit SampleVector(const BucketRanges* bucket_ranges);
  SampleVector(uint64_t id, const BucketRanges* bucket_ranges);
  SampleVector(const SampleVector&) = delete;
  SampleVector& operator=(const SampleVector&) = delete;
  ~SampleVector() override;

  // HistogramSamples:
  bool IsDefinitelyEmpty() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SampleVectorTest, GetPeakBucketSize);

  // HistogramSamples:
  std::string GetAsciiBody() const override;
  std::string GetAsciiHeader(StringPiece histogram_name,
                             int32_t flags) const override;

  // SampleVectorBase:
  bool MountExistingCountsStorage() const override;
  HistogramBase::Count* CreateCountsStorageWhileLocked() override;

  // Writes cumulative percentage information based on the number
  // of past, current, and remaining bucket samples.
  void WriteAsciiBucketContext(int64_t past,
                               HistogramBase::Count current,
                               int64_t remaining,
                               uint32_t current_bucket_index,
                               std::string* output) const;

  // Finds out how large (graphically) the largest bucket will appear to be.
  double GetPeakBucketSize() const;

  size_t bucket_count() const { return bucket_ranges()->bucket_count(); }

  // Simple local storage for counts.
  mutable std::vector<HistogramBase::AtomicCount> local_counts_;
};

// A sample vector that uses persistent memory for the counts array.
class BASE_EXPORT PersistentSampleVector : public SampleVectorBase {
 public:
  PersistentSampleVector(uint64_t id,
                         const BucketRanges* bucket_ranges,
                         Metadata* meta,
                         const DelayedPersistentAllocation& counts);
  PersistentSampleVector(const PersistentSampleVector&) = delete;
  PersistentSampleVector& operator=(const PersistentSampleVector&) = delete;
  ~PersistentSampleVector() override;

  // HistogramSamples:
  bool IsDefinitelyEmpty() const override;

 private:
  // SampleVectorBase:
  bool MountExistingCountsStorage() const override;
  HistogramBase::Count* CreateCountsStorageWhileLocked() override;

  // Persistent storage for counts.
  DelayedPersistentAllocation persistent_counts_;
};

}  // namespace base

#endif  // BASE_METRICS_SAMPLE_VECTOR_H_
