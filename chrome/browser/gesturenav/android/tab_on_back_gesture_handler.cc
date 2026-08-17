// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gesturenav/android/tab_on_back_gesture_handler.h"

#include "base/debug/crash_logging.h"
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

TabOnBackGestureHandler::~TabOnBackGestureHandler() = default;

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
      back_gesture, started_edge_,
      forward ? NavDirection::kForward : NavDirection::kBackward);
  gestured_web_contents_ = web_contents->GetWeakPtr();
}

bool TabOnBackGestureHandler::OnBackProgressed(JNIEnv* env,
                                               float progress,
                                               int edge,
                                               bool forward,
                                               bool is_gesture_mode) {
  SCOPED_CRASH_KEY_BOOL("OnBackProgressed", "gesture mode", is_gesture_mode);
  auto swipe_edge = static_cast<ui::BackGestureEventSwipeEdge>(edge);
  content::WebContents* web_contents = tab_android_->web_contents();
  if (!web_contents || !is_in_progress_ || started_edge_ != swipe_edge ||
      web_contents != gestured_web_contents_.get()) {
    // This event does not belong to the gesture we started, so give the gesture
    // back to the caller without mutating or cancelling an active gesture
    // belonging to a different owner or edge.
    //
    // This used to cancel and then restart the gesture from `edge` and
    // `forward` instead (crrev.com/c/5921004 for crbug.com/370105609,
    // crrev.com/c/5941357 for crbug.com/373617224). Restarting from here is not
    // safe: `forward` follows the swipe edge, and whoever sent this event never
    // checked it against session history, so the restart can ask
    // `BackForwardTransitionAnimationManager` to navigate in a direction that
    // has no destination entry. That is crbug.com/530682179.
    //
    // Simply dropping the event is not enough either: `OnBackCancelled()` and
    // `OnBackInvoked()` no-op while `is_in_progress_` is false, and the callers
    // stop doing their own back handling once they have a handler, so the
    // user's gesture would be silently swallowed. Report `false` instead;
    // the caller drops its reference to us and handles the gesture itself.
    return false;
  }

  CHECK(web_contents);

  // The OS can give us incorrect progress values.
  progress = std::clamp(progress, 0.f, 1.f);

  ui::BackGestureEvent back_gesture(progress);
  web_contents->GetBackForwardTransitionAnimationManager()->OnGestureProgressed(
      back_gesture);
  return true;
}

void TabOnBackGestureHandler::OnBackCancelled(JNIEnv* env,
                                              bool is_gesture_mode) {
  SCOPED_CRASH_KEY_BOOL("OnBackCancelled", "gesture mode", is_gesture_mode);
  if (!is_in_progress_) {
    return;
  }

  is_in_progress_ = false;
  auto* started_web_contents = gestured_web_contents_.get();
  gestured_web_contents_.reset();

  content::WebContents* web_contents = tab_android_->web_contents();
  if (!web_contents || web_contents != started_web_contents) {
    // The WebContents was swapped or destroyed mid-gesture. Do not forward
    // OnGestureCancelled() to a new WebContents that never received
    // OnGestureStarted(), otherwise its animation manager might hit
    // CHECK_NE(destination_entry_id_, kInvalidId). See crbug.com/530682179.
    return;
  }

  web_contents->GetBackForwardTransitionAnimationManager()
      ->OnGestureCancelled();
}

void TabOnBackGestureHandler::OnBackInvoked(JNIEnv* env, bool is_gesture_mode) {
  SCOPED_CRASH_KEY_BOOL("OnBackInvoked", "gesture mode", is_gesture_mode);
  if (!is_in_progress_) {
    return;
  }

  is_in_progress_ = false;
  auto* started_web_contents = gestured_web_contents_.get();
  gestured_web_contents_.reset();

  content::WebContents* web_contents = tab_android_->web_contents();
  if (!web_contents || web_contents != started_web_contents) {
    // The WebContents was swapped or destroyed mid-gesture. Do not forward
    // OnGestureInvoked() to a new WebContents that never received
    // OnGestureStarted(), otherwise its animation manager might hit
    // CHECK_NE(destination_entry_id_, kInvalidId). See crbug.com/530682179.
    return;
  }

  web_contents->GetBackForwardTransitionAnimationManager()->OnGestureInvoked();
}

void TabOnBackGestureHandler::Destroy(JNIEnv* env) {
  using AnimationStage =
      content::BackForwardTransitionAnimationManager::AnimationStage;
  auto* web_contents = tab_android_->web_contents();
  // Only cancel if `web_contents` matches `gestured_web_contents_`; otherwise
  // the gesture belonged to an older WebContents that has already been
  // detached.
  if (is_in_progress_ && web_contents &&
      web_contents == gestured_web_contents_.get() &&
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
static int64_t JNI_TabOnBackGestureHandler_Init(JNIEnv* env,
                                                const JavaRef<jobject>& jtab) {
  TabOnBackGestureHandler* handler =
      new TabOnBackGestureHandler(TabAndroid::GetNativeTab(env, jtab));
  return reinterpret_cast<intptr_t>(handler);
}

// static
static bool JNI_TabOnBackGestureHandler_ShouldAnimateNavigationTransition(
    JNIEnv* env,
    bool forward,
    int32_t edge) {
  return static_cast<bool>(
      content::BackForwardTransitionAnimationManager::
          ShouldAnimateNavigationTransition(
              forward ? NavDirection::kForward : NavDirection::kBackward,
              static_cast<ui::BackGestureEventSwipeEdge>(edge)));
}

}  // namespace gesturenav

DEFINE_JNI(TabOnBackGestureHandler)
