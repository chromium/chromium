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
class NinePatchLayer;
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
                    int32_t reorder_background_top_margin,
                    int32_t reorder_background_bottom_margin,
                    int32_t reorder_background_padding_short,
                    int32_t reorder_background_padding_long,
                    int32_t reorder_background_corner_radius,
                    float tab_underline_thickness,
                    float tab_underline_corner_radius,
                    float tab_underline_bottom_margin);

  void SetContentTree(JNIEnv* env,
                      const base::android::JavaRef<jobject>& jcontent_tree);

  void BeginBuildingFrame(
      JNIEnv* env,
      bool visible,
      const base::android::JavaRef<jobject>& jresource_manager,
      const base::android::JavaRef<jobject>& jlayer_title_cache);

  void FinishBuildingFrame(JNIEnv* env);

  void UpdateOffsetTag(JNIEnv* env,
                       const base::android::JavaRef<jobject>& joffset_tag);

  void UpdateTabStripLayer(JNIEnv* env,
                           int32_t width,
                           int32_t height,
                           float y_offset,
                           int32_t background_color,
                           int32_t scrim_color,
                           float scrim_opacity,
                           float left_padding,
                           float right_padding,
                           float top_padding,
                           float divider_y,
                           bool should_show_divider,
                           int32_t divider_resource_id,
                           int32_t divider_tint);

  void UpdateNewTabButton(JNIEnv* env,
                          int32_t resource_id,
                          int32_t bg_resource_id,
                          float x,
                          float y,
                          float touch_target_offset,
                          bool visible,
                          bool should_apply_hover_highlight,
                          int32_t tint,
                          int32_t background_tint,
                          float button_alpha,
                          bool is_keyboard_focused,
                          int32_t keyboard_focus_ring_resource_id,
                          int32_t keyboard_focus_ring_color);

  void UpdateGlicButton(JNIEnv* env,
                        int32_t resource_id,
                        float x,
                        float y,
                        float button_width,
                        float button_height,
                        bool visible,
                        bool should_apply_hover_highlight,
                        int32_t tint,
                        bool should_tint,
                        int32_t background_tint,
                        float button_alpha,
                        bool is_keyboard_focused,
                        int32_t keyboard_focus_ring_resource_id,
                        int32_t keyboard_focus_ring_color,
                        float keyboard_focus_ring_offset,
                        int32_t text_texture_id,
                        float button_start_padding,
                        float icon_text_padding,
                        float corner_radius_outer,
                        float corner_radius_inner,
                        int32_t dismiss_resource_id,
                        float dismiss_x,
                        float dismiss_y,
                        bool dismiss_visible,
                        int32_t dismiss_tint,
                        bool dismiss_is_keyboard_focused,
                        int32_t dismiss_keyboard_focus_ring_resource_id,
                        int32_t dismiss_keyboard_focus_ring_color);

  void UpdateGlicActorButton(JNIEnv* env,
                             int32_t resource_id,
                             float x,
                             float y,
                             float button_width,
                             float button_height,
                             bool visible,
                             bool should_apply_hover_highlight,
                             int32_t tint,
                             bool should_tint,
                             int32_t background_tint,
                             float button_alpha,
                             bool is_keyboard_focused,
                             int32_t keyboard_focus_ring_resource_id,
                             int32_t keyboard_focus_ring_color,
                             float keyboard_focus_ring_offset,
                             int32_t text_texture_id,
                             float button_start_padding,
                             float icon_text_padding,
                             float corner_radius_outer,
                             float corner_radius_inner);

  void UpdateModelSelectorButton(JNIEnv* env,
                                 int32_t resource_id,
                                 int32_t bg_resource_id,
                                 float x,
                                 float y,
                                 bool visible,
                                 bool should_apply_hover_highlight,
                                 int32_t tint,
                                 int32_t background_tint,
                                 float button_alpha,
                                 bool is_keyboard_focused,
                                 int32_t keyboard_focus_ring_resource_id,
                                 int32_t keyboard_focus_ring_color);

  void UpdateTabStripFade(JNIEnv* env,
                          bool is_left,
                          int32_t fade_color,
                          float opacity,
                          float gradient_width,
                          float opaque_width,
                          float padding);

  void PutStripTabLayer(JNIEnv* env,
                        int32_t id,
                        int32_t close_resource_id,
                        int32_t close_hover_bg_resource_id,
                        bool is_close_keyboard_focused,
                        int32_t close_keyboard_focus_ring_resource_id,
                        int32_t divider_resource_id,
                        int32_t handle_resource_id,
                        int32_t handle_outline_resource_id,
                        int32_t close_tint,
                        int32_t close_hover_bg_tint,
                        int32_t divider_tint,
                        int32_t handle_tint,
                        int32_t handle_outline_tint,
                        bool foreground,
                        bool shouldShowTabOutline,
                        bool close_pressed,
                        bool should_hide_favicon,
                        bool should_show_media_indicator,
                        int32_t media_indicator_resource_id,
                        int32_t media_indicator_tint,
                        float media_indicator_width,
                        float media_indicator_spacing,
                        float media_indicator_internal_padding,
                        float title_to_media_indicator_spacing,
                        int32_t tab_indicator_overlay_resource_id,
                        float tab_indicator_overlay_rotation,
                        float tab_indicator_overlay_width,
                        float toolbar_width,
                        float x,
                        float y,
                        float width,
                        float height,
                        float content_offset_y,
                        float divider_offset_x,
                        float bottom_margin,
                        float top_margin,
                        float close_button_padding,
                        float close_button_alpha,
                        float width_to_hide_tab_title,
                        bool is_start_divider_visible,
                        bool is_end_divider_visible,
                        bool is_loading,
                        float spinner_rotation,
                        float opacity,
                        bool is_keyboard_focused,
                        int32_t keyboard_focus_ring_resource_id,
                        int32_t keyboard_focus_ring_color,
                        int32_t keyboard_focus_ring_offset,
                        int32_t stroke_width,
                        float folio_foot_length,
                        bool is_pinned,
                        float pinned_icon_offset_x,
                        bool is_underlined,
                        int32_t underline_start_color,
                        int32_t underline_end_color,
                        int32_t underline_width_threshold);

  void PutGroupIndicatorLayer(
      JNIEnv* env,
      bool incognito,
      bool foreground,
      bool collapsed,
      bool show_bubble,
      const base::android::JavaRef<jobject>& jgroup_token,
      int32_t tint,
      int32_t reorder_background_tint,
      int32_t bubble_tint,
      float x,
      float y,
      float width,
      float height,
      float title_start_padding,
      float title_end_padding,
      float corner_radius,
      float bottom_indicator_width,
      float bottom_indicator_height,
      float bubble_padding,
      float bubble_size,
      bool is_keyboard_focused,
      int32_t keyboard_focus_ring_resource_id,
      int32_t keyboard_focus_ring_color,
      int32_t keyboard_focus_ring_offset,
      int32_t keyboard_focus_ring_width);

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

  void UpdateGlicButtonInternal(
      scoped_refptr<cc::slim::Layer> container_layer,
      scoped_refptr<cc::slim::SolidColorLayer> background_layer,
      scoped_refptr<cc::slim::UIResourceLayer> icon_layer,
      scoped_refptr<cc::slim::UIResourceLayer> text_layer,
      scoped_refptr<cc::slim::NinePatchLayer> focus_ring_layer,
      int32_t resource_id,
      float x,
      float y,
      float button_width,
      float button_height,
      bool visible,
      int32_t tint,
      bool should_tint,
      int32_t background_tint,
      float button_alpha,
      bool is_keyboard_focused,
      int32_t keyboard_focus_ring_resource_id,
      int32_t keyboard_focus_ring_color,
      float keyboard_focus_ring_offset,
      bool should_flip_keyboard_focus_ring,
      int32_t text_texture_id,
      float button_start_padding,
      float icon_text_padding,
      float corner_radius_outer,
      float corner_radius_inner);

  typedef std::vector<scoped_refptr<TabHandleLayer>> TabHandleLayerList;

  scoped_refptr<cc::slim::SolidColorLayer> background_layer_;
  scoped_refptr<cc::slim::SolidColorLayer> tab_strip_layer_;
  scoped_refptr<cc::slim::Layer> group_ui_parent_layer_;
  scoped_refptr<cc::slim::Layer> tab_ui_parent_layer_;
  scoped_refptr<cc::slim::Layer> foreground_layer_;
  scoped_refptr<cc::slim::Layer> foreground_tabs_;
  scoped_refptr<cc::slim::Layer> pinned_tabs_layer_;
  scoped_refptr<cc::slim::Layer> foreground_group_titles_;
  scoped_refptr<cc::slim::UIResourceLayer> new_tab_button_;
  scoped_refptr<cc::slim::UIResourceLayer> new_tab_button_background_;
  scoped_refptr<cc::slim::UIResourceLayer> new_tab_button_keyboard_focus_ring_;
  scoped_refptr<cc::slim::SolidColorLayer> left_fade_;
  scoped_refptr<cc::slim::SolidColorLayer> right_fade_;

  // Layers covering the tab strip padding area, used as an visual extension of
  // fading.
  scoped_refptr<cc::slim::SolidColorLayer> left_padding_layer_;
  scoped_refptr<cc::slim::SolidColorLayer> right_padding_layer_;
  scoped_refptr<cc::slim::UIResourceLayer> window_controls_divider_;

  scoped_refptr<cc::slim::Layer> glic_button_container_;
  scoped_refptr<cc::slim::UIResourceLayer> glic_button_;
  scoped_refptr<cc::slim::SolidColorLayer> glic_button_background_;
  scoped_refptr<cc::slim::UIResourceLayer> glic_button_text_;
  scoped_refptr<cc::slim::UIResourceLayer> glic_dismiss_nudge_button_;
  scoped_refptr<cc::slim::UIResourceLayer>
      glic_dismiss_nudge_button_keyboard_focus_ring_;
  scoped_refptr<cc::slim::NinePatchLayer> glic_button_keyboard_focus_ring_;

  scoped_refptr<cc::slim::Layer> glic_actor_button_container_;
  scoped_refptr<cc::slim::UIResourceLayer> glic_actor_button_;
  scoped_refptr<cc::slim::SolidColorLayer> glic_actor_button_background_;
  scoped_refptr<cc::slim::UIResourceLayer> glic_actor_button_text_;
  scoped_refptr<cc::slim::NinePatchLayer>
      glic_actor_button_keyboard_focus_ring_;

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
  raw_ptr<ui::ResourceManager> resource_manager_;
  raw_ptr<LayerTitleCache> layer_title_cache_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TAB_STRIP_SCENE_LAYER_H_
