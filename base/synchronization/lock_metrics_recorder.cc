// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/lock_metrics_recorder.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/containers/ring_buffer.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/metrics/histogram.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/threading/platform_thread_ref.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_local_storage.h"
#include "base/time/time.h"

namespace base {

// Due to this class being used to hold samples of metrics related to locks,
// there are several constraints to `LockMetricsRecorder`:
//
// 1. Must not allocate memory during recording or subsampling
// 2. Must not acquire a lock during recording or subsampling
// 3. Must not report metrics during TLS initialization
//
// (Note that `EnableRecordingOnCurrentThread()` is exempt from rules 1 and 2, as
// it allocates memory and potentially acquires locks (`pthread_lock`) during
// initialization.)
//
// In order to satisfy these constraints, we use a custom TLS Slot
// (`base::ThreadLocalStorage::Slot` via `base::ThreadLocalOwnedPointer`) to store
// the thread-local data, instead of the `thread_local` keyword. This is because
// the Chromium Style Guide forbids the use of `thread_local` inside any code that
// must not allocate memory, due to reentrancy and deadlock risks across
// different platforms. Upon further investigation, we found that:
//
// - Linux/glibc: When resolving a dynamic TLS variable for the first time on a
//   thread, glibc uses `__tls_get_addr()`. This dynamically resizes the DTV
//   table using malloc and realloc. Because `PartitionAlloc` overrides these
//   allocators using global locks via the `PartitionAlloc` shim (which then use
//   this lock metrics recorder), using thread_local here triggers an infinite
//   recursion loop and crashes. (Note: glibc uses a global counter
//   `_dl_tls_threads_in_update` to track loader state, but it is not per-thread
//   and `PartitionAlloc` cannot see it).
//
// - macOS / Windows: macOS uses the dynamic linker (dyld) and tlv_get_addr
//   for resolving TLS. Windows uses `__dyn_tls_init` +
//   `__dyn_tls_on_demand_init`. Both of these eventually call standard
//   allocators (e.g., `_base_malloc`) which redirect to the `PartitionAlloc` shim,
//   resulting in the same infinite reentrancy.
//
// - Android: On arm64 Android, `thread_local` uses the TLSDESC dynamic model.
//   It uses a resolver function in bionic, and on first access it
//   triggers `__tls_get_addr()` and uses its own custom `BionicAllocator`.

namespace {

// Global atomic pointer to the TLS slot.
//
// We use an atomic instead of a static function-local in `GetForCurrentThread()`
// to avoid compiler-generated guard locks (`__cxa_guard_acquire`) on the hot
// path. On some platforms without futex support (like macOS's `libcxxabi`), these
// guards acquire internal locks on first access which could cause deadlocks if
// those locks are recorded.
//
// Standard abstractions like `LazyInstance` or `Singleton` are discouraged
// in modern Chromium code, so we manage the atomic directly.
std::atomic<base::ThreadLocalOwnedPointer<LockMetricsRecorder>*> g_tls_slot{
    nullptr};

constexpr int kHistogramBucketCount = 100;

base::HistogramBase* CreateLockHistogram(std::string_view lock_name,
                                         std::string_view histogram_suffix) {
  return base::Histogram::FactoryMicrosecondsTimeGet(
      StrCat({"Scheduling.ContendedLockAcquisitionTime.", lock_name, ".",
              histogram_suffix}),
      Microseconds(1), Seconds(1), kHistogramBucketCount,
      base::HistogramBase::kUmaTargetedHistogramFlag);
}

std::vector<std::string>& GetAllowedThreads() {
  static base::NoDestructor<std::vector<std::string>> allowed_threads(
      base::SplitString(
          base::features::kRecordLockAcquisitionTimeAllowedThreads.Get(), ",",
          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));
  return *allowed_threads;
}

}  // namespace

// We must use acquire-release semantics on `g_tls_slot` in order
// to ensure the read of `g_tls_slot` happens after the
// construction of the object it points to.
// static
LockMetricsRecorder* LockMetricsRecorder::GetForCurrentThread() {
  base::ThreadLocalOwnedPointer<LockMetricsRecorder>* slot =
      g_tls_slot.load(std::memory_order_acquire);

  // Early return if we haven't initialized TLS.
  if (!slot) {
    return nullptr;
  }

  // A thread's TLS may be destroyed before the thread exits, and if we use
  // `ThreadLocalOwnedPointer`, we are using a `ThreadLocalStorage` slot. So, using
  // `HasBeenDestroyed` is necessary because we may acquire a lock after
  // TLS has been destroyed, which will cause a crash.
  if (base::ThreadLocalStorage::HasBeenDestroyed()) [[unlikely]] {
    return nullptr;
  }

  return slot->Get();
}

// static
void LockMetricsRecorder::ReportLockHistogram(
    TimeDelta sample,
    base::HistogramBase* histogram_pointer) {
  histogram_pointer->AddTimeMicrosecondsGranularity(sample);
}

bool LockMetricsRecorder::ShouldRecordLockAcquisitionTime() const {
  DCHECK(CalledOnValidThread());
  return !iterating_in_progress_ && subsampler_.ShouldSample(kSamplingRatio);
}

void LockMetricsRecorder::RecordLockAcquisitionTime(
    const LockMetricSample& sample) {
  DCHECK(CalledOnValidThread());
  unified_sample_buffer_.SaveToBuffer(sample);
}

void LockMetricsRecorder::ForEachSample(
    FunctionRef<void(const LockMetricSample&)> f) {
  DCHECK(CalledOnValidThread());
  CHECK(!iterating_in_progress_);
  // Set the `iterating_in_progress_` flag to true to prevent reentrancy due to
  // any lock contention during the recording of the histogram. This keeps the
  // recording and reporting logic simple at the cost of a tiny blind-spot in
  // our metrics.
  AutoReset<bool> mark_iterating_in_progress(&iterating_in_progress_, true);

  for (auto it = unified_sample_buffer_.Begin(); it; ++it) {
    f(**it);
  }
  unified_sample_buffer_.Clear();
}

void LockMetricsRecorder::ReportLockAcquisitionTimes() {
  DCHECK(CalledOnValidThread());

  if (iterating_in_progress_) {
    return;
  }

  // Copy guarded members to local variables to appease the static analyzer.
  // Clang's thread-safety analysis treats lambda scopes as new contexts and
  // generates false-positive "missing lock" errors, even though the context was
  // verified at the top of this function.
  base::HistogramBase* base_lock_histogram = base_lock_histogram_;
  base::HistogramBase* partition_alloc_lock_histogram =
      partition_alloc_lock_histogram_;

  ForEachSample([base_lock_histogram, partition_alloc_lock_histogram](
                    const LockMetricsRecorder::LockMetricSample& sample) {
    switch (sample.type) {
      case LockType::kBaseLock:
        ReportLockHistogram(sample.wait_time, base_lock_histogram);
        break;
      case LockType::kPartitionAllocLock:
        ReportLockHistogram(sample.wait_time, partition_alloc_lock_histogram);
        break;
    }
  });
}

// `EnableRecordingOnCurrentThread()` is the only function responsible for
// initializing the TLS slot and publishing it to the global atomic `g_tls_slot`.
//
// This may acquire compiler-generated guard locks, callers must not be in a
// non-allocating hot path.
//
// By doing this during explicit setup (which is exempt from non-allocating
// rules), `GetForCurrentThread()` can safely read via a lock-free atomic load
// without reentrancy risks.
// static
void LockMetricsRecorder::EnableRecordingOnCurrentThread(
    std::string_view histogram_suffix) {
  CHECK(!histogram_suffix.empty());

  if (!base::FeatureList::IsEnabled(
          base::features::kRecordLockAcquisitionTime)) {
    return;
  }

  std::vector<std::string>& allowed = GetAllowedThreads();
  if (std::ranges::find(allowed, histogram_suffix) == allowed.end()) {
    return;
  }

  static base::NoDestructor<base::ThreadLocalOwnedPointer<LockMetricsRecorder>>
      tls_slot;
  g_tls_slot.store(tls_slot.get(), std::memory_order_release);

  if (!tls_slot->Get()) {
    tls_slot->Set(std::make_unique<LockMetricsRecorder>(
        base::PassKey<LockMetricsRecorder>(), histogram_suffix));
  }
}

LockMetricsRecorder::LockMetricsRecorder(PassKey,
                                         std::string_view histogram_suffix)
    : base_lock_histogram_(CreateLockHistogram("BaseLock", histogram_suffix)),
      partition_alloc_lock_histogram_(
          CreateLockHistogram("PartitionAllocLock", histogram_suffix)) {}

// static
LockMetricsRecorder::ScopedLockAcquisitionTimer
LockMetricsRecorder::ScopedLockAcquisitionTimer::CreateForTest(
    LockMetricsRecorder* recorder) {
  return LockMetricsRecorder::ScopedLockAcquisitionTimer(recorder);
}

// static
void LockMetricsRecorder::DisableRecordingOnCurrentThreadForTesting() {
  base::ThreadLocalOwnedPointer<LockMetricsRecorder>* slot =
      g_tls_slot.load(std::memory_order_acquire);
  if (slot) {
    slot->Set(nullptr);
  }
}

// static
void LockMetricsRecorder::SetAllowedThreadsForTesting(
    std::vector<std::string> allowed_threads) {
  GetAllowedThreads() = std::move(allowed_threads);
}

}  // namespace base
