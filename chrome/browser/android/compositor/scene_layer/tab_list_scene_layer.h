// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TAB_LIST_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TAB_LIST_SCENE_LAYER_H_

#include <map>
#include <memory>
#include <set>
#include <unordered_set>

#include "base/macros.h"
#include "cc/layers/layer.h"
#include "cc/layers/ui_resource_layer.h"
#include "chrome/browser/android/compositor/layer/layer.h"
#include "chrome/browser/android/compositor/scene_layer/scene_layer.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ui {
class ResourceManager;
}

namespace android {

class LayerTitleCache;
class TabContentManager;
class TabLayer;

class TabListSceneLayer : public SceneLayer {
 public:
  TabListSceneLayer(JNIEnv* env, const base::android::JavaRef<jobject>& jobj);
  ~TabListSceneLayer() override;

  void BeginBuildingFrame(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& jobj);
  void FinishBuildingFrame(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& jobj);
  void UpdateLayer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jint background_color,
      jfloat viewport_x,
      jfloat viewport_y,
      jfloat viewport_width,
      jfloat viewport_height,
      const base::android::JavaParamRef<jobject>& jlayer_title_cache,
      const base::android::JavaParamRef<jobject>& jtab_content_manager,
      const base::android::JavaParamRef<jobject>& jresource_manager);
  // TODO(meiliang): This method needs another parameter, a resource that can be
  // used to indicate the currently selected tab for the TabLayer.
  // TODO(dtrainor): This method is ridiculous.  Break this apart?
  void PutTabLayer(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& jobj,
                   jint id,
                   const base::android::JavaRef<jintArray>& tab_ids_list,
                   jboolean use_tab_ids_list,
                   jint toolbar_resource_id,
                   jint close_button_resource_id,
                   jint shadow_resource_id,
                   jint contour_resource_id,
                   jint back_logo_resource_id,
                   jint border_resource_id,
                   jint border_inner_shadow_resource_id,
                   jboolean can_use_live_layer,
                   jint tab_background_color,
                   jint back_logo_color,
                   jboolean incognito,
                   jboolean close_button_on_right,
                   jfloat x,
                   jfloat y,
                   jfloat width,
                   jfloat height,
                   jfloat content_width,
                   jfloat content_height,
                   jfloat visible_content_height,
                   jfloat shadow_x,
                   jfloat shadow_y,
                   jfloat shadow_width,
                   jfloat shadow_height,
                   jfloat pivot_x,
                   jfloat pivot_y,
                   jfloat rotation_x,
                   jfloat rotation_y,
                   jfloat alpha,
                   jfloat border_alpha,
                   jfloat border_inner_shadow_alpha,
                   jfloat contour_alpha,
                   jfloat shadow_alpha,
                   jfloat close_alpha,
                   jfloat close_btn_width,
                   jfloat close_btn_asset_size,
                   jfloat static_to_view_blend,
                   jfloat border_scale,
                   jfloat saturation,
                   jfloat brightness,
                   jboolean show_toolbar,
                   jint default_theme_color,
                   jint toolbar_background_color,
                   jint close_button_color,
                   jboolean anonymize_toolbar,
                   jboolean show_tab_title,
                   jint toolbar_textbox_resource_id,
                   jint toolbar_textbox_background_color,
                   jfloat toolbar_textbox_alpha,
                   jfloat toolbar_alpha,
                   jfloat toolbar_y_offset,
                   jfloat side_border_scale,
                   jboolean inset_border);

  void PutBackgroundLayer(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& jobj,
                          jint resource_id,
                          jfloat alpha);

  void OnDetach() override;
  bool ShouldShowBackground() override;
  SkColor GetBackgroundColor() override;

 private:
  // The set of tint colors that were used for a frame.
  std::unordered_set<int> used_tints_;

  typedef std::map<int, scoped_refptr<TabLayer>> TabMap;
  TabMap tab_map_;
  std::set<int> visible_tabs_this_frame_;

  scoped_refptr<cc::UIResourceLayer> background_layer_;

  bool content_obscures_self_;
  ui::ResourceManager* resource_manager_;
  LayerTitleCache* layer_title_cache_;
  TabContentManager* tab_content_manager_;
  SkColor background_color_;

  scoped_refptr<cc::Layer> own_tree_;

  DISALLOW_COPY_AND_ASSIGN(TabListSceneLayer);
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TAB_LIST_SCENE_LAYER_H_
