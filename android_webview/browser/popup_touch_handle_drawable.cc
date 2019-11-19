// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/popup_touch_handle_drawable.h"

#include "android_webview/browser_jni_headers/PopupTouchHandleDrawable_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace android_webview {

PopupTouchHandleDrawable::PopupTouchHandleDrawable(
    JNIEnv* env,
    jobject obj,
    float horizontal_padding_ratio)
    : java_ref_(env, obj),
      drawable_horizontal_padding_ratio_(horizontal_padding_ratio) {
  DCHECK(obj);
}

PopupTouchHandleDrawable::~PopupTouchHandleDrawable() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_PopupTouchHandleDrawable_destroy(env, obj);
}

void PopupTouchHandleDrawable::SetEnabled(bool enabled) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  if (enabled)
    Java_PopupTouchHandleDrawable_show(env, obj);
  else
    Java_PopupTouchHandleDrawable_hide(env, obj);
}

void PopupTouchHandleDrawable::SetOrientation(
    ui::TouchHandleOrientation orientation,
    bool mirror_vertical,
    bool mirror_horizontal) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null()) {
    Java_PopupTouchHandleDrawable_setOrientation(
        env, obj, static_cast<int>(orientation), mirror_vertical,
        mirror_horizontal);
  }
}

void PopupTouchHandleDrawable::SetOrigin(const gfx::PointF& origin) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null()) {
    Java_PopupTouchHandleDrawable_setOrigin(env, obj, origin.x(), origin.y());
  }
}

void PopupTouchHandleDrawable::SetAlpha(float alpha) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  bool visible = alpha > 0;
  if (!obj.is_null())
    Java_PopupTouchHandleDrawable_setVisible(env, obj, visible);
}

gfx::RectF PopupTouchHandleDrawable::GetVisibleBounds() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return gfx::RectF();
  return gfx::RectF(
      Java_PopupTouchHandleDrawable_getOriginXDip(env, obj),
      Java_PopupTouchHandleDrawable_getOriginYDip(env, obj),
      Java_PopupTouchHandleDrawable_getVisibleWidthDip(env, obj),
      Java_PopupTouchHandleDrawable_getVisibleHeightDip(env, obj));
}

float PopupTouchHandleDrawable::GetDrawableHorizontalPaddingRatio() const {
  return drawable_horizontal_padding_ratio_;
}

static jlong JNI_PopupTouchHandleDrawable_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const jfloat horizontal_padding_ratio) {
  return reinterpret_cast<intptr_t>(
      new PopupTouchHandleDrawable(env, obj, horizontal_padding_ratio));
}

}  // namespace android_webview
