// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_EPHEMERAL_TAB_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_EPHEMERAL_TAB_SCENE_LAYER_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/android/compositor/scene_layer/scene_layer.h"

namespace cc {
class Layer;
class SolidColorLayer;
}  // namespace cc

namespace android {

class EphemeralTabLayer;

class EphemeralTabSceneLayer : public SceneLayer {
 public:
  EphemeralTabSceneLayer(JNIEnv* env,
                         const base::android::JavaRef<jobject>& jobj);
  ~EphemeralTabSceneLayer() override;

  void CreateEphemeralTabLayer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      const base::android::JavaParamRef<jobject>& jresource_manager,
      const base::android::JavaParamRef<jobject>& jfavicon_callback);

  void SetResourceIds(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& object,
                      jint text_resource_id,
                      jint panel_shadow_resource_id,
                      jint rounded_bar_top_resource_id,
                      jint bar_shadow_resource_id,
                      jint panel_icon_resource_id,
                      jint drag_handlebar_resource_id,
                      jint open_tab_icon_resource_id,
                      jint close_icon_resource_id);

  void Update(JNIEnv* env,
              const base::android::JavaParamRef<jobject>& object,
              jint title_view_resource_id,
              jint caption_view_resource_id,
              jint caption_icon_resource_id,
              jfloat caption_icon_opacity,
              jfloat caption_animation_percentage,
              jfloat text_layer_min_height,
              jfloat term_caption_spacing,
              jboolean caption_visible,
              jint progress_bar_background_resource_id,
              jint progress_bar_resource_id,
              jfloat dp_to_px,
              jfloat base_page_brightness,
              jfloat base_page_offset,
              const base::android::JavaParamRef<jobject>& jcontent_view_core,
              jfloat panel_X,
              jfloat panel_y,
              jfloat panel_width,
              jfloat panel_height,
              jint bar_background_color,
              jfloat bar_margin_side,
              jfloat bar_margin_top,
              jfloat bar_height,
              jboolean bar_border_visible,
              jfloat bar_border_height,
              jint icon_color,
              jint drag_handlebar_color,
              jfloat favicon_opacity,
              jboolean progress_bar_visible,
              jfloat progress_bar_height,
              jfloat progress_bar_opacity,
              jfloat progress_bar_completion,
              jint separator_line_color);

  void SetContentTree(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      const base::android::JavaParamRef<jobject>& jcontent_tree);

  void HideTree(JNIEnv* env, const base::android::JavaParamRef<jobject>& jobj);

 private:
  float base_page_brightness_;
  scoped_refptr<cc::Layer> content_layer_;
  scoped_refptr<EphemeralTabLayer> ephemeral_tab_layer_;
  scoped_refptr<cc::SolidColorLayer> color_overlay_;
  scoped_refptr<cc::Layer> content_container_;
  bool is_new_layout_;
  DISALLOW_COPY_AND_ASSIGN(EphemeralTabSceneLayer);
};

bool RegisterEphemeralTabSceneLayer(JNIEnv* env);
}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_EPHEMERAL_TAB_SCENE_LAYER_H_
