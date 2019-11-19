// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/toolbar_scene_layer.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "cc/layers/solid_color_layer.h"
#include "chrome/android/chrome_jni_headers/ToolbarSceneLayer_jni.h"
#include "chrome/browser/android/compositor/layer/toolbar_layer.h"
#include "ui/android/resources/resource_manager_impl.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace android {

ToolbarSceneLayer::ToolbarSceneLayer(JNIEnv* env, const JavaRef<jobject>& jobj)
    : SceneLayer(env, jobj),
      should_show_background_(false),
      background_color_(SK_ColorWHITE),
      content_container_(cc::Layer::Create()) {
  layer()->AddChild(content_container_);
  layer()->SetIsDrawable(true);
}

ToolbarSceneLayer::~ToolbarSceneLayer() {
}

void ToolbarSceneLayer::UpdateToolbarLayer(
    JNIEnv* env,
    const JavaParamRef<jobject>& object,
    const JavaParamRef<jobject>& jresource_manager,
    jint toolbar_resource_id,
    jint toolbar_background_color,
    jint url_bar_resource_id,
    jfloat url_bar_alpha,
    jint url_bar_color,
    jfloat y_offset,
    jfloat view_height,
    bool visible,
    bool show_shadow) {
  // If the toolbar layer has not been created yet, create it.
  if (!toolbar_layer_) {
    ui::ResourceManager* resource_manager =
        ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
    toolbar_layer_ = ToolbarLayer::Create(resource_manager);
    toolbar_layer_->layer()->SetHideLayerAndSubtree(true);
    layer_->AddChild(toolbar_layer_->layer());
  }

  toolbar_layer_->layer()->SetHideLayerAndSubtree(!visible);
  if (visible) {
    toolbar_layer_->PushResource(toolbar_resource_id, toolbar_background_color,
                                 false, url_bar_color, url_bar_resource_id,
                                 url_bar_alpha, view_height, y_offset, false,
                                 !show_shadow);
  }
}

void ToolbarSceneLayer::UpdateProgressBar(JNIEnv* env,
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
  if (!toolbar_layer_) return;
  toolbar_layer_->UpdateProgressBar(progress_bar_x,
                                    progress_bar_y,
                                    progress_bar_width,
                                    progress_bar_height,
                                    progress_bar_color,
                                    progress_bar_background_x,
                                    progress_bar_background_y,
                                    progress_bar_background_width,
                                    progress_bar_background_height,
                                    progress_bar_background_color);
}

void ToolbarSceneLayer::SetContentTree(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jobject>& jcontent_tree) {
  SceneLayer* content_tree = FromJavaObject(env, jcontent_tree);
  if (!content_tree || !content_tree->layer()) return;

  if (!content_tree->layer()->parent()
      || (content_tree->layer()->parent()->id() != content_container_->id())) {
    // Clear out all the children of the container when the content changes.
    // This indicates that the layout has switched.
    content_container_->RemoveAllChildren();
    content_container_->AddChild(content_tree->layer());
  }

  // Propagate the background color up from the content layer.
  should_show_background_ = content_tree->ShouldShowBackground();
  background_color_ = content_tree->GetBackgroundColor();
}

SkColor ToolbarSceneLayer::GetBackgroundColor() {
  return background_color_;
}

bool ToolbarSceneLayer::ShouldShowBackground() {
  return should_show_background_;
}

static jlong JNI_ToolbarSceneLayer_Init(JNIEnv* env,
                                        const JavaParamRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  ToolbarSceneLayer* toolbar_scene_layer =
      new ToolbarSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(toolbar_scene_layer);
}

}  // namespace android
