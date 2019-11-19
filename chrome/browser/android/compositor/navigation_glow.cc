// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/navigation_glow.h"

#include "base/android/build_info.h"
#include "chrome/android/chrome_jni_headers/CompositorNavigationGlow_jni.h"
#include "chrome/browser/android/compositor/scene_layer/scene_layer.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/edge_effect.h"
#include "ui/android/edge_effect_l.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_compositor.h"

using base::android::JavaParamRef;

namespace {

// Used for conditional creation of EdgeEffect types for the overscroll glow.
bool IsAndroidLOrNewer() {
  static bool android_l_or_newer =
      base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_LOLLIPOP;
  return android_l_or_newer;
}

}  // namespace

namespace android {

NavigationGlow::NavigationGlow(float dip_scale,
                               content::WebContents* web_contents)
    : dip_scale_(dip_scale),
      glow_effect_(std::make_unique<ui::OverscrollGlow>(this)) {
  DCHECK(web_contents);
  view_ = web_contents->GetNativeView();
  view_->AddObserver(this);
  layer_ = view_->GetLayer();
  OnAttachedToWindow();
}

NavigationGlow::~NavigationGlow() = default;

void NavigationGlow::OnAttachedToWindow() {
  window_ = view_->GetWindowAndroid();
  if (window_) {
    window_->AddObserver(this);
    if (!glow_effect_)
      glow_effect_ = std::make_unique<ui::OverscrollGlow>(this);
  }
}

void NavigationGlow::OnDetachedFromWindow() {
  if (window_) {
    window_->RemoveObserver(this);
    glow_effect_.reset();
  }
  window_ = nullptr;
}

void NavigationGlow::Prepare(JNIEnv* env,
                             const JavaParamRef<jobject>& obj,
                             jfloat start_x,
                             jfloat start_y,
                             jint width,
                             jint height) {
  start_pos_ = gfx::Vector2dF(start_x, start_y);
  const gfx::SizeF viewport_size(width, height);
  // |OverscrollGlow| activates glow effect only when content is bigger than
  // viewport. Make it bigger by 1.f.
  const gfx::SizeF content_size(width + 1.f, height);
  const gfx::Vector2dF content_scroll_offset(1, 0);
  glow_effect_->OnFrameUpdated(viewport_size, content_size,
                               content_scroll_offset);
}

void NavigationGlow::OnOverscroll(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  jfloat accumulated_overscroll_x,
                                  jfloat delta_x) {
  gfx::Vector2dF accumulated_overscroll(accumulated_overscroll_x, 0);
  gfx::Vector2dF overscroll_delta(delta_x, 0);
  if (glow_effect_->OnOverscrolled(base::TimeTicks::Now(),
                                   accumulated_overscroll, overscroll_delta,
                                   gfx::Vector2dF(0, 0), start_pos_)) {
    window_->SetNeedsAnimate();
  }
}

void NavigationGlow::OnReset(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  glow_effect_->Reset();
}

void NavigationGlow::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  OnDetachedFromWindow();
  view_->RemoveObserver(this);
  delete this;
}

void NavigationGlow::OnAnimate(base::TimeTicks frame_time) {
  if (glow_effect_->Animate(frame_time, layer_))
    window_->SetNeedsAnimate();
}

std::unique_ptr<ui::EdgeEffectBase> NavigationGlow::CreateEdgeEffect() {
  auto& resource_manager = window_->GetCompositor()->GetResourceManager();
  if (IsAndroidLOrNewer())
    return std::make_unique<ui::EdgeEffectL>(&resource_manager);

  return std::make_unique<ui::EdgeEffect>(&resource_manager, dip_scale_);
}

static jlong JNI_CompositorNavigationGlow_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const jfloat dip_scale,
    const JavaParamRef<jobject>& jweb_contents) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  return reinterpret_cast<intptr_t>(
      new NavigationGlow(dip_scale, web_contents));
}

}  // namespace android
