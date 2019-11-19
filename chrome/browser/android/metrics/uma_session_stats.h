// Copyright 2015 The Chromium Authors. All rights reserved.
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
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/time/time.h"

// The native part of java UmaSessionStats class. This is a singleton.
class UmaSessionStats {
 public:
  static UmaSessionStats* GetInstance();

  void UmaResumeSession(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  void UmaEndSession(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);

  // Called before an UMA log is completed to record associated metrics.
  void ProvideCurrentSessionData();

  // Called once on browser startup.
  static void OnStartup();

  static void RegisterSyntheticFieldTrial(const std::string& trial_name,
                                          const std::string& group_name);

  static void RegisterSyntheticMultiGroupFieldTrial(
      const std::string& trial_name,
      const std::vector<uint32_t>& group_name_hashes);

 private:
  friend class base::NoDestructor<UmaSessionStats>;
  UmaSessionStats();
  ~UmaSessionStats();

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

  DISALLOW_COPY_AND_ASSIGN(UmaSessionStats);
};

#endif  // CHROME_BROWSER_ANDROID_METRICS_UMA_SESSION_STATS_H_
