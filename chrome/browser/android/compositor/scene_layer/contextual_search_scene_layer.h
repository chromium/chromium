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
      const base::android::JavaRef<jobject>& jresource_manager);

  void UpdateContextualSearchLayer(
      JNIEnv* env,
      int32_t search_bar_background_resource_id,
      int32_t search_bar_background_color,
      int32_t search_context_resource_id,
      int32_t search_term_resource_id,
      int32_t search_caption_resource_id,
      int32_t search_bar_shadow_resource_id,
      int32_t search_provider_icon_resource_id,
      int32_t quick_action_icon_resource_id,
      int32_t drag_handlebar_resource_id,
      int32_t open_tab_icon_resource_id,
      int32_t close_icon_resource_id,
      int32_t progress_bar_background_resource_id,
      int32_t progress_bar_background_tint,
      int32_t progress_bar_resource_id,
      int32_t progress_bar_tint,
      int32_t search_promo_resource_id,
      float dp_to_px,
      float layout_width,
      float layout_height,
      float base_page_brightness,
      float base_page_offset,
      content::WebContents* web_contents,
      bool search_promo_visible,
      float search_promo_height,
      float search_promo_opacity,
      int32_t search_promo_background_color,
      // Related Searches
      int32_t related_searches_in_bar_resource_id,
      bool related_searches_in_bar_visible,
      float related_searches_in_bar_height,
      float related_searches_in_bar_redundant_padding,
      // Panel position etc
      float search_panel_x,
      float search_panel_y,
      float search_panel_width,
      float search_panel_height,
      float search_bar_margin_side,
      float search_bar_margin_top,
      float search_bar_margin_bottom,
      float search_bar_height,
      float search_context_opacity,
      float search_text_layer_min_height,
      float search_term_opacity,
      float search_term_caption_spacing,
      float search_caption_animation_percentage,
      bool search_caption_visible,
      bool search_bar_border_visible,
      float search_bar_border_height,
      bool quick_action_icon_visible,
      bool thumbnail_visible,
      const std::string& thumbnail_url,
      float custom_image_visibility_percentage,
      int32_t bar_image_size,
      int32_t icon_color,
      int32_t drag_handlebar_color,
      float close_icon_opacity,
      bool progress_bar_visible,
      float progress_bar_height,
      float progress_bar_opacity,
      float progress_bar_completion,
      bool touch_highlight_visible,
      float touch_highlight_x_offset,
      float touch_highlight_width,
      Profile* profile,
      int32_t bar_background_resource_id,
      int32_t separator_line_color,
      int32_t callout_resource_id,
      float callout_opacity);

  // Inherited from BitmapFetcherDelegate
  void OnFetchComplete(
      const GURL& url,
      const SkBitmap* bitmap) override;

  void SetContentTree(JNIEnv* env,
                      const base::android::JavaRef<jobject>& jcontent_tree);

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
