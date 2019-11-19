// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/android_ui_gesture_target.h"

#include <cmath>

#include "chrome/android/features/vr/jni_headers/AndroidUiGestureTarget_jni.h"
#include "chrome/browser/vr/input_event.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using content::MotionEventAction;

static constexpr int kFrameDurationMs = 16;
static constexpr int kScrollEventsPerFrame = 2;

namespace vr {

AndroidUiGestureTarget::AndroidUiGestureTarget(JNIEnv* env,
                                               const JavaParamRef<jobject>& obj,
                                               float scale_factor,
                                               float scroll_ratio,
                                               int touch_slop)
    : scale_factor_(scale_factor),
      scroll_ratio_(scroll_ratio),
      touch_slop_(touch_slop),
      java_ref_(env, obj) {}

AndroidUiGestureTarget::~AndroidUiGestureTarget() = default;

void AndroidUiGestureTarget::DispatchInputEvent(
    std::unique_ptr<InputEvent> event) {
  int64_t event_time_ms = event->time_stamp().since_origin().InMilliseconds();
  switch (event->type()) {
    case InputEvent::kScrollBegin: {
      SetPointer(event->position_in_widget());
      Inject(content::MOTION_EVENT_ACTION_START, event_time_ms);

      float xdiff = event->scroll_data.delta_x;
      float ydiff = event->scroll_data.delta_y;

      if (xdiff == 0 && ydiff == 0)
        ydiff = touch_slop_;
      double dist = std::sqrt((xdiff * xdiff) + (ydiff * ydiff));
      if (dist < touch_slop_) {
        xdiff *= touch_slop_ / dist;
        ydiff *= touch_slop_ / dist;
      }

      float xtarget = xdiff * scroll_ratio_ + event->position_in_widget().x();
      float ytarget = ydiff * scroll_ratio_ + event->position_in_widget().y();
      scroll_x_ = xtarget > 0 ? std::ceil(xtarget) : std::floor(xtarget);
      scroll_y_ = ytarget > 0 ? std::ceil(ytarget) : std::floor(ytarget);

      SetPointer(scroll_x_, scroll_y_);
      // Send a move immediately so that we can't accidentally trigger a click.
      Inject(content::MOTION_EVENT_ACTION_MOVE, event_time_ms);
      break;
    }
    case InputEvent::kScrollEnd:
      SetPointer(scroll_x_, scroll_y_);
      Inject(content::MOTION_EVENT_ACTION_END, event_time_ms);
      break;
    case InputEvent::kScrollUpdate: {
      float scale = scroll_ratio_ / kScrollEventsPerFrame;
      scroll_x_ += event->scroll_data.delta_x * scale;
      scroll_y_ += event->scroll_data.delta_y * scale;

      SetPointer(scroll_x_, scroll_y_);
      Inject(content::MOTION_EVENT_ACTION_MOVE, event_time_ms);

      scroll_x_ += event->scroll_data.delta_x * scale;
      scroll_y_ += event->scroll_data.delta_y * scale;
      SetDelayedEvent(scroll_x_, scroll_y_, content::MOTION_EVENT_ACTION_MOVE,
                      event_time_ms, kFrameDurationMs / kScrollEventsPerFrame);

      break;
    }
    case InputEvent::kFlingCancel:
      Inject(content::MOTION_EVENT_ACTION_START, event_time_ms);
      Inject(content::MOTION_EVENT_ACTION_CANCEL, event_time_ms);
      break;
    case InputEvent::kHoverEnter:
      SetPointer(event->position_in_widget());
      Inject(content::MOTION_EVENT_ACTION_HOVER_ENTER, event_time_ms);
      break;
    case InputEvent::kHoverLeave:
    case InputEvent::kHoverMove:
      // The platform ignores HOVER_EXIT, so we instead send a fixed
      // out-of-bounds point (http://crbug.com/715114).
      SetPointer(event->position_in_widget());
      Inject(content::MOTION_EVENT_ACTION_HOVER_MOVE, event_time_ms);
      break;
    case InputEvent::kButtonDown:
      SetPointer(event->position_in_widget());
      Inject(content::MOTION_EVENT_ACTION_START, event_time_ms);
      break;
    case InputEvent::kButtonUp:
      SetPointer(event->position_in_widget());
      Inject(content::MOTION_EVENT_ACTION_END, event_time_ms);
      break;
    case InputEvent::kMove:
      SetPointer(event->position_in_widget());
      Inject(content::MOTION_EVENT_ACTION_MOVE, event_time_ms);
      break;
    default:
      NOTREACHED() << "Unsupported event type sent to Android UI.";
      break;
  }
}

void AndroidUiGestureTarget::Inject(MotionEventAction action, int64_t time_ms) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AndroidUiGestureTarget_inject(env, obj, action, time_ms);
}

void AndroidUiGestureTarget::SetPointer(const gfx::PointF& position) {
  SetPointer(position.x(), position.y());
}

void AndroidUiGestureTarget::SetPointer(float x, float y) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AndroidUiGestureTarget_setPointer(env, obj,
                                         static_cast<int>(x * scale_factor_),
                                         static_cast<int>(y * scale_factor_));
}

void AndroidUiGestureTarget::SetDelayedEvent(float x,
                                             float y,
                                             MotionEventAction action,
                                             int64_t time_ms,
                                             int delay_ms) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AndroidUiGestureTarget_setDelayedEvent(
      env, obj, static_cast<int>(x * scale_factor_),
      static_cast<int>(y * scale_factor_), action, time_ms, delay_ms);
}

// static
AndroidUiGestureTarget* AndroidUiGestureTarget::FromJavaObject(
    const JavaRef<jobject>& obj) {
  if (obj.is_null())
    return nullptr;

  JNIEnv* env = base::android::AttachCurrentThread();
  return reinterpret_cast<AndroidUiGestureTarget*>(
      Java_AndroidUiGestureTarget_getNativeObject(env, obj));
}

static jlong JNI_AndroidUiGestureTarget_Init(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj,
                                             jfloat scale_factor,
                                             jfloat scroll_ratio,
                                             jint touch_slop) {
  return reinterpret_cast<intptr_t>(new AndroidUiGestureTarget(
      env, obj, scale_factor, scroll_ratio, touch_slop));
}

}  // namespace vr
