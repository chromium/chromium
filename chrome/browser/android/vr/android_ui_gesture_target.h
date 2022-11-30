// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ANDROID_UI_GESTURE_TARGET_H_
#define CHROME_BROWSER_ANDROID_VR_ANDROID_UI_GESTURE_TARGET_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/browser/android/motion_event_action.h"

namespace gfx {
class PointF;
}

namespace vr {

class InputEvent;

// Used to forward events to MotionEventSynthesizer. Owned by VrShell.
class AndroidUiGestureTarget {
 public:
  AndroidUiGestureTarget(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         float scale_factor,
                         float scroll_ratio,
                         int touch_slop);

  AndroidUiGestureTarget(const AndroidUiGestureTarget&) = delete;
  AndroidUiGestureTarget& operator=(const AndroidUiGestureTarget&) = delete;

  ~AndroidUiGestureTarget();

  static AndroidUiGestureTarget* FromJavaObject(
      const base::android::JavaRef<jobject>& obj);

  void DispatchInputEvent(std::unique_ptr<InputEvent> event);

 private:
  void Inject(content::MotionEventAction action, int64_t time_ms);
  void SetPointer(const gfx::PointF& position);
  void SetPointer(float x, float y);
  void SetDelayedEvent(float x,
                       float y,
                       content::MotionEventAction action,
                       int64_t time_ms,
                       int delay_ms);

  float scroll_x_ = 0.0f;
  float scroll_y_ = 0.0f;
  float scale_factor_;
  float scroll_ratio_;
  int touch_slop_;

  JavaObjectWeakGlobalRef java_ref_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_ANDROID_UI_GESTURE_TARGET_H_
