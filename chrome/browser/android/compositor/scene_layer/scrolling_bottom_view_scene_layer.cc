// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/scrolling_bottom_view_scene_layer.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "cc/slim/layer.h"
#include "cc/slim/ui_resource_layer.h"
#include "ui/android/resources/resource_manager_impl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/toolbar/jni_headers/ScrollingBottomViewSceneLayer_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace android {

ScrollingBottomViewSceneLayer::ScrollingBottomViewSceneLayer(
    JNIEnv* env,
    const JavaRef<jobject>& jobj)
    : SceneLayer(env, jobj),
      should_show_background_(false),
      background_color_(SK_ColorWHITE),
      view_container_(cc::slim::Layer::Create()),
      view_layer_(cc::slim::UIResourceLayer::Create()) {
  layer()->SetIsDrawable(true);

  view_container_->SetIsDrawable(true);
  view_container_->SetMasksToBounds(true);

  view_layer_->SetIsDrawable(true);
  view_container_->AddChild(view_layer_);
}

ScrollingBottomViewSceneLayer::~ScrollingBottomViewSceneLayer() = default;

void ScrollingBottomViewSceneLayer::UpdateScrollingBottomViewLayer(
    JNIEnv* env,
    const JavaParamRef<jobject>& object,
    const JavaParamRef<jobject>& jresource_manager,
    jint view_resource_id,
    jint shadow_height,
    jfloat x_offset,
    jfloat y_offset,
    bool show_shadow) {
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  ui::Resource* bottom_view_resource = resource_manager->GetResource(
      ui::ANDROID_RESOURCE_TYPE_DYNAMIC, view_resource_id);

  // If the resource isn't available, don't bother doing anything else.
  if (!bottom_view_resource)
    return;

  view_layer_->SetUIResourceId(bottom_view_resource->ui_resource()->id());

  int container_height = bottom_view_resource->size().height();
  int texture_y_offset = 0;

  // The view container layer's height depends on whether the shadow is
  // showing. If the shadow should be clipped, reduce the height of the
  // container.
  if (!show_shadow) {
    container_height -= shadow_height;
    texture_y_offset -= shadow_height;
  }

  view_container_->SetBounds(
      gfx::Size(bottom_view_resource->size().width(), container_height));
  view_container_->SetPosition(gfx::PointF(0, y_offset - container_height));

  // The view's layer should be the same size as the texture.
  view_layer_->SetBounds(gfx::Size(bottom_view_resource->size().width(),
                                   bottom_view_resource->size().height()));
  view_layer_->SetPosition(gfx::PointF(x_offset, texture_y_offset));
}

void ScrollingBottomViewSceneLayer::SetContentTree(
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

SkColor ScrollingBottomViewSceneLayer::GetBackgroundColor() {
  return background_color_;
}

bool ScrollingBottomViewSceneLayer::ShouldShowBackground() {
  return should_show_background_;
}

static jlong JNI_ScrollingBottomViewSceneLayer_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  ScrollingBottomViewSceneLayer* scene_layer =
      new ScrollingBottomViewSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(scene_layer);
}

}  // namespace android
