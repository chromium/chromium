// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gesturenav/android/tab_on_back_gesture_handler.h"

#include "chrome/browser/gesturenav/android/jni_headers/TabOnBackGestureHandler_jni.h"
#include "content/public/browser/back_forward_transition_animation_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/gfx/geometry/point_f.h"

namespace gesturenav {

namespace {

using NavType = content::BackForwardTransitionAnimationManager::NavigationType;

void AssertHasWindowAndCompositor(content::WebContents* web_contents) {
  CHECK(web_contents);
  auto* window = web_contents->GetNativeView()->GetWindowAndroid();
  CHECK(window);
  CHECK(window->GetCompositor());
}

}  // namespace

TabOnBackGestureHandler::TabOnBackGestureHandler(TabAndroid* tab_android)
    : tab_android_(tab_android) {}

void TabOnBackGestureHandler::OnBackStarted(JNIEnv* env,
                                            float x,
                                            float y,
                                            float progress,
                                            int edge,
                                            bool forward) {
  CHECK(!is_in_progress_, base::NotFatalUntil::M123);
  is_in_progress_ = true;

  content::WebContents* web_contents = tab_android_->web_contents();
  CHECK(web_contents, base::NotFatalUntil::M123);
  AssertHasWindowAndCompositor(web_contents);

  ui::BackGestureEvent back_gesture(gfx::PointF(x, y), progress);
  started_edge_ = static_cast<ui::BackGestureEventSwipeEdge>(edge);

  web_contents->GetBackForwardTransitionAnimationManager()->OnGestureStarted(
      back_gesture, static_cast<ui::BackGestureEventSwipeEdge>(edge),
      forward ? NavType::kForward : NavType::kBackward);
}

void TabOnBackGestureHandler::OnBackProgressed(JNIEnv* env,
                                               float x,
                                               float y,
                                               float progress,
                                               int edge) {
  CHECK(is_in_progress_, base::NotFatalUntil::M123);

  content::WebContents* web_contents = tab_android_->web_contents();
  AssertHasWindowAndCompositor(web_contents);

  CHECK_EQ(started_edge_, static_cast<ui::BackGestureEventSwipeEdge>(edge));

  if (progress > 1.f) {
    // TODO(https://crbug.com/1510932): Happens in fling. Should figure out why
    // before launch. Cap the progress at 1.f for now.
    LOG(ERROR) << "TabOnBackGestureHandler::OnBackProgressed " << progress;
    progress = 1.f;
  }
  ui::BackGestureEvent back_gesture(gfx::PointF(x, y), progress);
  web_contents->GetBackForwardTransitionAnimationManager()->OnGestureProgressed(
      back_gesture);
}

void TabOnBackGestureHandler::OnBackCancelled(JNIEnv* env) {
  CHECK(is_in_progress_, base::NotFatalUntil::M123);
  is_in_progress_ = false;

  content::WebContents* web_contents = tab_android_->web_contents();
  AssertHasWindowAndCompositor(web_contents);

  web_contents->GetBackForwardTransitionAnimationManager()
      ->OnGestureCancelled();
}

void TabOnBackGestureHandler::OnBackInvoked(JNIEnv* env) {
  CHECK(is_in_progress_, base::NotFatalUntil::M123);
  is_in_progress_ = false;

  content::WebContents* web_contents = tab_android_->web_contents();
  AssertHasWindowAndCompositor(web_contents);

  web_contents->GetBackForwardTransitionAnimationManager()->OnGestureInvoked();
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
