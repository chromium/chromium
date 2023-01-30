// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_STATUS_INDICATOR_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_STATUS_INDICATOR_SCENE_LAYER_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/android/layouts/scene_layer.h"
#include "ui/android/resources/resource_manager_impl.h"

namespace cc::slim {
class Layer;
class UIResourceLayer;
}  // namespace cc::slim

namespace android {

class StatusIndicatorSceneLayer : public SceneLayer {
 public:
  StatusIndicatorSceneLayer(JNIEnv* env,
                            const base::android::JavaRef<jobject>& jobj);

  StatusIndicatorSceneLayer(const StatusIndicatorSceneLayer&) = delete;
  StatusIndicatorSceneLayer& operator=(const StatusIndicatorSceneLayer&) =
      delete;

  ~StatusIndicatorSceneLayer() override;

  // Update the compositor version of the view.
  void UpdateStatusIndicatorLayer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      const base::android::JavaParamRef<jobject>& jresource_manager,
      jint view_resource_id,
      jint y_offset);

  void SetContentTree(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      const base::android::JavaParamRef<jobject>& jcontent_tree);

  SkColor GetBackgroundColor() override;
  bool ShouldShowBackground() override;

 private:
  bool should_show_background_;
  SkColor background_color_;
  scoped_refptr<cc::slim::Layer> view_container_;
  scoped_refptr<cc::slim::UIResourceLayer> view_layer_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_STATUS_INDICATOR_SCENE_LAYER_H_
