// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/edge_to_edge_bottom_chin_scene_layer.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "cc/slim/layer.h"
#include "cc/slim/solid_color_layer.h"
#include "cc/slim/ui_resource_layer.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/android/edge_to_edge/jni_headers/EdgeToEdgeBottomChinSceneLayer_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace android {

EdgeToEdgeBottomChinSceneLayer::EdgeToEdgeBottomChinSceneLayer(
    JNIEnv* env,
    const JavaRef<jobject>& jobj)
    : SceneLayer(env, jobj),
      view_container_(cc::slim::Layer::Create()),
      view_layer_(cc::slim::SolidColorLayer::Create()),
      divider_layer_(cc::slim::SolidColorLayer::Create()),
      debug_layer_(cc::slim::SolidColorLayer::Create()) {
  layer()->SetIsDrawable(true);

  view_container_->SetIsDrawable(true);
  view_container_->SetMasksToBounds(true);

  view_layer_->SetIsDrawable(true);
  view_layer_->SetPosition(gfx::PointF(0, 0));
  view_container_->AddChild(view_layer_);

  divider_layer_->SetIsDrawable(true);
  divider_layer_->SetPosition(gfx::PointF(0, 0));
  divider_layer_->SetHideLayerAndSubtree(true);
  view_container_->AddChild(divider_layer_);

  is_debugging_ = chrome::android::kEdgeToEdgeBottomChinDebugParam.Get();
  if (is_debugging_) {
    debug_layer_->SetIsDrawable(true);
    debug_layer_->SetBackgroundColor(SkColors::kMagenta);
    debug_layer_->SetOpacity(0.5f);
    view_container_->AddChild(debug_layer_);
  }
}

EdgeToEdgeBottomChinSceneLayer::~EdgeToEdgeBottomChinSceneLayer() = default;

void EdgeToEdgeBottomChinSceneLayer::UpdateEdgeToEdgeBottomChinLayer(
    JNIEnv* env,
    jint container_width,
    jint container_height,
    jint color_argb,
    jint divider_color,
    jfloat y_offset) {
  view_container_->SetBounds(gfx::Size(container_width, container_height));
  view_container_->SetPosition(gfx::PointF(0, y_offset - container_height));

  view_layer_->SetBackgroundColor(SkColor4f::FromColor(color_argb));
  view_layer_->SetBounds(gfx::Size(container_width, container_height));

  // Divider is set to be a 1px ARBG color on top of the chin.
  // The color could be transparent or the same as |color_argb|
  divider_layer_->SetBackgroundColor(SkColor4f::FromColor(divider_color));
  divider_layer_->SetBounds(gfx::Size(container_width, 1));

  if (is_debugging_) {
    debug_layer_->SetBounds(gfx::Size(container_width / 2, container_height));
  }
}

void EdgeToEdgeBottomChinSceneLayer::SetContentTree(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jobject>& jcontent_tree) {
  SceneLayer* content_tree = FromJavaObject(env, jcontent_tree);
  if (!content_tree || !content_tree->layer()) {
    return;
  }

  if (!content_tree->layer()->parent() ||
      (content_tree->layer()->parent()->id() != layer_->id())) {
    layer_->AddChild(content_tree->layer());
    layer_->AddChild(view_container_);
  }

  // Propagate the background color up from the content layer.
  should_show_background_ = content_tree->ShouldShowBackground();
  background_color_ = content_tree->GetBackgroundColor();
}

SkColor EdgeToEdgeBottomChinSceneLayer::GetBackgroundColor() {
  return background_color_;
}

bool EdgeToEdgeBottomChinSceneLayer::ShouldShowBackground() {
  return should_show_background_;
}

static jlong JNI_EdgeToEdgeBottomChinSceneLayer_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  EdgeToEdgeBottomChinSceneLayer* scene_layer =
      new EdgeToEdgeBottomChinSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(scene_layer);
}

}  // namespace android
