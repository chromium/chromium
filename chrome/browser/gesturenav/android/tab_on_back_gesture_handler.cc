// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gesturenav/android/tab_on_back_gesture_handler.h"

#include <iomanip>

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "content/public/browser/back_forward_transition_animation_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/gfx/geometry/point_f.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/gesturenav/android/jni_headers/TabOnBackGestureHandler_jni.h"

namespace gesturenav {

namespace {

using NavDirection =
    content::BackForwardTransitionAnimationManager::NavigationDirection;

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
                                            float progress,
                                            int edge,
                                            bool forward,
                                            bool is_gesture_mode) {
  is_gesture_mode_ = is_gesture_mode;
  SCOPED_CRASH_KEY_BOOL("OnBackStarted", "gesture mode", is_gesture_mode);
  if (is_in_progress_) {
    OnBackCancelled(env, is_gesture_mode);
    CHECK(!is_in_progress_);
  }

  is_in_progress_ = true;
  content::WebContents* web_contents = tab_android_->web_contents();
  CHECK(web_contents);
  AssertHasWindowAndCompositor(web_contents);

  ui::BackGestureEvent back_gesture(progress);
  started_edge_ = static_cast<ui::BackGestureEventSwipeEdge>(edge);

  web_contents->GetBackForwardTransitionAnimationManager()->OnGestureStarted(
      back_gesture, static_cast<ui::BackGestureEventSwipeEdge>(edge),
      forward ? NavDirection::kForward : NavDirection::kBackward);
}

void TabOnBackGestureHandler::OnBackProgressed(JNIEnv* env,
                                               float progress,
                                               int edge,
                                               bool forward,
                                               bool is_gesture_mode) {
  SCOPED_CRASH_KEY_BOOL("OnBackProgressed", "gesture mode", is_gesture_mode);
  if (!is_in_progress_ ||
      started_edge_ != static_cast<ui::BackGestureEventSwipeEdge>(edge)) {
    if (is_in_progress_) {
      OnBackCancelled(env, is_gesture_mode);
    }

    CHECK(!is_in_progress_);
    OnBackStarted(env, progress, edge, forward, is_gesture_mode);
    return;
  }

  content::WebContents* web_contents = tab_android_->web_contents();
  AssertHasWindowAndCompositor(web_contents);

  // The OS can give us incorrect progress values.
  progress = std::clamp(progress, 0.f, 1.f);

  ui::BackGestureEvent back_gesture(progress);
  web_contents->GetBackForwardTransitionAnimationManager()->OnGestureProgressed(
      back_gesture);
}

void TabOnBackGestureHandler::OnBackCancelled(JNIEnv* env,
                                              bool is_gesture_mode) {
  SCOPED_CRASH_KEY_BOOL("OnBackCancelled", "gesture mode", is_gesture_mode);
  if (!is_in_progress_) {
    return;
  }

  is_in_progress_ = false;

  content::WebContents* web_contents = tab_android_->web_contents();
  AssertHasWindowAndCompositor(web_contents);

  web_contents->GetBackForwardTransitionAnimationManager()
      ->OnGestureCancelled();
}

void TabOnBackGestureHandler::OnBackInvoked(JNIEnv* env, bool is_gesture_mode) {
  SCOPED_CRASH_KEY_BOOL("OnBackInvoked", "gesture mode", is_gesture_mode);
  if (!is_in_progress_) {
    return;
  }

  is_in_progress_ = false;

  content::WebContents* web_contents = tab_android_->web_contents();
  AssertHasWindowAndCompositor(web_contents);

  web_contents->GetBackForwardTransitionAnimationManager()->OnGestureInvoked();
}

void TabOnBackGestureHandler::Destroy(JNIEnv* env) {
  using AnimationStage =
      content::BackForwardTransitionAnimationManager::AnimationStage;
  auto* web_contents = tab_android_->web_contents();
  if (is_in_progress_ && web_contents &&
      web_contents->GetBackForwardTransitionAnimationManager()
              ->GetCurrentAnimationStage() != AnimationStage::kNone) {
    // When the Java's Tab is destroyed, the compositor might already be
    // detached from the Window. No need to call `OnBackCancelled()` because the
    // animation is already aborted (thus `AnimationStage::kNone`).
    OnBackCancelled(env, is_gesture_mode_);
  }
  delete this;
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

// static
static jlong JNI_TabOnBackGestureHandler_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& jtab) {
  TabOnBackGestureHandler* handler =
      new TabOnBackGestureHandler(TabAndroid::GetNativeTab(env, jtab));
  return reinterpret_cast<intptr_t>(handler);
}

// static
static jboolean JNI_TabOnBackGestureHandler_ShouldAnimateNavigationTransition(
    JNIEnv* env,
    jboolean forward,
    jint edge) {
  return static_cast<jboolean>(
      content::BackForwardTransitionAnimationManager::
          ShouldAnimateNavigationTransition(
              static_cast<bool>(forward) ? NavDirection::kForward
                                         : NavDirection::kBackward,
              static_cast<ui::BackGestureEventSwipeEdge>(edge)));
}

}  // namespace gesturenav

DEFINE_JNI(TabOnBackGestureHandler)
