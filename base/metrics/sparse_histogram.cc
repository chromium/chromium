// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/sparse_histogram.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/dummy_histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_sample_map.h"
#include "base/metrics/sample_map.h"
#include "base/metrics/statistics_recorder.h"
#include "base/notreached.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/values.h"

namespace base {

typedef HistogramBase::Count Count;
typedef HistogramBase::Sample Sample;

// static
HistogramBase* SparseHistogram::FactoryGet(std::string_view name,
                                           int32_t flags) {
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(name);
  if (!histogram) {
    bool should_record =
        StatisticsRecorder::ShouldRecordHistogram(HashMetricNameAs32Bits(name));
    if (!should_record) {
      return DummyHistogram::GetInstance();
    }
    // Try to create the histogram using a "persistent" allocator. If the
    // allocator doesn't exist or if allocating from it fails, code below will
    // allocate the histogram from the process heap.
    PersistentMemoryAllocator::Reference histogram_ref = 0;
    std::unique_ptr<HistogramBase> tentative_histogram;
    PersistentHistogramAllocator* allocator = GlobalHistogramAllocator::Get();
    if (allocator) {
      tentative_histogram = allocator->AllocateHistogram(
          SPARSE_HISTOGRAM, name, 0, 0, nullptr, flags, &histogram_ref);
    }

    // Handle the case where no persistent allocator is present or the
    // persistent allocation fails (perhaps because it is full).
    if (!tentative_histogram) {
      DCHECK(!histogram_ref);  // Should never have been set.
      flags &= ~HistogramBase::kIsPersistent;
      tentative_histogram.reset(new SparseHistogram(GetPermanentName(name)));
      tentative_histogram->SetFlags(flags);
    }

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

  if (histogram->GetHistogramType() != SPARSE_HISTOGRAM) {
    // The type does not match the existing histogram. This can come about if an
    // extension updates in the middle of a Chrome run or simply by bad code
    // within Chrome itself. We can't return null since calling code does not
    // expect it, so return a dummy instance and log the name hash.
    //
    // Note: Theoretically the below line could be re-entrant if something has
    // gone very wrong, but crashing w/ an infinite recursion seems OK then.
    UmaHistogramSparse("Histogram.MismatchedConstructionArguments",
                       static_cast<Sample>(HashMetricName(name)));
    DLOG(ERROR) << "Histogram " << name << " has a mismatched type";
    return DummyHistogram::GetInstance();
  }
  return histogram;
}

// static
std::unique_ptr<HistogramBase> SparseHistogram::PersistentCreate(
    PersistentHistogramAllocator* allocator,
    const char* name,
    HistogramSamples::Metadata* meta,
    HistogramSamples::Metadata* logged_meta) {
  return WrapUnique(new SparseHistogram(allocator, name, meta, logged_meta));
}

SparseHistogram::~SparseHistogram() = default;

uint64_t SparseHistogram::name_hash() const {
  return unlogged_samples_->id();
}

HistogramType SparseHistogram::GetHistogramType() const {
  return SPARSE_HISTOGRAM;
}

bool SparseHistogram::HasConstructionArguments(
    Sample expected_minimum,
    Sample expected_maximum,
    size_t expected_bucket_count) const {
  // SparseHistogram never has min/max/bucket_count limit.
  return false;
}

void SparseHistogram::Add(Sample value) {
  AddCount(value, 1);
}

void SparseHistogram::AddCount(Sample value, int count) {
  if (count <= 0) {
    NOTREACHED();
  }
  {
    base::AutoLock auto_lock(lock_);
    unlogged_samples_->Accumulate(value, count);
  }

  if (StatisticsRecorder::have_active_callbacks()) [[unlikely]] {
    FindAndRunCallbacks(value);
  }
}

std::unique_ptr<HistogramSamples> SparseHistogram::SnapshotSamples() const {
  std::unique_ptr<SampleMap> snapshot(new SampleMap(name_hash()));

  base::AutoLock auto_lock(lock_);
  snapshot->Add(*unlogged_samples_);
  snapshot->Add(*logged_samples_);
  return std::move(snapshot);
}

std::unique_ptr<HistogramSamples> SparseHistogram::SnapshotUnloggedSamples()
    const {
  std::unique_ptr<SampleMap> snapshot(new SampleMap(name_hash()));

  base::AutoLock auto_lock(lock_);
  snapshot->Add(*unlogged_samples_);

  return std::move(snapshot);
}

void SparseHistogram::MarkSamplesAsLogged(const HistogramSamples& samples) {
  DCHECK(!final_delta_created_);

  base::AutoLock auto_lock(lock_);
  unlogged_samples_->Subtract(samples);
  logged_samples_->Add(samples);
}

std::unique_ptr<HistogramSamples> SparseHistogram::SnapshotDelta() {
  DCHECK(!final_delta_created_);

  std::unique_ptr<SampleMap> snapshot =
      std::make_unique<SampleMap>(name_hash());
  base::AutoLock auto_lock(lock_);
  snapshot->Extract(*unlogged_samples_);
  logged_samples_->Add(*snapshot);
  return std::move(snapshot);
}

std::unique_ptr<HistogramSamples> SparseHistogram::SnapshotFinalDelta() const {
  DCHECK(!final_delta_created_);
  final_delta_created_ = true;

  std::unique_ptr<SampleMap> snapshot(new SampleMap(name_hash()));
  base::AutoLock auto_lock(lock_);
  snapshot->Add(*unlogged_samples_);

  return std::move(snapshot);
}

bool SparseHistogram::AddSamples(const HistogramSamples& samples) {
  base::AutoLock auto_lock(lock_);
  return unlogged_samples_->Add(samples);
}

bool SparseHistogram::AddSamplesFromPickle(PickleIterator* iter) {
  base::AutoLock auto_lock(lock_);
  return unlogged_samples_->AddFromPickle(iter);
}

base::Value::Dict SparseHistogram::ToGraphDict() const {
  std::unique_ptr<HistogramSamples> snapshot = SnapshotSamples();
  return snapshot->ToGraphDict(histogram_name(), flags());
}

void SparseHistogram::SerializeInfoImpl(Pickle* pickle) const {
  pickle->WriteString(histogram_name());
  pickle->WriteInt(flags());
}

SparseHistogram::SparseHistogram(const char* name)
    : HistogramBase(name),
      unlogged_samples_(new SampleMap(HashMetricName(name))),
      logged_samples_(new SampleMap(unlogged_samples_->id())) {}

SparseHistogram::SparseHistogram(PersistentHistogramAllocator* allocator,
                                 const char* name,
                                 HistogramSamples::Metadata* meta,
                                 HistogramSamples::Metadata* logged_meta)
    : HistogramBase(name),
      // While other histogram types maintain a static vector of values with
      // sufficient space for both "active" and "logged" samples, with each
      // SampleVector being given the appropriate half, sparse histograms
      // have no such initial allocation. Each sample has its own record
      // attached to a single PersistentSampleMap by a common 64-bit identifier.
      // Since a sparse histogram has two sample maps (active and logged),
      // there must be two sets of sample records with diffent IDs. The
      // "active" samples use, for convenience purposes, an ID matching
      // that of the histogram while the "logged" samples use that number
      // plus 1.
      unlogged_samples_(
          new PersistentSampleMap(HashMetricName(name), allocator, meta)),
      logged_samples_(new PersistentSampleMap(unlogged_samples_->id() + 1,
                                              allocator,
                                              logged_meta)) {}

HistogramBase* SparseHistogram::DeserializeInfoImpl(PickleIterator* iter) {
  std::string histogram_name;
  int flags;
  if (!iter->ReadString(&histogram_name) || !iter->ReadInt(&flags)) {
    DLOG(ERROR) << "Pickle error decoding Histogram: " << histogram_name;
    return nullptr;
  }

  flags &= ~HistogramBase::kIPCSerializationSourceFlag;

  return SparseHistogram::FactoryGet(histogram_name, flags);
}

Value::Dict SparseHistogram::GetParameters() const {
  // Unlike Histogram::GetParameters, only set the type here, and no other
  // params. The other params do not make sense for sparse histograms.
  Value::Dict params;
  params.Set("type", HistogramTypeToString(GetHistogramType()));
  return params;
}

}  // namespace base
