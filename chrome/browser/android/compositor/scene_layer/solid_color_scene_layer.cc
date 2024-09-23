// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/solid_color_scene_layer.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/SolidColorSceneLayer_jni.h"

namespace android {

SolidColorSceneLayer::SolidColorSceneLayer(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj)
    : SceneLayer(env, jobj), background_color_(SK_ColorWHITE) {}

SolidColorSceneLayer::~SolidColorSceneLayer() = default;

bool SolidColorSceneLayer::ShouldShowBackground() {
  return true;
}

SkColor SolidColorSceneLayer::GetBackgroundColor() {
  return background_color_;
}

void SolidColorSceneLayer::SetBackgroundColor(JNIEnv* env,
                                              jint background_color) {
  background_color_ = static_cast<SkColor>(background_color);
}

static jlong JNI_SolidColorSceneLayer_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj) {
  // This binds to the Java jobject and gives it ownership.
  SolidColorSceneLayer* scene_layer = new SolidColorSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(scene_layer);
}

}  // namespace android
