// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_POST_LOGIN_METRICS_RECORDER_H_
#define ASH_METRICS_POST_LOGIN_METRICS_RECORDER_H_

#include <optional>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/metrics/deferred_metrics_reporter.h"
#include "ash/metrics/post_login_event_observer.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "cc/metrics/frame_sequence_metrics.h"

namespace ash {

class LoginUnlockThroughputRecorder;

// PostLoginMetricsRecorder observe post login events and record UMA metrics /
// trace events.
class ASH_EXPORT PostLoginMetricsRecorder : public PostLoginEventObserver {
 public:
  explicit PostLoginMetricsRecorder(
      LoginUnlockThroughputRecorder* login_unlock_throughput_recorder);
  PostLoginMetricsRecorder(const PostLoginMetricsRecorder&) = delete;
  PostLoginMetricsRecorder& operator=(const PostLoginMetricsRecorder&) = delete;
  ~PostLoginMetricsRecorder() override;

  // PostLoginEventObserver overrides:
  void OnAuthSuccess(base::TimeTicks ts) override;
  void OnUserLoggedIn(base::TimeTicks ts,
                      bool is_ash_restarted,
                      bool is_regular_user_or_owner) override;
  void OnAllExpectedShelfIconLoaded(base::TimeTicks ts) override;
  void OnSessionRestoreDataLoaded(base::TimeTicks ts,
                                  bool restore_automatically) override;
  void OnAllBrowserWindowsCreated(base::TimeTicks ts) override;
  void OnAllBrowserWindowsShown(base::TimeTicks ts) override;
  void OnAllBrowserWindowsPresented(base::TimeTicks ts) override;
  void OnShelfAnimationFinished(base::TimeTicks ts) override;
  void OnCompositorAnimationFinished(
      base::TimeTicks ts,
      const cc::FrameSequenceMetrics::CustomReportData& data) override;
  void OnArcUiReady(base::TimeTicks ts) override;
  void OnShelfIconsLoadedAndSessionRestoreDone(base::TimeTicks ts) override;
  void OnShelfAnimationAndCompositorAnimationDone(base::TimeTicks ts) override;

 private:
  class TimeMarker {
   public:
    TimeMarker(const std::string& name, base::TimeTicks time);
    TimeMarker(TimeMarker&& other) = default;
    TimeMarker& operator=(TimeMarker&& other) = default;
    ~TimeMarker() = default;

    const std::string& name() const { return name_; }
    base::TimeTicks time() const { return time_; }

    // Comparator for sorting.
    bool operator<(const TimeMarker& other) const {
      return time_ < other.time_;
    }

   private:
    friend class std::vector<TimeMarker>;

    std::string name_;
    base::TimeTicks time_;
  };

  // Add a time marker for login events. A timeline will be reported after
  // login animation is done.
  void AddLoginTimeMarker(const std::string& name, base::TimeTicks timestamp);
  void EnsureTracingSliceNamed(base::TimeTicks ts);
  void ReportTraceEvents();

  std::vector<TimeMarker> markers_;

  // Records the timestamp of `OnAuthSuccess` or `OnUserLoggedIn`, which
  // ever happens first, as the origin time of a user login.
  std::optional<base::TimeTicks> timestamp_origin_;

  // Used for reporting metrics with different names depending on the session
  // restore flow.
  DeferredMetricsReporter uma_login_perf_;

  base::ScopedObservation<LoginUnlockThroughputRecorder, PostLoginEventObserver>
      post_login_event_observation_{this};
};

}  // namespace ash

#endif  // ASH_METRICS_POST_LOGIN_METRICS_RECORDER_H_
