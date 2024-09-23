// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/statistics_recorder.h"

#include <string_view>

#include "base/at_exit.h"
#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/debug/leak_annotations.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_snapshot_manager.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/record_histogram_checker.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"

namespace base {
namespace {

bool HistogramNameLesser(const base::HistogramBase* a,
                         const base::HistogramBase* b) {
  return strcmp(a->histogram_name(), b->histogram_name()) < 0;
}

}  // namespace

// static
LazyInstance<Lock>::Leaky StatisticsRecorder::lock_ = LAZY_INSTANCE_INITIALIZER;

// static
LazyInstance<Lock>::Leaky StatisticsRecorder::snapshot_lock_ =
    LAZY_INSTANCE_INITIALIZER;

// static
StatisticsRecorder::SnapshotTransactionId
    StatisticsRecorder::last_snapshot_transaction_id_ = 0;

// static
StatisticsRecorder* StatisticsRecorder::top_ = nullptr;

// static
bool StatisticsRecorder::is_vlog_initialized_ = false;

// static
std::atomic<bool> StatisticsRecorder::have_active_callbacks_{false};

// static
std::atomic<StatisticsRecorder::GlobalSampleCallback>
    StatisticsRecorder::global_sample_callback_{nullptr};

StatisticsRecorder::ScopedHistogramSampleObserver::
    ScopedHistogramSampleObserver(const std::string& name,
                                  OnSampleCallback callback)
    : histogram_name_(name), callback_(callback) {
  StatisticsRecorder::AddHistogramSampleObserver(histogram_name_, this);
}

StatisticsRecorder::ScopedHistogramSampleObserver::
    ~ScopedHistogramSampleObserver() {
  StatisticsRecorder::RemoveHistogramSampleObserver(histogram_name_, this);
}

void StatisticsRecorder::ScopedHistogramSampleObserver::RunCallback(
    const char* histogram_name,
    uint64_t name_hash,
    HistogramBase::Sample sample) {
  callback_.Run(histogram_name, name_hash, sample);
}

StatisticsRecorder::~StatisticsRecorder() {
  const AutoLock auto_lock(GetLock());
  DCHECK_EQ(this, top_);
  top_ = previous_;
}

// static
void StatisticsRecorder::EnsureGlobalRecorderWhileLocked() {
  AssertLockHeld();
  if (top_) {
    return;
  }

  const StatisticsRecorder* const p = new StatisticsRecorder;
  // The global recorder is never deleted.
  ANNOTATE_LEAKING_OBJECT_PTR(p);
  DCHECK_EQ(p, top_);
}

// static
void StatisticsRecorder::RegisterHistogramProvider(
    const WeakPtr<HistogramProvider>& provider) {
  const AutoLock auto_lock(GetLock());
  EnsureGlobalRecorderWhileLocked();
  top_->providers_.push_back(provider);
}

// static
HistogramBase* StatisticsRecorder::RegisterOrDeleteDuplicate(
    HistogramBase* histogram) {
  CHECK(histogram);

  uint64_t hash = histogram->name_hash();

  // Ensure that histograms use HashMetricName() to compute their hash, since
  // that function is used to look up histograms. Intentionally a DCHECK since
  // this is expensive.
  DCHECK_EQ(hash, HashMetricName(histogram->histogram_name()));

  // Declared before |auto_lock| so that the histogram is deleted after the lock
  // is released (no point in holding the lock longer than needed).
  std::unique_ptr<HistogramBase> histogram_deleter;
  const AutoLock auto_lock(GetLock());
  EnsureGlobalRecorderWhileLocked();

  HistogramBase*& registered = top_->histograms_[hash];

  if (!registered) {
    registered = histogram;
    ANNOTATE_LEAKING_OBJECT_PTR(histogram);  // see crbug.com/79322
    // If there are callbacks for this histogram, we set the kCallbackExists
    // flag.
    if (base::Contains(top_->observers_, hash)) {
      // Note: SetFlags() does not write to persistent memory, it only writes to
      // an in-memory version of the flags.
      histogram->SetFlags(HistogramBase::kCallbackExists);
    }

    return histogram;
  }

  // Assert that there was no collision. Note that this is intentionally a
  // DCHECK because 1) this is expensive to call repeatedly, and 2) this
  // comparison may cause a read in persistent memory, which can cause I/O (this
  // is bad because |lock_| is currently being held).
  //
  // If you are a developer adding a new histogram and this DCHECK is being hit,
  // you are unluckily a victim of a hash collision. For now, the best solution
  // is to rename the histogram. Reach out to chrome-metrics-team@google.com if
  // you are unsure!
  DCHECK_EQ(strcmp(histogram->histogram_name(), registered->histogram_name()),
            0)
      << "Histogram name hash collision between " << histogram->histogram_name()
      << " and " << registered->histogram_name() << " (hash = " << hash << ")";

  if (histogram == registered) {
    // The histogram was registered before.
    return histogram;
  }

  // We already have a histogram with this name.
  histogram_deleter.reset(histogram);
  return registered;
}

// static
const BucketRanges* StatisticsRecorder::RegisterOrDeleteDuplicateRanges(
    const BucketRanges* ranges) {
  const BucketRanges* registered;
  {
    const AutoLock auto_lock(GetLock());
    EnsureGlobalRecorderWhileLocked();

    registered = top_->ranges_manager_.GetOrRegisterCanonicalRanges(ranges);
  }

  // Delete the duplicate ranges outside the lock to reduce contention.
  if (registered != ranges) {
    delete ranges;
  } else {
    ANNOTATE_LEAKING_OBJECT_PTR(ranges);
  }

  return registered;
}

// static
void StatisticsRecorder::WriteGraph(const std::string& query,
                                    std::string* output) {
  if (query.length())
    StringAppendF(output, "Collections of histograms for %s\n", query.c_str());
  else
    output->append("Collections of all histograms\n");

  for (const HistogramBase* const histogram :
       Sort(WithName(GetHistograms(), query))) {
    histogram->WriteAscii(output);
    output->append("\n");
  }
}

// static
std::string StatisticsRecorder::ToJSON(JSONVerbosityLevel verbosity_level) {
  std::string output = "{\"histograms\":[";
  const char* sep = "";
  for (const HistogramBase* const histogram : Sort(GetHistograms())) {
    output += sep;
    sep = ",";
    std::string json;
    histogram->WriteJSON(&json, verbosity_level);
    output += json;
  }
  output += "]}";
  return output;
}

// static
std::vector<const BucketRanges*> StatisticsRecorder::GetBucketRanges() {
  const AutoLock auto_lock(GetLock());

  // Manipulate |top_| through a const variable to ensure it is not mutated.
  const auto* const_top = top_;
  if (!const_top) {
    return std::vector<const BucketRanges*>();
  }

  return const_top->ranges_manager_.GetBucketRanges();
}

// static
HistogramBase* StatisticsRecorder::FindHistogram(std::string_view name) {
  uint64_t hash = HashMetricName(name);

  // This must be called *before* the lock is acquired below because it may call
  // back into StatisticsRecorder to register histograms. Those called methods
  // will acquire the lock at that time.
  ImportGlobalPersistentHistograms();

  const AutoLock auto_lock(GetLock());

  // Manipulate |top_| through a const variable to ensure it is not mutated.
  const auto* const_top = top_;
  if (!const_top) {
    return nullptr;
  }

  return const_top->FindHistogramByHashInternal(hash, name);
}

// static
StatisticsRecorder::HistogramProviders
StatisticsRecorder::GetHistogramProviders() {
  const AutoLock auto_lock(GetLock());

  // Manipulate |top_| through a const variable to ensure it is not mutated.
  const auto* const_top = top_;
  if (!const_top) {
    return StatisticsRecorder::HistogramProviders();
  }
  return const_top->providers_;
}

// static
void StatisticsRecorder::ImportProvidedHistograms(bool async,
                                                  OnceClosure done_callback) {
  // Merge histogram data from each provider in turn.
  HistogramProviders providers = GetHistogramProviders();
  auto barrier_callback =
      BarrierClosure(providers.size(), std::move(done_callback));
  for (const WeakPtr<HistogramProvider>& provider : providers) {
    // Weak-pointer may be invalid if the provider was destructed, though they
    // generally never are.
    if (!provider) {
      barrier_callback.Run();
      continue;
    }
    provider->MergeHistogramDeltas(async, barrier_callback);
  }
}

// static
void StatisticsRecorder::ImportProvidedHistogramsSync() {
  ImportProvidedHistograms(/*async=*/false, /*done_callback=*/DoNothing());
}

// static
StatisticsRecorder::SnapshotTransactionId StatisticsRecorder::PrepareDeltas(
    bool include_persistent,
    HistogramBase::Flags flags_to_set,
    HistogramBase::Flags required_flags,
    HistogramSnapshotManager* snapshot_manager) {
  Histograms histograms = Sort(GetHistograms(include_persistent));
  AutoLock lock(snapshot_lock_.Get());
  snapshot_manager->PrepareDeltas(std::move(histograms), flags_to_set,
                                  required_flags);
  return ++last_snapshot_transaction_id_;
}

// static
StatisticsRecorder::SnapshotTransactionId
StatisticsRecorder::SnapshotUnloggedSamples(
    HistogramBase::Flags required_flags,
    HistogramSnapshotManager* snapshot_manager) {
  Histograms histograms = Sort(GetHistograms());
  AutoLock lock(snapshot_lock_.Get());
  snapshot_manager->SnapshotUnloggedSamples(std::move(histograms),
                                            required_flags);
  return ++last_snapshot_transaction_id_;
}

// static
StatisticsRecorder::SnapshotTransactionId
StatisticsRecorder::GetLastSnapshotTransactionId() {
  AutoLock lock(snapshot_lock_.Get());
  return last_snapshot_transaction_id_;
}

// static
void StatisticsRecorder::InitLogOnShutdown() {
  const AutoLock auto_lock(GetLock());
  InitLogOnShutdownWhileLocked();
}

HistogramBase* StatisticsRecorder::FindHistogramByHashInternal(
    uint64_t hash,
    std::string_view name) const {
  AssertLockHeld();
  const HistogramMap::const_iterator it = histograms_.find(hash);
  if (it == histograms_.end()) {
    return nullptr;
  }
  // Assert that there was no collision. Note that this is intentionally a
  // DCHECK because 1) this is expensive to call repeatedly, and 2) this
  // comparison may cause a read in persistent memory, which can cause I/O (this
  // is bad because |lock_| is currently being held).
  //
  // If you are a developer adding a new histogram and this DCHECK is being hit,
  // you are unluckily a victim of a hash collision. For now, the best solution
  // is to rename the histogram. Reach out to chrome-metrics-team@google.com if
  // you are unsure!
  DCHECK_EQ(name, it->second->histogram_name())
      << "Histogram name hash collision between " << name << " and "
      << it->second->histogram_name() << " (hash = " << hash << ")";
  return it->second;
}

// static
void StatisticsRecorder::AddHistogramSampleObserver(
    const std::string& name,
    StatisticsRecorder::ScopedHistogramSampleObserver* observer) {
  DCHECK(observer);
  uint64_t hash = HashMetricName(name);

  const AutoLock auto_lock(GetLock());
  EnsureGlobalRecorderWhileLocked();

  auto iter = top_->observers_.find(hash);
  if (iter == top_->observers_.end()) {
    top_->observers_.insert(
        {hash, base::MakeRefCounted<HistogramSampleObserverList>(
                   ObserverListPolicy::EXISTING_ONLY)});
  }

  top_->observers_[hash]->AddObserver(observer);

  HistogramBase* histogram = top_->FindHistogramByHashInternal(hash, name);
  if (histogram) {
    // Note: SetFlags() does not write to persistent memory, it only writes to
    // an in-memory version of the flags.
    histogram->SetFlags(HistogramBase::kCallbackExists);
  }

  have_active_callbacks_.store(
      global_sample_callback() || !top_->observers_.empty(),
      std::memory_order_relaxed);
}

// static
void StatisticsRecorder::RemoveHistogramSampleObserver(
    const std::string& name,
    StatisticsRecorder::ScopedHistogramSampleObserver* observer) {
  uint64_t hash = HashMetricName(name);

  const AutoLock auto_lock(GetLock());
  EnsureGlobalRecorderWhileLocked();

  auto iter = top_->observers_.find(hash);
  CHECK(iter != top_->observers_.end(), base::NotFatalUntil::M125);

  auto result = iter->second->RemoveObserver(observer);
  if (result ==
      HistogramSampleObserverList::RemoveObserverResult::kWasOrBecameEmpty) {
    top_->observers_.erase(hash);

    // We also clear the flag from the histogram (if it exists).
    HistogramBase* histogram = top_->FindHistogramByHashInternal(hash, name);
    if (histogram) {
      // Note: ClearFlags() does not write to persistent memory, it only writes
      // to an in-memory version of the flags.
      histogram->ClearFlags(HistogramBase::kCallbackExists);
    }
  }

  have_active_callbacks_.store(
      global_sample_callback() || !top_->observers_.empty(),
      std::memory_order_relaxed);
}

// static
void StatisticsRecorder::FindAndRunHistogramCallbacks(
    base::PassKey<HistogramBase>,
    const char* histogram_name,
    uint64_t name_hash,
    HistogramBase::Sample sample) {
  DCHECK_EQ(name_hash, HashMetricName(histogram_name));

  const AutoLock auto_lock(GetLock());

  // Manipulate |top_| through a const variable to ensure it is not mutated.
  const auto* const_top = top_;
  if (!const_top) {
    return;
  }

  auto it = const_top->observers_.find(name_hash);

  // Ensure that this observer is still registered, as it might have been
  // unregistered before we acquired the lock.
  if (it == const_top->observers_.end()) {
    return;
  }

  it->second->Notify(FROM_HERE, &ScopedHistogramSampleObserver::RunCallback,
                     histogram_name, name_hash, sample);
}

// static
void StatisticsRecorder::SetGlobalSampleCallback(
    const GlobalSampleCallback& new_global_sample_callback) {
  const AutoLock auto_lock(GetLock());
  EnsureGlobalRecorderWhileLocked();

  DCHECK(!global_sample_callback() || !new_global_sample_callback);
  global_sample_callback_.store(new_global_sample_callback);

  have_active_callbacks_.store(
      new_global_sample_callback || !top_->observers_.empty(),
      std::memory_order_relaxed);
}

// static
size_t StatisticsRecorder::GetHistogramCount() {
  const AutoLock auto_lock(GetLock());

  // Manipulate |top_| through a const variable to ensure it is not mutated.
  const auto* const_top = top_;
  if (!const_top) {
    return 0;
  }
  return const_top->histograms_.size();
}

// static
void StatisticsRecorder::ForgetHistogramForTesting(std::string_view name) {
  const AutoLock auto_lock(GetLock());
  EnsureGlobalRecorderWhileLocked();

  uint64_t hash = HashMetricName(name);
  HistogramBase* base = top_->FindHistogramByHashInternal(hash, name);
  if (!base) {
    return;
  }

  if (base->GetHistogramType() != SPARSE_HISTOGRAM) {
    // When forgetting a histogram, it's likely that other information is also
    // becoming invalid. Clear the persistent reference that may no longer be
    // valid. There's no danger in this as, at worst, duplicates will be created
    // in persistent memory.
    static_cast<Histogram*>(base)->bucket_ranges()->set_persistent_reference(0);
  }

  // This performs another lookup in the map, but this is fine since this is
  // only used in tests.
  top_->histograms_.erase(hash);
}

// static
std::unique_ptr<StatisticsRecorder>
StatisticsRecorder::CreateTemporaryForTesting() {
  const AutoLock auto_lock(GetLock());
  std::unique_ptr<StatisticsRecorder> temporary_recorder =
      WrapUnique(new StatisticsRecorder());
  temporary_recorder->ranges_manager_
      .DoNotReleaseRangesOnDestroyForTesting();  // IN-TEST
  return temporary_recorder;
}

// static
void StatisticsRecorder::SetRecordChecker(
    std::unique_ptr<RecordHistogramChecker> record_checker) {
  const AutoLock auto_lock(GetLock());
  EnsureGlobalRecorderWhileLocked();
  top_->record_checker_ = std::move(record_checker);
}

// static
bool StatisticsRecorder::ShouldRecordHistogram(uint32_t histogram_hash) {
  const AutoLock auto_lock(GetLock());

  // Manipulate |top_| through a const variable to ensure it is not mutated.
  const auto* const_top = top_;
  return !const_top || !const_top->record_checker_ ||
         const_top->record_checker_->ShouldRecord(histogram_hash);
}

// static
StatisticsRecorder::Histograms StatisticsRecorder::GetHistograms(
    bool include_persistent) {
  // This must be called *before* the lock is acquired below because it will
  // call back into this object to register histograms. Those called methods
  // will acquire the lock at that time.
  ImportGlobalPersistentHistograms();

  Histograms out;

  const AutoLock auto_lock(GetLock());

  // Manipulate |top_| through a const variable to ensure it is not mutated.
  const auto* const_top = top_;
  if (!const_top) {
    return out;
  }

  out.reserve(const_top->histograms_.size());
  for (const auto& entry : const_top->histograms_) {
    // Note: HasFlags() does not read to persistent memory, it only reads an
    // in-memory version of the flags.
    bool is_persistent = entry.second->HasFlags(HistogramBase::kIsPersistent);
    if (!include_persistent && is_persistent) {
      continue;
    }
    out.push_back(entry.second);
  }

  return out;
}

// static
StatisticsRecorder::Histograms StatisticsRecorder::Sort(Histograms histograms) {
  ranges::sort(histograms, &HistogramNameLesser);
  return histograms;
}

// static
StatisticsRecorder::Histograms StatisticsRecorder::WithName(
    Histograms histograms,
    const std::string& query,
    bool case_sensitive) {
  // Need a C-string query for comparisons against C-string histogram name.
  std::string lowercase_query;
  const char* query_string;
  if (case_sensitive) {
    query_string = query.c_str();
  } else {
    lowercase_query = base::ToLowerASCII(query);
    query_string = lowercase_query.c_str();
  }

  histograms.erase(
      ranges::remove_if(
          histograms,
          [query_string, case_sensitive](const HistogramBase* const h) {
            return !strstr(
                case_sensitive
                    ? h->histogram_name()
                    : base::ToLowerASCII(h->histogram_name()).c_str(),
                query_string);
          }),
      histograms.end());
  return histograms;
}

// static
void StatisticsRecorder::ImportGlobalPersistentHistograms() {
  // Import histograms from known persistent storage. Histograms could have been
  // added by other processes and they must be fetched and recognized locally.
  // If the persistent memory segment is not shared between processes, this call
  // does nothing.
  if (GlobalHistogramAllocator* allocator = GlobalHistogramAllocator::Get())
    allocator->ImportHistogramsToStatisticsRecorder();
}

StatisticsRecorder::StatisticsRecorder() {
  AssertLockHeld();
  previous_ = top_;
  top_ = this;
  InitLogOnShutdownWhileLocked();
}

// static
void StatisticsRecorder::InitLogOnShutdownWhileLocked() {
  AssertLockHeld();
  if (!is_vlog_initialized_ && VLOG_IS_ON(1)) {
    is_vlog_initialized_ = true;
    const auto dump_to_vlog = [](void*) {
      std::string output;
      WriteGraph("", &output);
      VLOG(1) << output;
    };
    AtExitManager::RegisterCallback(dump_to_vlog, nullptr);
  }
}

}  // namespace base
