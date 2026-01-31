// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/overscroll_scene_layer.h"

#include "ui/android/resources/resource_manager_impl.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/OverscrollSceneLayer_jni.h"

using base::android::JavaRef;

namespace android {

OverscrollSceneLayer::OverscrollSceneLayer(JNIEnv* env,
                                           const JavaRef<jobject>& jobj,
                                           const JavaRef<jobject>& jwindow)
    : SceneLayer(env, jobj),
      window_(ui::WindowAndroid::FromJavaWindowAndroid(jwindow)),
      glow_effect_(std::make_unique<ui::OverscrollGlow>(this)) {
  window_->AddObserver(this);
}

OverscrollSceneLayer::~OverscrollSceneLayer() {}

std::unique_ptr<ui::EdgeEffect> OverscrollSceneLayer::CreateEdgeEffect() {
  return std::make_unique<ui::EdgeEffect>(resource_manager_);
}

void OverscrollSceneLayer::Prepare(JNIEnv* env,
                                   float start_x,
                                   float start_y,
                                   int32_t width,
                                   int32_t height) {
  start_pos_ = gfx::Vector2dF(start_x, start_y);
  const gfx::SizeF viewport_size(width, height);

  if (!glow_effect_) {
    return;
  }

  // |OverscrollGlow| activates glow effect only when content is bigger than
  // viewport. Make it bigger by 1.f.
  const gfx::SizeF content_size(width + 1.f, height);
  const gfx::PointF content_scroll_offset(1, 0);
  glow_effect_->OnFrameUpdated(viewport_size, content_size,
                               content_scroll_offset);
}

bool OverscrollSceneLayer::Update(JNIEnv* env,
                                  const JavaRef<jobject>& jresource_manager,
                                  float accumulated_overscroll_x,
                                  float delta_x) {
  if (!resource_manager_) {
    if (jresource_manager.is_null()) {
      return false;
    }
    resource_manager_ =
        ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  }
  gfx::Vector2dF accumulated_overscroll(accumulated_overscroll_x, 0);
  gfx::Vector2dF overscroll_delta(delta_x, 0);
  if (glow_effect_ && glow_effect_->OnOverscrolled(
                          base::TimeTicks::Now(), accumulated_overscroll,
                          overscroll_delta, gfx::Vector2dF(0, 0), start_pos_)) {
    window_->SetNeedsAnimate();
    return true;
  }
  return false;
}

void OverscrollSceneLayer::OnAnimate(base::TimeTicks frame_time) {
  if (glow_effect_ && glow_effect_->Animate(frame_time, layer().get())) {
    window_->SetNeedsAnimate();
  }
}

void OverscrollSceneLayer::OnAttachCompositor() {
  window_->AddObserver(this);
}

void OverscrollSceneLayer::OnDetachCompositor() {
  window_->RemoveObserver(this);
}

void OverscrollSceneLayer::SetContentTree(
    JNIEnv* env,
    const JavaRef<jobject>& jcontent_tree) {
  SceneLayer* content_tree = FromJavaObject(env, jcontent_tree);
  if (!content_tree || !content_tree->layer()) {
    return;
  }

  if (!content_tree->layer()->parent() ||
      (content_tree->layer()->parent()->id() != layer()->id())) {
    layer()->AddChild(content_tree->layer());
  }
}

void OverscrollSceneLayer::OnReset(JNIEnv* env) {
  glow_effect_->Reset();
}

static int64_t JNI_OverscrollSceneLayer_Init(JNIEnv* env,
                                             const JavaRef<jobject>& jobj,
                                             const JavaRef<jobject>& jwindow) {
  // This will automatically bind to the Java object and pass ownership there.
  OverscrollSceneLayer* tree_provider =
      new OverscrollSceneLayer(env, jobj, jwindow);
  return reinterpret_cast<intptr_t>(tree_provider);
}

}  // namespace android

DEFINE_JNI(OverscrollSceneLayer)
