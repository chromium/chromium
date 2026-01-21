// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TOP_TOOLBAR_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TOP_TOOLBAR_SCENE_LAYER_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/android/layouts/scene_layer.h"
#include "ui/android/resources/resource_manager_impl.h"

namespace cc::slim {
class Layer;
}

namespace android {

class ToolbarLayer;

class TopToolbarSceneLayer : public SceneLayer {
 public:
  TopToolbarSceneLayer(JNIEnv* env,
                       const base::android::JavaRef<jobject>& jobj);

  TopToolbarSceneLayer(const TopToolbarSceneLayer&) = delete;
  TopToolbarSceneLayer& operator=(const TopToolbarSceneLayer&) = delete;

  ~TopToolbarSceneLayer() override;

  // Update the compositor version of the toolbar.
  void UpdateToolbarLayer(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jresource_manager,
      int32_t toolbar_resource_id,
      int32_t toolbar_background_color,
      int32_t url_bar_resource_id,
      int32_t url_bar_color,
      float x_offset,
      float y_offset,
      float legacy_content_offset,
      bool show_shadow,
      bool visible,
      bool anonymize,
      const base::android::JavaRef<jobject>& joffset_tag);

  // Update the progress bar.
  void UpdateProgressBar(JNIEnv* env,
                         int32_t progress_bar_x,
                         int32_t progress_bar_y,
                         int32_t progress_bar_width,
                         int32_t progress_bar_height,
                         int32_t progress_bar_color,
                         int32_t progress_bar_background_x,
                         int32_t progress_bar_background_y,
                         int32_t progress_bar_background_width,
                         int32_t progress_bar_background_height,
                         int32_t progress_bar_background_color,
                         int32_t progress_bar_static_background_x,
                         int32_t progress_bar_static_background_width,
                         int32_t progress_bar_static_background_color,
                         float corner_radius,
                         bool progress_bar_visual_update_available,
                         bool visible);

  void SetContentTree(JNIEnv* env,
                      const base::android::JavaRef<jobject>& jcontent_tree);

  SkColor GetBackgroundColor() override;

  bool ShouldShowBackground() override;

 private:
  bool should_show_background_;
  SkColor background_color_;
  scoped_refptr<cc::slim::Layer> content_container_;
  scoped_refptr<ToolbarLayer> toolbar_layer_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TOP_TOOLBAR_SCENE_LAYER_H_
