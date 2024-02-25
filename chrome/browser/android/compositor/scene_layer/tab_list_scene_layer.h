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

  void BeginBuildingFrame(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& jobj);
  void FinishBuildingFrame(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& jobj);
  void UpdateLayer(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& jobj,
                   jint background_color,
                   jfloat viewport_x,
                   jfloat viewport_y,
                   jfloat viewport_width,
                   jfloat viewport_height);
  // TODO(meiliang): This method needs another parameter, a resource that can be
  // used to indicate the currently selected tab for the TabLayer.
  // TODO(dtrainor): This method is ridiculous.  Break this apart?
  void PutTabLayer(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& jobj,
                   jint id,
                   jint toolbar_resource_id,
                   jint shadow_resource_id,
                   jint contour_resource_id,
                   jint border_resource_id,
                   jint border_inner_shadow_resource_id,
                   jboolean can_use_live_layer,
                   jint tab_background_color,
                   jboolean incognito,
                   jfloat x,
                   jfloat y,
                   jfloat width,
                   jfloat height,
                   jfloat content_width,
                   jfloat visible_content_height,
                   jfloat shadow_width,
                   jfloat shadow_height,
                   jfloat alpha,
                   jfloat border_alpha,
                   jfloat border_inner_shadow_alpha,
                   jfloat contour_alpha,
                   jfloat shadow_alpha,
                   jfloat static_to_view_blend,
                   jfloat border_scale,
                   jfloat saturation,
                   jboolean show_toolbar,
                   jint default_theme_color,
                   jint toolbar_background_color,
                   jboolean anonymize_toolbar,
                   jint toolbar_textbox_resource_id,
                   jint toolbar_textbox_background_color,
                   jfloat content_offset);

  void PutBackgroundLayer(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& jobj,
                          jint resource_id,
                          jfloat alpha,
                          jint top_offset);

  void SetDependencies(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      const base::android::JavaParamRef<jobject>& jtab_content_manager,
      const base::android::JavaParamRef<jobject>& jresource_manager);

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
