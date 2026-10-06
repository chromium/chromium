// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_EDGE_TO_EDGE_TOP_SCALP_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_EDGE_TO_EDGE_TOP_SCALP_SCENE_LAYER_H_

#include <cstdint>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/android/layouts/scene_layer.h"

namespace cc::slim {
class Layer;
class SolidColorLayer;
}  // namespace cc::slim

namespace android {

class EdgeToEdgeTopScalpSceneLayer : public SceneLayer {
 public:
  EdgeToEdgeTopScalpSceneLayer(JNIEnv* env,
                               const base::android::JavaRef<jobject>& jobj);

  EdgeToEdgeTopScalpSceneLayer(const EdgeToEdgeTopScalpSceneLayer&) = delete;
  EdgeToEdgeTopScalpSceneLayer& operator=(const EdgeToEdgeTopScalpSceneLayer&) =
      delete;

  ~EdgeToEdgeTopScalpSceneLayer() override;

  // Update the compositor version of the view.
  void UpdateEdgeToEdgeTopScalpLayer(
      JNIEnv* env,
      int32_t container_width,
      int32_t container_height,
      int32_t color_argb,
      float y_offset,
      const base::android::JavaRef<jobject>& joffset_tag);

  void SetContentTree(JNIEnv* env,
                      const base::android::JavaRef<jobject>& jcontent_tree);

  SkColor GetBackgroundColor() override;

  bool ShouldShowBackground() override;

 private:
  bool should_show_background_{false};

  SkColor background_color_{SK_ColorWHITE};
  scoped_refptr<cc::slim::Layer> view_container_;
  scoped_refptr<cc::slim::SolidColorLayer> view_layer_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_EDGE_TO_EDGE_TOP_SCALP_SCENE_LAYER_H_
