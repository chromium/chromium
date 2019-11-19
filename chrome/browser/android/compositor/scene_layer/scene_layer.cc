// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/scene_layer.h"

#include "cc/layers/layer.h"
#include "chrome/android/chrome_jni_headers/SceneLayer_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace android {

// static
SceneLayer* SceneLayer::FromJavaObject(JNIEnv* env,
                                       const JavaRef<jobject>& jobj) {
  if (jobj.is_null())
    return nullptr;
  return reinterpret_cast<SceneLayer*>(Java_SceneLayer_getNativePtr(env, jobj));
}

SceneLayer::SceneLayer(JNIEnv* env, const JavaRef<jobject>& jobj)
    : SceneLayer(env, jobj, cc::Layer::Create()) {}

SceneLayer::SceneLayer(JNIEnv* env,
                       const JavaRef<jobject>& jobj,
                       scoped_refptr<cc::Layer> layer)
    : weak_java_scene_layer_(env, jobj), layer_(layer) {
  Java_SceneLayer_setNativePtr(env, jobj, reinterpret_cast<intptr_t>(this));
}

SceneLayer::~SceneLayer() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = weak_java_scene_layer_.get(env);
  if (jobj.is_null())
    return;

  Java_SceneLayer_setNativePtr(
      env, jobj, reinterpret_cast<intptr_t>(static_cast<SceneLayer*>(NULL)));
}

void SceneLayer::OnDetach() {
  layer()->RemoveFromParent();
}

void SceneLayer::Destroy(JNIEnv* env, const JavaParamRef<jobject>& jobj) {
  delete this;
}

bool SceneLayer::ShouldShowBackground() {
  return false;
}

SkColor SceneLayer::GetBackgroundColor() {
  return SK_ColorWHITE;
}

static jlong JNI_SceneLayer_Init(JNIEnv* env,
                                 const JavaParamRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  SceneLayer* tree_provider = new SceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(tree_provider);
}

}  // namespace android
