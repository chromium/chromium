// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_READALOUD_MINI_PLAYER_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_READALOUD_MINI_PLAYER_SCENE_LAYER_H_

#include "chrome/browser/ui/android/layouts/scene_layer.h"

namespace cc::slim {
class SolidColorLayer;
}  // namespace cc::slim

namespace android {

class ReadAloudMiniPlayerSceneLayer : public SceneLayer {
 public:
  ReadAloudMiniPlayerSceneLayer(JNIEnv* env,
                                const base::android::JavaRef<jobject>& jobj);

  ReadAloudMiniPlayerSceneLayer(const ReadAloudMiniPlayerSceneLayer&) = delete;
  ReadAloudMiniPlayerSceneLayer& operator=(
      const ReadAloudMiniPlayerSceneLayer&) = delete;

  ~ReadAloudMiniPlayerSceneLayer() override;

  void Destroy(JNIEnv* env) override;

  // Update the compositor version of the view.
  void UpdateReadAloudMiniPlayerLayer(JNIEnv* env,
                                      int32_t color_argb,
                                      int32_t width,
                                      int32_t viewport_height,
                                      int32_t container_height,
                                      int32_t bottom_offset);

  void SetContentTree(JNIEnv* env,
                      const base::android::JavaRef<jobject>& jcontent_tree);

  SkColor GetBackgroundColor() override;

  bool ShouldShowBackground() override;

 private:
  bool should_show_background_;
  SkColor background_color_;
  scoped_refptr<cc::slim::SolidColorLayer> view_layer_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_READALOUD_MINI_PLAYER_SCENE_LAYER_H_
