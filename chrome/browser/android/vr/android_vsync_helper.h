// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ANDROID_VSYNC_HELPER_H_
#define CHROME_BROWSER_ANDROID_VR_ANDROID_VSYNC_HELPER_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/callback.h"
#include "base/time/time.h"

namespace vr {

class AndroidVSyncHelper {
 public:
  using Callback = base::RepeatingCallback<void(base::TimeTicks)>;
  explicit AndroidVSyncHelper(Callback callback);

  AndroidVSyncHelper(const AndroidVSyncHelper&) = delete;
  AndroidVSyncHelper& operator=(const AndroidVSyncHelper&) = delete;

  ~AndroidVSyncHelper();
  void OnVSync(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               jlong time_nanos);
  void RequestVSync();
  void CancelVSyncRequest();

  // The last interval will be a multiple of the actual refresh interval, use
  // with care.
  base::TimeDelta LastVSyncInterval() { return last_interval_; }

  // Nominal display VSync interval from Java Display.getRefreshRate()
  base::TimeDelta DisplayVSyncInterval() { return display_vsync_interval_; }

 private:
  base::TimeTicks last_vsync_;
  base::TimeDelta last_interval_;
  base::TimeDelta display_vsync_interval_;
  Callback callback_;
  bool vsync_requested_ = false;

  base::android::ScopedJavaGlobalRef<jobject> j_object_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_ANDROID_VSYNC_HELPER_H_
