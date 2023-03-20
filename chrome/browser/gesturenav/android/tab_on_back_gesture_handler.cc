// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gesturenav/android/tab_on_back_gesture_handler.h"

#include "chrome/browser/gesturenav/android/jni_headers/TabOnBackGestureHandler_jni.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/point_f.h"

namespace gesturenav {

TabOnBackGestureHandler::TabOnBackGestureHandler(TabAndroid* tab_android)
    : tab_android_(tab_android) {}

// TODO(crbug.com/1413521): Implement these methods to trigger visual
// transition for edge swipe.
void TabOnBackGestureHandler::OnBackStarted(JNIEnv* env,
                                            float x,
                                            float y,
                                            float progress,
                                            int edge,
                                            bool forward) {
  DCHECK(!is_in_progress_);
  is_in_progress_ = true;
  content::WebContents* web_contents = tab_android_->web_contents();
  DCHECK(web_contents);
  ui::BackGestureEvent backEvent(
      gfx::PointF(x, y), progress,
      static_cast<ui::BackGestureEventSwipeEdge>(edge));
}

void TabOnBackGestureHandler::OnBackProgressed(JNIEnv* env,
                                               float x,
                                               float y,
                                               float progress,
                                               int edge) {
  DCHECK(is_in_progress_);
  ui::BackGestureEvent backEvent(
      gfx::PointF(x, y), progress,
      static_cast<ui::BackGestureEventSwipeEdge>(edge));
}

void TabOnBackGestureHandler::OnBackCancelled(JNIEnv* env) {
  DCHECK(is_in_progress_);
  is_in_progress_ = false;
}

void TabOnBackGestureHandler::OnBackInvoked(JNIEnv* env) {
  DCHECK(is_in_progress_);
  is_in_progress_ = false;
}

void TabOnBackGestureHandler::Destroy(JNIEnv* env) {
  if (is_in_progress_) {
    OnBackCancelled(env);
    is_in_progress_ = false;
  }
  delete this;
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong JNI_TabOnBackGestureHandler_Init(JNIEnv* env,
                                       const JavaParamRef<jobject>& jtab) {
  TabOnBackGestureHandler* handler =
      new TabOnBackGestureHandler(TabAndroid::GetNativeTab(env, jtab));
  return reinterpret_cast<intptr_t>(handler);
}

}  // namespace gesturenav
