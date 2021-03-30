// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/chrome_jni_headers/ChromePowerModeVoter_jni.h"

#include <memory>

#include "base/no_destructor.h"
#include "components/power_scheduler/power_mode.h"
#include "components/power_scheduler/power_mode_arbiter.h"
#include "components/power_scheduler/power_mode_voter.h"

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
  static base::NoDestructor<std::unique_ptr<power_scheduler::PowerModeVoter>>
      voter(power_scheduler::PowerModeArbiter::GetInstance()->NewVoter(
          "PowerModeVoter.Animation.Java"));
  (*voter)->VoteFor(power_scheduler::PowerMode::kAnimation);
  (*voter)->ResetVoteAfterTimeout(
      power_scheduler::PowerModeVoter::kAnimationTimeout);
}

static void JNI_ChromePowerModeVoter_OnCoordinatorTouchEvent(JNIEnv* env) {
  static base::NoDestructor<std::unique_ptr<power_scheduler::PowerModeVoter>>
      voter(power_scheduler::PowerModeArbiter::GetInstance()->NewVoter(
          "PowerModeVoter.Input.Java"));
  (*voter)->VoteFor(power_scheduler::PowerMode::kResponse);
  (*voter)->ResetVoteAfterTimeout(
      power_scheduler::PowerModeVoter::kResponseTimeout);
}
