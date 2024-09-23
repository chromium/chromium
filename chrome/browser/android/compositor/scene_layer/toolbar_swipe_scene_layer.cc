// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/toolbar_swipe_scene_layer.h"

#include "chrome/browser/android/compositor/layer/content_layer.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ToolbarSwipeSceneLayer_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace android {

ToolbarSwipeSceneLayer::ToolbarSwipeSceneLayer(JNIEnv* env,
                                               const JavaRef<jobject>& jobj)
    : SceneLayer(env, jobj),
      left_content_layer_(nullptr),
      right_content_layer_(nullptr),
      tab_content_manager_(nullptr) {}

ToolbarSwipeSceneLayer::~ToolbarSwipeSceneLayer() {}

void ToolbarSwipeSceneLayer::UpdateLayer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    jint id,
    jboolean left_tab,
    jboolean can_use_live_layer,
    jint default_background_color,
    jfloat x,
    jfloat y) {
  background_color_ = default_background_color;
  ContentLayer* content_layer =
      left_tab ? left_content_layer_.get() : right_content_layer_.get();
  if (!content_layer)
    return;

  // Update layer visibility based on whether there is a valid tab ID.
  content_layer->layer()->SetHideLayerAndSubtree(id < 0);

  if (id < 0)
    return;

  content_layer->SetProperties(id, can_use_live_layer,
                               can_use_live_layer ? 0.0f : 1.0f, false, 1.0f,
                               1.0f, false, gfx::Rect());
  content_layer->layer()->SetPosition(gfx::PointF(x, y));
}

void ToolbarSwipeSceneLayer::SetTabContentManager(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    const base::android::JavaParamRef<jobject>& jtab_content_manager) {
  tab_content_manager_ =
      TabContentManager::FromJavaObject(jtab_content_manager);

  left_content_layer_ = ContentLayer::Create(tab_content_manager_);
  layer()->AddChild(left_content_layer_->layer());

  right_content_layer_ = ContentLayer::Create(tab_content_manager_);
  layer()->AddChild(right_content_layer_->layer());
}

bool ToolbarSwipeSceneLayer::ShouldShowBackground() {
  return true;
}

SkColor ToolbarSwipeSceneLayer::GetBackgroundColor() {
  return background_color_;
}

static jlong JNI_ToolbarSwipeSceneLayer_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  ToolbarSwipeSceneLayer* scene_layer = new ToolbarSwipeSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(scene_layer);
}

}  // namespace android
