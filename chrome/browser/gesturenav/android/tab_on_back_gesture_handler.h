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
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/tab_android.h"
#include "ui/events/back_gesture_event.h"

using base::android::JavaRef;

namespace content {
class WebContents;
}  // namespace content

namespace gesturenav {

// A handler to trigger seamless navigation / predictive back GESTURE.
class TabOnBackGestureHandler {
 public:
  explicit TabOnBackGestureHandler(TabAndroid* tab_android);

  TabOnBackGestureHandler(const TabOnBackGestureHandler&) = delete;
  TabOnBackGestureHandler& operator=(const TabOnBackGestureHandler&) = delete;

  ~TabOnBackGestureHandler();

  // forward: true if this gesture is supposed to forward a page, instead of
  // navigating back.
  void OnBackStarted(JNIEnv* env,
                     float progress,
                     /* ui::BackGestureEventSwipeEdge */ int edge,
                     bool forward,
                     bool is_gesture_mode);
  // Returns whether this handler is still driving the caller's gesture. When
  // it returns false the caller owns the gesture again: it must drop its
  // reference to this handler and do its own back handling, otherwise the
  // navigation is lost. The handler is not necessarily idle then: on an edge
  // mismatch it may still be driving a newer gesture for another owner.
  bool OnBackProgressed(JNIEnv* env,
                        float progress,
                        /* ui::BackGestureEventSwipeEdge */ int edge,
                        bool forward,
                        bool is_gesture_mode);
  void OnBackCancelled(JNIEnv* env, bool is_gesture_mode);
  void OnBackInvoked(JNIEnv* env, bool is_gesture_mode);
  void Destroy(JNIEnv* env);

 private:
  const raw_ptr<TabAndroid> tab_android_;
  bool is_in_progress_ = false;
  bool is_gesture_mode_ = false;
  ui::BackGestureEventSwipeEdge started_edge_ =
      ui::BackGestureEventSwipeEdge::LEFT;

  // Tracks the WebContents that started the current navigation gesture.
  // Used to prevent forwarding progress/cancel/invoke events to a newly
  // swapped WebContents that never received OnGestureStarted().
  base::WeakPtr<content::WebContents> gestured_web_contents_;
};
}  // namespace gesturenav

#endif  // CHROME_BROWSER_GESTURENAV_ANDROID_TAB_ON_BACK_GESTURE_HANDLER_H_
