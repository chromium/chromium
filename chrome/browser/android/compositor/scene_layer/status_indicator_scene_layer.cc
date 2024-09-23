// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/status_indicator_scene_layer.h"

#include "cc/slim/layer.h"
#include "cc/slim/ui_resource_layer.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/StatusIndicatorSceneLayer_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace android {

StatusIndicatorSceneLayer::StatusIndicatorSceneLayer(
    JNIEnv* env,
    const JavaRef<jobject>& jobj)
    : SceneLayer(env, jobj),
      view_container_(cc::slim::Layer::Create()),
      view_layer_(cc::slim::UIResourceLayer::Create()) {
  layer()->SetIsDrawable(true);

  view_container_->SetIsDrawable(true);
  view_container_->SetMasksToBounds(true);

  view_layer_->SetIsDrawable(true);
  view_container_->AddChild(view_layer_);
}

StatusIndicatorSceneLayer::~StatusIndicatorSceneLayer() {}

void StatusIndicatorSceneLayer::UpdateStatusIndicatorLayer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& object,
    const base::android::JavaParamRef<jobject>& jresource_manager,
    jint view_resource_id,
    jint y_offset) {
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  ui::Resource* resource = resource_manager->GetResource(
      ui::ANDROID_RESOURCE_TYPE_DYNAMIC, view_resource_id);

  // If the resource isn't available, don't bother doing anything else.
  if (!resource)
    return;

  view_layer_->SetUIResourceId(resource->ui_resource()->id());

  view_container_->SetBounds(resource->size());
  view_container_->SetPosition(gfx::PointF(0, 0));

  // The view's layer should be the same size as the texture.
  view_layer_->SetBounds(resource->size());
  // Position the layer at the bottom of the offset.
  view_layer_->SetPosition(
      gfx::PointF(0, y_offset - resource->size().height()));
}

void StatusIndicatorSceneLayer::SetContentTree(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jobject>& jcontent_tree) {
  SceneLayer* content_tree = FromJavaObject(env, jcontent_tree);
  if (!content_tree || !content_tree->layer())
    return;

  if (!content_tree->layer()->parent() ||
      (content_tree->layer()->parent()->id() != layer_->id())) {
    layer_->AddChild(content_tree->layer());
    layer_->AddChild(view_container_);
  }

  // Propagate the background color up from the content layer.
  should_show_background_ = content_tree->ShouldShowBackground();
  background_color_ = content_tree->GetBackgroundColor();
}

SkColor StatusIndicatorSceneLayer::GetBackgroundColor() {
  return background_color_;
}

bool StatusIndicatorSceneLayer::ShouldShowBackground() {
  return should_show_background_;
}

static jlong JNI_StatusIndicatorSceneLayer_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  StatusIndicatorSceneLayer* scene_layer =
      new StatusIndicatorSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(scene_layer);
}

}  // namespace android
