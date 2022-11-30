// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_APP_WINDOW_METRICS_TRACKER_H_
#define CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_APP_WINDOW_METRICS_TRACKER_H_

#include <map>

#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class TickClock;
}

namespace extensions {
class AppWindow;
}

namespace lock_screen_apps {

// Helper for tracking metrics for lock screen app window launches.
class AppWindowMetricsTracker : public content::WebContentsObserver {
 public:
  explicit AppWindowMetricsTracker(const base::TickClock* clock);

  AppWindowMetricsTracker(const AppWindowMetricsTracker&) = delete;
  AppWindowMetricsTracker& operator=(const AppWindowMetricsTracker&) = delete;

  ~AppWindowMetricsTracker() override;

  // Register app launch request.
  void AppLaunchRequested();

  // Registers the app window created for lock screen action - the class
  // will begin observing the app window state as a result.
  void AppWindowCreated(extensions::AppWindow* app_window);

  // Updates metrics state for app window being moved to foreground.
  void MovedToForeground();

  // Updates metrics state for app window being moved to background.
  void MovedToBackground();

  // Stops tracking current app window state, and resets collected timestamps.
  void Reset();

  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* frame_host) override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

 private:
  // NOTE: Used in histograms - do not change order, or remove entries.
  // Also, update LockScreenAppSessionState enum.
  enum class State {
    kInitial = 0,
    kLaunchRequested = 1,
    kWindowCreated = 2,
    kWindowShown = 3,
    kForeground = 4,
    kBackground = 5,
    kCount
  };

  void SetState(State state);

  const base::TickClock* clock_;

  State state_ = State::kInitial;

  // Maps states to their last occurrence time.
  std::map<State, base::TimeTicks> time_stamps_;

  // Number of times app launch was requested during the
  int app_launch_count_ = 0;

  // The state to which the metrics tracker should move after
  // the window contents is loaded.
  // Should be either kForeground or kBackground.
  absl::optional<State> state_after_window_contents_load_ = State::kForeground;
};

}  // namespace lock_screen_apps

#endif  // CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_APP_WINDOW_METRICS_TRACKER_H_
