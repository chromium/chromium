// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Histogram is an object that aggregates statistics, and can summarize them in
// various forms, including ASCII graphical, HTML, and numerically (as a
// vector of numbers corresponding to each of the aggregating buckets).
// See header file for details and examples.

#include "base/metrics/histogram.h"

#include <inttypes.h>
#include <limits.h>
#include <math.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/dummy_histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/sample_vector.h"
#include "base/metrics/statistics_recorder.h"
#include "base/notreached.h"
#include "base/pickle.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "build/build_config.h"

namespace base {

namespace {

bool ReadHistogramArguments(PickleIterator* iter,
                            std::string* histogram_name,
                            int* flags,
                            int* declared_min,
                            int* declared_max,
                            size_t* bucket_count,
                            uint32_t* range_checksum) {
  uint32_t bucket_count_u32;
  if (!iter->ReadString(histogram_name) || !iter->ReadInt(flags) ||
      !iter->ReadInt(declared_min) || !iter->ReadInt(declared_max) ||
      !iter->ReadUInt32(&bucket_count_u32) ||
      !iter->ReadUInt32(range_checksum)) {
    DLOG(ERROR) << "Pickle error decoding Histogram: " << *histogram_name;
    return false;
  }
  *bucket_count = bucket_count_u32;

  // Since these fields may have come from an untrusted renderer, do additional
  // checks above and beyond those in Histogram::Initialize()
  if (*declared_max <= 0 ||
      *declared_min <= 0 ||
      *declared_max < *declared_min ||
      INT_MAX / sizeof(HistogramBase::Count) <= *bucket_count ||
      *bucket_count < 2) {
    DLOG(ERROR) << "Values error decoding Histogram: " << histogram_name;
    return false;
  }

  // We use the arguments to find or create the local version of the histogram
  // in this process, so we need to clear any IPC flag.
  *flags &= ~HistogramBase::kIPCSerializationSourceFlag;

  return true;
}

bool ValidateRangeChecksum(const HistogramBase& histogram,
                           uint32_t range_checksum) {
  // Normally, |histogram| should have type HISTOGRAM or be inherited from it.
  // However, if it's expired, it will actually be a DUMMY_HISTOGRAM.
  // Skip the checks in that case.
  if (histogram.GetHistogramType() == DUMMY_HISTOGRAM)
    return true;
  const Histogram& casted_histogram =
      static_cast<const Histogram&>(histogram);

  return casted_histogram.bucket_ranges()->checksum() == range_checksum;
}

}  // namespace

typedef HistogramBase::Count Count;
typedef HistogramBase::Sample Sample;

class Histogram::Factory {
 public:
  Factory(std::string_view name,
          HistogramBase::Sample minimum,
          HistogramBase::Sample maximum,
          size_t bucket_count,
          int32_t flags)
      : Factory(name, HISTOGRAM, minimum, maximum, bucket_count, flags) {}

  Factory(const Factory&) = delete;
  Factory& operator=(const Factory&) = delete;

  // Create histogram based on construction parameters. Caller takes
  // ownership of the returned object.
  HistogramBase* Build();

 protected:
  Factory(std::string_view name,
          HistogramType histogram_type,
          HistogramBase::Sample minimum,
          HistogramBase::Sample maximum,
          size_t bucket_count,
          int32_t flags)
      : name_(name),
        histogram_type_(histogram_type),
        minimum_(minimum),
        maximum_(maximum),
        bucket_count_(bucket_count),
        flags_(flags) {}

  // Create a BucketRanges structure appropriate for this histogram.
  virtual BucketRanges* CreateRanges() {
    BucketRanges* ranges = new BucketRanges(bucket_count_ + 1);
    Histogram::InitializeBucketRanges(minimum_, maximum_, ranges);
    return ranges;
  }

  // Allocate the correct Histogram object off the heap (in case persistent
  // memory is not available).
  virtual std::unique_ptr<HistogramBase> HeapAlloc(const BucketRanges* ranges) {
    return WrapUnique(new Histogram(GetPermanentName(name_), ranges));
  }

  // Perform any required datafill on the just-created histogram.  If
  // overridden, be sure to call the "super" version -- this method may not
  // always remain empty.
  virtual void FillHistogram(HistogramBase* histogram) {}

  // These values are protected (instead of private) because they need to
  // be accessible to methods of sub-classes in order to avoid passing
  // unnecessary parameters everywhere.
  const std::string_view name_;
  const HistogramType histogram_type_;
  HistogramBase::Sample minimum_;
  HistogramBase::Sample maximum_;
  size_t bucket_count_;
  int32_t flags_;
};

HistogramBase* Histogram::Factory::Build() {
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(name_);
  if (!histogram) {
    // constructor. Refactor code to avoid the additional call.
    bool should_record = StatisticsRecorder::ShouldRecordHistogram(
        HashMetricNameAs32Bits(name_));
    if (!should_record)
      return DummyHistogram::GetInstance();
    // To avoid racy destruction at shutdown, the following will be leaked.
    const BucketRanges* created_ranges = CreateRanges();

    const BucketRanges* registered_ranges =
        StatisticsRecorder::RegisterOrDeleteDuplicateRanges(created_ranges);

    // In most cases, the bucket-count, minimum, and maximum values are known
    // when the code is written and so are passed in explicitly. In other
    // cases (such as with a CustomHistogram), they are calculated dynamically
    // at run-time. In the latter case, those ctor parameters are zero and
    // the results extracted from the result of CreateRanges().
    if (bucket_count_ == 0) {
      bucket_count_ = registered_ranges->bucket_count();
      minimum_ = registered_ranges->range(1);
      maximum_ = registered_ranges->range(bucket_count_ - 1);
    }
    DCHECK_EQ(minimum_, registered_ranges->range(1));
    DCHECK_EQ(maximum_, registered_ranges->range(bucket_count_ - 1));

    // Try to create the histogram using a "persistent" allocator. As of
    // 2016-02-25, the availability of such is controlled by a base::Feature
    // that is off by default. If the allocator doesn't exist or if
    // allocating from it fails, code below will allocate the histogram from
    // the process heap.
    PersistentHistogramAllocator::Reference histogram_ref = 0;
    std::unique_ptr<HistogramBase> tentative_histogram;
    PersistentHistogramAllocator* allocator = GlobalHistogramAllocator::Get();
    if (allocator) {
      tentative_histogram = allocator->AllocateHistogram(
          histogram_type_, name_, minimum_, maximum_, registered_ranges, flags_,
          &histogram_ref);
    }

    // Handle the case where no persistent allocator is present or the
    // persistent allocation fails (perhaps because it is full).
    if (!tentative_histogram) {
      DCHECK(!histogram_ref);  // Should never have been set.
      flags_ &= ~HistogramBase::kIsPersistent;
      tentative_histogram = HeapAlloc(registered_ranges);
      tentative_histogram->SetFlags(flags_);
    }

    FillHistogram(tentative_histogram.get());

    // Register this histogram with the StatisticsRecorder. Keep a copy of
    // the pointer value to tell later whether the locally created histogram
    // was registered or deleted. The type is "void" because it could point
    // to released memory after the following line.
    const void* tentative_histogram_ptr = tentative_histogram.get();
    histogram = StatisticsRecorder::RegisterOrDeleteDuplicate(
        tentative_histogram.release());

    // Persistent histograms need some follow-up processing.
    if (histogram_ref) {
      allocator->FinalizeHistogram(histogram_ref,
                                   histogram == tentative_histogram_ptr);
    }
  }

  if (histogram_type_ != histogram->GetHistogramType() ||
      (bucket_count_ != 0 && !histogram->HasConstructionArguments(
                                 minimum_, maximum_, bucket_count_))) {
    // The construction arguments do not match the existing histogram.  This can
    // come about if an extension updates in the middle of a chrome run and has
    // changed one of them, or simply by bad code within Chrome itself.  A NULL
    // return would cause Chrome to crash; better to just record it for later
    // analysis.
    UmaHistogramSparse("Histogram.MismatchedConstructionArguments",
                       static_cast<Sample>(HashMetricName(name_)));
    DLOG(ERROR) << "Histogram " << name_
                << " has mismatched construction arguments";
    return DummyHistogram::GetInstance();
  }
  return histogram;
}

HistogramBase* Histogram::FactoryGet(std::string_view name,
                                     Sample minimum,
                                     Sample maximum,
                                     size_t bucket_count,
                                     int32_t flags) {
  return FactoryGetInternal(name, minimum, maximum, bucket_count, flags);
}

HistogramBase* Histogram::FactoryTimeGet(std::string_view name,
                                         TimeDelta minimum,
                                         TimeDelta maximum,
                                         size_t bucket_count,
                                         int32_t flags) {
  return FactoryTimeGetInternal(name, minimum, maximum, bucket_count, flags);
}

HistogramBase* Histogram::FactoryMicrosecondsTimeGet(std::string_view name,
                                                     TimeDelta minimum,
                                                     TimeDelta maximum,
                                                     size_t bucket_count,
                                                     int32_t flags) {
  return FactoryMicrosecondsTimeGetInternal(name, minimum, maximum,
                                            bucket_count, flags);
}

HistogramBase* Histogram::FactoryGet(const std::string& name,
                                     Sample minimum,
                                     Sample maximum,
                                     size_t bucket_count,
                                     int32_t flags) {
  return FactoryGetInternal(name, minimum, maximum, bucket_count, flags);
}

HistogramBase* Histogram::FactoryTimeGet(const std::string& name,
                                         TimeDelta minimum,
                                         TimeDelta maximum,
                                         size_t bucket_count,
                                         int32_t flags) {
  return FactoryTimeGetInternal(name, minimum, maximum, bucket_count, flags);
}

HistogramBase* Histogram::FactoryMicrosecondsTimeGet(const std::string& name,
                                                     TimeDelta minimum,
                                                     TimeDelta maximum,
                                                     size_t bucket_count,
                                                     int32_t flags) {
  return FactoryMicrosecondsTimeGetInternal(name, minimum, maximum,
                                            bucket_count, flags);
}

HistogramBase* Histogram::FactoryGet(const char* name,
                                     Sample minimum,
                                     Sample maximum,
                                     size_t bucket_count,
                                     int32_t flags) {
  return FactoryGetInternal(name, minimum, maximum, bucket_count, flags);
}

HistogramBase* Histogram::FactoryTimeGet(const char* name,
                                         TimeDelta minimum,
                                         TimeDelta maximum,
                                         size_t bucket_count,
                                         int32_t flags) {
  return FactoryTimeGetInternal(name, minimum, maximum, bucket_count, flags);
}

HistogramBase* Histogram::FactoryMicrosecondsTimeGet(const char* name,
                                                     TimeDelta minimum,
                                                     TimeDelta maximum,
                                                     size_t bucket_count,
                                                     int32_t flags) {
  return FactoryMicrosecondsTimeGetInternal(name, minimum, maximum,
                                            bucket_count, flags);
}

std::unique_ptr<HistogramBase> Histogram::PersistentCreate(
    const char* name,
    const BucketRanges* ranges,
    const DelayedPersistentAllocation& counts,
    const DelayedPersistentAllocation& logged_counts,
    HistogramSamples::Metadata* meta,
    HistogramSamples::Metadata* logged_meta) {
  return WrapUnique(
      new Histogram(name, ranges, counts, logged_counts, meta, logged_meta));
}

// Calculate what range of values are held in each bucket.
// We have to be careful that we don't pick a ratio between starting points in
// consecutive buckets that is sooo small, that the integer bounds are the same
// (effectively making one bucket get no values).  We need to avoid:
//   ranges(i) == ranges(i + 1)
// To avoid that, we just do a fine-grained bucket width as far as we need to
// until we get a ratio that moves us along at least 2 units at a time.  From
// that bucket onward we do use the exponential growth of buckets.
//
// static
void Histogram::InitializeBucketRanges(Sample minimum,
                                       Sample maximum,
                                       BucketRanges* ranges) {
  double log_max = log(static_cast<double>(maximum));
  double log_ratio;
  double log_next;
  size_t bucket_index = 1;
  Sample current = minimum;
  ranges->set_range(bucket_index, current);
  size_t bucket_count = ranges->bucket_count();

  while (bucket_count > ++bucket_index) {
    double log_current;
    log_current = log(static_cast<double>(current));
    debug::Alias(&log_current);
    // Calculate the count'th root of the range.
    log_ratio = (log_max - log_current) / (bucket_count - bucket_index);
    // See where the next bucket would start.
    log_next = log_current + log_ratio;
    Sample next;
    next = static_cast<int>(std::round(exp(log_next)));
    if (next > current)
      current = next;
    else
      ++current;  // Just do a narrow bucket, and keep trying.
    ranges->set_range(bucket_index, current);
  }
  ranges->set_range(ranges->bucket_count(), HistogramBase::kSampleType_MAX);
  ranges->ResetChecksum();
}

// static
const int Histogram::kCommonRaceBasedCountMismatch = 5;

uint32_t Histogram::FindCorruption(const HistogramSamples& samples) const {
  uint32_t inconsistencies = NO_INCONSISTENCIES;
  Sample previous_range = -1;  // Bottom range is always 0.
  for (size_t index = 0; index < bucket_count(); ++index) {
    int new_range = ranges(index);
    if (previous_range >= new_range)
      inconsistencies |= BUCKET_ORDER_ERROR;
    previous_range = new_range;
  }

  if (!bucket_ranges()->HasValidChecksum())
    inconsistencies |= RANGE_CHECKSUM_ERROR;

  int64_t delta64 = samples.redundant_count() - samples.TotalCount();
  if (delta64 != 0) {
    int delta = static_cast<int>(delta64);
    if (delta != delta64)
      delta = INT_MAX;  // Flag all giant errors as INT_MAX.
    if (delta > 0) {
      if (delta > kCommonRaceBasedCountMismatch)
        inconsistencies |= COUNT_HIGH_ERROR;
    } else {
      DCHECK_GT(0, delta);
      if (-delta > kCommonRaceBasedCountMismatch)
        inconsistencies |= COUNT_LOW_ERROR;
    }
  }
  return inconsistencies;
}

const BucketRanges* Histogram::bucket_ranges() const {
  return unlogged_samples_->bucket_ranges();
}

Sample Histogram::declared_min() const {
  const BucketRanges* ranges = bucket_ranges();
  if (ranges->bucket_count() < 2)
    return -1;
  return ranges->range(1);
}

Sample Histogram::declared_max() const {
  const BucketRanges* ranges = bucket_ranges();
  if (ranges->bucket_count() < 2)
    return -1;
  return ranges->range(ranges->bucket_count() - 1);
}

Sample Histogram::ranges(size_t i) const {
  return bucket_ranges()->range(i);
}

size_t Histogram::bucket_count() const {
  return bucket_ranges()->bucket_count();
}

// static
bool Histogram::InspectConstructionArguments(std::string_view name,
                                             Sample* minimum,
                                             Sample* maximum,
                                             size_t* bucket_count) {
  bool check_okay = true;

  // Checks below must be done after any min/max swap.
  if (*minimum > *maximum) {
    DLOG(ERROR) << "Histogram: " << name << " has swapped minimum/maximum";
    check_okay = false;
    std::swap(*minimum, *maximum);
  }

  // Defensive code for backward compatibility.
  if (*minimum < 1) {
    // TODO(crbug.com/40211696): Temporarily disabled during cleanup.
    // DLOG(ERROR) << "Histogram: " << name << " has bad minimum: " << *minimum;
    *minimum = 1;
    if (*maximum < 1)
      *maximum = 1;
  }
  if (*maximum >= kSampleType_MAX) {
    DLOG(ERROR) << "Histogram: " << name << " has bad maximum: " << *maximum;
    *maximum = kSampleType_MAX - 1;
  }
  if (*bucket_count > kBucketCount_MAX) {
    UmaHistogramSparse("Histogram.TooManyBuckets.1000",
                       static_cast<Sample>(HashMetricName(name)));

    // Blink.UseCounter legitimately has more than 1000 entries in its enum.
    if (!StartsWith(name, "Blink.UseCounter")) {
      DLOG(ERROR) << "Histogram: " << name
                  << " has bad bucket_count: " << *bucket_count << " (limit "
                  << kBucketCount_MAX << ")";

      // Assume it's a mistake and limit to 100 buckets, plus under and over.
      // If the DCHECK doesn't alert the user then hopefully the small number
      // will be obvious on the dashboard. If not, then it probably wasn't
      // important.
      *bucket_count = 102;
      check_okay = false;
    }
  }

  // Ensure parameters are sane.
  if (*maximum == *minimum) {
    check_okay = false;
    *maximum = *minimum + 1;
  }
  if (*bucket_count < 3) {
    check_okay = false;
    *bucket_count = 3;
  }
  // The swap at the top of the function guarantees this cast is safe.
  const size_t max_buckets = static_cast<size_t>(*maximum - *minimum + 2);
  if (*bucket_count > max_buckets) {
    check_okay = false;
    *bucket_count = max_buckets;
  }

  if (!check_okay) {
    UmaHistogramSparse("Histogram.BadConstructionArguments",
                       static_cast<Sample>(HashMetricName(name)));
  }

  return check_okay;
}

uint64_t Histogram::name_hash() const {
  return unlogged_samples_->id();
}

HistogramType Histogram::GetHistogramType() const {
  return HISTOGRAM;
}

bool Histogram::HasConstructionArguments(Sample expected_minimum,
                                         Sample expected_maximum,
                                         size_t expected_bucket_count) const {
  return (expected_bucket_count == bucket_count() &&
          expected_minimum == declared_min() &&
          expected_maximum == declared_max());
}

void Histogram::Add(int value) {
  AddCount(value, 1);
}

void Histogram::AddCount(int value, int count) {
  DCHECK_EQ(0, ranges(0));
  DCHECK_EQ(kSampleType_MAX, ranges(bucket_count()));

  if (value > kSampleType_MAX - 1)
    value = kSampleType_MAX - 1;
  if (value < 0)
    value = 0;
  if (count <= 0) {
    NOTREACHED();
  }
  unlogged_samples_->Accumulate(value, count);

  if (StatisticsRecorder::have_active_callbacks()) [[unlikely]] {
    FindAndRunCallbacks(value);
  }
}

std::unique_ptr<HistogramSamples> Histogram::SnapshotSamples() const {
  return SnapshotAllSamples();
}

std::unique_ptr<HistogramSamples> Histogram::SnapshotUnloggedSamples() const {
  return SnapshotUnloggedSamplesImpl();
}

void Histogram::MarkSamplesAsLogged(const HistogramSamples& samples) {
  // |final_delta_created_| only exists when DCHECK is on.
#if DCHECK_IS_ON()
  DCHECK(!final_delta_created_);
#endif

  unlogged_samples_->Subtract(samples);
  logged_samples_->Add(samples);
}

std::unique_ptr<HistogramSamples> Histogram::SnapshotDelta() {
  // |final_delta_created_| only exists when DCHECK is on.
#if DCHECK_IS_ON()
  DCHECK(!final_delta_created_);
#endif

  // The code below has subtle thread-safety guarantees! All changes to
  // the underlying SampleVectors use atomic integer operations, which guarantee
  // eventual consistency, but do not guarantee full synchronization between
  // different entries in the SampleVector. In particular, this means that
  // concurrent updates to the histogram might result in the reported sum not
  // matching the individual bucket counts; or there being some buckets that are
  // logically updated "together", but end up being only partially updated when
  // a snapshot is captured. Note that this is why it's important to subtract
  // exactly the snapshotted unlogged samples, rather than simply resetting the
  // vector: this way, the next snapshot will include any concurrent updates
  // missed by the current snapshot.

  std::unique_ptr<HistogramSamples> snapshot =
      std::make_unique<SampleVector>(unlogged_samples_->id(), bucket_ranges());
  snapshot->Extract(*unlogged_samples_);
  logged_samples_->Add(*snapshot);

  return snapshot;
}

std::unique_ptr<HistogramSamples> Histogram::SnapshotFinalDelta() const {
  // |final_delta_created_| only exists when DCHECK is on.
#if DCHECK_IS_ON()
  DCHECK(!final_delta_created_);
  final_delta_created_ = true;
#endif

  return SnapshotUnloggedSamples();
}

bool Histogram::AddSamples(const HistogramSamples& samples) {
  return unlogged_samples_->Add(samples);
}

bool Histogram::AddSamplesFromPickle(PickleIterator* iter) {
  return unlogged_samples_->AddFromPickle(iter);
}

base::Value::Dict Histogram::ToGraphDict() const {
  std::unique_ptr<SampleVector> snapshot = SnapshotAllSamples();
  return snapshot->ToGraphDict(histogram_name(), flags());
}

void Histogram::SerializeInfoImpl(Pickle* pickle) const {
  DCHECK(bucket_ranges()->HasValidChecksum());
  pickle->WriteString(histogram_name());
  pickle->WriteInt(flags());
  pickle->WriteInt(declared_min());
  pickle->WriteInt(declared_max());
  // Limited to kBucketCount_MAX, which fits in a uint32_t.
  pickle->WriteUInt32(static_cast<uint32_t>(bucket_count()));
  pickle->WriteUInt32(bucket_ranges()->checksum());
}

Histogram::Histogram(const char* name, const BucketRanges* ranges)
    : HistogramBase(name) {
  DCHECK(ranges) << name;
  unlogged_samples_ =
      std::make_unique<SampleVector>(HashMetricName(name), ranges);
  logged_samples_ =
      std::make_unique<SampleVector>(unlogged_samples_->id(), ranges);
}

Histogram::Histogram(const char* name,
                     const BucketRanges* ranges,
                     const DelayedPersistentAllocation& counts,
                     const DelayedPersistentAllocation& logged_counts,
                     HistogramSamples::Metadata* meta,
                     HistogramSamples::Metadata* logged_meta)
    : HistogramBase(name) {
  DCHECK(ranges) << name;
  unlogged_samples_ = std::make_unique<PersistentSampleVector>(
      HashMetricName(name), ranges, meta, counts);
  logged_samples_ = std::make_unique<PersistentSampleVector>(
      unlogged_samples_->id(), ranges, logged_meta, logged_counts);
}

Histogram::~Histogram() = default;

const std::string Histogram::GetAsciiBucketRange(size_t i) const {
  return GetSimpleAsciiBucketRange(ranges(i));
}

//------------------------------------------------------------------------------
// Private methods

// static
HistogramBase* Histogram::DeserializeInfoImpl(PickleIterator* iter) {
  std::string histogram_name;
  int flags;
  int declared_min;
  int declared_max;
  size_t bucket_count;
  uint32_t range_checksum;

  if (!ReadHistogramArguments(iter, &histogram_name, &flags, &declared_min,
                              &declared_max, &bucket_count, &range_checksum)) {
    return nullptr;
  }

  // Find or create the local version of the histogram in this process.
  HistogramBase* histogram = Histogram::FactoryGet(
      histogram_name, declared_min, declared_max, bucket_count, flags);
  if (!histogram)
    return nullptr;

  // The serialized histogram might be corrupted.
  if (!ValidateRangeChecksum(*histogram, range_checksum))
    return nullptr;

  return histogram;
}

// static
HistogramBase* Histogram::FactoryGetInternal(std::string_view name,
                                             Sample minimum,
                                             Sample maximum,
                                             size_t bucket_count,
                                             int32_t flags) {
  bool valid_arguments =
      InspectConstructionArguments(name, &minimum, &maximum, &bucket_count);
  DCHECK(valid_arguments) << name;
  if (!valid_arguments) {
    DLOG(ERROR) << "Histogram " << name << " dropped for invalid parameters.";
    return DummyHistogram::GetInstance();
  }

  return Factory(name, minimum, maximum, bucket_count, flags).Build();
}

// static
HistogramBase* Histogram::FactoryTimeGetInternal(std::string_view name,
                                                 TimeDelta minimum,
                                                 TimeDelta maximum,
                                                 size_t bucket_count,
                                                 int32_t flags) {
  DCHECK_LT(minimum.InMilliseconds(), std::numeric_limits<Sample>::max());
  DCHECK_LT(maximum.InMilliseconds(), std::numeric_limits<Sample>::max());
  return FactoryGetInternal(name, static_cast<Sample>(minimum.InMilliseconds()),
                            static_cast<Sample>(maximum.InMilliseconds()),
                            bucket_count, flags);
}

// static
HistogramBase* Histogram::FactoryMicrosecondsTimeGetInternal(
    std::string_view name,
    TimeDelta minimum,
    TimeDelta maximum,
    size_t bucket_count,
    int32_t flags) {
  DCHECK_LT(minimum.InMicroseconds(), std::numeric_limits<Sample>::max());
  DCHECK_LT(maximum.InMicroseconds(), std::numeric_limits<Sample>::max());
  return FactoryGetInternal(name, static_cast<Sample>(minimum.InMicroseconds()),
                            static_cast<Sample>(maximum.InMicroseconds()),
                            bucket_count, flags);
}

std::unique_ptr<SampleVector> Histogram::SnapshotAllSamples() const {
  std::unique_ptr<SampleVector> samples = SnapshotUnloggedSamplesImpl();
  samples->Add(*logged_samples_);
  return samples;
}

std::unique_ptr<SampleVector> Histogram::SnapshotUnloggedSamplesImpl() const {
  std::unique_ptr<SampleVector> samples(
      new SampleVector(unlogged_samples_->id(), bucket_ranges()));
  samples->Add(*unlogged_samples_);
  return samples;
}

Value::Dict Histogram::GetParameters() const {
  Value::Dict params;
  params.Set("type", HistogramTypeToString(GetHistogramType()));
  params.Set("min", declared_min());
  params.Set("max", declared_max());
  params.Set("bucket_count", static_cast<int>(bucket_count()));
  return params;
}

//------------------------------------------------------------------------------
// LinearHistogram: This histogram uses a traditional set of evenly spaced
// buckets.
//------------------------------------------------------------------------------

class LinearHistogram::Factory : public Histogram::Factory {
 public:
  Factory(std::string_view name,
          HistogramBase::Sample minimum,
          HistogramBase::Sample maximum,
          size_t bucket_count,
          int32_t flags,
          const DescriptionPair* descriptions)
      : Histogram::Factory(name,
                           LINEAR_HISTOGRAM,
                           minimum,
                           maximum,
                           bucket_count,
                           flags) {
    descriptions_ = descriptions;
  }

  Factory(const Factory&) = delete;
  Factory& operator=(const Factory&) = delete;

 protected:
  BucketRanges* CreateRanges() override {
    BucketRanges* ranges = new BucketRanges(bucket_count_ + 1);
    LinearHistogram::InitializeBucketRanges(minimum_, maximum_, ranges);
    return ranges;
  }

  std::unique_ptr<HistogramBase> HeapAlloc(
      const BucketRanges* ranges) override {
    return WrapUnique(new LinearHistogram(GetPermanentName(name_), ranges));
  }

  void FillHistogram(HistogramBase* base_histogram) override {
    Histogram::Factory::FillHistogram(base_histogram);
    // Normally, |base_histogram| should have type LINEAR_HISTOGRAM or be
    // inherited from it. However, if it's expired, it will actually be a
    // DUMMY_HISTOGRAM. Skip filling in that case.
    if (base_histogram->GetHistogramType() == DUMMY_HISTOGRAM)
      return;
    LinearHistogram* histogram = static_cast<LinearHistogram*>(base_histogram);
    // Set range descriptions.
    if (descriptions_) {
      for (int i = 0; descriptions_[i].description; ++i) {
        histogram->bucket_description_[descriptions_[i].sample] =
            descriptions_[i].description;
      }
    }
  }

 private:
  raw_ptr<const DescriptionPair, AllowPtrArithmetic> descriptions_;
};

LinearHistogram::~LinearHistogram() = default;

HistogramBase* LinearHistogram::FactoryGet(std::string_view name,
                                           Sample minimum,
                                           Sample maximum,
                                           size_t bucket_count,
                                           int32_t flags) {
  return FactoryGetInternal(name, minimum, maximum, bucket_count, flags);
}

HistogramBase* LinearHistogram::FactoryTimeGet(std::string_view name,
                                               TimeDelta minimum,
                                               TimeDelta maximum,
                                               size_t bucket_count,
                                               int32_t flags) {
  return FactoryTimeGetInternal(name, minimum, maximum, bucket_count, flags);
}

HistogramBase* LinearHistogram::FactoryGet(const std::string& name,
                                           Sample minimum,
                                           Sample maximum,
                                           size_t bucket_count,
                                           int32_t flags) {
  return FactoryGetInternal(name, minimum, maximum, bucket_count, flags);
}

HistogramBase* LinearHistogram::FactoryTimeGet(const std::string& name,
                                               TimeDelta minimum,
                                               TimeDelta maximum,
                                               size_t bucket_count,
                                               int32_t flags) {
  return FactoryTimeGetInternal(name, minimum, maximum, bucket_count, flags);
}

HistogramBase* LinearHistogram::FactoryGet(const char* name,
                                           Sample minimum,
                                           Sample maximum,
                                           size_t bucket_count,
                                           int32_t flags) {
  return FactoryGetInternal(name, minimum, maximum, bucket_count, flags);
}

HistogramBase* LinearHistogram::FactoryTimeGet(const char* name,
                                               TimeDelta minimum,
                                               TimeDelta maximum,
                                               size_t bucket_count,
                                               int32_t flags) {
  return FactoryTimeGetInternal(name, minimum, maximum, bucket_count, flags);
}

std::unique_ptr<HistogramBase> LinearHistogram::PersistentCreate(
    const char* name,
    const BucketRanges* ranges,
    const DelayedPersistentAllocation& counts,
    const DelayedPersistentAllocation& logged_counts,
    HistogramSamples::Metadata* meta,
    HistogramSamples::Metadata* logged_meta) {
  return WrapUnique(new LinearHistogram(name, ranges, counts, logged_counts,
                                        meta, logged_meta));
}

HistogramBase* LinearHistogram::FactoryGetWithRangeDescription(
    std::string_view name,
    Sample minimum,
    Sample maximum,
    size_t bucket_count,
    int32_t flags,
    const DescriptionPair descriptions[]) {
  // Originally, histograms were required to have at least one sample value
  // plus underflow and overflow buckets. For single-entry enumerations,
  // that one value is usually zero (which IS the underflow bucket)
  // resulting in a |maximum| value of 1 (the exclusive upper-bound) and only
  // the two outlier buckets. Handle this by making max==2 and buckets==3.
  // This usually won't have any cost since the single-value-optimization
  // will be used until the count exceeds 16 bits.
  if (maximum == 1 && bucket_count == 2) {
    maximum = 2;
    bucket_count = 3;
  }

  bool valid_arguments = Histogram::InspectConstructionArguments(
      name, &minimum, &maximum, &bucket_count);
  DCHECK(valid_arguments) << name;
  if (!valid_arguments) {
    DLOG(ERROR) << "Histogram " << name << " dropped for invalid parameters.";
    return DummyHistogram::GetInstance();
  }

  return Factory(name, minimum, maximum, bucket_count, flags, descriptions)
      .Build();
}

HistogramType LinearHistogram::GetHistogramType() const {
  return LINEAR_HISTOGRAM;
}

LinearHistogram::LinearHistogram(const char* name, const BucketRanges* ranges)
    : Histogram(name, ranges) {}

LinearHistogram::LinearHistogram(
    const char* name,
    const BucketRanges* ranges,
    const DelayedPersistentAllocation& counts,
    const DelayedPersistentAllocation& logged_counts,
    HistogramSamples::Metadata* meta,
    HistogramSamples::Metadata* logged_meta)
    : Histogram(name, ranges, counts, logged_counts, meta, logged_meta) {}

const std::string LinearHistogram::GetAsciiBucketRange(size_t i) const {
  int range = ranges(i);
  BucketDescriptionMap::const_iterator it = bucket_description_.find(range);
  if (it == bucket_description_.end())
    return Histogram::GetAsciiBucketRange(i);
  return it->second;
}

// static
void LinearHistogram::InitializeBucketRanges(Sample minimum,
                                             Sample maximum,
                                             BucketRanges* ranges) {
  double min = minimum;
  double max = maximum;
  size_t bucket_count = ranges->bucket_count();

  for (size_t i = 1; i < bucket_count; ++i) {
    double linear_range =
        (min * (bucket_count - 1 - i) + max * (i - 1)) / (bucket_count - 2);
    auto range = static_cast<Sample>(linear_range + 0.5);
    ranges->set_range(i, range);
  }
  ranges->set_range(ranges->bucket_count(), HistogramBase::kSampleType_MAX);
  ranges->ResetChecksum();
}

// static
HistogramBase* LinearHistogram::FactoryGetInternal(std::string_view name,
                                                   Sample minimum,
                                                   Sample maximum,
                                                   size_t bucket_count,
                                                   int32_t flags) {
  return FactoryGetWithRangeDescription(name, minimum, maximum, bucket_count,
                                        flags, nullptr);
}

// static
HistogramBase* LinearHistogram::FactoryTimeGetInternal(std::string_view name,
                                                       TimeDelta minimum,
                                                       TimeDelta maximum,
                                                       size_t bucket_count,
                                                       int32_t flags) {
  DCHECK_LT(minimum.InMilliseconds(), std::numeric_limits<Sample>::max());
  DCHECK_LT(maximum.InMilliseconds(), std::numeric_limits<Sample>::max());
  return FactoryGetInternal(name, static_cast<Sample>(minimum.InMilliseconds()),
                            static_cast<Sample>(maximum.InMilliseconds()),
                            bucket_count, flags);
}

// static
HistogramBase* LinearHistogram::DeserializeInfoImpl(PickleIterator* iter) {
  std::string histogram_name;
  int flags;
  int declared_min;
  int declared_max;
  size_t bucket_count;
  uint32_t range_checksum;

  if (!ReadHistogramArguments(iter, &histogram_name, &flags, &declared_min,
                              &declared_max, &bucket_count, &range_checksum)) {
    return nullptr;
  }

  HistogramBase* histogram = LinearHistogram::FactoryGet(
      histogram_name, declared_min, declared_max, bucket_count, flags);
  if (!histogram)
    return nullptr;

  if (!ValidateRangeChecksum(*histogram, range_checksum)) {
    // The serialized histogram might be corrupted.
    return nullptr;
  }
  return histogram;
}

//------------------------------------------------------------------------------
// ScaledLinearHistogram: This is a wrapper around a LinearHistogram that
// scales input counts.
//------------------------------------------------------------------------------

ScaledLinearHistogram::ScaledLinearHistogram(std::string_view name,
                                             Sample minimum,
                                             Sample maximum,
                                             size_t bucket_count,
                                             int32_t scale,
                                             int32_t flags)
    : histogram_(LinearHistogram::FactoryGet(name,
                                             minimum,
                                             maximum,
                                             bucket_count,
                                             flags)),
      scale_(scale) {
  DCHECK(histogram_);
  DCHECK_LT(1, scale);
  DCHECK_EQ(1, minimum);
  CHECK_EQ(static_cast<Sample>(bucket_count), maximum - minimum + 2)
      << " ScaledLinearHistogram requires buckets of size 1";

  // Normally, |histogram_| should have type LINEAR_HISTOGRAM or be
  // inherited from it. However, if it's expired, it will be DUMMY_HISTOGRAM.
  if (histogram_->GetHistogramType() == DUMMY_HISTOGRAM)
    return;

  DCHECK_EQ(histogram_->GetHistogramType(), LINEAR_HISTOGRAM);
  LinearHistogram* histogram = static_cast<LinearHistogram*>(histogram_);
  remainders_.resize(histogram->bucket_count(), 0);
}

ScaledLinearHistogram::ScaledLinearHistogram(const std::string& name,
                                             Sample minimum,
                                             Sample maximum,
                                             size_t bucket_count,
                                             int32_t scale,
                                             int32_t flags)
    : ScaledLinearHistogram(std::string_view(name),
                            minimum,
                            maximum,
                            bucket_count,
                            scale,
                            flags) {}

ScaledLinearHistogram::ScaledLinearHistogram(const char* name,
                                             Sample minimum,
                                             Sample maximum,
                                             size_t bucket_count,
                                             int32_t scale,
                                             int32_t flags)
    : ScaledLinearHistogram(std::string_view(name),
                            minimum,
                            maximum,
                            bucket_count,
                            scale,
                            flags) {}

ScaledLinearHistogram::~ScaledLinearHistogram() = default;

void ScaledLinearHistogram::AddScaledCount(Sample value, int64_t count) {
  if (histogram_->GetHistogramType() == DUMMY_HISTOGRAM)
    return;
  if (count == 0)
    return;
  if (count < 0) {
    DUMP_WILL_BE_NOTREACHED();
    return;
  }

  DCHECK_EQ(histogram_->GetHistogramType(), LINEAR_HISTOGRAM);
  LinearHistogram* histogram = static_cast<LinearHistogram*>(histogram_);
  const auto max_value = static_cast<Sample>(histogram->bucket_count() - 1);
  value = std::clamp(value, 0, max_value);

  int64_t scaled_count = count / scale_;
  subtle::Atomic32 remainder = static_cast<int>(count - scaled_count * scale_);

  // ScaledLinearHistogram currently requires 1-to-1 mappings between value
  // and bucket which alleviates the need to do a bucket lookup here (something
  // that is internal to the HistogramSamples object).
  if (remainder > 0) {
    remainder = subtle::NoBarrier_AtomicIncrement(
        &remainders_[static_cast<size_t>(value)], remainder);
    // If remainder passes 1/2 scale, increment main count (thus rounding up).
    // The remainder is decremented by the full scale, though, which will
    // cause it to go negative and thus requrire another increase by the full
    // scale amount before another bump of the scaled count.
    if (remainder >= scale_ / 2) {
      scaled_count += 1;
      subtle::NoBarrier_AtomicIncrement(
          &remainders_[static_cast<size_t>(value)], -scale_);
    }
  }

  if (scaled_count > 0) {
    DCHECK(scaled_count <= std::numeric_limits<int>::max());
    histogram->AddCount(value, static_cast<int>(scaled_count));
  }
}

//------------------------------------------------------------------------------
// This section provides implementation for BooleanHistogram.
//------------------------------------------------------------------------------

class BooleanHistogram::Factory : public Histogram::Factory {
 public:
  Factory(std::string_view name, int32_t flags)
      : Histogram::Factory(name, BOOLEAN_HISTOGRAM, 1, 2, 3, flags) {}

  Factory(const Factory&) = delete;
  Factory& operator=(const Factory&) = delete;

 protected:
  BucketRanges* CreateRanges() override {
    BucketRanges* ranges = new BucketRanges(3 + 1);
    LinearHistogram::InitializeBucketRanges(1, 2, ranges);
    return ranges;
  }

  std::unique_ptr<HistogramBase> HeapAlloc(
      const BucketRanges* ranges) override {
    return WrapUnique(new BooleanHistogram(GetPermanentName(name_), ranges));
  }
};

HistogramBase* BooleanHistogram::FactoryGet(std::string_view name,
                                            int32_t flags) {
  return FactoryGetInternal(name, flags);
}

HistogramBase* BooleanHistogram::FactoryGet(const std::string& name,
                                            int32_t flags) {
  return FactoryGetInternal(name, flags);
}

HistogramBase* BooleanHistogram::FactoryGet(const char* name, int32_t flags) {
  return FactoryGetInternal(name, flags);
}

std::unique_ptr<HistogramBase> BooleanHistogram::PersistentCreate(
    const char* name,
    const BucketRanges* ranges,
    const DelayedPersistentAllocation& counts,
    const DelayedPersistentAllocation& logged_counts,
    HistogramSamples::Metadata* meta,
    HistogramSamples::Metadata* logged_meta) {
  return WrapUnique(new BooleanHistogram(name, ranges, counts, logged_counts,
                                         meta, logged_meta));
}

HistogramType BooleanHistogram::GetHistogramType() const {
  return BOOLEAN_HISTOGRAM;
}

// static
HistogramBase* BooleanHistogram::FactoryGetInternal(std::string_view name,
                                                    int32_t flags) {
  return Factory(name, flags).Build();
}

BooleanHistogram::BooleanHistogram(const char* name, const BucketRanges* ranges)
    : LinearHistogram(name, ranges) {}

BooleanHistogram::BooleanHistogram(
    const char* name,
    const BucketRanges* ranges,
    const DelayedPersistentAllocation& counts,
    const DelayedPersistentAllocation& logged_counts,
    HistogramSamples::Metadata* meta,
    HistogramSamples::Metadata* logged_meta)
    : LinearHistogram(name, ranges, counts, logged_counts, meta, logged_meta) {}

HistogramBase* BooleanHistogram::DeserializeInfoImpl(PickleIterator* iter) {
  std::string histogram_name;
  int flags;
  int declared_min;
  int declared_max;
  size_t bucket_count;
  uint32_t range_checksum;

  if (!ReadHistogramArguments(iter, &histogram_name, &flags, &declared_min,
                              &declared_max, &bucket_count, &range_checksum)) {
    return nullptr;
  }

  HistogramBase* histogram = BooleanHistogram::FactoryGet(
      histogram_name, flags);
  if (!histogram)
    return nullptr;

  if (!ValidateRangeChecksum(*histogram, range_checksum)) {
    // The serialized histogram might be corrupted.
    return nullptr;
  }
  return histogram;
}

//------------------------------------------------------------------------------
// CustomHistogram:
//------------------------------------------------------------------------------

class CustomHistogram::Factory : public Histogram::Factory {
 public:
  Factory(std::string_view name,
          const std::vector<Sample>* custom_ranges,
          int32_t flags)
      : Histogram::Factory(name, CUSTOM_HISTOGRAM, 0, 0, 0, flags) {
    custom_ranges_ = custom_ranges;
  }

  Factory(const Factory&) = delete;
  Factory& operator=(const Factory&) = delete;

 protected:
  BucketRanges* CreateRanges() override {
    // Remove the duplicates in the custom ranges array.
    std::vector<int> ranges = *custom_ranges_;
    ranges.push_back(0);  // Ensure we have a zero value.
    ranges.push_back(HistogramBase::kSampleType_MAX);
    ranges::sort(ranges);
    ranges.erase(ranges::unique(ranges), ranges.end());

    BucketRanges* bucket_ranges = new BucketRanges(ranges.size());
    for (size_t i = 0; i < ranges.size(); i++) {
      bucket_ranges->set_range(i, ranges[i]);
    }
    bucket_ranges->ResetChecksum();
    return bucket_ranges;
  }

  std::unique_ptr<HistogramBase> HeapAlloc(
      const BucketRanges* ranges) override {
    return WrapUnique(new CustomHistogram(GetPermanentName(name_), ranges));
  }

 private:
  raw_ptr<const std::vector<Sample>> custom_ranges_;
};

HistogramBase* CustomHistogram::FactoryGet(
    std::string_view name,
    const std::vector<Sample>& custom_ranges,
    int32_t flags) {
  return FactoryGetInternal(name, custom_ranges, flags);
}

HistogramBase* CustomHistogram::FactoryGet(
    const std::string& name,
    const std::vector<Sample>& custom_ranges,
    int32_t flags) {
  return FactoryGetInternal(name, custom_ranges, flags);
}

HistogramBase* CustomHistogram::FactoryGet(
    const char* name,
    const std::vector<Sample>& custom_ranges,
    int32_t flags) {
  return FactoryGetInternal(name, custom_ranges, flags);
}

std::unique_ptr<HistogramBase> CustomHistogram::PersistentCreate(
    const char* name,
    const BucketRanges* ranges,
    const DelayedPersistentAllocation& counts,
    const DelayedPersistentAllocation& logged_counts,
    HistogramSamples::Metadata* meta,
    HistogramSamples::Metadata* logged_meta) {
  return WrapUnique(new CustomHistogram(name, ranges, counts, logged_counts,
                                        meta, logged_meta));
}

HistogramType CustomHistogram::GetHistogramType() const {
  return CUSTOM_HISTOGRAM;
}

// static
std::vector<Sample> CustomHistogram::ArrayToCustomEnumRanges(
    base::span<const Sample> values) {
  std::vector<Sample> all_values;
  for (Sample value : values) {
    all_values.push_back(value);

    // Ensure that a guard bucket is added. If we end up with duplicate
    // values, FactoryGet will take care of removing them.
    all_values.push_back(value + 1);
  }
  return all_values;
}

CustomHistogram::CustomHistogram(const char* name, const BucketRanges* ranges)
    : Histogram(name, ranges) {}

CustomHistogram::CustomHistogram(
    const char* name,
    const BucketRanges* ranges,
    const DelayedPersistentAllocation& counts,
    const DelayedPersistentAllocation& logged_counts,
    HistogramSamples::Metadata* meta,
    HistogramSamples::Metadata* logged_meta)
    : Histogram(name, ranges, counts, logged_counts, meta, logged_meta) {}

void CustomHistogram::SerializeInfoImpl(Pickle* pickle) const {
  Histogram::SerializeInfoImpl(pickle);

  // Serialize ranges. First and last ranges are alwasy 0 and INT_MAX, so don't
  // write them.
  for (size_t i = 1; i < bucket_ranges()->bucket_count(); ++i)
    pickle->WriteInt(bucket_ranges()->range(i));
}

// static
HistogramBase* CustomHistogram::DeserializeInfoImpl(PickleIterator* iter) {
  std::string histogram_name;
  int flags;
  int declared_min;
  int declared_max;
  size_t bucket_count;
  uint32_t range_checksum;

  if (!ReadHistogramArguments(iter, &histogram_name, &flags, &declared_min,
                              &declared_max, &bucket_count, &range_checksum)) {
    return nullptr;
  }

  // First and last ranges are not serialized.
  std::vector<Sample> sample_ranges(bucket_count - 1);

  for (Sample& sample : sample_ranges) {
    if (!iter->ReadInt(&sample))
      return nullptr;
  }

  HistogramBase* histogram = CustomHistogram::FactoryGet(
      histogram_name, sample_ranges, flags);
  if (!histogram)
    return nullptr;

  if (!ValidateRangeChecksum(*histogram, range_checksum)) {
    // The serialized histogram might be corrupted.
    return nullptr;
  }
  return histogram;
}

// static
HistogramBase* CustomHistogram::FactoryGetInternal(
    std::string_view name,
    const std::vector<Sample>& custom_ranges,
    int32_t flags) {
  CHECK(ValidateCustomRanges(custom_ranges));

  return Factory(name, &custom_ranges, flags).Build();
}

// static
bool CustomHistogram::ValidateCustomRanges(
    const std::vector<Sample>& custom_ranges) {
  bool has_valid_range = false;
  for (Sample sample : custom_ranges) {
    if (sample < 0 || sample > HistogramBase::kSampleType_MAX - 1)
      return false;
    if (sample != 0)
      has_valid_range = true;
  }
  return has_valid_range;
}

namespace internal {

namespace {
// The pointer to the atomic const-pointer also needs to be atomic as some
// threads might already be alive when it's set. It requires acquire-release
// semantics to ensure the memory it points to is seen in its initialized state.
constinit std::atomic<const std::atomic<TimeTicks>*> g_last_foreground_time_ref;
}  // namespace

void SetSharedLastForegroundTimeForMetrics(
    const std::atomic<TimeTicks>* last_foreground_time_ref) {
  g_last_foreground_time_ref.store(last_foreground_time_ref,
                                   std::memory_order_release);
}

const std::atomic<TimeTicks>*
GetSharedLastForegroundTimeForMetricsForTesting() {
  return g_last_foreground_time_ref.load(std::memory_order_acquire);
}

bool OverlapsBestEffortRange(TimeTicks sample_time, TimeDelta sample_interval) {
  // std::memory_order_acquire semantics required as documented above to make
  // sure the memory pointed to by the stored `const std::atomic<TimeTicks>*`
  // is initialized from this thread's POV.
  auto last_foreground_time_ref =
      g_last_foreground_time_ref.load(std::memory_order_acquire);
  if (!last_foreground_time_ref) {
    return false;
  }

  // std::memory_order_relaxed is sufficient here as we care about the stored
  // TimeTicks value but don't assume the state of any other shared memory based
  // on the result.
  auto last_foreground_time =
      last_foreground_time_ref->load(std::memory_order_relaxed);
  // `last_foreground_time.is_null()` indicates we're currently under
  // best-effort priority and thus assume overlap. Otherwise we compare whether
  // the range of interest is fully contained within the last time this process
  // was running at a foreground priority.
  return last_foreground_time.is_null() ||
         (sample_time - sample_interval) < last_foreground_time;
}

}  // namespace internal

}  // namespace base
