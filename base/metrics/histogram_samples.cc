// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_samples.h"

#include <limits>
#include <string_view>
#include <utility>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace base {

namespace {

// A shorthand constant for the max value of size_t.
constexpr size_t kSizeMax = std::numeric_limits<size_t>::max();

// A constant stored in an AtomicSingleSample (as_atomic) to indicate that the
// sample is "disabled" and no further accumulation should be done with it. The
// value is chosen such that it will be MAX_UINT16 for both |bucket| & |count|,
// and thus less likely to conflict with real use. Conflicts are explicitly
// handled in the code but it's worth making them as unlikely as possible.
constexpr int32_t kDisabledSingleSample = -1;

class SampleCountPickleIterator : public SampleCountIterator {
 public:
  explicit SampleCountPickleIterator(PickleIterator* iter);

  bool Done() const override;
  void Next() override;
  void Get(HistogramBase::Sample* min,
           int64_t* max,
           HistogramBase::Count* count) override;

 private:
  const raw_ptr<PickleIterator> iter_;

  HistogramBase::Sample min_;
  int64_t max_;
  HistogramBase::Count count_;
  bool is_done_;
};

SampleCountPickleIterator::SampleCountPickleIterator(PickleIterator* iter)
    : iter_(iter),
      is_done_(false) {
  Next();
}

bool SampleCountPickleIterator::Done() const {
  return is_done_;
}

void SampleCountPickleIterator::Next() {
  DCHECK(!Done());
  if (!iter_->ReadInt(&min_) || !iter_->ReadInt64(&max_) ||
      !iter_->ReadInt(&count_)) {
    is_done_ = true;
  }
}

void SampleCountPickleIterator::Get(HistogramBase::Sample* min,
                                    int64_t* max,
                                    HistogramBase::Count* count) {
  DCHECK(!Done());
  *min = min_;
  *max = max_;
  *count = count_;
}

}  // namespace

static_assert(sizeof(HistogramSamples::AtomicSingleSample) ==
                  sizeof(subtle::Atomic32),
              "AtomicSingleSample isn't 32 bits");

HistogramSamples::SingleSample HistogramSamples::AtomicSingleSample::Load()
    const {
  AtomicSingleSample single_sample(subtle::Acquire_Load(&as_atomic));

  // If the sample was extracted/disabled, it's still zero to the outside.
  if (single_sample.as_atomic == kDisabledSingleSample)
    single_sample.as_atomic = 0;

  return single_sample.as_parts;
}

HistogramSamples::SingleSample HistogramSamples::AtomicSingleSample::Extract(
    AtomicSingleSample new_value) {
  DCHECK(new_value.as_atomic != kDisabledSingleSample)
      << "Disabling an AtomicSingleSample should be done through "
         "ExtractAndDisable().";

  AtomicSingleSample old_value;

  // Because a concurrent call may modify and/or disable this object as we are
  // trying to extract its value, a compare-and-swap loop must be done to ensure
  // that the value was not changed between the reading and writing (and to
  // prevent accidentally re-enabling this object).
  while (true) {
    old_value.as_atomic = subtle::Acquire_Load(&as_atomic);

    // If this object was already disabled, return an empty sample and keep it
    // disabled.
    if (old_value.as_atomic == kDisabledSingleSample) {
      old_value.as_atomic = 0;
      return old_value.as_parts;
    }

    // Extract the single-sample from memory. |existing| is what was in that
    // memory location at the time of the call; if it doesn't match |original|
    // (i.e., the single-sample was concurrently modified during this
    // iteration), then the swap did not happen, so try again.
    subtle::Atomic32 existing = subtle::Release_CompareAndSwap(
        &as_atomic, old_value.as_atomic, new_value.as_atomic);
    if (existing == old_value.as_atomic) {
      return old_value.as_parts;
    }
  }
}

HistogramSamples::SingleSample
HistogramSamples::AtomicSingleSample::ExtractAndDisable() {
  AtomicSingleSample old_value(
      subtle::NoBarrier_AtomicExchange(&as_atomic, kDisabledSingleSample));
  // If this object was already disabled, return an empty sample.
  if (old_value.as_atomic == kDisabledSingleSample) {
    old_value.as_atomic = 0;
  }
  return old_value.as_parts;
}

bool HistogramSamples::AtomicSingleSample::Accumulate(
    size_t bucket,
    HistogramBase::Count count) {
  if (count == 0)
    return true;

  // Convert the parameters to 16-bit variables because it's all 16-bit below.
  // To support decrements/subtractions, divide the |count| into sign/value and
  // do the proper operation below. The alternative is to change the single-
  // sample's count to be a signed integer (int16_t) and just add an int16_t
  // |count16| but that is somewhat wasteful given that the single-sample is
  // never expected to have a count less than zero.
  if (count < -std::numeric_limits<uint16_t>::max() ||
      count > std::numeric_limits<uint16_t>::max() ||
      bucket > std::numeric_limits<uint16_t>::max()) {
    return false;
  }
  bool count_is_negative = count < 0;
  uint16_t count16 = static_cast<uint16_t>(count_is_negative ? -count : count);
  uint16_t bucket16 = static_cast<uint16_t>(bucket);

  // A local, unshared copy of the single-sample is necessary so the parts
  // can be manipulated without worrying about atomicity.
  AtomicSingleSample single_sample;

  bool sample_updated;
  do {
    subtle::Atomic32 original = subtle::Acquire_Load(&as_atomic);
    if (original == kDisabledSingleSample)
      return false;
    single_sample.as_atomic = original;
    if (single_sample.as_atomic != 0) {
      // Only the same bucket (parameter and stored) can be counted multiple
      // times.
      if (single_sample.as_parts.bucket != bucket16)
        return false;
    } else {
      // The |single_ sample| was zero so becomes the |bucket| parameter, the
      // contents of which were checked above to fit in 16 bits.
      single_sample.as_parts.bucket = bucket16;
    }

    // Update count, making sure that it doesn't overflow.
    CheckedNumeric<uint16_t> new_count(single_sample.as_parts.count);
    if (count_is_negative)
      new_count -= count16;
    else
      new_count += count16;
    if (!new_count.AssignIfValid(&single_sample.as_parts.count))
      return false;

    // Don't let this become equivalent to the "disabled" value.
    if (single_sample.as_atomic == kDisabledSingleSample)
      return false;

    // Store the updated single-sample back into memory. |existing| is what
    // was in that memory location at the time of the call; if it doesn't
    // match |original| then the swap didn't happen so loop again.
    subtle::Atomic32 existing = subtle::Release_CompareAndSwap(
        &as_atomic, original, single_sample.as_atomic);
    sample_updated = (existing == original);
  } while (!sample_updated);

  return true;
}

bool HistogramSamples::AtomicSingleSample::IsDisabled() const {
  return subtle::Acquire_Load(&as_atomic) == kDisabledSingleSample;
}

HistogramSamples::LocalMetadata::LocalMetadata() {
  // This is the same way it's done for persistent metadata since no ctor
  // is called for the data members in that case.
  memset(this, 0, sizeof(*this));
}

HistogramSamples::HistogramSamples(uint64_t id, Metadata* meta)
    : meta_(meta) {
  DCHECK(meta_->id == 0 || meta_->id == id);

  // It's possible that |meta| is contained in initialized, read-only memory
  // so it's essential that no write be done in that case.
  if (!meta_->id)
    meta_->id = id;
}

HistogramSamples::HistogramSamples(uint64_t id, std::unique_ptr<Metadata> meta)
    : HistogramSamples(id, meta.get()) {
  meta_owned_ = std::move(meta);
}

// This mustn't do anything with |meta_|. It was passed to the ctor and may
// be invalid by the time this dtor gets called.
HistogramSamples::~HistogramSamples() = default;

bool HistogramSamples::Add(const HistogramSamples& other) {
  IncreaseSumAndCount(other.sum(), other.redundant_count());
  std::unique_ptr<SampleCountIterator> it = other.Iterator();
  return AddSubtractImpl(it.get(), ADD);
}

bool HistogramSamples::AddFromPickle(PickleIterator* iter) {
  int64_t sum;
  HistogramBase::Count redundant_count;

  if (!iter->ReadInt64(&sum) || !iter->ReadInt(&redundant_count))
    return false;

  IncreaseSumAndCount(sum, redundant_count);

  SampleCountPickleIterator pickle_iter(iter);
  return AddSubtractImpl(&pickle_iter, ADD);
}

bool HistogramSamples::Subtract(const HistogramSamples& other) {
  IncreaseSumAndCount(-other.sum(), -other.redundant_count());
  std::unique_ptr<SampleCountIterator> it = other.Iterator();
  return AddSubtractImpl(it.get(), SUBTRACT);
}

bool HistogramSamples::Extract(HistogramSamples& other) {
  static_assert(sizeof(other.meta_->sum) == 8);

#ifdef ARCH_CPU_64_BITS
  // NoBarrier_AtomicExchange() is only defined for 64-bit types if
  // the ARCH_CPU_64_BITS macro is set.
  subtle::Atomic64 other_sum =
      subtle::NoBarrier_AtomicExchange(&other.meta_->sum, 0);
#else
  // |sum| is only atomic on 64 bit archs. Make |other_sum| volatile so that
  // the following code is not optimized or rearranged to be something like:
  //     IncreaseSumAndCount(other.meta_->sum, ...);
  //     other.meta_->sum = 0;
  // Or:
  //     int64_t other_sum = other.meta_->sum;
  //     other.meta_->sum = 0;
  //     IncreaseSumAndCount(other_sum, ...);
  // Which do not guarantee eventual consistency anymore (other.meta_->sum may
  // be modified concurrently at any time). However, despite this, eventual
  // consistency is still not guaranteed here because performing 64-bit
  // operations (loading, storing, adding, etc.) on a 32-bit machine cannot be
  // done atomically, but this at least reduces the odds of inconsistencies, at
  // the cost of a few extra instructions.
  volatile int64_t other_sum = other.meta_->sum;
  other.meta_->sum -= other_sum;
#endif  // ARCH_CPU_64_BITS
  HistogramBase::AtomicCount other_redundant_count =
      subtle::NoBarrier_AtomicExchange(&other.meta_->redundant_count, 0);
  IncreaseSumAndCount(other_sum, other_redundant_count);
  std::unique_ptr<SampleCountIterator> it = other.ExtractingIterator();
  return AddSubtractImpl(it.get(), ADD);
}

bool HistogramSamples::IsDefinitelyEmpty() const {
  return sum() == 0 && redundant_count() == 0;
}

void HistogramSamples::Serialize(Pickle* pickle) const {
  pickle->WriteInt64(sum());
  pickle->WriteInt(redundant_count());

  HistogramBase::Sample min;
  int64_t max;
  HistogramBase::Count count;
  for (std::unique_ptr<SampleCountIterator> it = Iterator(); !it->Done();
       it->Next()) {
    it->Get(&min, &max, &count);
    pickle->WriteInt(min);
    pickle->WriteInt64(max);
    pickle->WriteInt(count);
  }
}

bool HistogramSamples::AccumulateSingleSample(HistogramBase::Sample value,
                                              HistogramBase::Count count,
                                              size_t bucket) {
  if (single_sample().Accumulate(bucket, count)) {
    // Success. Update the (separate) sum and redundant-count.
    IncreaseSumAndCount(strict_cast<int64_t>(value) * count, count);
    return true;
  }
  return false;
}

void HistogramSamples::IncreaseSumAndCount(int64_t sum,
                                           HistogramBase::Count count) {
#ifdef ARCH_CPU_64_BITS
  subtle::NoBarrier_AtomicIncrement(&meta_->sum, sum);
#else
  meta_->sum += sum;
#endif
  subtle::NoBarrier_AtomicIncrement(&meta_->redundant_count, count);
}

void HistogramSamples::RecordNegativeSample(NegativeSampleReason reason,
                                            HistogramBase::Count increment) {
  UMA_HISTOGRAM_ENUMERATION("UMA.NegativeSamples.Reason", reason,
                            MAX_NEGATIVE_SAMPLE_REASONS);
  UMA_HISTOGRAM_CUSTOM_COUNTS("UMA.NegativeSamples.Increment", increment, 1,
                              1 << 30, 100);
  UmaHistogramSparse("UMA.NegativeSamples.Histogram",
                     static_cast<int32_t>(id()));
}

base::Value::Dict HistogramSamples::ToGraphDict(std::string_view histogram_name,
                                                int32_t flags) const {
  base::Value::Dict dict;
  dict.Set("name", histogram_name);
  dict.Set("header", GetAsciiHeader(histogram_name, flags));
  dict.Set("body", GetAsciiBody());
  return dict;
}

std::string HistogramSamples::GetAsciiHeader(std::string_view histogram_name,
                                             int32_t flags) const {
  std::string output;
  StrAppend(&output, {"Histogram: ", histogram_name, " recorded ",
                      NumberToString(TotalCount()), " samples"});
  if (flags)
    StringAppendF(&output, " (flags = 0x%x)", flags);
  return output;
}

std::string HistogramSamples::GetAsciiBody() const {
  HistogramBase::Count total_count = TotalCount();
  double scaled_total_count = total_count / 100.0;

  // Determine how wide the largest bucket range is (how many digits to print),
  // so that we'll be able to right-align starts for the graphical bars.
  // Determine which bucket has the largest sample count so that we can
  // normalize the graphical bar-width relative to that sample count.
  HistogramBase::Count largest_count = 0;
  HistogramBase::Sample largest_sample = 0;
  std::unique_ptr<SampleCountIterator> it = Iterator();
  while (!it->Done()) {
    HistogramBase::Sample min;
    int64_t max;
    HistogramBase::Count count;
    it->Get(&min, &max, &count);
    if (min > largest_sample)
      largest_sample = min;
    if (count > largest_count)
      largest_count = count;
    it->Next();
  }
  // Scale histogram bucket counts to take at most 72 characters.
  // Note: Keep in sync w/ kLineLength sample_vector.cc
  const double kLineLength = 72;
  double scaling_factor = 1;
  if (largest_count > kLineLength)
    scaling_factor = kLineLength / largest_count;
  size_t print_width = GetSimpleAsciiBucketRange(largest_sample).size() + 1;

  // iterate over each item and display them
  it = Iterator();
  std::string output;
  while (!it->Done()) {
    HistogramBase::Sample min;
    int64_t max;
    HistogramBase::Count count;
    it->Get(&min, &max, &count);

    // value is min, so display it
    std::string range = GetSimpleAsciiBucketRange(min);
    output.append(range);
    if (const auto range_size = range.size(); print_width >= range_size) {
      output.append(print_width + 1 - range_size, ' ');
    }
    HistogramBase::Count current_size = round(count * scaling_factor);
    WriteAsciiBucketGraph(current_size, kLineLength, &output);
    WriteAsciiBucketValue(count, scaled_total_count, &output);
    output.append(1, '\n');
    it->Next();
  }
  return output;
}

// static
void HistogramSamples::WriteAsciiBucketGraph(double x_count,
                                             int line_length,
                                             std::string* output) {
  output->reserve(ClampAdd(output->size(), ClampAdd(line_length, 1)));

  const size_t count = ClampRound<size_t>(x_count);
  output->append(count, '-');
  output->append(1, 'O');
  if (const auto len = as_unsigned(line_length); count < len) {
    output->append(len - count, ' ');
  }
}

void HistogramSamples::WriteAsciiBucketValue(HistogramBase::Count current,
                                             double scaled_sum,
                                             std::string* output) const {
  StringAppendF(output, " (%d = %3.1f%%)", current, current / scaled_sum);
}

const std::string HistogramSamples::GetSimpleAsciiBucketRange(
    HistogramBase::Sample sample) const {
  return StringPrintf("%d", sample);
}

SampleCountIterator::~SampleCountIterator() = default;

bool SampleCountIterator::GetBucketIndex(size_t* index) const {
  DCHECK(!Done());
  return false;
}

SingleSampleIterator::SingleSampleIterator(HistogramBase::Sample min,
                                           int64_t max,
                                           HistogramBase::Count count,
                                           size_t bucket_index,
                                           bool value_was_extracted)
    : min_(min),
      max_(max),
      bucket_index_(bucket_index),
      count_(count),
      value_was_extracted_(value_was_extracted) {}

SingleSampleIterator::~SingleSampleIterator() {
  // Because this object may have been instantiated in such a way that the
  // samples it is holding were already extracted from the underlying data, we
  // add a DCHECK to ensure that in those cases, users of this iterator read the
  // samples, otherwise they may be lost.
  DCHECK(!value_was_extracted_ || Done());
}

bool SingleSampleIterator::Done() const {
  return count_ == 0;
}

void SingleSampleIterator::Next() {
  DCHECK(!Done());
  count_ = 0;
}

void SingleSampleIterator::Get(HistogramBase::Sample* min,
                               int64_t* max,
                               HistogramBase::Count* count) {
  DCHECK(!Done());
  *min = min_;
  *max = max_;
  *count = count_;
}

bool SingleSampleIterator::GetBucketIndex(size_t* index) const {
  DCHECK(!Done());
  if (bucket_index_ == kSizeMax)
    return false;
  *index = bucket_index_;
  return true;
}

}  // namespace base
