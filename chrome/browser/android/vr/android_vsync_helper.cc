// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/android_vsync_helper.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "chrome/android/features/vr/jni_headers/AndroidVSyncHelper_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace vr {

AndroidVSyncHelper::AndroidVSyncHelper(Callback callback)
    : callback_(std::move(callback)) {
  JNIEnv* env = AttachCurrentThread();
  j_object_.Reset(
      Java_AndroidVSyncHelper_create(env, reinterpret_cast<jlong>(this)));
  float refresh_rate = Java_AndroidVSyncHelper_getRefreshRate(env, j_object_);
  display_vsync_interval_ = base::TimeDelta::FromSecondsD(1.0 / refresh_rate);
  DVLOG(1) << "display_vsync_interval_=" << display_vsync_interval_;
}

AndroidVSyncHelper::~AndroidVSyncHelper() {
  CancelVSyncRequest();
}

void AndroidVSyncHelper::OnVSync(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 jlong time_nanos) {
  // See WindowAndroid::OnVSync.
  DCHECK(vsync_requested_);
  vsync_requested_ = false;
  DCHECK_EQ(base::TimeTicks::GetClock(),
            base::TimeTicks::Clock::LINUX_CLOCK_MONOTONIC);
  base::TimeTicks frame_time =
      base::TimeTicks() + base::TimeDelta::FromNanoseconds(time_nanos);
  last_interval_ = frame_time - last_vsync_;
  last_vsync_ = frame_time;
  callback_.Run(frame_time);
}

void AndroidVSyncHelper::RequestVSync() {
  DCHECK(!vsync_requested_);
  vsync_requested_ = true;
  JNIEnv* env = AttachCurrentThread();
  Java_AndroidVSyncHelper_requestVSync(env, j_object_);
}

void AndroidVSyncHelper::CancelVSyncRequest() {
  if (!vsync_requested_)
    return;
  vsync_requested_ = false;
  JNIEnv* env = AttachCurrentThread();
  Java_AndroidVSyncHelper_cancelVSyncRequest(env, j_object_);
}

}  // namespace vr
