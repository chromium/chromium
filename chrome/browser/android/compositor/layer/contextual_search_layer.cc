// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/contextual_search_layer.h"

#include "cc/resources/scoped_ui_resource.h"
#include "cc/slim/layer.h"
#include "cc/slim/nine_patch_layer.h"
#include "cc/slim/solid_color_layer.h"
#include "cc/slim/ui_resource_layer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/resources/nine_patch_resource.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/base/l10n/l10n_util_android.h"
#include "ui/gfx/color_utils.h"

namespace {

const SkColor kSearchBackgroundColor = SkColorSetRGB(0xee, 0xee, 0xee);
const SkColor kTouchHighlightColor = SkColorSetARGB(0x33, 0x99, 0x99, 0x99);

}  // namespace

namespace android {

// static
scoped_refptr<ContextualSearchLayer> ContextualSearchLayer::Create(
    ui::ResourceManager* resource_manager) {
  return base::WrapRefCounted(new ContextualSearchLayer(resource_manager));
}

void ContextualSearchLayer::SetProperties(
    int panel_shadow_resource_id,
    int search_bar_background_color,
    int search_context_resource_id,
    int search_term_resource_id,
    int search_caption_resource_id,
    int search_bar_shadow_resource_id,
    int search_provider_icon_resource_id,
    int quick_action_icon_resource_id,
    int drag_handlebar_resource_id,
    int open_tab_icon_resource_id,
    int close_icon_resource_id,
    int progress_bar_background_resource_id,
    int progress_bar_background_tint,
    int progress_bar_resource_id,
    int progress_bar_tint,
    int search_promo_resource_id,
    float dp_to_px,
    const scoped_refptr<cc::slim::Layer>& content_layer,
    bool search_promo_visible,
    float search_promo_height,
    float search_promo_opacity,
    int search_promo_background_color,
    // Related Searches
    int related_searches_in_bar_resource_id,
    bool related_searches_in_bar_visible,
    float related_searches_in_bar_height,
    float related_searches_in_bar_redundant_padding,
    // Position etc
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
    float custom_image_visibility_percentage,
    int bar_image_size,
    int icon_color,
    int drag_handlebar_color,
    float close_icon_opacity,
    bool progress_bar_visible,
    float progress_bar_height,
    float progress_bar_opacity,
    float progress_bar_completion,
    bool touch_highlight_visible,
    float touch_highlight_x_offset,
    float touch_highlight_width,
    int rounded_bar_top_resource_id,
    int separator_line_color) {
  // Round values to avoid pixel gap between layers.
  search_bar_height = floor(search_bar_height);

  float search_bar_top = 0.f;
  float search_bar_bottom = search_bar_top + search_bar_height;
  bool should_render_progress_bar =
      progress_bar_visible && progress_bar_opacity > 0.f;

  OverlayPanelLayer::SetResourceIds(
      search_term_resource_id, panel_shadow_resource_id,
      rounded_bar_top_resource_id, search_bar_shadow_resource_id,
      search_provider_icon_resource_id, drag_handlebar_resource_id,
      open_tab_icon_resource_id, close_icon_resource_id);

  //  TODO(donnd): Update when moving Related Searches.
  float content_view_top = search_bar_bottom + search_promo_height;
  float should_render_bar_border = search_bar_border_visible
      && !should_render_progress_bar;

  // -----------------------------------------------------------------
  // Overlay Panel
  // -----------------------------------------------------------------
  OverlayPanelLayer::SetProperties(
      dp_to_px, content_layer, content_view_top, search_panel_x, search_panel_y,
      search_panel_width, search_panel_height, search_bar_background_color,
      search_bar_margin_side, search_bar_margin_top, search_bar_margin_bottom,
      search_bar_height, search_bar_top, search_term_opacity,
      should_render_bar_border, search_bar_border_height, icon_color,
      drag_handlebar_color, close_icon_opacity, separator_line_color,
      related_searches_in_bar_height);

  // -----------------------------------------------------------------
  // Content setup, to center in space below drag handle.
  // -----------------------------------------------------------------
  int content_height = search_bar_height - search_bar_margin_top -
                       related_searches_in_bar_height -
                       search_bar_margin_bottom;
  int content_top = search_bar_top + search_bar_margin_top;

  // ---------------------------------------------------------------------------
  // Search Term, Context and Search Caption
  // ---------------------------------------------------------------------------
  int text_layer_height =
      SetupTextLayer(content_top, content_height, search_text_layer_min_height,
                     search_caption_resource_id, search_caption_visible,
                     search_caption_animation_percentage, search_term_opacity,
                     search_context_resource_id, search_context_opacity,
                     search_term_caption_spacing);

  // Tracks the top of the next section to draw.
  int next_section_top = search_bar_bottom;

  // ---------------------------------------------------------------------------
  // Related Searches In-Bar Control
  // ---------------------------------------------------------------------------
  if (related_searches_in_bar_visible) {
    // Grabs the Related Searches in-bar resource.
    ui::Resource* related_searches_resource = resource_manager_->GetResource(
        ui::ANDROID_RESOURCE_TYPE_DYNAMIC, related_searches_in_bar_resource_id);
    DCHECK(related_searches_resource);
    if (related_searches_resource) {
      gfx::Size related_searches_size(
          search_panel_width, related_searches_resource->size().height());
      if (related_searches_in_bar_->parent() != layer_) {
        layer_->AddChild(related_searches_in_bar_);
      }
      related_searches_in_bar_->SetUIResourceId(
          related_searches_resource->ui_resource()->id());
      related_searches_in_bar_->SetBounds(related_searches_size);
      int related_searches_top =
          search_bar_bottom - related_searches_in_bar_height -
          related_searches_in_bar_redundant_padding - search_bar_margin_bottom;
      related_searches_in_bar_->SetPosition(
          gfx::PointF(0.f, related_searches_top));
    }
  } else if (related_searches_in_bar_.get() &&
             related_searches_in_bar_->parent()) {
    related_searches_in_bar_->RemoveFromParent();
  }

  // ---------------------------------------------------------------------------
  // Search Promo
  // ---------------------------------------------------------------------------
  if (search_promo_visible) {
    // Grabs the Search Opt Out Promo resource.
    ui::Resource* search_promo_resource = resource_manager_->GetResource(
        ui::ANDROID_RESOURCE_TYPE_DYNAMIC, search_promo_resource_id);
    // Search Promo Container
    if (search_promo_container_->parent() != layer_) {
      // NOTE(donnd): This layer can appear just below the Bar so it should be
      // always placed before the Search Bar Shadow to make sure it won't
      // occlude the shadow. Since layer 0 is the shadow for the sheet itself,
      // this needs to be layer 1.
      layer_->InsertChild(search_promo_container_, 1);
    }

    if (search_promo_resource) {
      int search_promo_content_height = search_promo_resource->size().height();
      gfx::Size search_promo_size(search_panel_width, search_promo_height);
      search_promo_container_->SetBounds(search_promo_size);
      search_promo_container_->SetPosition(gfx::PointF(0.f, next_section_top));
      search_promo_container_->SetMasksToBounds(true);
      // TODO(crbug.com/40219248): Remove FromColor and make all SkColor4f.
      search_promo_container_->SetBackgroundColor(
          SkColor4f::FromColor(search_promo_background_color));

      // Search Promo
      if (search_promo_->parent() != search_promo_container_)
        search_promo_container_->AddChild(search_promo_);

      search_promo_->SetUIResourceId(
          search_promo_resource->ui_resource()->id());
      search_promo_->SetBounds(search_promo_resource->size());
      // Align promo at the bottom of the container so the confirmation button
      // is not clipped when resizing the promo.
      search_promo_->SetPosition(
          gfx::PointF(0.f, search_promo_height - search_promo_content_height));
      search_promo_->SetOpacity(search_promo_opacity);
      // Next section goes beyond this section.
      next_section_top += search_promo_content_height;
    }
  } else {
    // Search Promo Container
    if (search_promo_container_.get() && search_promo_container_->parent())
      search_promo_container_->RemoveFromParent();
  }

  // ---------------------------------------------------------------------------
  // Progress Bar
  // ---------------------------------------------------------------------------
  OverlayPanelLayer::SetProgressBar(
      progress_bar_background_resource_id, progress_bar_background_tint,
      progress_bar_resource_id, progress_bar_tint, progress_bar_visible,
      search_bar_bottom, progress_bar_height, progress_bar_opacity,
      progress_bar_completion, search_panel_width);

  // ---------------------------------------------------------------------------
  // Touch Highlight Layer
  // ---------------------------------------------------------------------------
  if (touch_highlight_visible) {
    if (touch_highlight_layer_->parent() != layer_)
      layer_->AddChild(touch_highlight_layer_);
    // In the new layout don't highlight the whole bar due to rounded corners.
    int highlight_height = text_layer_height;
    int highlight_top = content_top;
    highlight_top += (content_height - text_layer_height) / 2;
    gfx::Size background_size(touch_highlight_width, highlight_height);
    touch_highlight_layer_->SetBounds(background_size);
    touch_highlight_layer_->SetPosition(
        gfx::PointF(touch_highlight_x_offset, highlight_top));
  } else {
    touch_highlight_layer_->RemoveFromParent();
  }

  // ---------------------------------------------------------------------------
  // Icon Layer
  // ---------------------------------------------------------------------------
  bar_image_size_ = bar_image_size;
  SetupIconLayer(search_provider_icon_resource_id, quick_action_icon_visible,
                 quick_action_icon_resource_id, thumbnail_visible,
                 custom_image_visibility_percentage);
}

scoped_refptr<cc::slim::Layer> ContextualSearchLayer::GetIconLayer() {
  return icon_layer_;
}

void ContextualSearchLayer::SetupIconLayer(
    int search_provider_icon_resource_id,
    bool quick_action_icon_visible,
    int quick_action_icon_resource_id,
    bool thumbnail_visible,
    float custom_image_visibility_percentage) {
  icon_layer_->SetBounds(gfx::Size(bar_image_size_, bar_image_size_));
  icon_layer_->SetMasksToBounds(true);

  if (quick_action_icon_visible) {
    if (quick_action_icon_layer_->parent() != icon_layer_)
      icon_layer_->AddChild(quick_action_icon_layer_);

    ui::Resource* quick_action_icon_resource = resource_manager_->GetResource(
        ui::ANDROID_RESOURCE_TYPE_DYNAMIC, quick_action_icon_resource_id);
    if (quick_action_icon_resource) {
      quick_action_icon_layer_->SetUIResourceId(
          quick_action_icon_resource->ui_resource()->id());
      quick_action_icon_layer_->SetBounds(
          gfx::Size(bar_image_size_, bar_image_size_));

      SetCustomImageProperties(quick_action_icon_layer_, 0, 0,
                               custom_image_visibility_percentage);
    }
  } else if (quick_action_icon_layer_->parent()) {
    quick_action_icon_layer_->RemoveFromParent();
  }

  // Thumbnail
  if (!quick_action_icon_visible && thumbnail_visible) {
    if (thumbnail_layer_->parent() != icon_layer_)
          icon_layer_->AddChild(thumbnail_layer_);

    SetCustomImageProperties(thumbnail_layer_, thumbnail_top_margin_,
                             thumbnail_side_margin_,
                             custom_image_visibility_percentage);
  } else if (thumbnail_layer_->parent()) {
    thumbnail_layer_->RemoveFromParent();
  }

  // Search Provider Icon
  if (search_provider_icon_layer_->parent() != icon_layer_)
    icon_layer_->AddChild(search_provider_icon_layer_);

  ui::Resource* search_provider_icon_resource = resource_manager_->GetResource(
      ui::ANDROID_RESOURCE_TYPE_STATIC, search_provider_icon_resource_id);
  if (search_provider_icon_resource) {
    gfx::Size icon_size = search_provider_icon_resource->size();
    search_provider_icon_layer_->SetUIResourceId(
        search_provider_icon_resource->ui_resource()->id());
    search_provider_icon_layer_->SetBounds(icon_size);

    search_provider_icon_layer_->SetOpacity(1.f -
                                            custom_image_visibility_percentage);

    // Determine x and y offsets to center the icon in its parent layer
    float icon_x_offset = (bar_image_size_ - icon_size.width()) / 2;
    float icon_y_offset = (bar_image_size_ - icon_size.height()) / 2;

    // Determine extra y-offset if thumbnail or quick action are visible.
    icon_y_offset -= (bar_image_size_ * custom_image_visibility_percentage);

    search_provider_icon_layer_->SetPosition(
        gfx::PointF(icon_x_offset, icon_y_offset));
  }
}

void ContextualSearchLayer::SetCustomImageProperties(
    scoped_refptr<cc::slim::UIResourceLayer> custom_image_layer,
    float top_margin,
    float side_margin,
    float visibility_percentage) {
  custom_image_layer->SetOpacity(visibility_percentage);

  // When animating, the custom image and search provider icon slide through
  // |icon_layer_|. This effect is achieved by changing the y-offset
  // for each child layer.
  // If the custom image has a height less than |bar_image_size_|, it will
  // have a top margin that needs to be accounted for while running the
  // animation. The final |custom_image_y_offset| should be equal to
  // |tpp_margin|.
  float custom_image_y_offset =
      (bar_image_size_ * (1.f - visibility_percentage)) + top_margin;
  custom_image_layer->SetPosition(
      gfx::PointF(side_margin, custom_image_y_offset));
}

int ContextualSearchLayer::SetupTextLayer(float content_top,
                                          float content_height,
                                          float search_text_layer_min_height,
                                          int caption_resource_id,
                                          bool caption_visible,
                                          float animation_percentage,
                                          float search_term_opacity,
                                          int context_resource_id,
                                          float context_opacity,
                                          float term_caption_spacing) {
  // ---------------------------------------------------------------------------
  // Setup the Drawing Hierarchy
  // ---------------------------------------------------------------------------
  // Search Term
  DCHECK(text_layer_.get());
  DCHECK(bar_text_.get());
  DCHECK(search_caption_.get());
  bool bar_text_visible = search_term_opacity > 0.0f;
  if (bar_text_visible && bar_text_->parent() != text_layer_)
    text_layer_->AddChild(bar_text_);

  // Search Context
  ui::Resource* context_resource = resource_manager_->GetResource(
      ui::ANDROID_RESOURCE_TYPE_DYNAMIC, context_resource_id);
  if (context_resource) {
    search_context_->SetUIResourceId(context_resource->ui_resource()->id());
    search_context_->SetBounds(context_resource->size());
  }

  // Search Caption
  ui::Resource* caption_resource = nullptr;
  if (caption_visible) {
    // Grabs the dynamic Search Caption resource so we can get a snapshot.
    caption_resource  = resource_manager_->GetResource(
        ui::ANDROID_RESOURCE_TYPE_DYNAMIC, caption_resource_id);
  }

  if (caption_visible && animation_percentage != 0.f) {
    if (search_caption_->parent() != text_layer_) {
      text_layer_->AddChild(search_caption_);
    }
    if (caption_resource) {
      search_caption_->SetUIResourceId(caption_resource->ui_resource()->id());
      search_caption_->SetBounds(caption_resource->size());
    }
  } else if (search_caption_->parent()) {
    search_caption_->RemoveFromParent();
  }

  // ---------------------------------------------------------------------------
  // Calculate Text Layer Size
  // ---------------------------------------------------------------------------
  // If space allows, the Term, Context and Caption should occupy a Text
  // section of fixed size.
  // We may not be able to fit these inside the ideal size as the user may have
  // their Font Size set to large.

  // The Term might not be visible or initialized yet, so set up main_text with
  // whichever main bar text seems appropriate.
  scoped_refptr<cc::slim::UIResourceLayer> main_text =
      (bar_text_visible ? bar_text_ : search_context_);

  // The search_caption_ may not have had it's resource set by this point, if so
  // the bounds will be zero and everything will still work.
  float term_height = main_text->bounds().height();
  float caption_height = search_caption_->bounds().height();

  float layer_height = std::max(search_text_layer_min_height,
      term_height + caption_height + term_caption_spacing);
  float layer_width =
      std::max(main_text->bounds().width(), search_caption_->bounds().width());

  float layer_top = content_top + (content_height - layer_height) / 2;
  text_layer_->SetBounds(gfx::Size(layer_width, layer_height));
  text_layer_->SetPosition(gfx::PointF(0.f, layer_top));
  text_layer_->SetMasksToBounds(true);

  // ---------------------------------------------------------------------------
  // Layout Text Layer
  // ---------------------------------------------------------------------------
  // ---Top of Search Bar--- <- bar_top
  //
  // ---Top of Text Layer--- <- layer_top
  //                         } remaining_height / 2
  // Term & Context          } term_height
  //                         } term_caption_spacing
  // Caption                 } caption_height
  //                         } remaining_height / 2
  // --Bottom of Text Layer-
  //
  // --Bottom of Search Bar-
  // If the Caption is not visible the Term is centered in this space, when
  // the Caption becomes visible it is animated sliding up into it's position
  // with the spacings determined by UI.
  // The Term and the Context are assumed to be the same height and will be
  // positioned one on top of the other. When the Context is resolved it will
  // fade out and the Term will fade in.

  search_context_->SetOpacity(context_opacity);
  bar_text_->SetOpacity(search_term_opacity);

  // If there is no caption, just vertically center the Search Term.
  float term_top = (layer_height - term_height) / 2;

  // If we aren't displaying the caption we're done.
  if (!caption_visible || animation_percentage == 0.f || !caption_resource) {
    bar_text_->SetPosition(gfx::PointF(0.f, term_top));
    search_context_->SetPosition(gfx::PointF(0.f, term_top));
    return layer_height;
  }

  // Calculate the positions for the Term and Caption when the Caption
  // animation is complete.
  float remaining_height = layer_height
                         - term_height
                         - term_caption_spacing
                         - caption_height;

  float term_top_end = remaining_height / 2;
  float caption_top_end = term_top_end + term_height + term_caption_spacing;

  // Interpolate between the animation start and end positions (short cut
  // if the animation is at the end or start).
  term_top = term_top * (1.f - animation_percentage)
           + term_top_end * animation_percentage;

  // The Caption starts off the bottom of the Text Layer.
  float caption_top = layer_height * (1.f - animation_percentage)
                    + caption_top_end * animation_percentage;

  bar_text_->SetPosition(gfx::PointF(0.f, term_top));
  search_context_->SetPosition(gfx::PointF(0.f, term_top));
  search_caption_->SetPosition(gfx::PointF(0.f, caption_top));
  return layer_height;
}

void ContextualSearchLayer::SetThumbnail(const SkBitmap* thumbnail) {
  // Determine the scaled thumbnail width and height. If both the height and
  // width of |thumbnail| are larger than |bar_image_size_|, the thumbnail
  // will be scaled down by a call to Layer::SetBounds() below.
  int min_dimension = std::min(thumbnail->width(), thumbnail->height());
  int scaled_thumbnail_width = thumbnail->width();
  int scaled_thumbnail_height = thumbnail->height();
  if (min_dimension > bar_image_size_) {
    scaled_thumbnail_width =
        scaled_thumbnail_width * bar_image_size_ / min_dimension;
    scaled_thumbnail_height =
        scaled_thumbnail_height * bar_image_size_ / min_dimension;
  }

  // Determine the UV transform coordinates. This will crop the thumbnail.
  // (0, 0) is the default top left corner. (1, 1) is the default bottom
  // right corner.
  float top_left_x = 0;
  float top_left_y = 0;
  float bottom_right_x = 1;
  float bottom_right_y = 1;

  if (scaled_thumbnail_width > bar_image_size_) {
    // Crop an even amount on the left and right sides of the thumbnail.
    float top_left_x_px = (scaled_thumbnail_width - bar_image_size_) / 2.f;
    float bottom_right_x_px = top_left_x_px + bar_image_size_;

    top_left_x = top_left_x_px / scaled_thumbnail_width;
    bottom_right_x = bottom_right_x_px / scaled_thumbnail_width;
  } else if (scaled_thumbnail_height > bar_image_size_) {
    // Crop an even amount on the top and bottom of the thumbnail.
    float top_left_y_px = (scaled_thumbnail_height - bar_image_size_) / 2.f;
    float bottom_right_y_px = top_left_y_px + bar_image_size_;

    top_left_y = top_left_y_px / scaled_thumbnail_height;
    bottom_right_y = bottom_right_y_px / scaled_thumbnail_height;
  }

  // If the original |thumbnail| height or width is smaller than
  // |bar_image_size_| determine the side and top margins needed to center
  // the thumbnail.
  thumbnail_side_margin_ = 0;
  thumbnail_top_margin_ = 0;

  if (scaled_thumbnail_width < bar_image_size_) {
    thumbnail_side_margin_ = (bar_image_size_ - scaled_thumbnail_width) / 2.f;
  }

  if (scaled_thumbnail_height < bar_image_size_) {
    thumbnail_top_margin_ = (bar_image_size_ - scaled_thumbnail_height) / 2.f;
  }

  // Determine the layer bounds. This will down scale the thumbnail if
  // necessary and ensure it is displayed at |bar_image_size_|. If
  // either the original |thumbnail| height or width is smaller than
  // |bar_image_size_|, the thumbnail will not be scaled.
  int layer_width = std::min(bar_image_size_, scaled_thumbnail_width);
  int layer_height = std::min(bar_image_size_, scaled_thumbnail_height);

  // UIResourceLayer requires an immutable copy of the input |thumbnail|.
  SkBitmap thumbnail_copy;
  if (thumbnail->isImmutable()) {
    thumbnail_copy = *thumbnail;
  } else {
    if (thumbnail_copy.tryAllocPixels(thumbnail->info())) {
      thumbnail->readPixels(thumbnail_copy.info(), thumbnail_copy.getPixels(),
                            thumbnail_copy.rowBytes(), 0, 0);
    }
    thumbnail_copy.setImmutable();
  }

  thumbnail_layer_->SetBitmap(thumbnail_copy);
  thumbnail_layer_->SetBounds(gfx::Size(layer_width, layer_height));
  thumbnail_layer_->SetPosition(
      gfx::PointF(thumbnail_side_margin_, thumbnail_top_margin_));
  thumbnail_layer_->SetUV(gfx::PointF(top_left_x, top_left_y),
                          gfx::PointF(bottom_right_x, bottom_right_y));
}

ContextualSearchLayer::ContextualSearchLayer(
    ui::ResourceManager* resource_manager)
    : OverlayPanelLayer(resource_manager),
      search_context_(cc::slim::UIResourceLayer::Create()),
      icon_layer_(cc::slim::Layer::Create()),
      search_provider_icon_layer_(cc::slim::UIResourceLayer::Create()),
      thumbnail_layer_(cc::slim::UIResourceLayer::Create()),
      quick_action_icon_layer_(cc::slim::UIResourceLayer::Create()),
      search_promo_(cc::slim::UIResourceLayer::Create()),
      search_promo_container_(cc::slim::SolidColorLayer::Create()),
      related_searches_in_bar_(cc::slim::UIResourceLayer::Create()),
      search_caption_(cc::slim::UIResourceLayer::Create()),
      text_layer_(cc::slim::UIResourceLayer::Create()),
      touch_highlight_layer_(cc::slim::SolidColorLayer::Create()) {
  // Search Bar Text
  search_context_->SetIsDrawable(true);

  // Search Bar Caption
  search_caption_->SetIsDrawable(true);

  // Search Opt Out Promo
  search_promo_container_->SetIsDrawable(true);
  search_promo_container_->SetBackgroundColor(
      SkColor4f::FromColor(kSearchBackgroundColor));
  search_promo_->SetIsDrawable(true);

  // Related Searches sections
  related_searches_in_bar_->SetIsDrawable(true);

  // Icon - holds thumbnail, search provider icon and/or quick action icon
  icon_layer_->SetIsDrawable(true);
  layer_->AddChild(icon_layer_);

  // Search provider icon
  search_provider_icon_layer_->SetIsDrawable(true);

  // Thumbnail
  thumbnail_layer_->SetIsDrawable(true);

  // Quick action icon
  quick_action_icon_layer_->SetIsDrawable(true);

  // Content layer
  text_layer_->SetIsDrawable(true);
  // NOTE(mdjones): This can be called multiple times to add other text layers.
  AddBarTextLayer(text_layer_);
  text_layer_->AddChild(search_context_);

  // Touch Highlight Layer
  touch_highlight_layer_->SetIsDrawable(true);
  touch_highlight_layer_->SetBackgroundColor(
      SkColor4f::FromColor(kTouchHighlightColor));
}

ContextualSearchLayer::~ContextualSearchLayer() {
}

}  //  namespace android
