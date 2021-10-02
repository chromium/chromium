// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/chrome_jni_headers/ChromePowerModeVoter_jni.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/power_scheduler/power_mode.h"
#include "components/power_scheduler/power_mode_arbiter.h"
#include "components/power_scheduler/power_mode_voter.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Only accessed on the Android UI thread and only after native initialization
// which means its well after C++ initialization order issues. We could make
// these static variables inside recordViewTreeDrawsUMA() to prevent them being
// global but it would add a thread safe access on a hot UI path so we avoid
// that here.
base::TimeTicks g_android_view_ondraw_emitted = base::TimeTicks();
int g_android_view_ondraw_count = 0;

void RecordOnViewTreeDrawUMA() {
  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // This is the first onDraw we've seen so start counting from now.
  if (g_android_view_ondraw_emitted.is_null()) {
    g_android_view_ondraw_count = 1;
    g_android_view_ondraw_emitted = base::TimeTicks::Now();
    return;
  }
  base::TimeTicks curr = base::TimeTicks::Now();
  if (curr - g_android_view_ondraw_emitted > base::Seconds(30)) {
    // a 144hz monitor refreshes 120 times a second so 3600 frames in 30
    // seconds. So 10000 should be more than enough for now (famous last words).
    UMA_HISTOGRAM_COUNTS_10000("Android.View.onDraw.30Seconds",
                               g_android_view_ondraw_count);
    g_android_view_ondraw_emitted = curr;
    g_android_view_ondraw_count = 1;
    return;
  }
  ++g_android_view_ondraw_count;
}
}  // namespace

static void JNI_ChromePowerModeVoter_OnChromeActivityStateChange(
    JNIEnv* env,
    jboolean active) {
  static base::NoDestructor<std::unique_ptr<power_scheduler::PowerModeVoter>>
      voter(power_scheduler::PowerModeArbiter::GetInstance()->NewVoter(
          "PowerModeVoter.NonWebActivity"));
  (*voter)->VoteFor(active ? power_scheduler::PowerMode::kIdle
                           : power_scheduler::PowerMode::kNonWebActivity);
}

static void JNI_ChromePowerModeVoter_OnViewTreeDraw(JNIEnv* env) {
  static base::NoDestructor<power_scheduler::DebouncedPowerModeVoter> voter(
      "PowerModeVoter.Animation.Java",
      power_scheduler::PowerModeVoter::kAnimationTimeout);
  RecordOnViewTreeDrawUMA();
  voter->VoteFor(power_scheduler::PowerMode::kAnimation);
  voter->ResetVoteAfterTimeout();
}

static void JNI_ChromePowerModeVoter_OnCoordinatorTouchEvent(JNIEnv* env) {
  static base::NoDestructor<std::unique_ptr<power_scheduler::PowerModeVoter>>
      voter(power_scheduler::PowerModeArbiter::GetInstance()->NewVoter(
          "PowerModeVoter.Input.Java"));
  (*voter)->VoteFor(power_scheduler::PowerMode::kResponse);
  (*voter)->ResetVoteAfterTimeout(
      power_scheduler::PowerModeVoter::kResponseTimeout);
}
