// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/tab_layer.h"

#include <vector>

#include "base/i18n/rtl.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/nine_patch_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/ui_resource_layer.h"
#include "cc/resources/scoped_ui_resource.h"
#include "chrome/browser/android/compositor/decoration_title.h"
#include "chrome/browser/android/compositor/layer/content_layer.h"
#include "chrome/browser/android/compositor/layer/tabgroup_content_layer.h"
#include "chrome/browser/android/compositor/layer/toolbar_layer.h"
#include "chrome/browser/android/compositor/layer_title_cache.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "ui/android/resources/nine_patch_resource.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/base/l10n/l10n_util_android.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"

namespace android {

// static
scoped_refptr<TabLayer> TabLayer::Create(
    bool incognito,
    ui::ResourceManager* resource_manager,
    LayerTitleCache* layer_title_cache,
    TabContentManager* tab_content_manager) {
  return base::WrapRefCounted(new TabLayer(
      incognito, resource_manager, layer_title_cache, tab_content_manager));
}

// static
void TabLayer::ComputePaddingPositions(const gfx::Size& content_size,
                                       const gfx::Size& desired_size,
                                       gfx::Rect* side_padding_rect,
                                       gfx::Rect* bottom_padding_rect) {
  if (content_size.width() < desired_size.width()) {
    side_padding_rect->set_x(content_size.width());
    side_padding_rect->set_width(desired_size.width() - content_size.width());
    // Restrict the side padding height to avoid overdrawing when both the side
    // and bottom padding are used.
    side_padding_rect->set_height(std::min(content_size.height(),
                                           desired_size.height()));
  }

  if (content_size.height() < desired_size.height()) {
    bottom_padding_rect->set_y(content_size.height());
    // The side padding height is restricted to the min of bounds.height() and
    // desired_bounds.height(), so it will not extend all the way to the bottom
    // of the desired_bounds. The width of the bottom padding is set at
    // desired_bounds.width() so that there is not a hole where the side padding
    // stops.
    bottom_padding_rect->set_width(desired_size.width());
    bottom_padding_rect->set_height(
        desired_size.height() - content_size.height());
  }
}

static void PositionPadding(scoped_refptr<cc::SolidColorLayer> padding_layer,
                            gfx::Rect padding_rect,
                            float content_scale,
                            float alpha,
                            gfx::PointF content_position,
                            gfx::RectF descaled_local_content_area) {
  if (padding_rect.IsEmpty()) {
    padding_layer->SetHideLayerAndSubtree(true);
    return;
  }

  padding_layer->SetHideLayerAndSubtree(false);
  padding_layer->SetBounds(padding_rect.size());
  padding_layer->SetOpacity(alpha);

  gfx::Transform transform;
  transform.Scale(content_scale, content_scale);
  transform.Translate(padding_rect.x() + content_position.x(),
                      padding_rect.y() + content_position.y());
  transform.Translate(descaled_local_content_area.x(),
                      descaled_local_content_area.y());
  padding_layer->SetTransformOrigin(gfx::Point3F(0.f, 0.f, 0.f));
  padding_layer->SetTransform(transform);
}

void TabLayer::SetProperties(int id,
                             const std::vector<int>& ids,
                             bool can_use_live_layer,
                             int toolbar_resource_id,
                             int close_button_resource_id,
                             int shadow_resource_id,
                             int contour_resource_id,
                             int back_logo_resource_id,
                             int border_resource_id,
                             int border_inner_shadow_resource_id,
                             int default_background_color,
                             int back_logo_color,
                             bool close_button_on_right,
                             float x,
                             float y,
                             float width,
                             float height,
                             float shadow_x,
                             float shadow_y,
                             float shadow_width,
                             float shadow_height,
                             float pivot_x,
                             float pivot_y,
                             float rotation_x,
                             float rotation_y,
                             float alpha,
                             float border_alpha,
                             float border_inner_shadow_alpha,
                             float contour_alpha,
                             float shadow_alpha,
                             float close_alpha,
                             float border_scale,
                             float saturation,
                             float brightness,
                             float close_btn_width,
                             float close_btn_asset_size,
                             float static_to_view_blend,
                             float content_width,
                             float content_height,
                             float view_width,
                             float view_height,
                             bool show_toolbar,
                             int default_theme_color,
                             int toolbar_background_color,
                             int close_button_color,
                             bool anonymize_toolbar,
                             bool show_tab_title,
                             int toolbar_textbox_resource_id,
                             int toolbar_textbox_background_color,
                             float toolbar_textbox_alpha,
                             float toolbar_alpha,
                             float toolbar_y_offset,
                             float side_border_scale,
                             bool inset_border) {
  if (alpha <= 0) {
    layer_->SetHideLayerAndSubtree(true);
    return;
  }

  layer_->SetHideLayerAndSubtree(false);

  // Grab required resources
  ui::NinePatchResource* border_resource =
      ui::NinePatchResource::From(resource_manager_->GetStaticResourceWithTint(
          border_resource_id, default_theme_color));
  ui::NinePatchResource* border_inner_shadow_resource =
      ui::NinePatchResource::From(resource_manager_->GetResource(
          ui::ANDROID_RESOURCE_TYPE_STATIC, border_inner_shadow_resource_id));
  ui::NinePatchResource* shadow_resource =
      ui::NinePatchResource::From(resource_manager_->GetResource(
          ui::ANDROID_RESOURCE_TYPE_STATIC, shadow_resource_id));
  ui::NinePatchResource* contour_resource =
      ui::NinePatchResource::From(resource_manager_->GetResource(
          ui::ANDROID_RESOURCE_TYPE_STATIC, contour_resource_id));
  ui::Resource* close_btn_resource =
      resource_manager_->GetStaticResourceWithTint(close_button_resource_id,
                                                   close_button_color);
  ui::Resource* back_logo_resource = nullptr;

  DecorationTitle* title_layer = nullptr;

  //----------------------------------------------------------------------------
  // Handle Border Scaling (Upscale/Downscale everything until final scaling)
  //----------------------------------------------------------------------------
  width /= border_scale;
  height /= border_scale;
  shadow_x /= border_scale;
  shadow_y /= border_scale;
  shadow_width /= border_scale;
  shadow_height /= border_scale;

  //----------------------------------------------------------------------------
  // Precalculate Helper Values
  //----------------------------------------------------------------------------
  const gfx::RectF border_padding(border_resource->padding());
  const gfx::RectF border_inner_shadow_padding(
      border_inner_shadow_resource->padding());
  const gfx::RectF shadow_padding(shadow_resource->padding());
  const gfx::RectF contour_padding(contour_resource->padding());

  const bool back_visible = cos(rotation_x * SK_MScalarPI / 180.0f) < 0 ||
                            cos(rotation_y * SK_MScalarPI / 180.0f) < 0;

  const float content_scale = width / content_width;
  gfx::RectF content_area(0.f, 0.f, content_width, content_height);
  gfx::RectF scaled_local_content_area(shadow_x, shadow_y, shadow_width,
                                       shadow_height);
  gfx::RectF descaled_local_content_area(
      scaled_local_content_area.x() / content_scale,
      scaled_local_content_area.y() / content_scale,
      scaled_local_content_area.width() / content_scale,
      scaled_local_content_area.height() / content_scale);

  const gfx::Size shadow_padding_size(
      shadow_resource->size().width() - shadow_padding.width(),
      shadow_resource->size().height() - shadow_padding.height());
  const gfx::Size border_padding_size(
      border_resource->size().width() - border_padding.width(),
      border_resource->size().height() - border_padding.height());
  const gfx::Size border_inner_shadow_padding_size(
      border_inner_shadow_resource->size().width() -
          border_inner_shadow_padding.width(),
      border_inner_shadow_resource->size().height() -
          border_inner_shadow_padding.height());
  const gfx::Size contour_padding_size(
      contour_resource->size().width() - contour_padding.width(),
      contour_resource->size().height() - contour_padding.height());

  const float close_btn_effective_width = close_btn_width * close_alpha;

  //--------------------------------------------------------------------------
  // Update Resource Ids For Layers That Impact Layout
  //--------------------------------------------------------------------------

  // TODO(kkimlabs): Tab switcher doesn't show the progress bar.
  toolbar_layer_->PushResource(
      toolbar_resource_id, toolbar_background_color, anonymize_toolbar,
      toolbar_textbox_background_color, toolbar_textbox_resource_id,
      toolbar_textbox_alpha, view_height,
      // TODO(mdjones): Feels odd to pass 0 here when
      // we have access to toolbar_y_offset.
      0, false, false);
  toolbar_layer_->UpdateProgressBar(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

  float toolbar_impact_height = 0;
  if (show_toolbar && !back_visible)
    toolbar_impact_height = toolbar_layer_->layer()->bounds().height();

  //----------------------------------------------------------------------------
  // Compute Alpha and Visibility
  //----------------------------------------------------------------------------
  border_alpha *= alpha;
  contour_alpha *= alpha;
  shadow_alpha *= alpha;
  close_alpha *= alpha;
  toolbar_alpha *= alpha;

  if (back_visible) {
    border_alpha = 0.f;
    border_inner_shadow_alpha = 0.f;
  }

  bool border_visible = border_alpha > 0.f;
  bool border_inner_shadow_visible = border_inner_shadow_alpha > 0.f;
  bool contour_visible = border_alpha < contour_alpha && contour_alpha > 0.f;
  bool shadow_visible = shadow_alpha > 0.f && border_alpha > 0.f;

  //----------------------------------------------------------------------------
  // Compute Layer Sizes
  //----------------------------------------------------------------------------
  gfx::Size shadow_size(width + shadow_padding_size.width() * side_border_scale,
                        height + shadow_padding_size.height());
  gfx::Size border_size(width + border_padding_size.width() * side_border_scale,
                        height + border_padding_size.height());
  gfx::Size border_inner_shadow_size(
      width + border_inner_shadow_padding_size.width(),
      height + border_inner_shadow_padding_size.height());
  gfx::Size contour_size(
      width + contour_padding_size.width() * side_border_scale,
      height + contour_padding_size.height());
  gfx::Size close_button_size(close_btn_width, border_padding.y());
  gfx::Size title_size(width - close_btn_effective_width, border_padding.y());
  gfx::Size back_logo_size;
  // TODO(clholgat): Figure out why the back logo is null sometimes.
  if (back_visible) {
    back_logo_resource =
        resource_manager_->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                       back_logo_resource_id);
    if (back_logo_resource)
      back_logo_size = back_logo_resource->size();
  }

  // Store this size at a point as it might go negative during the inset
  // calculations.
  gfx::Point desired_content_size_pt(
      descaled_local_content_area.width(),
      descaled_local_content_area.height() - toolbar_impact_height);

  // Shrink the toolbar layer so we properly clip if it's offset.
  gfx::Size toolbar_size(
      toolbar_layer_->layer()->bounds().width(),
      toolbar_layer_->layer()->bounds().height() - toolbar_y_offset);

  //----------------------------------------------------------------------------
  // Compute Layer Positions
  //----------------------------------------------------------------------------
  gfx::PointF shadow_position(-shadow_padding.x() * side_border_scale,
                              -shadow_padding.y());
  gfx::PointF border_position(-border_padding.x() * side_border_scale,
                              -border_padding.y());
  gfx::PointF border_inner_shadow_position(-border_inner_shadow_padding.x(),
                                           -border_inner_shadow_padding.y());
  gfx::PointF contour_position(-contour_padding.x() * side_border_scale,
                               -contour_padding.y());
  gfx::PointF toolbar_position(
      0.f, toolbar_layer_->layer()->bounds().height() - toolbar_size.height());
  gfx::PointF content_position(0.f, toolbar_impact_height);
  gfx::PointF back_logo_position(
      ((descaled_local_content_area.width() - back_logo_->bounds().width()) *
       content_scale) /
          2.0f,
      ((descaled_local_content_area.height() - back_logo_->bounds().height()) *
       content_scale) /
          2.0f);
  gfx::PointF close_button_position;
  gfx::PointF title_position;

  close_button_position.set_y(-border_padding.y());
  title_position.set_y(-border_padding.y());
  if (close_button_on_right)
    close_button_position.set_x(width - close_button_size.width());
  else
    title_position.set_x(close_btn_effective_width);

  //----------------------------------------------------------------------------
  // Center Specific Assets in the Rects
  //----------------------------------------------------------------------------
  close_button_position.Offset(
      (close_button_size.width() - close_btn_asset_size) / 2.f,
      (close_button_size.height() - close_btn_asset_size) / 2.f);
  close_button_size.SetSize(close_btn_asset_size, close_btn_asset_size);

  //----------------------------------------------------------------------------
  // Handle Insetting the Top Border Component
  //----------------------------------------------------------------------------
  if (inset_border) {
    float inset_diff = inset_border ? border_padding.y() : 0.f;
    descaled_local_content_area.set_height(
        descaled_local_content_area.height() - inset_diff);
    scaled_local_content_area.set_height(scaled_local_content_area.height() -
                                         inset_diff * content_scale);
    shadow_size.set_height(shadow_size.height() - inset_diff);
    border_size.set_height(border_size.height() - inset_diff);
    border_inner_shadow_size.set_height(border_inner_shadow_size.height() -
                                        inset_diff);
    contour_size.set_height(contour_size.height() - inset_diff);
    shadow_position.set_y(shadow_position.y() + inset_diff);
    border_position.set_y(border_position.y() + inset_diff);
    border_inner_shadow_position.set_y(border_inner_shadow_position.y() +
                                       inset_diff);
    contour_position.set_y(contour_position.y() + inset_diff);
    close_button_position.set_y(close_button_position.y() + inset_diff);
    title_position.set_y(title_position.y() + inset_diff);

    // Scaled eventually, so have to descale the size difference first.
    toolbar_position.set_y(toolbar_position.y() + inset_diff / content_scale);
    content_position.set_y(content_position.y() + inset_diff / content_scale);
    desired_content_size_pt.set_y(desired_content_size_pt.y() -
                                  inset_diff / content_scale);
  }

  const bool inset_toolbar = !inset_border;
  if (!inset_toolbar) {
    float inset_diff = toolbar_impact_height;
    toolbar_position.set_y(toolbar_position.y() - inset_diff);
    content_position.set_y(content_position.y() - inset_diff);
    desired_content_size_pt.set_y(desired_content_size_pt.y() + inset_diff);
  }

  // Finally build the sizes that might have calculations that go negative.
  gfx::Size desired_content_size(desired_content_size_pt.x(),
                                 desired_content_size_pt.y());

  //----------------------------------------------------------------------------
  // Calculate Content Visibility
  //----------------------------------------------------------------------------
  // Check if the rect we are drawing is larger than the content rect.
  bool content_visible = desired_content_size.GetArea() > 0.f;

  // TODO(dtrainor): Improve these calculations to prune these layers out.
  bool title_visible = border_alpha > 0.f && !back_visible && show_tab_title;
  bool close_btn_visible = title_visible;
  bool toolbar_visible = show_toolbar && toolbar_alpha > 0.f && !back_visible;

  //----------------------------------------------------------------------------
  // Fix jaggies
  //----------------------------------------------------------------------------
  border_position.Offset(0.5f, 0.5f);
  border_inner_shadow_position.Offset(0.5f, 0.5f);
  shadow_position.Offset(0.5f, 0.5f);
  contour_position.Offset(0.5f, 0.5f);
  title_position.Offset(0.5f, 0.5f);
  close_button_position.Offset(0.5f, 0.5f);
  toolbar_position.Offset(0.5f, 0.5f);

  border_size.Enlarge(-1.f, -1.f);
  border_inner_shadow_size.Enlarge(-1.f, -1.f);
  shadow_size.Enlarge(-1.f, -1.f);

  //----------------------------------------------------------------------------
  // Update Resource Ids
  //----------------------------------------------------------------------------
  shadow_->SetUIResourceId(shadow_resource->ui_resource()->id());
  shadow_->SetBorder(shadow_resource->Border(shadow_size));
  shadow_->SetAperture(shadow_resource->aperture());

  contour_shadow_->SetUIResourceId(contour_resource->ui_resource()->id());
  contour_shadow_->SetBorder(contour_resource->Border(contour_size));
  contour_shadow_->SetAperture(contour_resource->aperture());

  front_border_->SetUIResourceId(border_resource->ui_resource()->id());
  front_border_->SetAperture(border_resource->aperture());
  front_border_->SetBorder(border_resource->Border(
      border_size,
      gfx::InsetsF(1.f, side_border_scale, 1.f, side_border_scale)));

  front_border_inner_shadow_->SetUIResourceId(
      border_inner_shadow_resource->ui_resource()->id());
  front_border_inner_shadow_->SetAperture(
      border_inner_shadow_resource->aperture());
  front_border_inner_shadow_->SetBorder(border_inner_shadow_resource->Border(
      border_inner_shadow_size));

  side_padding_->SetBackgroundColor(back_visible ? back_logo_color
      : default_background_color);
  bottom_padding_->SetBackgroundColor(back_visible ? back_logo_color
      : default_background_color);

  if (title_visible && layer_title_cache_)
    title_layer = layer_title_cache_->GetTitleLayer(id);
  SetTitle(title_layer);

  close_button_->SetUIResourceId(close_btn_resource->ui_resource()->id());

  if (!back_visible) {
    gfx::Rect rounded_descaled_content_area(
        round(descaled_local_content_area.x()),
        round(descaled_local_content_area.y()),
        round(desired_content_size.width()),
        round(desired_content_size.height()));

    SetContentProperties(
        id, ids, can_use_live_layer, static_to_view_blend, true, alpha,
        saturation, true, rounded_descaled_content_area,
        border_inner_shadow_resource, border_inner_shadow_alpha);

  } else if (back_logo_resource) {
    back_logo_->SetUIResourceId(back_logo_resource->ui_resource()->id());
  }

  //----------------------------------------------------------------------------
  // Push Size, Position, Alpha and Transformations to Layers
  //----------------------------------------------------------------------------
  shadow_->SetHideLayerAndSubtree(!shadow_visible);
  if (shadow_visible) {
    shadow_->SetPosition(shadow_position);
    shadow_->SetBounds(shadow_size);
    shadow_->SetOpacity(shadow_alpha);
  }

  contour_shadow_->SetHideLayerAndSubtree(!contour_visible);
  if (contour_visible) {
    contour_shadow_->SetPosition(contour_position);
    contour_shadow_->SetBounds(contour_size);
    contour_shadow_->SetOpacity(contour_alpha);
  }

  front_border_->SetHideLayerAndSubtree(!border_visible);
  if (border_visible) {
    front_border_->SetPosition(border_position);
    front_border_->SetBounds(border_size);
    front_border_->SetOpacity(border_alpha);
    front_border_->SetNearestNeighbor(toolbar_visible);
  }

  front_border_inner_shadow_->SetHideLayerAndSubtree(
      !border_inner_shadow_visible);
  if (border_inner_shadow_visible) {
    front_border_inner_shadow_->SetPosition(border_inner_shadow_position);
    front_border_inner_shadow_->SetBounds(border_inner_shadow_size);
    front_border_inner_shadow_->SetOpacity(border_inner_shadow_alpha);
  }

  toolbar_layer_->layer()->SetHideLayerAndSubtree(!toolbar_visible);
  if (toolbar_visible) {
    // toolbar_ Transform
    gfx::Transform transform;
    transform.Scale(content_scale, content_scale);
    transform.Translate(toolbar_position.x(), toolbar_position.y());
    toolbar_layer_->layer()->SetTransformOrigin(gfx::Point3F(0.f, 0.f, 0.f));
    toolbar_layer_->layer()->SetTransform(transform);
    toolbar_layer_->SetOpacity(toolbar_alpha);

    toolbar_layer_->layer()->SetMasksToBounds(
        toolbar_layer_->layer()->bounds() != toolbar_size);
    toolbar_layer_->layer()->SetBounds(toolbar_size);
  }

  if (title_layer) {
    gfx::PointF vertically_centered_position(
        title_position.x(),
        title_position.y() +
            (title_size.height() - title_layer->size().height()) / 2.f);

    title_->SetPosition(vertically_centered_position);
    title_layer->setBounds(title_size);
    title_layer->setOpacity(border_alpha);
  }

  close_button_->SetHideLayerAndSubtree(!close_btn_visible);
  if (close_btn_visible) {
    close_button_->SetPosition(close_button_position);
    close_button_->SetBounds(close_button_size);
    // Non-linear alpha looks better.
    close_button_->SetOpacity(close_alpha * close_alpha * border_alpha);
  }

  if (content_visible) {
    {
      // content_ and back_logo_ Transforms
      gfx::Transform transform;
      transform.Scale(content_scale, content_scale);
      transform.Translate(content_position.x(), content_position.y());
      transform.Translate(descaled_local_content_area.x(),
                          descaled_local_content_area.y());

      content_->layer()->SetHideLayerAndSubtree(back_visible);
      back_logo_->SetHideLayerAndSubtree(!back_visible);

      if (!back_visible) {
        content_->layer()->SetTransformOrigin(gfx::Point3F(0.f, 0.f, 0.f));
        content_->layer()->SetTransform(transform);
      } else {
        back_logo_->SetPosition(back_logo_position);
        back_logo_->SetBounds(back_logo_size);
        back_logo_->SetTransformOrigin(gfx::Point3F(0.f, 0.f, 0.f));
        back_logo_->SetTransform(transform);
        // TODO: Set back logo alpha on leaf.
      }
    }

    {
      // padding_ Transform
      gfx::Size content_bounds;
      if (!back_visible)
        content_bounds = content_->ComputeSize(id);

      gfx::Rect side_padding_rect;
      gfx::Rect bottom_padding_rect;
      if (content_bounds.width() == 0 || content_bounds.height() == 0) {
        // If content_ has 0 width or height, use the side padding to fill
        // the desired_content_size.
        side_padding_rect.set_size(desired_content_size);
      } else {
            ComputePaddingPositions(content_bounds, desired_content_size,
                                    &side_padding_rect, &bottom_padding_rect);
      }

      PositionPadding(side_padding_, side_padding_rect, content_scale,
                      alpha, content_position, descaled_local_content_area);
      PositionPadding(bottom_padding_, bottom_padding_rect, content_scale,
                      alpha, content_position, descaled_local_content_area);
    }
  } else {
    back_logo_->SetHideLayerAndSubtree(true);
    side_padding_->SetHideLayerAndSubtree(true);
    bottom_padding_->SetHideLayerAndSubtree(true);
    content_->layer()->SetHideLayerAndSubtree(true);
  }

  {
    // Global Transform
    gfx::PointF pivot_origin(pivot_y, pivot_x);

    gfx::Transform transform;

    if (rotation_x != 0 || rotation_y != 0) {
      // Apply screen perspective if there are rotations.
      transform.Translate(content_width / 2, content_height / 2);
      transform.ApplyPerspectiveDepth(
          content_width > content_height ? content_width : content_height);
      transform.Translate(-content_width / 2, -content_height / 2);

      // Translate to correct position on the screen
      transform.Translate(x, y);

      // Apply pivot rotations
      transform.Translate(pivot_origin.x(), pivot_origin.y());
      transform.RotateAboutYAxis(rotation_y);
      transform.RotateAboutXAxis(-rotation_x);
      transform.Translate(-pivot_origin.x(), -pivot_origin.y());
    } else {
      // Translate to correct position on the screen
      transform.Translate(x, y);
    }
    transform.Scale(border_scale, border_scale);
    layer_->SetTransform(transform);
  }

  // Only applies the brightness filter if the value has changed and is less
  // than 1.
  if (brightness != brightness_) {
    brightness_ = brightness;
    cc::FilterOperations filters;
    if (brightness_ < 1.f)
      filters.Append(cc::FilterOperation::CreateBrightnessFilter(brightness_));
    layer_->SetFilters(filters);
  }
}

scoped_refptr<cc::Layer> TabLayer::layer() {
  return layer_;
}

TabLayer::TabLayer(bool incognito,
                   ui::ResourceManager* resource_manager,
                   LayerTitleCache* layer_title_cache,
                   TabContentManager* tab_content_manager)
    : incognito_(incognito),
      resource_manager_(resource_manager),
      tab_content_manager_(tab_content_manager),
      layer_title_cache_(layer_title_cache),
      layer_(cc::Layer::Create()),
      toolbar_layer_(ToolbarLayer::Create(resource_manager)),
      title_(cc::Layer::Create()),
      content_(ContentLayer::Create(tab_content_manager)),
      side_padding_(cc::SolidColorLayer::Create()),
      bottom_padding_(cc::SolidColorLayer::Create()),
      close_button_(cc::UIResourceLayer::Create()),
      front_border_(cc::NinePatchLayer::Create()),
      front_border_inner_shadow_(cc::NinePatchLayer::Create()),
      contour_shadow_(cc::NinePatchLayer::Create()),
      shadow_(cc::NinePatchLayer::Create()),
      back_logo_(cc::UIResourceLayer::Create()),
      brightness_(1.f) {
  layer_->AddChild(shadow_);
  layer_->AddChild(contour_shadow_);
  layer_->AddChild(side_padding_);
  layer_->AddChild(bottom_padding_);
  layer_->AddChild(content_->layer());
  layer_->AddChild(back_logo_);
  layer_->AddChild(front_border_inner_shadow_);
  layer_->AddChild(front_border_);
  layer_->AddChild(title_.get());
  layer_->AddChild(close_button_);
  layer_->AddChild(toolbar_layer_->layer());

  contour_shadow_->SetIsDrawable(true);
  side_padding_->SetIsDrawable(true);
  bottom_padding_->SetIsDrawable(true);
  front_border_->SetIsDrawable(true);
  front_border_inner_shadow_->SetIsDrawable(true);
  shadow_->SetIsDrawable(true);
  close_button_->SetIsDrawable(true);
  back_logo_->SetIsDrawable(true);

  front_border_->SetFillCenter(false);
}

TabLayer::~TabLayer() {
}

void TabLayer::SetTitle(DecorationTitle* title) {
  scoped_refptr<cc::Layer> layer = title ? title->layer() : nullptr;

  if (!layer.get()) {
    title_->RemoveAllChildren();
  } else {
    const cc::LayerList& children = title_->children();
    if (children.size() == 0 || children[0]->id() != layer->id()) {
      title_->RemoveAllChildren();
      title_->AddChild(layer);
    }
  }

  if (title)
    title->SetUIResourceIds();
}

void TabLayer::SetContentProperties(
    int id,
    const std::vector<int>& tab_ids,
    bool can_use_live_layer,
    float static_to_view_blend,
    bool should_override_content_alpha,
    float content_alpha_override,
    float saturation,
    bool should_clip,
    const gfx::Rect& clip,
    ui::NinePatchResource* inner_shadow_resource,
    float inner_shadow_alpha) {
  if (tab_ids.size() == 0) {
    content_->SetProperties(id, can_use_live_layer, static_to_view_blend,
                            should_override_content_alpha,
                            content_alpha_override, saturation, should_clip,
                            clip);
  } else {
    scoped_refptr<TabGroupContentLayer> tabgroup_content_layer =
        TabGroupContentLayer::Create(tab_content_manager_);
    layer_->ReplaceChild(content_->layer().get(),
                         tabgroup_content_layer->layer());
    content_ = tabgroup_content_layer;

    tabgroup_content_layer->SetProperties(
        id, tab_ids, can_use_live_layer, static_to_view_blend,
        should_override_content_alpha, content_alpha_override, saturation,
        should_clip, clip, inner_shadow_resource, inner_shadow_alpha);

    front_border_inner_shadow_->SetIsDrawable(false);
  }
}

}  //  namespace android
