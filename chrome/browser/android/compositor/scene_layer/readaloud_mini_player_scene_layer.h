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

  void Destroy(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& jobj) override;

  // Update the compositor version of the view.
  void UpdateReadAloudMiniPlayerLayer(JNIEnv* env,
                                      jint color_argb,
                                      jint width,
                                      jint viewport_height,
                                      jint container_height,
                                      jint bottom_offset);

  void SetContentTree(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcontent_tree);

  SkColor GetBackgroundColor() override;

  bool ShouldShowBackground() override;

 private:
  bool should_show_background_;
  SkColor background_color_;
  scoped_refptr<cc::slim::SolidColorLayer> view_layer_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_READALOUD_MINI_PLAYER_SCENE_LAYER_H_
