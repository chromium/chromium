// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PERF_METRIC_COLLECTOR_H_
#define CHROME_BROWSER_METRICS_PERF_METRIC_COLLECTOR_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/metrics/perf/collection_params.h"

namespace metrics {

class SampledProfile;

namespace internal {

// MetricCollector implements the basic collector functionality, including the
// scheduling and synchronization logic between the various trigger events, and
// the profile post-processing logic. Except for the constructor that can be
// invoked from a different thread, all its methods run on the same sequence,
// which is enforced by a sequence checker. A collector is managed by a
// MetricProvider, including the sequence on which it runs.
class MetricCollector {
 public:
  using ProfileDoneCallback =
      base::RepeatingCallback<void(std::unique_ptr<SampledProfile>)>;

  // It may be invoked on a difference sequence than the member functions.
  MetricCollector(const std::string& name,
                  const CollectionParams& collection_params);

  MetricCollector(const MetricCollector&) = delete;
  MetricCollector& operator=(const MetricCollector&) = delete;

  virtual ~MetricCollector();

  virtual const char* ToolName() const = 0;

  void Init();

  // These methods are used to update the cached_data_size_ field via PostTask
  // from the provider.
  void AddCachedDataDelta(size_t delta);
  void ResetCachedDataSize();

  // Turns on profile collection. Resets the timer that's used to schedule
  // collections.
  void RecordUserLogin(base::TimeTicks login_time);

  // Deactivates the timer and turns off profile collection. Does not delete any
  // profile data that was already collected.
  void StopTimer();

  // Schedules a collection after a resume from suspend event. A collection is
  // scheduled with the probablity given by the sampling factor stored in
  // |collection_params_|.
  void ScheduleSuspendDoneCollection(base::TimeDelta sleep_duration);

  // Schedules a collection after a session restore event. A collection is
  // scheduled with the probablity given by the sampling factor stored in
  // |collection_params_|.
  void ScheduleSessionRestoreCollection(int num_tabs_restored);

  // Called when a jank started/stopped.
  void OnJankStarted();
  void OnJankStopped();

  void set_profile_done_callback(ProfileDoneCallback cb) {
    profile_done_callback_ = std::move(cb);
  }

 protected:
  // Enumeration representing success and various failure modes for collecting
  // profile data. These values are persisted to logs. Entries should not be
  // renumbered and numeric values should never be reused.
  enum class CollectionAttemptStatus {
    SUCCESS,
    NOT_READY_TO_COLLECT,
    INCOGNITO_ACTIVE,
    INCOGNITO_LAUNCHED,
    PROTOBUF_NOT_PARSED,
    ILLEGAL_DATA_RETURNED,
    ALREADY_COLLECTING,
    UNABLE_TO_COLLECT,
    DATA_COLLECTION_FAILED,
    SESSION_HAS_ZERO_SAMPLES,
    NUM_OUTCOMES
  };

  // Returns a WeakPtr to this instance.
  virtual base::WeakPtr<MetricCollector> GetWeakPtr() = 0;

  // Collector specific initialization.
  virtual void SetUp() {}

  // Saves the given outcome to the uma histogram associated with the collector.
  void AddToUmaHistogram(CollectionAttemptStatus outcome) const;

  // Returns whether the underlying timer is running or not.
  bool IsRunning() { return timer_.IsRunning(); }
  // Returns the current timer delay. Useful for debugging.
  base::TimeDelta CurrentTimerDelay() { return timer_.GetCurrentDelay(); }

  base::TimeTicks login_time() const { return login_time_; }

  // Collects perf data after a resume. |sleep_duration| is the duration the
  // system was suspended before resuming. |time_after_resume_ms| is how long
  // ago the system resumed.
  void CollectPerfDataAfterResume(base::TimeDelta sleep_duration,
                                  base::TimeDelta time_after_resume);

  // Collects perf data after a session restore. |time_after_restore| is how
  // long ago the session restore started. |num_tabs_restored| is the total
  // number of tabs being restored.
  void CollectPerfDataAfterSessionRestore(base::TimeDelta time_after_restore,
                                          int num_tabs_restored);

  // Selects a random time in the upcoming profiling interval that begins at
  // |next_profiling_interval_start|. Schedules |timer| to invoke
  // DoPeriodicCollection() when that time comes.
  void ScheduleIntervalCollection();

  // Collects profiles on a repeating basis by calling CollectIfNecessary() and
  // reschedules it to be collected again.
  void DoPeriodicCollection();

  // Collects a profile for a given |trigger_event| if necessary.
  void CollectIfNecessary(std::unique_ptr<SampledProfile> sampled_profile);

  // Returns if it's valid and safe for a collector to gather a profile.
  // A collector implementation can override this logic.
  virtual bool ShouldCollect() const;

  // Collector specific logic for collecting a profile.
  virtual void CollectProfile(
      std::unique_ptr<SampledProfile> sampled_profile) = 0;

  // Collector specific logic for stopping the current collection.
  virtual void StopCollection() {}

  // Parses the given serialized perf data proto. If valid, it adds it to the
  // given sampled_profile and stores it in the local profile data cache.
  void SaveSerializedPerfProto(std::unique_ptr<SampledProfile> sampled_profile,
                               std::string serialized_proto);

  // Returns a const reference to the collection_params.
  const CollectionParams& collection_params() const {
    return collection_params_;
  }

  // Returns a mutable reference to the collection_params, so that collectors
  // and tests can update the params.
  CollectionParams& collection_params() { return collection_params_; }

  // The size of cached profile data.
  size_t cached_data_size_ = 0;

  // Checks that some methods are called on the collector sequence.
  SEQUENCE_CHECKER(sequence_checker_);

 private:
  // Parameters controlling how profiles are collected. Initialized at
  // collector creation time. Then accessed only on the collector sequence.
  CollectionParams collection_params_;

  // For scheduling collection of profile data.
  base::OneShotTimer timer_;

  // Record of the last login time.
  base::TimeTicks login_time_;

  // Record of the start of the upcoming profiling interval.
  base::TimeTicks next_profiling_interval_start_;

  // Tracks the last time a session restore was collected.
  base::TimeTicks last_session_restore_collection_time_;

  // Name of the histogram that represents the success and various failure
  // modes of collection attempts.
  const std::string collect_uma_histogram_;

  // A callback to be Run on each successfully collected profile.
  ProfileDoneCallback profile_done_callback_;
};

}  // namespace internal

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_PERF_METRIC_COLLECTOR_H_
