// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TAB_LIST_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TAB_LIST_SCENE_LAYER_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "cc/slim/layer.h"
#include "cc/slim/ui_resource_layer.h"
#include "chrome/browser/android/compositor/layer/layer.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "chrome/browser/ui/android/layouts/scene_layer.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ui {
class ResourceManager;
}

namespace android {

class TabContentManager;
class TabLayer;

class TabListSceneLayer : public SceneLayer {
 public:
  TabListSceneLayer(JNIEnv* env, const base::android::JavaRef<jobject>& jobj);

  TabListSceneLayer(const TabListSceneLayer&) = delete;
  TabListSceneLayer& operator=(const TabListSceneLayer&) = delete;

  ~TabListSceneLayer() override;

  void BeginBuildingFrame(JNIEnv* env);
  void FinishBuildingFrame(JNIEnv* env);
  void UpdateLayer(JNIEnv* env,
                   int32_t background_color,
                   float viewport_x,
                   float viewport_y,
                   float viewport_width,
                   float viewport_height);
  // TODO(meiliang): This method needs another parameter, a resource that can be
  // used to indicate the currently selected tab for the TabLayer.
  // TODO(dtrainor): This method is ridiculous.  Break this apart?
  void PutTabLayer(JNIEnv* env,
                   int32_t id,
                   int32_t toolbar_resource_id,
                   int32_t shadow_resource_id,
                   int32_t contour_resource_id,
                   int32_t border_resource_id,
                   int32_t border_inner_shadow_resource_id,
                   bool can_use_live_layer,
                   int32_t tab_background_color,
                   bool incognito,
                   float x,
                   float y,
                   float width,
                   float height,
                   float content_width,
                   float visible_content_height,
                   float shadow_width,
                   float shadow_height,
                   float alpha,
                   float border_alpha,
                   float border_inner_shadow_alpha,
                   float contour_alpha,
                   float shadow_alpha,
                   float static_to_view_blend,
                   float border_scale,
                   float saturation,
                   bool show_toolbar,
                   int32_t default_theme_color,
                   int32_t toolbar_background_color,
                   bool anonymize_toolbar,
                   int32_t toolbar_textbox_resource_id,
                   int32_t toolbar_textbox_background_color,
                   float content_offset);

  void PutBackgroundLayer(JNIEnv* env,
                          int32_t resource_id,
                          float alpha,
                          int32_t top_offset);

  void SetDependencies(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jtab_content_manager,
      const base::android::JavaRef<jobject>& jresource_manager);

  void OnDetach() override;
  bool ShouldShowBackground() override;
  SkColor GetBackgroundColor() override;

 private:
  typedef base::flat_map<int, scoped_refptr<TabLayer>> TabMap;
  TabMap tab_map_;
  base::flat_set<int> visible_tabs_this_frame_;

  scoped_refptr<cc::slim::UIResourceLayer> background_layer_;

  bool content_obscures_self_;
  raw_ptr<ui::ResourceManager> resource_manager_;
  raw_ptr<TabContentManager> tab_content_manager_;
  SkColor background_color_;

  scoped_refptr<cc::slim::Layer> own_tree_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TAB_LIST_SCENE_LAYER_H_
