// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TAB_STRIP_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TAB_STRIP_SCENE_LAYER_H_

#include <memory>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "cc/input/android/offset_tag_android.h"
#include "chrome/browser/ui/android/layouts/scene_layer.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/android/resources/resource_manager.h"

namespace cc::slim {
class Layer;
class SolidColorLayer;
class UIResourceLayer;
}  // namespace cc::slim

namespace android {

class GroupIndicatorLayer;
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

  void SetConstants(JNIEnv* env,
                    jint reorder_background_top_margin,
                    jint reorder_background_bottom_margin,
                    jint reorder_background_padding_short,
                    jint reorder_background_padding_long,
                    jint reorder_background_corner_radius);

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
      jfloat touch_target_offset,
      jboolean visible,
      jboolean should_apply_hover_highlight,
      jint tint,
      jint background_tint,
      jfloat button_alpha,
      jboolean is_keyboard_focused,
      jint keyboard_focus_ring_resource_id,
      jint keyboard_focus_ring_color,
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
      jboolean is_keyboard_focused,
      jint keyboard_focus_ring_resource_id,
      jint keyboard_focus_ring_color,
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
      jboolean is_close_keyboard_focused,
      jint close_keyboard_focus_ring_resource_id,
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
      jboolean is_keyboard_focused,
      jint keyboard_focus_ring_resource_id,
      jint keyboard_focus_ring_color,
      jint keyboard_focus_ring_offset,
      jint stroke_width,
      jfloat folio_foot_length,
      const base::android::JavaParamRef<jobject>& jlayer_title_cache,
      const base::android::JavaParamRef<jobject>& jresource_manager);

  void PutGroupIndicatorLayer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jboolean incognito,
      jboolean foreground,
      jboolean collapsed,
      jboolean show_bubble,
      const base::android::JavaParamRef<jobject>& jgroup_token,
      jint tint,
      jint reorder_background_tint,
      jint bubble_tint,
      jfloat x,
      jfloat y,
      jfloat width,
      jfloat height,
      jfloat title_start_padding,
      jfloat title_end_padding,
      jfloat corner_radius,
      jfloat bottom_indicator_width,
      jfloat bottom_indicator_height,
      jfloat bubble_padding,
      jfloat bubble_size,
      jboolean is_keyboard_focused,
      jint keyboard_focus_ring_resource_id,
      jint keyboard_focus_ring_color,
      jint keyboard_focus_ring_offset,
      jint keyboard_focus_ring_width,
      const base::android::JavaParamRef<jobject>& jlayer_title_cache,
      const base::android::JavaParamRef<jobject>& jresource_manager);

  bool ShouldShowBackground() override;
  SkColor GetBackgroundColor() override;

 private:
  scoped_refptr<TabHandleLayer> GetNextTabLayer(
      LayerTitleCache* layer_title_cache);

  scoped_refptr<GroupIndicatorLayer> GetNextGroupIndicatorLayer(
      LayerTitleCache* layer_title_cache);

  void UpdateCompositorButton(
      scoped_refptr<cc::slim::UIResourceLayer> button,
      scoped_refptr<cc::slim::UIResourceLayer> background,
      ui::Resource* button_resource,
      ui::Resource* background_resource,
      float x,
      float y,
      bool visible,
      bool should_apply_hover_highlight,
      float button_alpha,
      scoped_refptr<cc::slim::UIResourceLayer> keyboard_focus_ring_layer,
      bool is_keyboard_focused,
      ui::Resource* keyboard_focus_ring_drawable);

  typedef std::vector<scoped_refptr<TabHandleLayer>> TabHandleLayerList;

  scoped_refptr<cc::slim::SolidColorLayer> background_layer_;
  scoped_refptr<cc::slim::SolidColorLayer> tab_strip_layer_;
  scoped_refptr<cc::slim::Layer> group_ui_parent_layer_;
  scoped_refptr<cc::slim::Layer> tab_ui_parent_layer_;
  scoped_refptr<cc::slim::Layer> foreground_layer_;
  scoped_refptr<cc::slim::Layer> foreground_tabs_;
  scoped_refptr<cc::slim::Layer> foreground_group_titles_;
  scoped_refptr<cc::slim::UIResourceLayer> new_tab_button_;
  scoped_refptr<cc::slim::UIResourceLayer> new_tab_button_background_;
  scoped_refptr<cc::slim::UIResourceLayer> new_tab_button_keyboard_focus_ring_;
  scoped_refptr<cc::slim::UIResourceLayer> left_fade_;
  scoped_refptr<cc::slim::UIResourceLayer> right_fade_;

  // Layers covering the tab strip padding area, used as an visual extension of
  // fading.
  scoped_refptr<cc::slim::SolidColorLayer> left_padding_layer_;
  scoped_refptr<cc::slim::SolidColorLayer> right_padding_layer_;

  scoped_refptr<cc::slim::UIResourceLayer> model_selector_button_;
  scoped_refptr<cc::slim::UIResourceLayer> model_selector_button_background_;
  scoped_refptr<cc::slim::UIResourceLayer>
      model_selector_button_keyboard_focus_ring_;
  scoped_refptr<cc::slim::SolidColorLayer> scrim_layer_;

  unsigned write_index_ = 0;
  TabHandleLayerList tab_handle_layers_;
  unsigned group_write_index_ = 0;
  std::vector<scoped_refptr<GroupIndicatorLayer>> group_title_layers_;
  raw_ptr<SceneLayer> content_tree_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TAB_STRIP_SCENE_LAYER_H_
