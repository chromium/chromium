// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/readaloud_mini_player_scene_layer.h"

#include "cc/slim/layer.h"
#include "cc/slim/solid_color_layer.h"
#include "third_party/skia/include/core/SkColor.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/readaloud/android/jni_headers/ReadAloudMiniPlayerSceneLayer_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace android {

ReadAloudMiniPlayerSceneLayer::ReadAloudMiniPlayerSceneLayer(
    JNIEnv* env,
    const JavaRef<jobject>& jobj)
    : SceneLayer(env, jobj),
      should_show_background_(false),
      background_color_(SK_ColorWHITE),
      view_layer_(cc::slim::SolidColorLayer::Create()) {
  layer()->SetIsDrawable(true);

  view_layer_->SetMasksToBounds(true);
  view_layer_->SetIsDrawable(true);
}

ReadAloudMiniPlayerSceneLayer::~ReadAloudMiniPlayerSceneLayer() = default;

void ReadAloudMiniPlayerSceneLayer::Destroy(JNIEnv* env,
                                            const JavaParamRef<jobject>& jobj) {
  delete this;
}

void ReadAloudMiniPlayerSceneLayer::UpdateReadAloudMiniPlayerLayer(
    JNIEnv* env,
    jint color_argb,
    jint width,
    jint viewport_height,
    jint container_height,
    jint bottom_offset) {
  view_layer_->SetBounds(gfx::Size(width, container_height));
  view_layer_->SetPosition(gfx::PointF(0, viewport_height - bottom_offset));
  view_layer_->SetBackgroundColor(SkColor4f::FromColor(color_argb));
}

void ReadAloudMiniPlayerSceneLayer::SetContentTree(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcontent_tree) {
  SceneLayer* content_tree = FromJavaObject(env, jcontent_tree);
  if (!content_tree || !content_tree->layer()) {
    return;
  }

  if (!content_tree->layer()->parent() ||
      (content_tree->layer()->parent()->id() != layer_->id())) {
    layer_->AddChild(content_tree->layer());
    layer_->AddChild(view_layer_);
  }

  // Propagate the layer background color up from the content layer.
  should_show_background_ = content_tree->ShouldShowBackground();
  background_color_ = content_tree->GetBackgroundColor();
}

SkColor ReadAloudMiniPlayerSceneLayer::GetBackgroundColor() {
  return background_color_;
}

bool ReadAloudMiniPlayerSceneLayer::ShouldShowBackground() {
  return should_show_background_;
}

static jlong JNI_ReadAloudMiniPlayerSceneLayer_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  ReadAloudMiniPlayerSceneLayer* scene_layer =
      new ReadAloudMiniPlayerSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(scene_layer);
}

}  // namespace android
