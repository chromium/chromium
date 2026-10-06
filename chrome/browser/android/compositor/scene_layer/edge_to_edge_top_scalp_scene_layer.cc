// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/edge_to_edge_top_scalp_scene_layer.h"

#include "base/android/jni_android.h"
#include "cc/input/android/offset_tag_android.h"
#include "cc/slim/layer.h"
#include "cc/slim/solid_color_layer.h"
#include "chrome/browser/ui/android/edge_to_edge/jni_headers/EdgeToEdgeTopScalpSceneLayer_jni.h"
#include "components/viz/common/quads/offset_tag.h"

using base::android::JavaRef;

namespace android {

EdgeToEdgeTopScalpSceneLayer::EdgeToEdgeTopScalpSceneLayer(
    JNIEnv* env,
    const JavaRef<jobject>& jobj)
    : SceneLayer(env, jobj),
      view_container_(cc::slim::Layer::Create()),
      view_layer_(cc::slim::SolidColorLayer::Create()) {
  layer()->SetIsDrawable(true);

  view_container_->SetIsDrawable(true);
  view_container_->SetMasksToBounds(true);

  view_layer_->SetIsDrawable(true);
  view_layer_->SetPosition(gfx::PointF(0, 0));
  view_container_->AddChild(view_layer_);
  layer()->AddChild(view_container_);
}

EdgeToEdgeTopScalpSceneLayer::~EdgeToEdgeTopScalpSceneLayer() = default;

void EdgeToEdgeTopScalpSceneLayer::UpdateEdgeToEdgeTopScalpLayer(
    JNIEnv* env,
    int32_t container_width,
    int32_t container_height,
    int32_t color_argb,
    float y_offset,
    const base::android::JavaRef<jobject>& joffset_tag) {
  view_container_->SetBounds(gfx::Size(container_width, container_height));
  view_container_->SetPosition(gfx::PointF(0, y_offset));

  viz::OffsetTag offset_tag = cc::android::FromJavaOffsetTag(env, joffset_tag);
  view_container_->SetOffsetTag(offset_tag);

  view_layer_->SetBackgroundColor(SkColor4f::FromColor(color_argb));
  view_layer_->SetBounds(gfx::Size(container_width, container_height));
}

void EdgeToEdgeTopScalpSceneLayer::SetContentTree(
    JNIEnv* env,
    const JavaRef<jobject>& jcontent_tree) {
  SceneLayer* content_tree = FromJavaObject(env, jcontent_tree);
  if (!content_tree || !content_tree->layer()) {
    return;
  }

  if (!content_tree->layer()->parent() ||
      (content_tree->layer()->parent()->id() != layer_->id())) {
    // The content tree changes. Remove all the children.
    layer_->RemoveAllChildren();
    layer_->AddChild(content_tree->layer());
    layer_->AddChild(view_container_);
  }

  // Propagate the background color up from the content layer.
  should_show_background_ = content_tree->ShouldShowBackground();
  background_color_ = content_tree->GetBackgroundColor();
}

SkColor EdgeToEdgeTopScalpSceneLayer::GetBackgroundColor() {
  return background_color_;
}

bool EdgeToEdgeTopScalpSceneLayer::ShouldShowBackground() {
  return should_show_background_;
}

static int64_t JNI_EdgeToEdgeTopScalpSceneLayer_Init(
    JNIEnv* env,
    const JavaRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  EdgeToEdgeTopScalpSceneLayer* scene_layer =
      new EdgeToEdgeTopScalpSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(scene_layer);
}

}  // namespace android

DEFINE_JNI(EdgeToEdgeTopScalpSceneLayer)
