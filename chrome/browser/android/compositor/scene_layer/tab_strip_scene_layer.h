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
#include "cc/input/android/offset_tag_android.h"
#include "chrome/browser/ui/android/layouts/scene_layer.h"
#include "ui/android/resources/resource_manager.h"

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
  TabStripSceneLayer(JNIEnv* env, const base::android::JavaRef<jobject>& jobj);

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

  void UpdateOffsetTag(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& jobj,
                       const base::android::JavaParamRef<jobject>& joffset_tag);

  void UpdateTabStripLayer(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& jobj,
                           jint width,
                           jint height,
                           jfloat y_offset,
                           jint background_color,
                           jint scrim_color,
                           jfloat scrim_opacity,
                           jfloat left_padding,
                           jfloat right_padding,
                           jfloat top_padding);

  void UpdateNewTabButton(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jint resource_id,
      jint bg_resource_id,
      jfloat x,
      jfloat y,
      jfloat top_padding,
      jfloat touch_target_offset,
      jboolean visible,
      jboolean should_apply_hover_highlight,
      jint tint,
      jint background_tint,
      jfloat button_alpha,
      const base::android::JavaParamRef<jobject>& jresource_manager);

  void UpdateModelSelectorButton(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jint resource_id,
      jint bg_resource_id,
      jfloat x,
      jfloat y,
      jboolean visible,
      jboolean should_apply_hover_highlight,
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
      jint leftFadeColor,
      jfloat left_padding);

  void UpdateTabStripRightFade(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jint resource_id,
      jfloat opacity,
      const base::android::JavaParamRef<jobject>& jresource_manager,
      jint rightFadeColor,
      jfloat right_padding);

  void PutStripTabLayer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jint id,
      jint close_resource_id,
      jint close_hover_bg_resource_id,
      jint divider_resource_id,
      jint handle_resource_id,
      jint handle_outline_resource_id,
      jint close_tint,
      jint close_hover_bg_tint,
      jint divider_tint,
      jint handle_tint,
      jint handle_outline_tint,
      jboolean foreground,
      jboolean shouldShowTabOutline,
      jboolean close_pressed,
      jfloat toolbar_width,
      jfloat x,
      jfloat y,
      jfloat width,
      jfloat height,
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
      jfloat opacity,
      const base::android::JavaParamRef<jobject>& jlayer_title_cache,
      const base::android::JavaParamRef<jobject>& jresource_manager);

  void PutGroupIndicatorLayer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jboolean incognito,
      jint id,
      jint tint,
      jfloat x,
      jfloat y,
      jfloat width,
      jfloat height,
      jfloat title_text_padding,
      jfloat corner_radius,
      jfloat bottom_indicator_width,
      jfloat bottom_indicator_height,
      const base::android::JavaParamRef<jobject>& jlayer_title_cache);

  bool ShouldShowBackground() override;
  SkColor GetBackgroundColor() override;

 private:
  scoped_refptr<TabHandleLayer> GetNextLayer(
      LayerTitleCache* layer_title_cache);

  scoped_refptr<cc::slim::SolidColorLayer> GetNextGroupTitleLayer();
  scoped_refptr<cc::slim::SolidColorLayer> GetNextGroupBottomLayer();

  void UpdateCompositorButton(
      scoped_refptr<cc::slim::UIResourceLayer> button,
      scoped_refptr<cc::slim::UIResourceLayer> background,
      ui::Resource* button_resource,
      ui::Resource* background_resource,
      float x,
      float y,
      bool visible,
      bool should_apply_hover_highlight,
      float button_alpha);

  typedef std::vector<scoped_refptr<TabHandleLayer>> TabHandleLayerList;

  scoped_refptr<cc::slim::SolidColorLayer> tab_strip_layer_;
  scoped_refptr<cc::slim::Layer> scrollable_strip_layer_;
  scoped_refptr<cc::slim::Layer> group_indicator_layer_;
  scoped_refptr<cc::slim::UIResourceLayer> new_tab_button_;
  scoped_refptr<cc::slim::UIResourceLayer> new_tab_button_background_;
  scoped_refptr<cc::slim::UIResourceLayer> left_fade_;
  scoped_refptr<cc::slim::UIResourceLayer> right_fade_;

  // Layers covering the tab strip padding area, used as an visual extension of
  // fading.
  scoped_refptr<cc::slim::SolidColorLayer> left_padding_layer_;
  scoped_refptr<cc::slim::SolidColorLayer> right_padding_layer_;

  scoped_refptr<cc::slim::UIResourceLayer> model_selector_button_;
  scoped_refptr<cc::slim::UIResourceLayer> model_selector_button_background_;
  scoped_refptr<cc::slim::SolidColorLayer> scrim_layer_;

  unsigned write_index_ = 0;
  TabHandleLayerList tab_handle_layers_;
  unsigned group_write_index_ = 0;
  std::vector<scoped_refptr<cc::slim::SolidColorLayer>> group_title_layers_;
  std::vector<scoped_refptr<cc::slim::SolidColorLayer>> group_bottom_layers_;
  raw_ptr<SceneLayer> content_tree_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TAB_STRIP_SCENE_LAYER_H_
