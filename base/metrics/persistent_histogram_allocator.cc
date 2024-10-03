// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/metrics/persistent_histogram_allocator.h"

#include <atomic>
#include <limits>
#include <string_view>
#include <utility>

#include "base/debug/crash_logging.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/files/memory_mapped_file.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/persistent_sample_map.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/notreached.h"
#include "base/pickle.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"

namespace base {

namespace {

// Type identifiers used when storing in persistent memory so they can be
// identified during extraction; the first 4 bytes of the SHA1 of the name
// is used as a unique integer. A "version number" is added to the base
// so that, if the structure of that object changes, stored older versions
// will be safely ignored.
enum : uint32_t {
  kTypeIdRangesArray = 0xBCEA225A + 1,  // SHA1(RangesArray) v1
  kTypeIdCountsArray = 0x53215530 + 1,  // SHA1(CountsArray) v1
};

// The current globally-active persistent allocator for all new histograms.
// The object held here will obviously not be destructed at process exit
// but that's best since PersistentMemoryAllocator objects (that underlie
// GlobalHistogramAllocator objects) are explicitly forbidden from doing
// anything essential at exit anyway due to the fact that they depend on data
// managed elsewhere and which could be destructed first. An AtomicWord is
// used instead of std::atomic because the latter can create global ctors
// and dtors.
subtle::AtomicWord g_histogram_allocator = 0;

// Take an array of range boundaries and create a proper BucketRanges object
// which is returned to the caller. A return of nullptr indicates that the
// passed boundaries are invalid.
std::unique_ptr<BucketRanges> CreateRangesFromData(
    HistogramBase::Sample* ranges_data,
    uint32_t ranges_checksum,
    size_t count) {
  // To avoid racy destruction at shutdown, the following may be leaked.
  std::unique_ptr<BucketRanges> ranges(new BucketRanges(count));
  DCHECK_EQ(count, ranges->size());
  for (size_t i = 0; i < count; ++i) {
    if (i > 0 && ranges_data[i] <= ranges_data[i - 1])
      return nullptr;
    ranges->set_range(i, ranges_data[i]);
  }

  ranges->ResetChecksum();
  if (ranges->checksum() != ranges_checksum)
    return nullptr;

  return ranges;
}

// Calculate the number of bytes required to store all of a histogram's
// "counts". This will return zero (0) if |bucket_count| is not valid.
size_t CalculateRequiredCountsBytes(size_t bucket_count) {
  // 2 because each "sample count" also requires a backup "logged count"
  // used for calculating the delta during snapshot operations.
  const size_t kBytesPerBucket = 2 * sizeof(HistogramBase::AtomicCount);

  // If the |bucket_count| is such that it would overflow the return type,
  // perhaps as the result of a malicious actor, then return zero to
  // indicate the problem to the caller.
  if (bucket_count > std::numeric_limits<size_t>::max() / kBytesPerBucket)
    return 0;

  return bucket_count * kBytesPerBucket;
}

bool MergeSamplesToExistingHistogram(
    HistogramBase* existing,
    const HistogramBase* histogram,
    std::unique_ptr<HistogramSamples> samples) {
  // Check if the histograms match, which is necessary for merging their data.
  HistogramType existing_type = existing->GetHistogramType();
  if (existing_type == HistogramType::DUMMY_HISTOGRAM) {
    // Merging into a dummy histogram (e.g. histogram is expired) is a no-op and
    // not considered a failure case.
    return true;
  }
  if (histogram->GetHistogramType() != existing_type) {
    return false;  // Merge failed due to different histogram types.
  }

  if (existing_type == HistogramType::HISTOGRAM ||
      existing_type == HistogramType::LINEAR_HISTOGRAM ||
      existing_type == HistogramType::BOOLEAN_HISTOGRAM ||
      existing_type == HistogramType::CUSTOM_HISTOGRAM) {
    // Only numeric histograms make use of BucketRanges.
    const BucketRanges* existing_buckets =
        static_cast<const Histogram*>(existing)->bucket_ranges();
    const BucketRanges* histogram_buckets =
        static_cast<const Histogram*>(histogram)->bucket_ranges();
    // DCHECK because HasValidChecksum() recomputes the checksum which can be
    // expensive to do in a loop.
    DCHECK(existing_buckets->HasValidChecksum());
    DCHECK(histogram_buckets->HasValidChecksum());

    if (existing_buckets->checksum() != histogram_buckets->checksum()) {
      return false;  // Merge failed due to different buckets.
    }
  }

  // Merge the delta from the passed object to the one in the SR.

  // It's possible for the buckets to differ but their checksums to match due
  // to a collision, in which case AddSamples() will return false, which we
  // propagate to the caller (indicating histogram mismatch).
  return existing->AddSamples(*samples);
}

}  // namespace

PersistentSparseHistogramDataManager::PersistentSparseHistogramDataManager(
    PersistentMemoryAllocator* allocator)
    : allocator_(allocator), record_iterator_(allocator) {}

PersistentSparseHistogramDataManager::~PersistentSparseHistogramDataManager() =
    default;

std::unique_ptr<PersistentSampleMapRecords>
PersistentSparseHistogramDataManager::CreateSampleMapRecords(uint64_t id) {
  base::AutoLock auto_lock(lock_);
  return std::make_unique<PersistentSampleMapRecords>(
      this, id, GetSampleMapRecordsWhileLocked(id));
}

std::vector<PersistentSparseHistogramDataManager::ReferenceAndSample>*
PersistentSparseHistogramDataManager::GetSampleMapRecordsWhileLocked(
    uint64_t id) {
  auto* samples = &sample_records_[id];
  if (!samples->get()) {
    *samples = std::make_unique<std::vector<ReferenceAndSample>>();
  }
  return samples->get();
}

std::vector<PersistentMemoryAllocator::Reference>
PersistentSparseHistogramDataManager::LoadRecords(
    PersistentSampleMapRecords* sample_map_records,
    std::optional<HistogramBase::Sample> until_value) {
  // DataManager must be locked in order to access the |sample_records_|
  // vectors.
  base::AutoLock auto_lock(lock_);

  // Acquiring a lock is a semi-expensive operation so load some records with
  // each call. More than this number may be loaded if it takes longer to
  // find at least one matching record for the passed object.
  const size_t kMinimumNumberToLoad = 10;
  const uint64_t match_id = sample_map_records->sample_map_id_;

  // Loop while no entry is found OR we haven't yet loaded the minimum number.
  // This will continue reading even after a match is found. Note that it is
  // possible that entries for the passed object were already found in a
  // different call.
  auto& found_records = *sample_map_records->records_;
  bool found = (found_records.size() > sample_map_records->seen_);
  size_t new_records = 0;
  while (!found || new_records < kMinimumNumberToLoad) {
    // Get the next sample-record. The iterator will always resume from where
    // it left off even if it previously had nothing further to return.
    uint64_t found_id;
    HistogramBase::Sample value;
    PersistentMemoryAllocator::Reference ref =
        PersistentSampleMap::GetNextPersistentRecord(record_iterator_,
                                                     &found_id, &value);

    // Stop immediately if there are none.
    if (!ref) {
      break;
    }
    ++new_records;

    // The sample-record could be for any sparse histogram. Add the reference
    // to the appropriate collection for later use.
    if (found_id == match_id) {
      found_records.emplace_back(ref, value);
      found = true;
    } else {
      std::vector<ReferenceAndSample>* samples =
          GetSampleMapRecordsWhileLocked(found_id);
      CHECK(samples);
      samples->emplace_back(ref, value);
    }
  }

  // Return all references found that have not yet been seen by
  // |sample_map_records|, up until |until_value| (if applicable).
  std::vector<PersistentMemoryAllocator::Reference> new_references;
  CHECK_GE(found_records.size(), sample_map_records->seen_);
  auto new_found_records = base::make_span(found_records)
                               .subspan(/*offset=*/sample_map_records->seen_);
  new_references.reserve(new_found_records.size());
  for (const auto& new_record : new_found_records) {
    new_references.push_back(new_record.reference);
    // Maybe references after |until_value| were found. Stop here immediately in
    // such a case, since the caller will not expect any more samples after
    // |until_value|.
    if (until_value.has_value() && new_record.value == until_value.value()) {
      break;
    }
  }
  return new_references;
}

PersistentSampleMapRecords::PersistentSampleMapRecords(
    PersistentSparseHistogramDataManager* data_manager,
    uint64_t sample_map_id,
    std::vector<PersistentSparseHistogramDataManager::ReferenceAndSample>*
        records)
    : data_manager_(data_manager),
      sample_map_id_(sample_map_id),
      records_(records) {}

PersistentSampleMapRecords::~PersistentSampleMapRecords() = default;

std::vector<PersistentMemoryAllocator::Reference>
PersistentSampleMapRecords::GetNextRecords(
    std::optional<HistogramBase::Sample> until_value) {
  auto references = data_manager_->LoadRecords(this, until_value);
  seen_ += references.size();
  return references;
}

PersistentMemoryAllocator::Reference PersistentSampleMapRecords::CreateNew(
    HistogramBase::Sample value) {
  return PersistentSampleMap::CreatePersistentRecord(data_manager_->allocator_,
                                                     sample_map_id_, value);
}


// This data will be held in persistent memory in order for processes to
// locate and use histograms created elsewhere.
struct PersistentHistogramAllocator::PersistentHistogramData {
  // SHA1(Histogram): Increment this if structure changes!
  static constexpr uint32_t kPersistentTypeId = 0xF1645910 + 3;

  // Expected size for 32/64-bit check.
  static constexpr size_t kExpectedInstanceSize =
      40 + 2 * HistogramSamples::Metadata::kExpectedInstanceSize;

  int32_t histogram_type;
  int32_t flags;
  int32_t minimum;
  int32_t maximum;
  uint32_t bucket_count;
  PersistentMemoryAllocator::Reference ranges_ref;
  uint32_t ranges_checksum;
  std::atomic<PersistentMemoryAllocator::Reference> counts_ref;
  HistogramSamples::Metadata samples_metadata;
  HistogramSamples::Metadata logged_metadata;

  // Space for the histogram name will be added during the actual allocation
  // request. This must be the last field of the structure. A zero-size array
  // or a "flexible" array would be preferred but is not (yet) valid C++.
  char name[sizeof(uint64_t)];  // Force 64-bit alignment on 32-bit builds.
};

PersistentHistogramAllocator::Iterator::Iterator(
    PersistentHistogramAllocator* allocator)
    : allocator_(allocator), memory_iter_(allocator->memory_allocator()) {}

std::unique_ptr<HistogramBase>
PersistentHistogramAllocator::Iterator::GetNextWithIgnore(Reference ignore) {
  PersistentMemoryAllocator::Reference ref;
  while ((ref = memory_iter_.GetNextOfType<PersistentHistogramData>()) != 0) {
    if (ref != ignore)
      return allocator_->GetHistogram(ref);
  }
  return nullptr;
}

PersistentHistogramAllocator::PersistentHistogramAllocator(
    std::unique_ptr<PersistentMemoryAllocator> memory)
    : memory_allocator_(std::move(memory)),
      sparse_histogram_data_manager_(memory_allocator_.get()) {}

PersistentHistogramAllocator::~PersistentHistogramAllocator() = default;

std::unique_ptr<HistogramBase> PersistentHistogramAllocator::GetHistogram(
    Reference ref) {
  // Unfortunately, the histogram "pickle" methods cannot be used as part of
  // the persistance because the deserialization methods always create local
  // count data (while these must reference the persistent counts) and always
  // add it to the local list of known histograms (while these may be simple
  // references to histograms in other processes).
  PersistentHistogramData* data =
      memory_allocator_->GetAsObject<PersistentHistogramData>(ref);
  const size_t length = memory_allocator_->GetAllocSize(ref);

  // Check that metadata is reasonable: name is null-terminated and non-empty,
  // ID fields have been loaded with a hash of the name (0 is considered
  // unset/invalid).
  if (!data || data->name[0] == '\0' ||
      reinterpret_cast<char*>(data)[length - 1] != '\0' ||
      data->samples_metadata.id == 0 || data->logged_metadata.id == 0 ||
      // Note: Sparse histograms use |id + 1| in |logged_metadata|.
      (data->logged_metadata.id != data->samples_metadata.id &&
       data->logged_metadata.id != data->samples_metadata.id + 1) ||
      // Most non-matching values happen due to truncated names. Ideally, we
      // could just verify the name length based on the overall alloc length,
      // but that doesn't work because the allocated block may have been
      // aligned to the next boundary value.
      HashMetricName(data->name) != data->samples_metadata.id) {
    return nullptr;
  }
  return CreateHistogram(data);
}

std::unique_ptr<HistogramBase> PersistentHistogramAllocator::AllocateHistogram(
    HistogramType histogram_type,
    std::string_view name,
    int minimum,
    int maximum,
    const BucketRanges* bucket_ranges,
    int32_t flags,
    Reference* ref_ptr) {
  // If the allocator is corrupt, don't waste time trying anything else.
  // This also allows differentiating on the dashboard between allocations
  // failed due to a corrupt allocator and the number of process instances
  // with one, the latter being idicated by "newly corrupt", below.
  if (memory_allocator_->IsCorrupt())
    return nullptr;

  // Create the metadata necessary for a persistent sparse histogram. This
  // is done first because it is a small subset of what is required for
  // other histograms. The type is "under construction" so that a crash
  // during the datafill doesn't leave a bad record around that could cause
  // confusion by another process trying to read it. It will be corrected
  // once histogram construction is complete.
  PersistentHistogramData* histogram_data =
      memory_allocator_->New<PersistentHistogramData>(
          offsetof(PersistentHistogramData, name) + name.size() + 1);
  if (histogram_data) {
    memcpy(histogram_data->name, name.data(), name.size());
    histogram_data->name[name.size()] = '\0';
    histogram_data->histogram_type = histogram_type;
    histogram_data->flags = flags | HistogramBase::kIsPersistent;

    // |counts_ref| relies on being zero'd out initially. Even though this
    // should always be the case, manually zero it out again here in case there
    // was memory corruption (e.g. if the memory was mapped from a corrupted
    // spare file).
    // TODO(crbug.com/40064026): Remove this if this has no effect, and try to
    // understand better why there is sometimes garbage written in this field.
    histogram_data->counts_ref.store(0, std::memory_order_relaxed);
  }

  // Create the remaining metadata necessary for regular histograms.
  if (histogram_type != SPARSE_HISTOGRAM) {
    size_t bucket_count = bucket_ranges->bucket_count();
    size_t counts_bytes = CalculateRequiredCountsBytes(bucket_count);
    if (counts_bytes == 0) {
      // |bucket_count| was out-of-range.
      return nullptr;
    }

    // Since the StasticsRecorder keeps a global collection of BucketRanges
    // objects for re-use, it would be dangerous for one to hold a reference
    // from a persistent allocator that is not the global one (which is
    // permanent once set). If this stops being the case, this check can
    // become an "if" condition beside "!ranges_ref" below and before
    // set_persistent_reference() farther down.
    DCHECK_EQ(this, GlobalHistogramAllocator::Get());

    // Re-use an existing BucketRanges persistent allocation if one is known;
    // otherwise, create one.
    PersistentMemoryAllocator::Reference ranges_ref =
        bucket_ranges->persistent_reference();
    if (!ranges_ref) {
      size_t ranges_count = bucket_count + 1;
      size_t ranges_bytes = ranges_count * sizeof(HistogramBase::Sample);
      ranges_ref =
          memory_allocator_->Allocate(ranges_bytes, kTypeIdRangesArray);
      if (ranges_ref) {
        HistogramBase::Sample* ranges_data =
            memory_allocator_->GetAsArray<HistogramBase::Sample>(
                ranges_ref, kTypeIdRangesArray, ranges_count);
        if (ranges_data) {
          for (size_t i = 0; i < bucket_ranges->size(); ++i)
            ranges_data[i] = bucket_ranges->range(i);
          bucket_ranges->set_persistent_reference(ranges_ref);
        } else {
          // This should never happen but be tolerant if it does.
          ranges_ref = PersistentMemoryAllocator::kReferenceNull;
        }
      }
    } else {
      DCHECK_EQ(kTypeIdRangesArray, memory_allocator_->GetType(ranges_ref));
    }


    // Only continue here if all allocations were successful. If they weren't,
    // there is no way to free the space but that's not really a problem since
    // the allocations only fail because the space is full or corrupt and so
    // any future attempts will also fail.
    if (ranges_ref && histogram_data) {
      histogram_data->minimum = minimum;
      histogram_data->maximum = maximum;
      // |bucket_count| must fit within 32-bits or the allocation of the counts
      // array would have failed for being too large; the allocator supports
      // less than 4GB total size.
      histogram_data->bucket_count = static_cast<uint32_t>(bucket_count);
      histogram_data->ranges_ref = ranges_ref;
      histogram_data->ranges_checksum = bucket_ranges->checksum();
    } else {
      histogram_data = nullptr;  // Clear this for proper handling below.
    }
  }

  if (histogram_data) {
    // Create the histogram using resources in persistent memory. This ends up
    // resolving the "ref" values stored in histogram_data instad of just
    // using what is already known above but avoids duplicating the switch
    // statement here and serves as a double-check that everything is
    // correct before commiting the new histogram to persistent space.
    std::unique_ptr<HistogramBase> histogram = CreateHistogram(histogram_data);
    DCHECK(histogram);
    DCHECK_NE(0U, histogram_data->samples_metadata.id);
    DCHECK_NE(0U, histogram_data->logged_metadata.id);

    PersistentMemoryAllocator::Reference histogram_ref =
        memory_allocator_->GetAsReference(histogram_data);
    if (ref_ptr != nullptr)
      *ref_ptr = histogram_ref;

    // By storing the reference within the allocator to this histogram, the
    // next import (which will happen before the next histogram creation)
    // will know to skip it.
    // See also the comment in ImportHistogramsToStatisticsRecorder().
    last_created_.store(histogram_ref, std::memory_order_relaxed);
    return histogram;
  }

  return nullptr;
}

void PersistentHistogramAllocator::FinalizeHistogram(Reference ref,
                                                     bool registered) {
  if (registered) {
    // If the created persistent histogram was registered then it needs to
    // be marked as "iterable" in order to be found by other processes. This
    // happens only after the histogram is fully formed so it's impossible for
    // code iterating through the allocator to read a partially created record.
    memory_allocator_->MakeIterable(ref);
  } else {
    // If it wasn't registered then a race condition must have caused two to
    // be created. The allocator does not support releasing the acquired memory
    // so just change the type to be empty.
    memory_allocator_->ChangeType(ref, 0,
                                  PersistentHistogramData::kPersistentTypeId,
                                  /*clear=*/false);
  }
}

bool PersistentHistogramAllocator::MergeHistogramDeltaToStatisticsRecorder(
    HistogramBase* histogram) {
  DCHECK(histogram);

  // Return immediately if the histogram has no samples since the last delta
  // snapshot. This is to prevent looking up or registering the histogram with
  // the StatisticsRecorder, which requires acquiring a lock.
  std::unique_ptr<HistogramSamples> samples = histogram->SnapshotDelta();
  if (samples->IsDefinitelyEmpty()) {
    return true;
  }

  HistogramBase* existing = GetOrCreateStatisticsRecorderHistogram(histogram);
  if (!existing) {
    // The above should never fail but if it does, no real harm is done.
    // Some metric data will be lost but that is better than crashing.
    return false;
  }

  return MergeSamplesToExistingHistogram(existing, histogram,
                                         std::move(samples));
}

bool PersistentHistogramAllocator::MergeHistogramFinalDeltaToStatisticsRecorder(
    const HistogramBase* histogram) {
  DCHECK(histogram);

  // Return immediately if the histogram has no samples. This is to prevent
  // looking up or registering the histogram with the StatisticsRecorder, which
  // requires acquiring a lock.
  std::unique_ptr<HistogramSamples> samples = histogram->SnapshotFinalDelta();
  if (samples->IsDefinitelyEmpty()) {
    return true;
  }

  HistogramBase* existing = GetOrCreateStatisticsRecorderHistogram(histogram);
  if (!existing) {
    // The above should never fail but if it does, no real harm is done.
    // Some metric data will be lost but that is better than crashing.
    return false;
  }

  return MergeSamplesToExistingHistogram(existing, histogram,
                                         std::move(samples));
}

std::unique_ptr<PersistentSampleMapRecords>
PersistentHistogramAllocator::CreateSampleMapRecords(uint64_t id) {
  return sparse_histogram_data_manager_.CreateSampleMapRecords(id);
}

void PersistentHistogramAllocator::CreateTrackingHistograms(
    std::string_view name) {
  memory_allocator_->CreateTrackingHistograms(name);
}

void PersistentHistogramAllocator::UpdateTrackingHistograms() {
  memory_allocator_->UpdateTrackingHistograms();
}

void PersistentHistogramAllocator::SetRangesManager(
    RangesManager* ranges_manager) {
  ranges_manager_.reset(ranges_manager);
}

void PersistentHistogramAllocator::ClearLastCreatedReferenceForTesting() {
  last_created_.store(0, std::memory_order_relaxed);
}

std::unique_ptr<HistogramBase> PersistentHistogramAllocator::CreateHistogram(
    PersistentHistogramData* histogram_data_ptr) {
  if (!histogram_data_ptr)
    return nullptr;

  // Sparse histograms are quite different so handle them as a special case.
  if (histogram_data_ptr->histogram_type == SPARSE_HISTOGRAM) {
    std::unique_ptr<HistogramBase> histogram =
        SparseHistogram::PersistentCreate(this, histogram_data_ptr->name,
                                          &histogram_data_ptr->samples_metadata,
                                          &histogram_data_ptr->logged_metadata);
    DCHECK(histogram);
    histogram->SetFlags(histogram_data_ptr->flags);
    return histogram;
  }

  // Copy the configuration fields from histogram_data_ptr to local storage
  // because anything in persistent memory cannot be trusted as it could be
  // changed at any moment by a malicious actor that shares access. The local
  // values are validated below and then used to create the histogram, knowing
  // they haven't changed between validation and use.
  int32_t histogram_type = histogram_data_ptr->histogram_type;
  int32_t histogram_flags = histogram_data_ptr->flags;
  int32_t histogram_minimum = histogram_data_ptr->minimum;
  int32_t histogram_maximum = histogram_data_ptr->maximum;
  uint32_t histogram_bucket_count = histogram_data_ptr->bucket_count;
  uint32_t histogram_ranges_ref = histogram_data_ptr->ranges_ref;
  uint32_t histogram_ranges_checksum = histogram_data_ptr->ranges_checksum;

  HistogramBase::Sample* ranges_data =
      memory_allocator_->GetAsArray<HistogramBase::Sample>(
          histogram_ranges_ref, kTypeIdRangesArray,
          PersistentMemoryAllocator::kSizeAny);

  const uint32_t max_buckets =
      std::numeric_limits<uint32_t>::max() / sizeof(HistogramBase::Sample);
  size_t required_bytes =
      (histogram_bucket_count + 1) * sizeof(HistogramBase::Sample);
  size_t allocated_bytes =
      memory_allocator_->GetAllocSize(histogram_ranges_ref);
  if (!ranges_data || histogram_bucket_count < 2 ||
      histogram_bucket_count >= max_buckets ||
      allocated_bytes < required_bytes) {
    return nullptr;
  }

  std::unique_ptr<const BucketRanges> created_ranges = CreateRangesFromData(
      ranges_data, histogram_ranges_checksum, histogram_bucket_count + 1);
  if (!created_ranges || created_ranges->size() != histogram_bucket_count + 1 ||
      created_ranges->range(1) != histogram_minimum ||
      created_ranges->range(histogram_bucket_count - 1) != histogram_maximum) {
    return nullptr;
  }
  const BucketRanges* ranges;
  if (ranges_manager_) {
    ranges =
        ranges_manager_->GetOrRegisterCanonicalRanges(created_ranges.get());
    if (ranges == created_ranges.get()) {
      // `ranges_manager_` took ownership of `created_ranges`.
      created_ranges.release();
    }
  } else {
    ranges = StatisticsRecorder::RegisterOrDeleteDuplicateRanges(
        created_ranges.release());
  }

  size_t counts_bytes = CalculateRequiredCountsBytes(histogram_bucket_count);
  PersistentMemoryAllocator::Reference counts_ref =
      histogram_data_ptr->counts_ref.load(std::memory_order_acquire);
  if (counts_bytes == 0 ||
      (counts_ref != 0 &&
       memory_allocator_->GetAllocSize(counts_ref) < counts_bytes)) {
    return nullptr;
  }

  // The "counts" data (including both samples and logged samples) is a delayed
  // persistent allocation meaning that though its size and storage for a
  // reference is defined, no space is reserved until actually needed. When
  // it is needed, memory will be allocated from the persistent segment and
  // a reference to it stored at the passed address. Other threads can then
  // notice the valid reference and access the same data.
  DelayedPersistentAllocation counts_data(memory_allocator_.get(),
                                          &histogram_data_ptr->counts_ref,
                                          kTypeIdCountsArray, counts_bytes);

  // A second delayed allocations is defined using the same reference storage
  // location as the first so the allocation of one will automatically be found
  // by the other. Within the block, the first half of the space is for "counts"
  // and the second half is for "logged counts".
  DelayedPersistentAllocation logged_data(
      memory_allocator_.get(), &histogram_data_ptr->counts_ref,
      kTypeIdCountsArray, counts_bytes, counts_bytes / 2);

  // Create the right type of histogram.
  const char* name = histogram_data_ptr->name;
  std::unique_ptr<HistogramBase> histogram;
  switch (histogram_type) {
    case HISTOGRAM:
      histogram =
          Histogram::PersistentCreate(name, ranges, counts_data, logged_data,
                                      &histogram_data_ptr->samples_metadata,
                                      &histogram_data_ptr->logged_metadata);
      DCHECK(histogram);
      break;
    case LINEAR_HISTOGRAM:
      histogram = LinearHistogram::PersistentCreate(
          name, ranges, counts_data, logged_data,
          &histogram_data_ptr->samples_metadata,
          &histogram_data_ptr->logged_metadata);
      DCHECK(histogram);
      break;
    case BOOLEAN_HISTOGRAM:
      histogram = BooleanHistogram::PersistentCreate(
          name, ranges, counts_data, logged_data,
          &histogram_data_ptr->samples_metadata,
          &histogram_data_ptr->logged_metadata);
      DCHECK(histogram);
      break;
    case CUSTOM_HISTOGRAM:
      histogram = CustomHistogram::PersistentCreate(
          name, ranges, counts_data, logged_data,
          &histogram_data_ptr->samples_metadata,
          &histogram_data_ptr->logged_metadata);
      DCHECK(histogram);
      break;
    default:
      return nullptr;
  }

  if (histogram) {
    DCHECK_EQ(histogram_type, histogram->GetHistogramType());
    histogram->SetFlags(histogram_flags);
  }

  return histogram;
}

HistogramBase*
PersistentHistogramAllocator::GetOrCreateStatisticsRecorderHistogram(
    const HistogramBase* histogram) {
  // This should never be called on the global histogram allocator as objects
  // created there are already within the global statistics recorder.
  DCHECK_NE(GlobalHistogramAllocator::Get(), this);
  DCHECK(histogram);

  HistogramBase* existing =
      StatisticsRecorder::FindHistogram(histogram->histogram_name());
  if (existing) {
    return existing;
  }

  // Adding the passed histogram to the SR would cause a problem if the
  // allocator that holds it eventually goes away. Instead, create a new
  // one from a serialized version. Deserialization calls the appropriate
  // FactoryGet() which will create the histogram in the global persistent-
  // histogram allocator if such is set.
  base::Pickle pickle;
  histogram->SerializeInfo(&pickle);
  PickleIterator iter(pickle);
  existing = DeserializeHistogramInfo(&iter);
  if (!existing)
    return nullptr;

  // Make sure there is no "serialization" flag set.
  DCHECK(!existing->HasFlags(HistogramBase::kIPCSerializationSourceFlag));
  // Record the newly created histogram in the SR.
  return StatisticsRecorder::RegisterOrDeleteDuplicate(existing);
}

GlobalHistogramAllocator::~GlobalHistogramAllocator() {
  // GlobalHistogramAllocator should never be destroyed because Histogram
  // objects may keep pointers to its memory.
  NOTREACHED();
}

// static
void GlobalHistogramAllocator::CreateWithPersistentMemory(
    void* base,
    size_t size,
    size_t page_size,
    uint64_t id,
    std::string_view name) {
  Set(new GlobalHistogramAllocator(std::make_unique<PersistentMemoryAllocator>(
      base, size, page_size, id, name, PersistentMemoryAllocator::kReadWrite)));
}

// static
void GlobalHistogramAllocator::CreateWithLocalMemory(size_t size,
                                                     uint64_t id,
                                                     std::string_view name) {
  Set(new GlobalHistogramAllocator(
      std::make_unique<LocalPersistentMemoryAllocator>(size, id, name)));
}

#if !BUILDFLAG(IS_NACL)
// static
bool GlobalHistogramAllocator::CreateWithFile(const FilePath& file_path,
                                              size_t size,
                                              uint64_t id,
                                              std::string_view name,
                                              bool exclusive_write) {
  uint32_t flags = File::FLAG_OPEN_ALWAYS | File::FLAG_WIN_SHARE_DELETE |
                   File::FLAG_READ | File::FLAG_WRITE;
  if (exclusive_write)
    flags |= File::FLAG_WIN_EXCLUSIVE_WRITE;
  File file(file_path, flags);
  if (!file.IsValid())
    return false;

  std::unique_ptr<MemoryMappedFile> mmfile(new MemoryMappedFile());
  bool success = false;
  const bool file_created = file.created();
  if (file_created) {
    success = mmfile->Initialize(std::move(file), {0, size},
                                 MemoryMappedFile::READ_WRITE_EXTEND);
  } else {
    success = mmfile->Initialize(std::move(file), MemoryMappedFile::READ_WRITE);
  }
  if (!success ||
      !FilePersistentMemoryAllocator::IsFileAcceptable(*mmfile, true)) {
    if (file_created) {
      // If we created the file, but it couldn't be used, delete it.
      // This could happen if we were able to create a file of all-zeroes, but
      // couldn't write to it due to lack of disk space.
      base::DeleteFile(file_path);
    }
    return false;
  }

  Set(new GlobalHistogramAllocator(
      std::make_unique<FilePersistentMemoryAllocator>(
          std::move(mmfile), 0, id, name,
          PersistentMemoryAllocator::kReadWrite)));
  Get()->SetPersistentLocation(file_path);
  return true;
}

// static
bool GlobalHistogramAllocator::CreateWithActiveFile(const FilePath& base_path,
                                                    const FilePath& active_path,
                                                    const FilePath& spare_path,
                                                    size_t size,
                                                    uint64_t id,
                                                    std::string_view name) {
  // Old "active" becomes "base".
  if (!base::ReplaceFile(active_path, base_path, nullptr))
    base::DeleteFile(base_path);
  if (base::PathExists(active_path))
    return false;

  // Move any "spare" into "active". Okay to continue if file doesn't exist.
  if (!spare_path.empty())
    base::ReplaceFile(spare_path, active_path, nullptr);

  return base::GlobalHistogramAllocator::CreateWithFile(active_path, size, id,
                                                        name);
}

// static
bool GlobalHistogramAllocator::CreateWithActiveFileInDir(
    const FilePath& dir,
    size_t size,
    uint64_t id,
    std::string_view name) {
  FilePath base_path = ConstructFilePath(dir, name);
  FilePath active_path = ConstructFilePathForActiveFile(dir, name);
  FilePath spare_path = ConstructFilePath(dir, std::string(name) + "-spare");
  return CreateWithActiveFile(base_path, active_path, spare_path, size, id,
                              name);
}

// static
FilePath GlobalHistogramAllocator::ConstructFilePath(const FilePath& dir,
                                                     std::string_view name) {
  return dir.AppendASCII(name).AddExtension(
      PersistentMemoryAllocator::kFileExtension);
}

// static
FilePath GlobalHistogramAllocator::ConstructFilePathForActiveFile(
    const FilePath& dir,
    std::string_view name) {
  return ConstructFilePath(dir, std::string(name) + "-active");
}

// static
FilePath GlobalHistogramAllocator::ConstructFilePathForUploadDir(
    const FilePath& dir,
    std::string_view name,
    base::Time stamp,
    ProcessId pid) {
  return ConstructFilePath(
      dir,
      StringPrintf("%.*s-%lX-%lX", static_cast<int>(name.length()), name.data(),
                   static_cast<long>(stamp.ToTimeT()), static_cast<long>(pid)));
}

// static
FilePath GlobalHistogramAllocator::ConstructFilePathForUploadDir(
    const FilePath& dir,
    std::string_view name) {
  return ConstructFilePathForUploadDir(dir, name, Time::Now(),
                                       GetCurrentProcId());
}

// static
bool GlobalHistogramAllocator::ParseFilePath(const FilePath& path,
                                             std::string* out_name,
                                             Time* out_stamp,
                                             ProcessId* out_pid) {
  std::string filename = path.BaseName().AsUTF8Unsafe();
  std::vector<std::string_view> parts = base::SplitStringPiece(
      filename, "-.", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 4)
    return false;

  if (out_name)
    *out_name = std::string(parts[0]);

  if (out_stamp) {
    int64_t stamp;
    if (!HexStringToInt64(parts[1], &stamp))
      return false;
    *out_stamp = Time::FromTimeT(static_cast<time_t>(stamp));
  }

  if (out_pid) {
    int64_t pid;
    if (!HexStringToInt64(parts[2], &pid))
      return false;
    *out_pid = static_cast<ProcessId>(pid);
  }

  return true;
}

bool GlobalHistogramAllocator::CreateSpareFile(const FilePath& spare_path,
                                               size_t size) {
  // If the spare file already exists, it was created in a previous session and
  // is still unused, so do nothing.
  if (base::PathExists(spare_path)) {
    return false;
  }
  FilePath temp_spare_path = spare_path.AddExtension(FILE_PATH_LITERAL(".tmp"));
  bool success;
  {
    File spare_file(temp_spare_path, File::FLAG_CREATE_ALWAYS |
                                         File::FLAG_READ | File::FLAG_WRITE);
    success = spare_file.IsValid();

    if (success) {
      MemoryMappedFile mmfile;
      success = mmfile.Initialize(std::move(spare_file), {0, size},
                                  MemoryMappedFile::READ_WRITE_EXTEND);
    }
  }

  if (success)
    success = ReplaceFile(temp_spare_path, spare_path, nullptr);

  if (!success)
    DeleteFile(temp_spare_path);

  return success;
}
#endif  // !BUILDFLAG(IS_NACL)

// static
void GlobalHistogramAllocator::CreateWithSharedMemoryRegion(
    const UnsafeSharedMemoryRegion& region) {
  CHECK_EQ(Get(), nullptr) << "Histogram allocator has already been created";

  base::WritableSharedMemoryMapping mapping = region.Map();
  if (!mapping.IsValid() ||
      !WritableSharedPersistentMemoryAllocator::IsSharedMemoryAcceptable(
          mapping)) {
    DVLOG(1) << "Shared memory region is invalid or unacceptable.";
    return;
  }

  DVLOG(1) << "Global histogram allocator initialized.";
  Set(new GlobalHistogramAllocator(
      std::make_unique<WritableSharedPersistentMemoryAllocator>(
          std::move(mapping), 0, std::string_view())));
}

// static
void GlobalHistogramAllocator::Set(GlobalHistogramAllocator* allocator) {
  // Releasing or changing an allocator is extremely dangerous because it
  // likely has histograms stored within it. If the backing memory is also
  // also released, future accesses to those histograms will seg-fault.
  CHECK(!subtle::NoBarrier_Load(&g_histogram_allocator));
  subtle::Release_Store(&g_histogram_allocator,
                        reinterpret_cast<intptr_t>(allocator));

  // Record the number of histograms that were sampled before the global
  // histogram allocator was initialized.
  //
  // TODO(crbug.com/40945497): CHECK(histogram_count == 0) and remove emit of
  // early histogram count once |histogram_count| is reliably zero (0) for all
  // process types.
  size_t histogram_count = StatisticsRecorder::GetHistogramCount();
  if (histogram_count != 0) {
    DVLOG(1) << histogram_count
             << " histogram(s) created before persistence was enabled.";

    if (allocator && allocator->Name() && allocator->Name()[0]) {
      UmaHistogramCounts100(StrCat({"UMA.PersistentAllocator.EarlyHistograms.",
                                    allocator->Name()}),
                            static_cast<int>(histogram_count));
    }
  }
}

// static
GlobalHistogramAllocator* GlobalHistogramAllocator::Get() {
  return reinterpret_cast<GlobalHistogramAllocator*>(
      subtle::Acquire_Load(&g_histogram_allocator));
}

// static
GlobalHistogramAllocator* GlobalHistogramAllocator::ReleaseForTesting() {
  GlobalHistogramAllocator* histogram_allocator = Get();
  if (!histogram_allocator)
    return nullptr;
  PersistentMemoryAllocator* memory_allocator =
      histogram_allocator->memory_allocator();

  // Before releasing the memory, it's necessary to have the Statistics-
  // Recorder forget about the histograms contained therein; otherwise,
  // some operations will try to access them and the released memory.
  PersistentMemoryAllocator::Iterator iter(memory_allocator);
  const PersistentHistogramData* data;
  while ((data = iter.GetNextOfObject<PersistentHistogramData>()) != nullptr) {
    StatisticsRecorder::ForgetHistogramForTesting(data->name);
  }

  subtle::Release_Store(&g_histogram_allocator, 0);
  ANNOTATE_LEAKING_OBJECT_PTR(histogram_allocator);
  return histogram_allocator;
}

void GlobalHistogramAllocator::SetPersistentLocation(const FilePath& location) {
  persistent_location_ = location;
}

const FilePath& GlobalHistogramAllocator::GetPersistentLocation() const {
  return persistent_location_;
}

bool GlobalHistogramAllocator::HasPersistentLocation() const {
  return !persistent_location_.empty();
}

bool GlobalHistogramAllocator::MovePersistentFile(const FilePath& dir) {
  DCHECK(HasPersistentLocation());

  FilePath new_file_path = dir.Append(persistent_location_.BaseName());

  // Change the location of the persistent file. This is fine to do even though
  // the file is currently "opened" by this process.
  if (!base::ReplaceFile(persistent_location_, new_file_path, nullptr)) {
    return false;
  }

  SetPersistentLocation(new_file_path);
  return true;
}

bool GlobalHistogramAllocator::WriteToPersistentLocation() {
#if BUILDFLAG(IS_NACL)
  // NACL doesn't support file operations, including ImportantFileWriter.
  NOTREACHED();
#else
  // Stop if no destination is set.
  if (!HasPersistentLocation()) {
    NOTREACHED() << "Could not write \"" << Name() << "\" persistent histograms"
                 << " to file because no location was set.";
  }

  std::string_view contents(static_cast<const char*>(data()), used());
  if (!ImportantFileWriter::WriteFileAtomically(
          persistent_location_, contents, "PersistentHistogramAllocator")) {
    LOG(ERROR) << "Could not write \"" << Name() << "\" persistent histograms"
               << " to file: " << persistent_location_.value();
    return false;
  }

  return true;
#endif
}

void GlobalHistogramAllocator::DeletePersistentLocation() {
  memory_allocator()->SetMemoryState(PersistentMemoryAllocator::MEMORY_DELETED);

#if BUILDFLAG(IS_NACL)
  NOTREACHED();
#else
  if (!HasPersistentLocation()) {
    return;
  }

  // Open (with delete) and then immediately close the file by going out of
  // scope. This is the only cross-platform safe way to delete a file that may
  // be open elsewhere. Open handles will continue to operate normally but
  // new opens will not be possible.
  File file(persistent_location_,
            File::FLAG_OPEN | File::FLAG_READ | File::FLAG_DELETE_ON_CLOSE);
#endif
}

GlobalHistogramAllocator::GlobalHistogramAllocator(
    std::unique_ptr<PersistentMemoryAllocator> memory)
    : PersistentHistogramAllocator(std::move(memory)),
      import_iterator_(this) {
}

void GlobalHistogramAllocator::ImportHistogramsToStatisticsRecorder() {
  // Skip the import if it's the histogram that was last created. Should a
  // race condition cause the "last created" to be overwritten before it
  // is recognized here then the histogram will be created and be ignored
  // when it is detected as a duplicate by the statistics-recorder. This
  // simple check reduces the time of creating persistent histograms by
  // about 40%.
  Reference record_to_ignore = last_created();

  // There is no lock on this because the iterator is lock-free while still
  // guaranteed to only return each entry only once. The StatisticsRecorder
  // has its own lock so the Register operation is safe.
  while (true) {
    std::unique_ptr<HistogramBase> histogram =
        import_iterator_.GetNextWithIgnore(record_to_ignore);
    if (!histogram)
      break;
    StatisticsRecorder::RegisterOrDeleteDuplicate(histogram.release());
  }
}

}  // namespace base
