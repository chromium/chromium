// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_NAVIGATION_GLOW_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_NAVIGATION_GLOW_H_

#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "ui/android/overscroll_glow.h"
#include "ui/android/view_android_observer.h"
#include "ui/android/window_android_observer.h"

namespace content {
class WebContents;
}

namespace ui {
class ViewAndroid;
class WindowAndroid;
}  // namespace ui

namespace android {

// Native part handling the edge glow effect in history navigation UI.
class NavigationGlow : public ui::OverscrollGlowClient,
                       public ui::WindowAndroidObserver,
                       public ui::ViewAndroidObserver {
 public:
  explicit NavigationGlow(float dip_scale, content::WebContents* web_contents);
  ~NavigationGlow() override;

  void Prepare(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               jfloat start_x,
               jfloat start_y,
               jint width,
               jint height);
  void OnOverscroll(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    jfloat accumulated_overscroll_x,
                    jfloat delta_x);
  void OnReset(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // ui::WindowAndroidObserver implementation.
  void OnCompositingDidCommit() override {}
  void OnRootWindowVisibilityChanged(bool visible) override {}
  void OnAttachCompositor() override {}
  void OnDetachCompositor() override {}
  void OnAnimate(base::TimeTicks begin_frame_time) override;
  void OnActivityStopped() override {}
  void OnActivityStarted() override {}
  void OnCursorVisibilityChanged(bool visible) override {}
  void OnFallbackCursorModeToggled(bool is_on) override {}

  // ui::ViewAndroidObserver implementation.
  void OnAttachedToWindow() override;
  void OnDetachedFromWindow() override;

 private:
  // OverscrollGlowClient implementation.
  std::unique_ptr<ui::EdgeEffectBase> CreateEdgeEffect() override;

  float dip_scale_;
  cc::Layer* layer_ = nullptr;
  ui::WindowAndroid* window_ = nullptr;
  ui::ViewAndroid* view_ = nullptr;
  std::unique_ptr<ui::OverscrollGlow> glow_effect_;
  gfx::Vector2dF start_pos_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_NAVIGATION_GLOW_H_
