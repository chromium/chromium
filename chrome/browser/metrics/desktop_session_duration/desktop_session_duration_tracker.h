// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_DESKTOP_SESSION_DURATION_TRACKER_H_
#define CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_DESKTOP_SESSION_DURATION_TRACKER_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/metrics/desktop_session_duration/audible_contents_tracker.h"
#include "chrome/browser/metrics/desktop_session_duration/chrome_visibility_observer.h"

namespace metrics {

// Class for tracking and recording session length on desktop based on browser
// visibility, audio and user interaction.
class DesktopSessionDurationTracker : public AudibleContentsTracker::Observer {
 public:
  // The methods for the observer will be called on the UI thread.
  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnSessionStarted(base::TimeTicks session_start) {}
    virtual void OnSessionEnded(base::TimeDelta session_length,
                                base::TimeTicks session_end) {}
  };

  // Creates the |DesktopSessionDurationTracker| instance and initializes the
  // observers that notify to it.
  static void Initialize();

  // Returns true if the |DesktopSessionDurationTracker| instance has been
  // created.
  static bool IsInitialized();

  // Returns the |DesktopSessionDurationTracker| instance.
  static DesktopSessionDurationTracker* Get();

  DesktopSessionDurationTracker(const DesktopSessionDurationTracker&) = delete;
  DesktopSessionDurationTracker& operator=(
      const DesktopSessionDurationTracker&) = delete;

  // Called when user interaction with the browser is caught.
  void OnUserEvent();

  // Called when visibility of the browser changes. These events can be delayed
  // due to timeout logic, the extent of which can be communicated via
  // |time_ago|. This time is used to correct the session duration.
  void OnVisibilityChanged(bool visible, base::TimeDelta time_ago);

  bool is_visible() const { return is_visible_; }
  bool in_session() const { return in_session_; }
  bool is_audio_playing() const { return is_audio_playing_; }

  void SetInactivityTimeoutForTesting(base::TimeDelta inactivity_timeout) {
    inactivity_timeout_ = inactivity_timeout;
  }

  // For observing the status of the session tracker.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Cleans up any global state for testing.
  static void CleanupForTesting();

  void IncrementDefaultSearchCounter() { ++default_search_counter_; }

  void ResetDefaultSearchCounter() { default_search_counter_ = 0; }

 protected:
  DesktopSessionDurationTracker();
  ~DesktopSessionDurationTracker() override;

  // AudibleContentsTracker::Observer
  void OnAudioStart() override;
  void OnAudioEnd() override;

  // Decides whether session should be ended. Called when timer for inactivity
  // timeout was fired. Overridden by tests.
  virtual void OnTimerFired();

 private:
  // Starts timer based on |inactivity_timeout_|.
  void StartTimer(base::TimeDelta duration);

  // Marks the start of the session.
  void StartSession();

  // Ends the session and saves session information into histograms.
  // |time_to_discount| contains the amount of time that should be removed from
  // the apparent session length due to timeout logic.
  void EndSession(base::TimeDelta time_to_discount);

  // Sets |inactivity_timeout_| based on variation params.
  void InitInactivityTimeout();

  // Used for marking start if the session.
  base::TimeTicks session_start_;

  // Used for marking last user interaction.
  base::TimeTicks last_user_event_;

  // Used for marking current state of the user engagement.
  bool is_visible_ = false;
  bool in_session_ = false;
  bool is_audio_playing_ = false;
  bool is_first_session_ = true;

  // Used to keep the number of times we navigated to the default search engine.
  uint32_t default_search_counter_ = 0;

  // Timeout for waiting for user interaction.
  base::TimeDelta inactivity_timeout_;

  base::OneShotTimer timer_;

  base::ObserverList<Observer>::UncheckedAndDanglingUntriaged observer_list_;

  ChromeVisibilityObserver visibility_observer_;
  AudibleContentsTracker audio_tracker_;

  base::WeakPtrFactory<DesktopSessionDurationTracker> weak_factory_{this};
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_DESKTOP_SESSION_DURATION_TRACKER_H_
