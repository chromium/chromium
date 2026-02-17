// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/contextual_search_scene_layer.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "cc/slim/layer.h"
#include "cc/slim/solid_color_layer.h"
#include "chrome/browser/android/compositor/layer/contextual_search_layer.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "ui/android/resources/resource_manager_impl.h"
#include "ui/android/view_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/size_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ContextualSearchSceneLayer_jni.h"

using base::android::JavaRef;

namespace android {

ContextualSearchSceneLayer::ContextualSearchSceneLayer(
    JNIEnv* env,
    const JavaRef<jobject>& jobj)
    : SceneLayer(env, jobj),
      env_(env),
      object_(jobj),
      color_overlay_(cc::slim::SolidColorLayer::Create()),
      content_container_(cc::slim::Layer::Create()) {
  // Responsible for moving the base page without modifying the layer itself.
  content_container_->SetIsDrawable(true);
  content_container_->SetPosition(gfx::PointF(0.0f, 0.0f));
  layer()->AddChild(content_container_);

  color_overlay_->SetIsDrawable(true);
  color_overlay_->SetOpacity(0.0f);
  color_overlay_->SetBackgroundColor(SkColors::kBlack);
  color_overlay_->SetPosition(gfx::PointF(0.f, 0.f));
  layer()->AddChild(color_overlay_);
}

void ContextualSearchSceneLayer::CreateContextualSearchLayer(
    JNIEnv* env,
    const JavaRef<jobject>& jresource_manager) {
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  contextual_search_layer_ = ContextualSearchLayer::Create(resource_manager);

  // The Contextual Search layer is initially invisible.
  contextual_search_layer_->layer()->SetHideLayerAndSubtree(true);

  layer()->AddChild(contextual_search_layer_->layer());
}

ContextualSearchSceneLayer::~ContextualSearchSceneLayer() = default;

void ContextualSearchSceneLayer::UpdateContextualSearchLayer(
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
    int32_t rounded_bar_top_resource_id,
    int32_t separator_line_color,
    int32_t callout_resource_id,
    float callout_opacity) {
  // Load the thumbnail if necessary.
  if (thumbnail_url != thumbnail_url_) {
    thumbnail_url_ = thumbnail_url;
    FetchThumbnail(profile);
  }

  // NOTE(pedrosimonetti): The WebContents might not exist at this time if
  // the Contextual Search Result has not been requested yet. In this case,
  // we'll pass NULL to Contextual Search's Layer Tree.
  scoped_refptr<cc::slim::Layer> content_layer =
      web_contents ? web_contents->GetNativeView()->GetLayer() : nullptr;

  // Fade the base page out.
  color_overlay_->SetOpacity(1.f - base_page_brightness);
  color_overlay_->SetBounds(
      gfx::ToCeiledSize(gfx::SizeF(layout_width, layout_height)));

  // Move the base page contents up.
  content_container_->SetPosition(gfx::PointF(0.0f, base_page_offset));

  contextual_search_layer_->SetProperties(
      search_bar_background_resource_id, search_bar_background_color,
      search_context_resource_id, search_term_resource_id,
      search_caption_resource_id, search_bar_shadow_resource_id,
      search_provider_icon_resource_id, quick_action_icon_resource_id,
      drag_handlebar_resource_id, open_tab_icon_resource_id,
      close_icon_resource_id, progress_bar_background_resource_id,
      progress_bar_background_tint, progress_bar_resource_id, progress_bar_tint,
      search_promo_resource_id, dp_to_px, content_layer, search_promo_visible,
      search_promo_height, search_promo_opacity, search_promo_background_color,
      // Related Searches
      related_searches_in_bar_resource_id, related_searches_in_bar_visible,
      related_searches_in_bar_height, related_searches_in_bar_redundant_padding,
      // Panel position etc
      search_panel_x, search_panel_y, search_panel_width, search_panel_height,
      search_bar_margin_side, search_bar_margin_top, search_bar_margin_bottom,
      search_bar_height, search_context_opacity, search_text_layer_min_height,
      search_term_opacity, search_term_caption_spacing,
      search_caption_animation_percentage, search_caption_visible,
      search_bar_border_visible, search_bar_border_height,
      quick_action_icon_visible, thumbnail_visible,
      custom_image_visibility_percentage, bar_image_size, icon_color,
      drag_handlebar_color, close_icon_opacity, progress_bar_visible,
      progress_bar_height, progress_bar_opacity, progress_bar_completion,
      touch_highlight_visible, touch_highlight_x_offset, touch_highlight_width,
      rounded_bar_top_resource_id, separator_line_color, callout_resource_id,
      callout_opacity);

  // Make the layer visible if it is not already.
  contextual_search_layer_->layer()->SetHideLayerAndSubtree(false);
}

void ContextualSearchSceneLayer::FetchThumbnail(Profile* profile) {
  if (thumbnail_url_.empty())
    return;

  GURL gurl(thumbnail_url_);
  // Semantic details for this "Thumbnail" request.
  // The URLs processed access gstatic.com, which is considered a Google-owned
  // service.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("contextual_search_thumbnail",
                                          R"(
            semantics {
              sender: "Contextual Search"
              description:
                "This request is for a thumbnail image to show in the "
                "Contextual Search bottom sheet for an entity or similar "
                "object identified by the selected text."
              trigger:
                "Triggered by a server response to the "
                "contextual_search_resolve request which contains a thumbnail "
                "URL."
              data:
                "The URL of the thumbnail."
              destination: GOOGLE_OWNED_SERVICE
            }
            policy {
              cookies_allowed: NO
              setting:
                "This feature can be disabled by turning off 'Touch to Search' "
                "in Chrome for Android settings."
              chrome_policy {
                ContextualSearchEnabled {
                    policy_options {mode: MANDATORY}
                    ContextualSearchEnabled: false
                }
              }
            })");
  network::mojom::URLLoaderFactory* loader_factory =
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess()
          .get();
  fetcher_ = std::make_unique<BitmapFetcher>(gurl, this, traffic_annotation);
  fetcher_->Init(
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
      network::mojom::CredentialsMode::kOmit);
  fetcher_->Start(loader_factory);
}

void ContextualSearchSceneLayer::OnFetchComplete(const GURL& url,
                                                 const SkBitmap* bitmap) {
  bool success = bitmap && !bitmap->drawsNothing();
  Java_ContextualSearchSceneLayer_onThumbnailFetched(env_, object_, success);
  if (success)
    contextual_search_layer_->SetThumbnail(bitmap);

  fetcher_.reset();
}

void ContextualSearchSceneLayer::SetContentTree(
    JNIEnv* env,
    const JavaRef<jobject>& jcontent_tree) {
  SceneLayer* content_tree = FromJavaObject(env, jcontent_tree);
  if (!content_tree || !content_tree->layer())
    return;

  if (!content_tree->layer()->parent() ||
      (content_tree->layer()->parent()->id() != content_container_->id())) {
    content_container_->AddChild(content_tree->layer());
  }
}

void ContextualSearchSceneLayer::HideTree(JNIEnv* env) {
  // TODO(mdjones): Create super class for this logic.
  if (contextual_search_layer_) {
    contextual_search_layer_->layer()->SetHideLayerAndSubtree(true);
  }
  // Reset base page brightness.
  color_overlay_->SetOpacity(0.f);
  // Reset base page offset.
  content_container_->SetPosition(gfx::PointF(0.0f, 0.0f));
}

static int64_t JNI_ContextualSearchSceneLayer_Init(
    JNIEnv* env,
    const JavaRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  ContextualSearchSceneLayer* tree_provider =
      new ContextualSearchSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(tree_provider);
}

}  // namespace android

DEFINE_JNI(ContextualSearchSceneLayer)
