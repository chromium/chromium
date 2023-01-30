// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_SCROLLING_BOTTOM_VIEW_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_SCROLLING_BOTTOM_VIEW_SCENE_LAYER_H_

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

class ScrollingBottomViewSceneLayer : public SceneLayer {
 public:
  ScrollingBottomViewSceneLayer(JNIEnv* env,
                                const base::android::JavaRef<jobject>& jobj);

  ScrollingBottomViewSceneLayer(const ScrollingBottomViewSceneLayer&) = delete;
  ScrollingBottomViewSceneLayer& operator=(
      const ScrollingBottomViewSceneLayer&) = delete;

  ~ScrollingBottomViewSceneLayer() override;

  // Update the compositor version of the view.
  void UpdateScrollingBottomViewLayer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      const base::android::JavaParamRef<jobject>& jresource_manager,
      jint view_resource_id,
      jint shadow_height,
      jfloat x_offset,
      jfloat y_offset,
      bool show_shadow);

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

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_SCROLLING_BOTTOM_VIEW_SCENE_LAYER_H_
