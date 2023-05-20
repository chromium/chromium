// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/static_tab_scene_layer.h"

#include <vector>

#include "cc/slim/filter.h"
#include "cc/slim/layer.h"
#include "chrome/android/chrome_jni_headers/StaticTabSceneLayer_jni.h"
#include "chrome/browser/android/compositor/layer/content_layer.h"
#include "chrome/browser/android/compositor/layer_title_cache.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/resources/resource_manager_impl.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace android {

StaticTabSceneLayer::StaticTabSceneLayer(JNIEnv* env,
                                         const JavaRef<jobject>& jobj)
    : SceneLayer(env, jobj),
      tab_content_manager_(nullptr),
      background_color_(SK_ColorWHITE) {}

StaticTabSceneLayer::~StaticTabSceneLayer() = default;

bool StaticTabSceneLayer::ShouldShowBackground() {
  scoped_refptr<cc::slim::Layer> root = layer_->RootLayer();
  return root && root->bounds() != layer_->bounds();
}

SkColor StaticTabSceneLayer::GetBackgroundColor() {
  return background_color_;
}

void StaticTabSceneLayer::UpdateTabLayer(JNIEnv* env,
                                         const JavaParamRef<jobject>& jobj,
                                         jint id,
                                         jboolean can_use_live_layer,
                                         jint default_background_color,
                                         jfloat x,
                                         jfloat y,
                                         jfloat static_to_view_blend,
                                         jfloat saturation) {
  DCHECK(tab_content_manager_)
      << "TabContentManager must be set before updating the layer";

  background_color_ = default_background_color;
  if (!content_layer_.get()) {
    content_layer_ = android::ContentLayer::Create(tab_content_manager_);
    layer_->AddChild(content_layer_->layer());
  }

  content_layer_->SetProperties(id, can_use_live_layer, static_to_view_blend,
                                false, 1.f, saturation, false, gfx::Rect());

  content_layer_->layer()->SetPosition(gfx::PointF(x, y));
  content_layer_->layer()->SetIsDrawable(true);
}

void StaticTabSceneLayer::SetTabContentManager(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    const base::android::JavaParamRef<jobject>& jtab_content_manager) {
  if (!tab_content_manager_) {
    tab_content_manager_ =
        TabContentManager::FromJavaObject(jtab_content_manager);
  }
}

static jlong JNI_StaticTabSceneLayer_Init(JNIEnv* env,
                                          const JavaParamRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  StaticTabSceneLayer* scene_layer = new StaticTabSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(scene_layer);
}

}  // namespace android
