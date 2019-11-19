// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_STATUS_INDICATOR_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_STATUS_INDICATOR_SCENE_LAYER_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/android/compositor/scene_layer/scene_layer.h"
#include "ui/android/resources/resource_manager_impl.h"

namespace cc {
class Layer;
class UIResourceLayer;
}  // namespace cc

namespace android {

class StatusIndicatorSceneLayer : public SceneLayer {
 public:
  StatusIndicatorSceneLayer(JNIEnv* env,
                            const base::android::JavaRef<jobject>& jobj);
  ~StatusIndicatorSceneLayer() override;

  // Update the compositor version of the view.
  void UpdateStatusIndicatorLayer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      const base::android::JavaParamRef<jobject>& jresource_manager,
      jint view_resource_id);

  void SetContentTree(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      const base::android::JavaParamRef<jobject>& jcontent_tree);

  SkColor GetBackgroundColor() override;
  bool ShouldShowBackground() override;

 private:
  bool should_show_background_;
  SkColor background_color_;
  scoped_refptr<cc::Layer> view_container_;
  scoped_refptr<cc::UIResourceLayer> view_layer_;

  DISALLOW_COPY_AND_ASSIGN(StatusIndicatorSceneLayer);
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_STATUS_INDICATOR_SCENE_LAYER_H_
