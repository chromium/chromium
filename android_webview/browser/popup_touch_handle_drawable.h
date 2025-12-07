// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_POPUP_TOUCH_HANDLE_DRAWABLE_H_
#define ANDROID_WEBVIEW_BROWSER_POPUP_TOUCH_HANDLE_DRAWABLE_H_

#include "ui/touch_selection/touch_handle.h"

#include "base/android/jni_weak_ref.h"

namespace android_webview {

// Touch handle drawable backed by an Android PopupWindow.
class PopupTouchHandleDrawable : public ui::TouchHandleDrawable {
 public:
  PopupTouchHandleDrawable(JNIEnv* env,
                           const jni_zero::JavaRef<jobject>& obj,
                           float horizontal_padding_ratio);

  PopupTouchHandleDrawable(const PopupTouchHandleDrawable&) = delete;
  PopupTouchHandleDrawable& operator=(const PopupTouchHandleDrawable&) = delete;

  ~PopupTouchHandleDrawable() override;

  // ui::TouchHandleDrawable implementation.
  void SetEnabled(bool enabled) override;
  void SetOrientation(ui::TouchHandleOrientation orientation,
                      bool mirror_vertical,
                      bool mirror_horizontal) override;
  void SetOrigin(const gfx::PointF& origin) override;
  void SetAlpha(float alpha) override;
  gfx::RectF GetVisibleBounds() const override;
  float GetDrawableHorizontalPaddingRatio() const override;

 private:
  JavaObjectWeakGlobalRef java_ref_;

  const float drawable_horizontal_padding_ratio_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_POPUP_TOUCH_HANDLE_DRAWABLE_H_
