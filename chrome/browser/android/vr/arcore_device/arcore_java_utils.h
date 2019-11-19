// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_JAVA_UTILS_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_JAVA_UTILS_H_

#include <android/native_window_jni.h>
#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/vr/arcore_device/arcore_session_utils.h"

namespace vr {

class ArCoreJavaUtils : public ArCoreSessionUtils {
 public:
  ArCoreJavaUtils();
  ~ArCoreJavaUtils() override;

  // ArCoreSessionUtils:
  void RequestArSession(int render_process_id,
                        int render_frame_id,
                        bool use_overlay,
                        SurfaceReadyCallback ready_callback,
                        SurfaceTouchCallback touch_callback,
                        SurfaceDestroyedCallback destroyed_callback) override;
  void EndSession() override;
  bool EnsureLoaded() override;
  base::android::ScopedJavaLocalRef<jobject> GetApplicationContext() override;

  // Methods called from the Java side.
  void OnDrawingSurfaceReady(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& surface,
      int rotation,
      int width,
      int height);
  void OnDrawingSurfaceTouch(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             bool touching,
                             float x,
                             float y);
  void OnDrawingSurfaceDestroyed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

 private:
  base::android::ScopedJavaLocalRef<jobject> getTabFromRenderer(
      int render_process_id,
      int render_frame_id);

  base::android::ScopedJavaGlobalRef<jobject> j_arcore_java_utils_;

  SurfaceReadyCallback surface_ready_callback_;
  SurfaceTouchCallback surface_touch_callback_;
  SurfaceDestroyedCallback surface_destroyed_callback_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_JAVA_UTILS_H_
