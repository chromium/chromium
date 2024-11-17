// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/persistent_sample_map.h"

#include "base/atomicops.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"

namespace base {

typedef HistogramBase::Count Count;
typedef HistogramBase::Sample Sample;

namespace {

// An iterator for going through a PersistentSampleMap. The logic here is
// identical to that of the iterator for SampleMap but with different data
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
    while (!Done() && subtle::NoBarrier_Load(iter_->second) == 0) {
      ++iter_;
    }
  }

  I iter_;
  const I end_;
};

typedef std::map<HistogramBase::Sample,
                 raw_ptr<HistogramBase::Count, CtnExperimental>>
    SampleToCountMap;
typedef IteratorTemplate<const SampleToCountMap,
                         SampleToCountMap::const_iterator>
    PersistentSampleMapIterator;

template <>
PersistentSampleMapIterator::~IteratorTemplate() = default;

// Get() for an iterator of a PersistentSampleMap.
template <>
void PersistentSampleMapIterator::Get(Sample* min, int64_t* max, Count* count) {
  DCHECK(!Done());
  *min = iter_->first;
  *max = strict_cast<int64_t>(iter_->first) + 1;
  // We have to do the following atomically, because even if the caller is using
  // a lock, a separate process (that is not aware of this lock) may
  // concurrently modify the value (note that iter_->second is a pointer to a
  // sample count, which may live in shared memory).
  *count = subtle::NoBarrier_Load(iter_->second);
}

typedef IteratorTemplate<SampleToCountMap, SampleToCountMap::iterator>
    ExtractingPersistentSampleMapIterator;

template <>
ExtractingPersistentSampleMapIterator::~IteratorTemplate() {
  // Ensure that the user has consumed all the samples in order to ensure no
  // samples are lost.
  DCHECK(Done());
}

// Get() for an extracting iterator of a PersistentSampleMap.
template <>
void ExtractingPersistentSampleMapIterator::Get(Sample* min,
                                                int64_t* max,
                                                Count* count) {
  DCHECK(!Done());
  *min = iter_->first;
  *max = strict_cast<int64_t>(iter_->first) + 1;
  // We have to do the following atomically, because even if the caller is using
  // a lock, a separate process (that is not aware of this lock) may
  // concurrently modify the value (note that iter_->second is a pointer to a
  // sample count, which may live in shared memory).
  *count = subtle::NoBarrier_AtomicExchange(iter_->second, 0);
}

// This structure holds an entry for a PersistentSampleMap within a persistent
// memory allocator. The "id" must be unique across all maps held by an
// allocator or they will get attached to the wrong sample map.
struct SampleRecord {
  // SHA1(SampleRecord): Increment this if structure changes!
  static constexpr uint32_t kPersistentTypeId = 0x8FE6A69F + 1;

  // Expected size for 32/64-bit check.
  static constexpr size_t kExpectedInstanceSize = 16;

  uint64_t id;   // Unique identifier of owner.
  Sample value;  // The value for which this record holds a count.
  Count count;   // The count associated with the above value.
};

}  // namespace

PersistentSampleMap::PersistentSampleMap(
    uint64_t id,
    PersistentHistogramAllocator* allocator,
    Metadata* meta)
    : HistogramSamples(id, meta), allocator_(allocator) {}

PersistentSampleMap::~PersistentSampleMap() = default;

void PersistentSampleMap::Accumulate(Sample value, Count count) {
  // We have to do the following atomically, because even if the caller is using
  // a lock, a separate process (that is not aware of this lock) may
  // concurrently modify the value.
  subtle::NoBarrier_AtomicIncrement(GetOrCreateSampleCountStorage(value),
                                    count);
  IncreaseSumAndCount(strict_cast<int64_t>(count) * value, count);
}

Count PersistentSampleMap::GetCount(Sample value) const {
  // Have to override "const" to make sure all samples have been loaded before
  // being able to know what value to return.
  Count* count_pointer =
      const_cast<PersistentSampleMap*>(this)->GetSampleCountStorage(value);
  return count_pointer ? subtle::NoBarrier_Load(count_pointer) : 0;
}

Count PersistentSampleMap::TotalCount() const {
  // Have to override "const" in order to make sure all samples have been
  // loaded before trying to iterate over the map.
  const_cast<PersistentSampleMap*>(this)->ImportSamples(
      /*until_value=*/std::nullopt);

  Count count = 0;
  for (const auto& entry : sample_counts_) {
    count += subtle::NoBarrier_Load(entry.second);
  }
  return count;
}

std::unique_ptr<SampleCountIterator> PersistentSampleMap::Iterator() const {
  // Have to override "const" in order to make sure all samples have been
  // loaded before trying to iterate over the map.
  const_cast<PersistentSampleMap*>(this)->ImportSamples(
      /*until_value=*/std::nullopt);
  return std::make_unique<PersistentSampleMapIterator>(sample_counts_);
}

std::unique_ptr<SampleCountIterator> PersistentSampleMap::ExtractingIterator() {
  // Make sure all samples have been loaded before trying to iterate over the
  // map.
  ImportSamples(/*until_value=*/std::nullopt);
  return std::make_unique<ExtractingPersistentSampleMapIterator>(
      sample_counts_);
}

bool PersistentSampleMap::IsDefinitelyEmpty() const {
  // Not implemented.
  NOTREACHED();
}

// static
PersistentMemoryAllocator::Reference
PersistentSampleMap::GetNextPersistentRecord(
    PersistentMemoryAllocator::Iterator& iterator,
    uint64_t* sample_map_id,
    Sample* value) {
  const SampleRecord* record = iterator.GetNextOfObject<SampleRecord>();
  if (!record) {
    return 0;
  }

  *sample_map_id = record->id;
  *value = record->value;
  return iterator.GetAsReference(record);
}

// static
PersistentMemoryAllocator::Reference
PersistentSampleMap::CreatePersistentRecord(
    PersistentMemoryAllocator* allocator,
    uint64_t sample_map_id,
    Sample value) {
  SampleRecord* record = allocator->New<SampleRecord>();
  if (!record) {
    if (!allocator->IsFull()) {
#if !BUILDFLAG(IS_NACL)
      // TODO(crbug.com/40064026): Remove these. They are used to investigate
      // unexpected failures.
      SCOPED_CRASH_KEY_BOOL("PersistentSampleMap", "corrupted",
                            allocator->IsCorrupt());
#endif  // !BUILDFLAG(IS_NACL)
      DUMP_WILL_BE_NOTREACHED() << "corrupt=" << allocator->IsCorrupt();
    }
    return 0;
  }

  record->id = sample_map_id;
  record->value = value;
  record->count = 0;

  PersistentMemoryAllocator::Reference ref = allocator->GetAsReference(record);
  allocator->MakeIterable(ref);
  return ref;
}

bool PersistentSampleMap::AddSubtractImpl(SampleCountIterator* iter,
                                          Operator op) {
  Sample min;
  int64_t max;
  Count count;
  for (; !iter->Done(); iter->Next()) {
    iter->Get(&min, &max, &count);
    if (count == 0)
      continue;
    if (strict_cast<int64_t>(min) + 1 != max)
      return false;  // SparseHistogram only supports bucket with size 1.

    // We have to do the following atomically, because even if the caller is
    // using a lock, a separate process (that is not aware of this lock) may
    // concurrently modify the value.
    subtle::Barrier_AtomicIncrement(
        GetOrCreateSampleCountStorage(min),
        (op == HistogramSamples::ADD) ? count : -count);
  }
  return true;
}

Count* PersistentSampleMap::GetSampleCountStorage(Sample value) {
  // If |value| is already in the map, just return that.
  auto it = sample_counts_.find(value);
  if (it != sample_counts_.end())
    return it->second;

  // Import any new samples from persistent memory looking for the value.
  return ImportSamples(/*until_value=*/value);
}

Count* PersistentSampleMap::GetOrCreateSampleCountStorage(Sample value) {
  // Get any existing count storage.
  Count* count_pointer = GetSampleCountStorage(value);
  if (count_pointer)
    return count_pointer;

  // Create a new record in persistent memory for the value. |records_| will
  // have been initialized by the GetSampleCountStorage() call above.
  CHECK(records_);
  PersistentMemoryAllocator::Reference ref = records_->CreateNew(value);
  if (!ref) {
    // If a new record could not be created then the underlying allocator is
    // full or corrupt. Instead, allocate the counter from the heap. This
    // sample will not be persistent, will not be shared, and will leak...
    // but it's better than crashing.
    count_pointer = new Count(0);
    sample_counts_[value] = count_pointer;
    return count_pointer;
  }

  // A race condition between two independent processes (i.e. two independent
  // histogram objects sharing the same sample data) could cause two of the
  // above records to be created. The allocator, however, forces a strict
  // ordering on iterable objects so use the import method to actually add the
  // just-created record. This ensures that all PersistentSampleMap objects
  // will always use the same record, whichever was first made iterable.
  // Thread-safety within a process where multiple threads use the same
  // histogram object is delegated to the controlling histogram object which,
  // for sparse histograms, is a lock object.
  count_pointer = ImportSamples(/*until_value=*/value);
  DCHECK(count_pointer);
  return count_pointer;
}

PersistentSampleMapRecords* PersistentSampleMap::GetRecords() {
  // The |records_| pointer is lazily fetched from the |allocator_| only on
  // first use. Sometimes duplicate histograms are created by race conditions
  // and if both were to grab the records object, there would be a conflict.
  // Use of a histogram, and thus a call to this method, won't occur until
  // after the histogram has been de-dup'd.
  if (!records_) {
    records_ = allocator_->CreateSampleMapRecords(id());
  }
  return records_.get();
}

Count* PersistentSampleMap::ImportSamples(std::optional<Sample> until_value) {
  std::vector<PersistentMemoryAllocator::Reference> refs;
  PersistentSampleMapRecords* records = GetRecords();
  while (!(refs = records->GetNextRecords(until_value)).empty()) {
    // GetNextRecords() returns a list of new unseen records belonging to this
    // map. Iterate through them all and store them internally. Note that if
    // |until_value| was found, it will be the last element in |refs|.
    for (auto ref : refs) {
      SampleRecord* record = records->GetAsObject<SampleRecord>(ref);
      if (!record) {
        continue;
      }

      DCHECK_EQ(id(), record->id);

      // Check if the record's value is already known.
      if (!Contains(sample_counts_, record->value)) {
        // No: Add it to map of known values.
        sample_counts_[record->value] = &record->count;
      } else {
        // Yes: Ignore it; it's a duplicate caused by a race condition -- see
        // code & comment in GetOrCreateSampleCountStorage() for details.
        // Check that nothing ever operated on the duplicate record.
        DCHECK_EQ(0, record->count);
      }

      // Check if it's the value being searched for and, if so, stop here.
      // Because race conditions can cause multiple records for a single value,
      // be sure to return the first one found.
      if (until_value.has_value() && record->value == until_value.value()) {
        // Ensure that this was the last value in |refs|.
        CHECK_EQ(refs.back(), ref);

        return &record->count;
      }
    }
  }

  return nullptr;
}

}  // namespace base
