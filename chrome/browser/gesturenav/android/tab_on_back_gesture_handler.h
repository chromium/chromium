// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GESTURENAV_ANDROID_TAB_ON_BACK_GESTURE_HANDLER_H_
#define CHROME_BROWSER_GESTURENAV_ANDROID_TAB_ON_BACK_GESTURE_HANDLER_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/tab_android.h"
#include "ui/events/back_gesture_event.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace gesturenav {

// A handler to trigger seamless navigation / predictive back GESTURE.
class TabOnBackGestureHandler {
 public:
  explicit TabOnBackGestureHandler(TabAndroid* tab_android);

  TabOnBackGestureHandler(const TabOnBackGestureHandler&) = delete;
  TabOnBackGestureHandler& operator=(const TabOnBackGestureHandler&) = delete;

  ~TabOnBackGestureHandler() = default;

  // forward: true if this gesture is supposed to forward a page, instead of
  // navigating back.
  void OnBackStarted(JNIEnv* env,
                     float progress,
                     /* ui::BackGestureEventSwipeEdge */ int edge,
                     bool forward);
  void OnBackProgressed(JNIEnv* env,
                        float progress,
                        /* ui::BackGestureEventSwipeEdge */ int edge);
  void OnBackCancelled(JNIEnv* env);
  void OnBackInvoked(JNIEnv* env);
  void Destroy(JNIEnv* env);

 private:
  const raw_ptr<TabAndroid> tab_android_;
  bool is_in_progress_ = false;
  ui::BackGestureEventSwipeEdge started_edge_ =
      ui::BackGestureEventSwipeEdge::LEFT;
};
}  // namespace gesturenav

#endif  // CHROME_BROWSER_GESTURENAV_ANDROID_TAB_ON_BACK_GESTURE_HANDLER_H_
