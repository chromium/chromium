// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_SELF_COMPACTION_MANAGER_H_
#define BASE_ANDROID_SELF_COMPACTION_MANAGER_H_

// TODO(thiabaud): remove this include once we have separated the locks
// between these two classes.
#include "base/android/pre_freeze_background_memory_trimmer.h"
#include "base/byte_count.h"
#include "base/debug/proc_maps_linux.h"
#include "base/no_destructor.h"
#include "base/profiler/sample_metadata.h"

namespace base::android {

BASE_EXPORT BASE_DECLARE_FEATURE(kShouldFreezeSelf);
BASE_EXPORT BASE_DECLARE_FEATURE(kUseRunningCompact);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CompactCancellationReason {
  kAppFreezer,
  kPageResumed,
  kTimeout,
  kMaxValue = kTimeout
};

class BASE_EXPORT SelfCompactionManager {
 public:
  using CompactCancellationReason = CompactCancellationReason;
  static void OnSelfFreeze();
  static void OnRunningCompact();
  static void RequestRunningCompactWithDelay(const TimeDelta delay);

  // If we are currently doing self compaction, cancel it. If it was running,
  // record a metric with the reason for the cancellation.
  static void MaybeCancelCompaction(
      CompactCancellationReason cancellation_reason);

  // The callback runs in the thread pool. The caller cannot make any thread
  // safety assumptions for the callback execution (e.g. it could run
  // concurrently with the thread that registered it).
  static void SetOnStartSelfCompactionCallback(base::RepeatingClosure callback)
      LOCKS_EXCLUDED(lock());

  class CompactionMetric final : public RefCountedThreadSafe<CompactionMetric> {
   public:
    CompactionMetric(const std::string& name,
                     base::TimeTicks triggered_at,
                     base::TimeTicks started_at);

    void RecordDelayedMetrics();
    void RecordTimeMetrics(base::TimeTicks compaction_last_finished,
                           base::TimeTicks compaction_last_cancelled);

    void RecordBeforeMetrics();
    void MaybeRecordCompactionMetrics() LOCKS_EXCLUDED(lock());

   private:
    friend class RefCountedThreadSafe<CompactionMetric>;
    ~CompactionMetric();
    void RecordSmapsRollup(std::optional<debug::SmapsRollup>* target)
        LOCKS_EXCLUDED(lock());
    void RecordSmapsRollupWithDelay(std::optional<debug::SmapsRollup>* target,
                                    base::TimeDelta delay);
    std::string GetMetricName(std::string_view name) const;
    std::string GetMetricName(std::string_view name,
                              std::string_view suffix) const;
    void RecordCompactionMetrics(const debug::SmapsRollup& value,
                                 std::string_view suffix);
    void RecordCompactionMetric(ByteCount value_bytes,
                                std::string_view metric_name,
                                std::string_view suffix);
    void RecordCompactionDiffMetrics(const debug::SmapsRollup& before,
                                     const debug::SmapsRollup& after,
                                     std::string_view suffix);
    void RecordCompactionDiffMetric(ByteCount before_value_bytes,
                                    ByteCount after_value_bytes,
                                    std::string_view name,
                                    std::string_view suffix);

    const std::string name_;
    // When the self compaction was first triggered. There is a delay between
    // this time and when we actually begin the compaction.
    const base::TimeTicks compaction_triggered_at_;
    // When the self compaction first started. This should generally be
    // |compaction_triggered_at_ +
    // kShouldFreezeSelfDelayAfterPreFreezeTasks.Get()|, but may be longer if
    // the task was delayed.
    const base::TimeTicks compaction_started_at_;
    // We use std::optional here because:
    // - We record these incrementally.
    // - We may stop recording at some point.
    // - We only want to emit histograms if all values were recorded.
    std::optional<debug::SmapsRollup> smaps_before_;
    std::optional<debug::SmapsRollup> smaps_after_;
    std::optional<debug::SmapsRollup> smaps_after_1s_;
    std::optional<debug::SmapsRollup> smaps_after_10s_;
    std::optional<debug::SmapsRollup> smaps_after_60s_;
  };

  class CompactionState {
   public:
    CompactionState(scoped_refptr<SequencedTaskRunner> task_runner,
                    base::TimeTicks triggered_at,
                    uint64_t max_bytes);
    virtual ~CompactionState();

    virtual bool IsFeatureEnabled() const = 0;
    virtual std::string GetMetricName(std::string_view name) const = 0;
    void MaybeReadProcMaps();
    virtual scoped_refptr<CompactionMetric> MakeCompactionMetric(
        base::TimeTicks started_at) const = 0;
    virtual base::TimeDelta GetDelayAfterPreFreezeTasks() const = 0;

    scoped_refptr<SequencedTaskRunner> task_runner_;
    std::vector<debug::MappedMemoryRegion> regions_;
    const base::TimeTicks triggered_at_;
    const uint64_t max_bytes_;
  };

 private:
  friend class base::NoDestructor<SelfCompactionManager>;
  friend class PreFreezeBackgroundMemoryTrimmer;
  friend class SelfCompactionState;
  friend class RunningCompactionState;
  friend class PreFreezeSelfCompactionTest;
  friend class PreFreezeSelfCompactionTestWithParam;
  FRIEND_TEST_ALL_PREFIXES(PreFreezeSelfCompactionTestWithParam, Disabled);
  FRIEND_TEST_ALL_PREFIXES(PreFreezeSelfCompactionTestWithParam, TimeoutCancel);
  FRIEND_TEST_ALL_PREFIXES(PreFreezeSelfCompactionTestWithParam, Cancel);
  FRIEND_TEST_ALL_PREFIXES(PreFreezeSelfCompactionTest, NotCanceled);
  FRIEND_TEST_ALL_PREFIXES(PreFreezeSelfCompactionTest, OnSelfFreezeCancel);

  SelfCompactionManager();
  ~SelfCompactionManager();
  static SelfCompactionManager& Instance();
  static Lock& lock() LOCK_RETURNED(PreFreezeBackgroundMemoryTrimmer::lock()) {
    return PreFreezeBackgroundMemoryTrimmer::lock();
  }

  static bool CompactionIsSupported();
  static bool TimeoutExceeded();
  static base::TimeDelta GetDelayBetweenCompaction();

  static bool ShouldContinueCompaction(const CompactionState& state)
      LOCKS_EXCLUDED(lock());
  static bool ShouldContinueCompaction(base::TimeTicks compaction_triggered_at)
      LOCKS_EXCLUDED(lock());

  void MaybeCancelCompactionInternal(
      CompactCancellationReason cancellation_reason)
      EXCLUSIVE_LOCKS_REQUIRED(lock());

  // Compacts the memory for the process.
  static void CompactSelf(std::unique_ptr<CompactionState> state);
  template <class State>
  void OnTriggerCompact(scoped_refptr<SequencedTaskRunner> task_runner);
  void OnTriggerCompact(std::unique_ptr<CompactionState> state)
      EXCLUSIVE_LOCKS_REQUIRED(lock());
  static void OnTriggerRunningCompact(std::unique_ptr<CompactionState> state)
      LOCKS_EXCLUDED(lock());
  void StartCompaction(std::unique_ptr<CompactionState> state)
      LOCKS_EXCLUDED(lock());
  void MaybePostCompactionTask(std::unique_ptr<CompactionState> state,
                               scoped_refptr<CompactionMetric> metric)
      LOCKS_EXCLUDED(lock());
  void CompactionTask(std::unique_ptr<CompactionState> state,
                      scoped_refptr<CompactionMetric> metric)
      LOCKS_EXCLUDED(lock());
  void FinishCompaction(std::unique_ptr<CompactionState> state,
                        scoped_refptr<CompactionMetric> metric)
      LOCKS_EXCLUDED(lock());
  static std::optional<uint64_t> CompactMemory(
      std::vector<debug::MappedMemoryRegion>* regions,
      const uint64_t max_bytes);
  static std::optional<uint64_t> CompactRegion(
      debug::MappedMemoryRegion region);

  void MaybeRunOnSelfCompactCallback() EXCLUSIVE_LOCKS_REQUIRED(lock());

  static void ResetCompactionForTesting();
  static std::unique_ptr<CompactionState> GetSelfCompactionStateForTesting(
      scoped_refptr<SequencedTaskRunner> task_runner,
      const TimeTicks& triggered_at);
  static std::unique_ptr<CompactionState> GetRunningCompactionStateForTesting(
      scoped_refptr<SequencedTaskRunner> task_runner,
      const TimeTicks& triggered_at);

  // Whether or not we should continue self compaction. There are two reasons
  // why we would cancel:
  // (1) We have resumed, meaning we are likely to touch much of the process
  //     memory soon, and we do not want to waste CPU time with compaction,
  //     since it can block other work that needs to be done.
  // (2) We are going to be frozen by App Freezer, which will do the compaction
  //     work for us. This situation should be relatively rare, because we
  //     attempt to not do self compaction if we know that we are going to
  //     frozen by App Freezer.
  base::TimeTicks compaction_last_cancelled_ GUARDED_BY(lock()) =
      base::TimeTicks::Min();
  // When we last triggered self compaction. Used to record metrics.
  base::TimeTicks compaction_last_triggered_ GUARDED_BY(lock()) =
      base::TimeTicks::Min();
  // When we last started self compaction. Used to know if we should cancel
  // compaction due to it taking too long.
  base::TimeTicks compaction_last_started_ GUARDED_BY(lock()) =
      base::TimeTicks::Min();
  // When we last finished self compaction (either successfully, or from
  // being cancelled). Used to record metrics.
  base::TimeTicks compaction_last_finished_ GUARDED_BY(lock()) =
      base::TimeTicks::Min();
  base::RepeatingClosure on_self_compact_callback_ GUARDED_BY(lock());
  std::optional<base::ScopedSampleMetadata> process_compacted_metadata_
      GUARDED_BY(lock());
};

}  // namespace base::android

#endif  // BASE_ANDROID_SELF_COMPACTION_MANAGER_H_
