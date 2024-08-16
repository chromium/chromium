// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_POST_LOGIN_EVENT_OBSERVER_H_
#define ASH_METRICS_POST_LOGIN_EVENT_OBSERVER_H_

#include "ash/ash_export.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "cc/metrics/frame_sequence_metrics.h"

// This observer interface defines the set of post login events that we are
// currently interested in.
class ASH_EXPORT PostLoginEventObserver : public base::CheckedObserver {
 public:
  virtual void OnAuthSuccess(base::TimeTicks ts) {}
  virtual void OnUserLoggedIn(base::TimeTicks ts,
                              bool is_ash_restarted,
                              bool is_regular_user_or_owner) {}
  virtual void OnAllExpectedShelfIconLoaded(base::TimeTicks ts) {}
  virtual void OnSessionRestoreDataLoaded(base::TimeTicks ts,
                                          bool restore_automatically) {}
  // NOTE: This will only be triggered if `restore_automatically` is true.
  virtual void OnAllBrowserWindowsCreated(base::TimeTicks ts) {}
  // NOTE: This will only be triggered if `restore_automatically` is true.
  virtual void OnAllBrowserWindowsShown(base::TimeTicks ts) {}
  // NOTE: This will only be triggered if `restore_automatically` is true.
  virtual void OnAllBrowserWindowsPresented(base::TimeTicks ts) {}
  virtual void OnShelfAnimationFinished(base::TimeTicks ts) {}
  virtual void OnCompositorAnimationFinished(
      base::TimeTicks ts,
      const cc::FrameSequenceMetrics::CustomReportData& data) {}
  virtual void OnArcUiReady(base::TimeTicks ts) {}

  // Helper event. Triggered when both of `OnAllExpectedShelfIconLoaded` and
  // `OnAllBrowserWindowsPresented` are done.
  virtual void OnShelfIconsLoadedAndSessionRestoreDone(base::TimeTicks ts) {}

  // Helper event. Triggered when both of `OnShelfAnimationFinished` and
  // `OnCompositorAnimationFinished` are done.
  virtual void OnShelfAnimationAndCompositorAnimationDone(base::TimeTicks ts) {}
};

#endif  // ASH_METRICS_POST_LOGIN_EVENT_OBSERVER_H_
