// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_SCUDO_PURGE_COORDINATOR_H_
#define BASE_ANDROID_SCUDO_PURGE_COORDINATOR_H_

#include <memory>

#include "base/base_export.h"
#include "base/functional/callback.h"
#include "base/memory/post_delayed_memory_reduction_task.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace base {
class HistogramBase;
}  // namespace base

namespace base::android {

inline constexpr int kScudoPurge = -101;
inline constexpr int kScudoPurgeAll = -104;

// Coordinates memory purging using Scudo's mallopt extension parameters
// (kScudoPurge and kScudoPurgeAll) on Android.
class BASE_EXPORT ScudoPurgeCoordinator {
 public:
  using MalloptFn = base::RepeatingCallback<bool(int param, int value)>;

  struct BASE_EXPORT Configuration {
    // Factory that initializes configuration from
    // base::features::kPeriodicScudoPurge and its associated feature params.
    static Configuration FromFeatures();

    MalloptFn mallopt_fn_for_testing;
    base::TimeDelta initial_delay = base::Seconds(60);
    base::TimeDelta periodic_interval = base::Seconds(60);
    base::TimeDelta background_delay = base::Seconds(10);
    bool enable_foreground_periodic = true;
    bool enable_background_purge = true;
  };

  // Creates a coordinator instance with Configuration::FromFeatures() if
  // base::features::kPeriodicScudoPurge is enabled, or returns nullptr.
  static std::unique_ptr<ScudoPurgeCoordinator> CreateIfEnabled();

  // Calls mallopt(param, value) directly as default MalloptFn. Returns true on
  // success.
  static bool DefaultMallopt(int param, int value);

  explicit ScudoPurgeCoordinator(Configuration config);
  ~ScudoPurgeCoordinator();

  ScudoPurgeCoordinator(const ScudoPurgeCoordinator&) = delete;
  ScudoPurgeCoordinator& operator=(const ScudoPurgeCoordinator&) = delete;

  // Starts the purge coordinator.
  void Start();

  void OnForegrounded();
  void OnBackgrounded();

  bool is_foreground_periodic_running_for_testing() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return periodic_timer_.IsRunning();
  }
  bool is_background_timer_running_for_testing() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return background_timer_.IsRunning();
  }
  bool is_in_foreground_for_testing() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return in_foreground_;
  }

 private:
  void RunPeriodicForegroundPurge();
  void RunBackgroundPurge();

  void DispatchPurgeTask(int purge_param,
                         base::HistogramBase* duration_histogram);
  static void PerformPurgeInternal(MalloptFn fn,
                                   int param,
                                   base::HistogramBase* duration_histogram);

  void OnInitialDelayExpired();

  SEQUENCE_CHECKER(sequence_checker_);

  Configuration config_;
  MalloptFn mallopt_fn_;
  raw_ptr<base::HistogramBase> foreground_duration_histogram_ = nullptr;
  raw_ptr<base::HistogramBase> background_duration_histogram_ = nullptr;
  bool is_started_ = false;
  bool in_foreground_ GUARDED_BY_CONTEXT(sequence_checker_) = true;

  base::OneShotTimer initial_delay_timer_;
  base::RepeatingTimer periodic_timer_;
  base::OneShotDelayedBackgroundTimer background_timer_;

  base::WeakPtrFactory<ScudoPurgeCoordinator> weak_factory_{this};
};

}  // namespace base::android

#endif  // BASE_ANDROID_SCUDO_PURGE_COORDINATOR_H_
