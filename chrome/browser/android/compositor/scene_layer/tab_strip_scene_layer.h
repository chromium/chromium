// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TAB_STRIP_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TAB_STRIP_SCENE_LAYER_H_

#include <memory>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/android/layouts/scene_layer.h"

namespace cc::slim {
class Layer;
class SolidColorLayer;
class UIResourceLayer;
}  // namespace cc::slim

namespace android {

class LayerTitleCache;
class TabHandleLayer;

// A scene layer to draw one or more tab strips. Note that content tree can be
// added as a subtree.
class TabStripSceneLayer : public SceneLayer {
 public:
  TabStripSceneLayer(JNIEnv* env,
                     const base::android::JavaRef<jobject>& jobj,
                     jboolean is_tab_strip_redesign_enabled,
                     jboolean is_tsr_btn_style_disabled);

  TabStripSceneLayer(const TabStripSceneLayer&) = delete;
  TabStripSceneLayer& operator=(const TabStripSceneLayer&) = delete;

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
                           jint width,
                           jint height,
                           jfloat y_offset,
                           jint background_color);

  void UpdateNewTabButton(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jint resource_id,
      jint bg_resource_id,
      jfloat x,
      jfloat y,
      jfloat touch_target_offset,
      jboolean visible,
      jint tint,
      jint background_tint,
      jfloat button_alpha,
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
      jfloat button_alpha,
      const base::android::JavaParamRef<jobject>& jresource_manager);

  void UpdateModelSelectorButtonBackground(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jint resource_id,
      jint bg_resource_id,
      jfloat x,
      jfloat y,
      jfloat width,
      jfloat height,
      jboolean incognito,
      jboolean visible,
      jint tint,
      jint background_tint,
      jfloat button_alpha,
      const base::android::JavaParamRef<jobject>& jresource_manager);

  void UpdateTabStripLeftFade(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jint resource_id,
      jfloat opacity,
      const base::android::JavaParamRef<jobject>& jresource_manager,
      jint leftFadeColor);

  void UpdateTabStripRightFade(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jint resource_id,
      jfloat opacity,
      const base::android::JavaParamRef<jobject>& jresource_manager,
      jint rightFadeColor);

  void PutStripTabLayer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jint id,
      jint close_resource_id,
      jint divider_resource_id,
      jint handle_resource_id,
      jint handle_outline_resource_id,
      jint close_tint,
      jint divider_tint,
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
      jfloat content_offset_y,
      jfloat divider_offset_x,
      jfloat bottom_margin,
      jfloat top_margin,
      jfloat close_button_padding,
      jfloat close_button_alpha,
      jboolean is_start_divider_visible,
      jboolean is_end_divider_visible,
      jboolean is_loading,
      jfloat spinner_rotation,
      jfloat brightness,
      jfloat opacity,
      const base::android::JavaParamRef<jobject>& jlayer_title_cache,
      const base::android::JavaParamRef<jobject>& jresource_manager);

  bool ShouldShowBackground() override;
  SkColor GetBackgroundColor() override;

 private:
  scoped_refptr<TabHandleLayer> GetNextLayer(
      LayerTitleCache* layer_title_cache);

  typedef std::vector<scoped_refptr<TabHandleLayer>> TabHandleLayerList;

  scoped_refptr<cc::slim::SolidColorLayer> tab_strip_layer_;
  scoped_refptr<cc::slim::Layer> scrollable_strip_layer_;
  scoped_refptr<cc::slim::UIResourceLayer> new_tab_button_;
  scoped_refptr<cc::slim::UIResourceLayer> new_tab_button_background_;
  scoped_refptr<cc::slim::UIResourceLayer> left_fade_;
  scoped_refptr<cc::slim::UIResourceLayer> right_fade_;
  scoped_refptr<cc::slim::UIResourceLayer> model_selector_button_;
  scoped_refptr<cc::slim::UIResourceLayer> model_selector_button_background_;

  const bool is_tab_strip_redesign_enabled_ = false;
  const bool is_tsr_btn_style_disabled_ = false;

  unsigned write_index_;
  TabHandleLayerList tab_handle_layers_;
  raw_ptr<SceneLayer> content_tree_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TAB_STRIP_SCENE_LAYER_H_
