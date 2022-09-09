// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TOOLBAR_SWIPE_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TOOLBAR_SWIPE_SCENE_LAYER_H_

#include <jni.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/android/layouts/scene_layer.h"
#include "third_party/skia/include/core/SkColor.h"

namespace android {

class ContentLayer;
class TabContentManager;

class ToolbarSwipeSceneLayer : public SceneLayer {
 public:
  ToolbarSwipeSceneLayer(JNIEnv* env,
                         const base::android::JavaRef<jobject>& jobj);

  ~ToolbarSwipeSceneLayer() override;

  void UpdateLayer(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& jobj,
                   jint id,
                   jboolean left_tab,
                   jboolean can_use_live_layer,
                   jint default_background_color,
                   jfloat x,
                   jfloat y);

  void SetTabContentManager(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      const base::android::JavaParamRef<jobject>& jtab_content_manager);

  bool ShouldShowBackground() override;

  SkColor GetBackgroundColor() override;

 private:
  scoped_refptr<android::ContentLayer> left_content_layer_;
  scoped_refptr<android::ContentLayer> right_content_layer_;

  raw_ptr<TabContentManager> tab_content_manager_;

  SkColor background_color_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TOOLBAR_SWIPE_SCENE_LAYER_H_
