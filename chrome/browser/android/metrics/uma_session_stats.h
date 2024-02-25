// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_METRICS_UMA_SESSION_STATS_H_
#define CHROME_BROWSER_ANDROID_METRICS_UMA_SESSION_STATS_H_

#include <jni.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "components/variations/synthetic_trials.h"

// The native part of java UmaSessionStats class. This is a singleton.
class UmaSessionStats {
 public:
  void UmaResumeSession(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  void UmaEndSession(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);

  // Called before an UMA log is completed to record associated metrics.
  void ProvideCurrentSessionData();

  static UmaSessionStats* GetInstance();

  UmaSessionStats(const UmaSessionStats&) = delete;
  UmaSessionStats& operator=(const UmaSessionStats&) = delete;

  // Returns true if there is a visible activity. Android Chrome only.
  static bool HasVisibleActivity();

  // Called once on browser startup.
  static void OnStartup();

  static void RegisterSyntheticFieldTrial(
      const std::string& trial_name,
      const std::string& group_name,
      variations::SyntheticTrialAnnotationMode annotation_mode);

  static bool IsBackgroundSessionStartForTesting();

  // Reads counters Chrome.UMA.OnPostCreateCounter2 and
  // Chrome.UMA.OnResumeCounter2 that are written to in ChromeActivity.java. The
  // counters are encoded in an enum histogram, emitted and reset to 0.
  static void EmitAndResetCounters();

 private:
  friend class base::NoDestructor<UmaSessionStats>;
  UmaSessionStats() = default;
  ~UmaSessionStats() = default;

  class SessionTimeTracker {
   public:
    SessionTimeTracker() = default;
    SessionTimeTracker(const SessionTimeTracker&) = delete;
    SessionTimeTracker& operator=(const SessionTimeTracker&) = delete;

    // Adds time to |background_session_start_time_| if a background session is
    // currently active.
    void AccumulateBackgroundSessionTime();
    // Reports accumulated background session time, if any exists.
    void ReportBackgroundSessionTime();
    // Ends any background session, and begins a new foreground session timer.
    // Returns whether a background session was terminated by this foreground
    // session.
    bool BeginForegroundSession();
    // Marks the end of a foreground session and returns its duration.
    base::TimeDelta EndForegroundSession();
    // Begins a new background session timer.
    void BeginBackgroundSession();

    base::TimeTicks session_start_time() const { return session_start_time_; }
    base::TimeTicks background_session_start_time() const {
      return background_session_start_time_;
    }

   private:
    // Start of the current session.
    base::TimeTicks session_start_time_;
    // Start of the current background session. Null if there is no active
    // background session.
    base::TimeTicks background_session_start_time_;
    // Total accumulated and unreported background session time.
    base::TimeDelta background_session_accumulated_time_;
  };

  SessionTimeTracker session_time_tracker_;
  int active_session_count_ = 0;
};

#endif  // CHROME_BROWSER_ANDROID_METRICS_UMA_SESSION_STATS_H_
