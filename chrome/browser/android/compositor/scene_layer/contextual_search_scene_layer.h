// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_CONTEXTUAL_SEARCH_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_CONTEXTUAL_SEARCH_SCENE_LAYER_H_

#include <memory>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/ui/android/layouts/scene_layer.h"

class Profile;

namespace cc::slim {
class Layer;
class SolidColorLayer;
}  // namespace cc::slim

namespace content {
class WebContents;
}  // namespace content

namespace android {

class ContextualSearchLayer;

// A native-side, cc::slim::Layer-based representation of how a Contextual
// Search scene should be drawn. This class delegates to the
// ContextualSearchLayer that does the actual rendering of the Contextual Search
// Bar and content.
class ContextualSearchSceneLayer : public SceneLayer,
                                   public BitmapFetcherDelegate {
 public:
  ContextualSearchSceneLayer(JNIEnv* env,
                             const base::android::JavaRef<jobject>& jobj);

  ContextualSearchSceneLayer(const ContextualSearchSceneLayer&) = delete;
  ContextualSearchSceneLayer& operator=(const ContextualSearchSceneLayer&) =
      delete;

  ~ContextualSearchSceneLayer() override;

  void CreateContextualSearchLayer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jresource_manager);

  void UpdateContextualSearchLayer(
      JNIEnv* env,
      jint search_bar_background_resource_id,
      jint search_bar_background_color,
      jint search_context_resource_id,
      jint search_term_resource_id,
      jint search_caption_resource_id,
      jint search_bar_shadow_resource_id,
      jint search_provider_icon_resource_id,
      jint quick_action_icon_resource_id,
      jint drag_handlebar_resource_id,
      jint open_tab_icon_resource_id,
      jint close_icon_resource_id,
      jint progress_bar_background_resource_id,
      jint progress_bar_background_tint,
      jint progress_bar_resource_id,
      jint progress_bar_tint,
      jint search_promo_resource_id,
      jfloat dp_to_px,
      jfloat layout_width,
      jfloat layout_height,
      jfloat base_page_brightness,
      jfloat base_page_offset,
      content::WebContents* web_contents,
      jboolean search_promo_visible,
      jfloat search_promo_height,
      jfloat search_promo_opacity,
      jint search_promo_background_color,
      // Related Searches
      jint related_searches_in_bar_resource_id,
      jboolean related_searches_in_bar_visible,
      jfloat related_searches_in_bar_height,
      jfloat related_searches_in_bar_redundant_padding,
      // Panel position etc
      jfloat search_panel_x,
      jfloat search_panel_y,
      jfloat search_panel_width,
      jfloat search_panel_height,
      jfloat search_bar_margin_side,
      jfloat search_bar_margin_top,
      jfloat search_bar_margin_bottom,
      jfloat search_bar_height,
      jfloat search_context_opacity,
      jfloat search_text_layer_min_height,
      jfloat search_term_opacity,
      jfloat search_term_caption_spacing,
      jfloat search_caption_animation_percentage,
      jboolean search_caption_visible,
      jboolean search_bar_border_visible,
      jfloat search_bar_border_height,
      jboolean quick_action_icon_visible,
      jboolean thumbnail_visible,
      std::string& thumbnail_url,
      jfloat custom_image_visibility_percentage,
      jint bar_image_size,
      jint icon_color,
      jint drag_handlebar_color,
      jfloat close_icon_opacity,
      jboolean progress_bar_visible,
      jfloat progress_bar_height,
      jfloat progress_bar_opacity,
      jfloat progress_bar_completion,
      jboolean touch_highlight_visible,
      jfloat touch_highlight_x_offset,
      jfloat touch_highlight_width,
      Profile* profile,
      jint bar_background_resource_id,
      jint separator_line_color);

  // Inherited from BitmapFetcherDelegate
  void OnFetchComplete(
      const GURL& url,
      const SkBitmap* bitmap) override;

  void SetContentTree(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcontent_tree);

  void HideTree(JNIEnv* env);

 private:
  void FetchThumbnail(Profile* profile);

  raw_ptr<JNIEnv> env_;
  base::android::ScopedJavaGlobalRef<jobject> object_;
  std::string thumbnail_url_;
  std::unique_ptr<BitmapFetcher> fetcher_;

  scoped_refptr<ContextualSearchLayer> contextual_search_layer_;
  // Responsible for fading the base page content.
  scoped_refptr<cc::slim::SolidColorLayer> color_overlay_;
  scoped_refptr<cc::slim::Layer> content_container_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_CONTEXTUAL_SEARCH_SCENE_LAYER_H_
