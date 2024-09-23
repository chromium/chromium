// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/top_toolbar_scene_layer.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "cc/input/android/offset_tag_android.h"
#include "cc/slim/solid_color_layer.h"
#include "chrome/browser/android/compositor/layer/toolbar_layer.h"
#include "components/viz/common/quads/offset_tag.h"
#include "ui/android/resources/resource_manager_impl.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/toolbar/jni_headers/TopToolbarSceneLayer_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace android {

TopToolbarSceneLayer::TopToolbarSceneLayer(JNIEnv* env,
                                           const JavaRef<jobject>& jobj)
    : SceneLayer(env, jobj),
      should_show_background_(false),
      background_color_(SK_ColorWHITE),
      content_container_(cc::slim::Layer::Create()) {
  layer()->AddChild(content_container_);
  layer()->SetIsDrawable(true);
}

TopToolbarSceneLayer::~TopToolbarSceneLayer() = default;

void TopToolbarSceneLayer::UpdateToolbarLayer(
    JNIEnv* env,
    const JavaParamRef<jobject>& object,
    const JavaParamRef<jobject>& jresource_manager,
    jint toolbar_resource_id,
    jint toolbar_background_color,
    jint url_bar_resource_id,
    jint url_bar_color,
    jfloat x_offset,
    jfloat content_offset,
    bool show_shadow,
    bool visible,
    bool anonymize,
    const base::android::JavaParamRef<jobject>& joffset_tag) {
  // If the toolbar layer has not been created yet, create it.
  if (!toolbar_layer_) {
    ui::ResourceManager* resource_manager =
        ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
    toolbar_layer_ = ToolbarLayer::Create(resource_manager);
    toolbar_layer_->layer()->SetHideLayerAndSubtree(true);
    layer_->AddChild(toolbar_layer_->layer());
  }

  toolbar_layer_->layer()->SetHideLayerAndSubtree(!visible);
  if (!visible) {
    return;
  }

  viz::OffsetTag offset_tag = cc::android::FromJavaOffsetTag(env, joffset_tag);
  toolbar_layer_->PushResource(toolbar_resource_id, toolbar_background_color,
                               anonymize, url_bar_color, url_bar_resource_id,
                               x_offset, content_offset, false, !show_shadow,
                               offset_tag);
}

void TopToolbarSceneLayer::UpdateProgressBar(
    JNIEnv* env,
    const JavaParamRef<jobject>& object,
    jint progress_bar_x,
    jint progress_bar_y,
    jint progress_bar_width,
    jint progress_bar_height,
    jint progress_bar_color,
    jint progress_bar_background_x,
    jint progress_bar_background_y,
    jint progress_bar_background_width,
    jint progress_bar_background_height,
    jint progress_bar_background_color) {
  if (!toolbar_layer_)
    return;
  toolbar_layer_->UpdateProgressBar(
      progress_bar_x, progress_bar_y, progress_bar_width, progress_bar_height,
      progress_bar_color, progress_bar_background_x, progress_bar_background_y,
      progress_bar_background_width, progress_bar_background_height,
      progress_bar_background_color);
}

void TopToolbarSceneLayer::SetContentTree(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jobject>& jcontent_tree) {
  SceneLayer* content_tree = FromJavaObject(env, jcontent_tree);
  if (!content_tree || !content_tree->layer())
    return;

  if (!content_tree->layer()->parent() ||
      (content_tree->layer()->parent()->id() != content_container_->id())) {
    // Clear out all the children of the container when the content changes.
    // This indicates that the layout has switched.
    content_container_->RemoveAllChildren();
    content_container_->AddChild(content_tree->layer());
  }

  // Propagate the background color up from the content layer.
  should_show_background_ = content_tree->ShouldShowBackground();
  background_color_ = content_tree->GetBackgroundColor();
}

SkColor TopToolbarSceneLayer::GetBackgroundColor() {
  return background_color_;
}

bool TopToolbarSceneLayer::ShouldShowBackground() {
  return should_show_background_;
}

static jlong JNI_TopToolbarSceneLayer_Init(JNIEnv* env,
                                           const JavaParamRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  TopToolbarSceneLayer* toolbar_scene_layer =
      new TopToolbarSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(toolbar_scene_layer);
}

}  // namespace android
