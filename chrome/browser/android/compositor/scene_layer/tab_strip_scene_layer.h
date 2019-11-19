// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TAB_STRIP_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TAB_STRIP_SCENE_LAYER_H_

#include <memory>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "cc/layers/layer.h"
#include "cc/layers/ui_resource_layer.h"
#include "chrome/browser/android/compositor/scene_layer/scene_layer.h"

namespace cc {
class SolidColorLayer;
}

namespace android {

class LayerTitleCache;
class TabHandleLayer;

// A scene layer to draw one or more tab strips. Note that content tree can be
// added as a subtree.
class TabStripSceneLayer : public SceneLayer {
 public:
  TabStripSceneLayer(JNIEnv* env, const base::android::JavaRef<jobject>& jobj);
  ~TabStripSceneLayer() override;

  void SetContentTree(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      const base::android::JavaParamRef<jobject>& jcontent_tree);

  void BeginBuildingFrame(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& jobj,
                          jboolean visible);

  void FinishBuildingFrame(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& jobj);

  void UpdateTabStripLayer(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& jobj,
                           jfloat width,
                           jfloat height,
                           jfloat y_offset,
                           jfloat background_tab_brightness,
                           jfloat brightness,
                           jboolean should_readd_background);

  void UpdateNewTabButton(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jint resource_id,
      jfloat x,
      jfloat y,
      jfloat width,
      jfloat height,
      jboolean visible,
      const base::android::JavaParamRef<jobject>& jresource_manager);

  void UpdateModelSelectorButton(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jint resource_id,
      jfloat x,
      jfloat y,
      jfloat width,
      jfloat height,
      jboolean incognito,
      jboolean visible,
      const base::android::JavaParamRef<jobject>& jresource_manager);

  void UpdateTabStripLeftFade(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jint resource_id,
      jfloat opacity,
      const base::android::JavaParamRef<jobject>& jresource_manager);

  void UpdateTabStripRightFade(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jint resource_id,
      jfloat opacity,
      const base::android::JavaParamRef<jobject>& jresource_manager);

  void PutStripTabLayer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jint id,
      jint close_resource_id,
      jint handle_resource_id,
      jint handle_outline_resource_id,
      jint close_tint,
      jint handle_tint,
      jint handle_outline_tint,
      jboolean foreground,
      jboolean close_pressed,
      jfloat toolbar_width,
      jfloat x,
      jfloat y,
      jfloat width,
      jfloat height,
      jfloat content_offset_x,
      jfloat close_button_alpha,
      jboolean is_loading,
      jfloat spinner_rotation,
      const base::android::JavaParamRef<jobject>& jlayer_title_cache,
      const base::android::JavaParamRef<jobject>& jresource_manager);

  bool ShouldShowBackground() override;
  SkColor GetBackgroundColor() override;

 private:
  scoped_refptr<TabHandleLayer> GetNextLayer(
      LayerTitleCache* layer_title_cache);

  typedef std::vector<scoped_refptr<TabHandleLayer>> TabHandleLayerList;

  scoped_refptr<cc::SolidColorLayer> tab_strip_layer_;
  scoped_refptr<cc::Layer> scrollable_strip_layer_;
  scoped_refptr<cc::UIResourceLayer> new_tab_button_;
  scoped_refptr<cc::UIResourceLayer> left_fade_;
  scoped_refptr<cc::UIResourceLayer> right_fade_;
  scoped_refptr<cc::UIResourceLayer> model_selector_button_;

  float background_tab_brightness_;
  float brightness_;
  unsigned write_index_;
  TabHandleLayerList tab_handle_layers_;
  SceneLayer* content_tree_;

  DISALLOW_COPY_AND_ASSIGN(TabStripSceneLayer);
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TAB_STRIP_SCENE_LAYER_H_
