// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_SOLID_COLOR_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_SOLID_COLOR_SCENE_LAYER_H_

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/android/layouts/scene_layer.h"
#include "third_party/skia/include/core/SkColor.h"

namespace android {

// A SceneLayer to render a solid color.
class SolidColorSceneLayer : public SceneLayer {
 public:
  SolidColorSceneLayer(JNIEnv* env,
                       const base::android::JavaRef<jobject>& jobj);
  ~SolidColorSceneLayer() override;

  SolidColorSceneLayer(const SolidColorSceneLayer&) = delete;
  SolidColorSceneLayer& operator=(const SolidColorSceneLayer&) = delete;

  bool ShouldShowBackground() override;
  SkColor GetBackgroundColor() override;

  void SetBackgroundColor(JNIEnv* env, jint background_color);

 private:
  SkColor background_color_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_SOLID_COLOR_SCENE_LAYER_H_
