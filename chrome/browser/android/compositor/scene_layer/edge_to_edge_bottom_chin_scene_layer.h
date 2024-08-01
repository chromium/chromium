// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_EDGE_TO_EDGE_BOTTOM_CHIN_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_EDGE_TO_EDGE_BOTTOM_CHIN_SCENE_LAYER_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/android/layouts/scene_layer.h"

namespace cc::slim {
class Layer;
class SolidColorLayer;
}  // namespace cc::slim

namespace android {

class EdgeToEdgeBottomChinSceneLayer : public SceneLayer {
 public:
  EdgeToEdgeBottomChinSceneLayer(JNIEnv* env,
                                 const base::android::JavaRef<jobject>& jobj);

  EdgeToEdgeBottomChinSceneLayer(const EdgeToEdgeBottomChinSceneLayer&) =
      delete;
  EdgeToEdgeBottomChinSceneLayer& operator=(
      const EdgeToEdgeBottomChinSceneLayer&) = delete;

  ~EdgeToEdgeBottomChinSceneLayer() override;

  // Update the compositor version of the view.
  void UpdateEdgeToEdgeBottomChinLayer(JNIEnv* env,
                                       jint container_width,
                                       jint container_height,
                                       jint color_argb,
                                       jint divider_color,
                                       jfloat y_offset);

  void SetContentTree(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      const base::android::JavaParamRef<jobject>& jcontent_tree);

  SkColor GetBackgroundColor() override;

  bool ShouldShowBackground() override;

 private:
  bool should_show_background_{false};
  bool is_debugging_;

  SkColor background_color_{SK_ColorWHITE};
  // TODO(crbug.com/349876343) Add divider as a new layer in the container.
  scoped_refptr<cc::slim::Layer> view_container_;
  scoped_refptr<cc::slim::SolidColorLayer> view_layer_;
  scoped_refptr<cc::slim::SolidColorLayer> divider_layer_;

  // When adding new layers, add below to ensure debug layers stay on the top.
  scoped_refptr<cc::slim::SolidColorLayer> debug_layer_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_EDGE_TO_EDGE_BOTTOM_CHIN_SCENE_LAYER_H_
