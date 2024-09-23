// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/tab_layer.h"

#include <vector>

#include "base/i18n/rtl.h"
#include "base/numerics/safe_conversions.h"
#include "cc/resources/scoped_ui_resource.h"
#include "cc/slim/filter.h"
#include "cc/slim/layer.h"
#include "cc/slim/nine_patch_layer.h"
#include "cc/slim/solid_color_layer.h"
#include "chrome/browser/android/compositor/layer/content_layer.h"
#include "chrome/browser/android/compositor/layer/toolbar_layer.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "components/viz/common/quads/offset_tag.h"
#include "ui/android/resources/nine_patch_resource.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/base/l10n/l10n_util_android.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"

namespace android {

// static
scoped_refptr<TabLayer> TabLayer::Create(
    bool incognito,
    ui::ResourceManager* resource_manager,
    TabContentManager* tab_content_manager) {
  return base::WrapRefCounted(
      new TabLayer(incognito, resource_manager, tab_content_manager));
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

static void PositionPadding(
    scoped_refptr<cc::slim::SolidColorLayer> padding_layer,
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
  padding_layer->SetTransformOrigin(gfx::PointF(0.f, 0.f));
  padding_layer->SetTransform(transform);
}

void TabLayer::SetProperties(int id,
                             bool can_use_live_layer,
                             int toolbar_resource_id,
                             int shadow_resource_id,
                             int contour_resource_id,
                             int border_resource_id,
                             int border_inner_shadow_resource_id,
                             int default_background_color,
                             float x,
                             float y,
                             float width,
                             float height,
                             float shadow_width,
                             float shadow_height,
                             float alpha,
                             float border_alpha,
                             float border_inner_shadow_alpha,
                             float contour_alpha,
                             float shadow_alpha,
                             float border_scale,
                             float saturation,
                             float static_to_view_blend,
                             float content_width,
                             float content_height,
                             float view_width,
                             bool show_toolbar,
                             int default_theme_color,
                             int toolbar_background_color,
                             bool anonymize_toolbar,
                             int toolbar_textbox_resource_id,
                             int toolbar_textbox_background_color,
                             float content_offset) {
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

  //----------------------------------------------------------------------------
  // Handle Border Scaling (Upscale/Downscale everything until final scaling)
  //----------------------------------------------------------------------------
  width /= border_scale;
  height /= border_scale;
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

  const float content_scale = width / content_width;
  gfx::RectF content_area(0.f, 0.f, content_width, content_height);
  gfx::RectF scaled_local_content_area(0.f, 0.f, shadow_width, shadow_height);
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

  //--------------------------------------------------------------------------
  // Update Resource Ids For Layers That Impact Layout
  //--------------------------------------------------------------------------

  // TODO(kkimlabs): Tab switcher doesn't show the progress bar.
  toolbar_layer_->PushResource(
      toolbar_resource_id, toolbar_background_color, anonymize_toolbar,
      toolbar_textbox_background_color, toolbar_textbox_resource_id, 0,
      content_offset, false, false, viz::OffsetTag());
  toolbar_layer_->UpdateProgressBar(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

  float toolbar_impact_height = 0;
  if (show_toolbar)
    toolbar_impact_height = content_offset;

  //----------------------------------------------------------------------------
  // Compute Alpha and Visibility
  //----------------------------------------------------------------------------
  border_alpha *= alpha;
  contour_alpha *= alpha;
  shadow_alpha *= alpha;
  float toolbar_alpha = alpha;

  bool border_visible = border_alpha > 0.f;
  bool border_inner_shadow_visible = border_inner_shadow_alpha > 0.f;
  bool contour_visible = border_alpha < contour_alpha && contour_alpha > 0.f;
  bool shadow_visible = shadow_alpha > 0.f && border_alpha > 0.f;

  //----------------------------------------------------------------------------
  // Compute Layer Sizes
  //----------------------------------------------------------------------------
  gfx::Size shadow_size(width + shadow_padding_size.width(),
                        height + shadow_padding_size.height());
  gfx::Size border_size(width + border_padding_size.width(),
                        height + border_padding_size.height());
  gfx::Size border_inner_shadow_size(
      width + border_inner_shadow_padding_size.width(),
      height + border_inner_shadow_padding_size.height());
  gfx::Size contour_size(width + contour_padding_size.width(),
                         height + contour_padding_size.height());

  // Store this size at a point as it might go negative during the inset
  // calculations.
  gfx::Point desired_content_size_pt(
      descaled_local_content_area.width(),
      descaled_local_content_area.height() - toolbar_impact_height);

  // Shrink the toolbar layer so we properly clip if it's offset.
  gfx::Size toolbar_size(toolbar_layer_->layer()->bounds().width(),
                         toolbar_layer_->layer()->bounds().height());

  //----------------------------------------------------------------------------
  // Compute Layer Positions
  //----------------------------------------------------------------------------
  gfx::PointF shadow_position(-shadow_padding.x(), -shadow_padding.y());
  gfx::PointF border_position(-border_padding.x(), -border_padding.y());
  gfx::PointF border_inner_shadow_position(-border_inner_shadow_padding.x(),
                                           -border_inner_shadow_padding.y());
  gfx::PointF contour_position(-contour_padding.x(), -contour_padding.y());
  gfx::PointF toolbar_position(
      0.f, toolbar_layer_->layer()->bounds().height() - toolbar_size.height());
  gfx::PointF content_position(0.f, toolbar_impact_height);

  // Finally build the sizes that might have calculations that go negative.
  gfx::Size desired_content_size(desired_content_size_pt.x(),
                                 desired_content_size_pt.y());

  //----------------------------------------------------------------------------
  // Calculate Content Visibility
  //----------------------------------------------------------------------------
  // Check if the rect we are drawing is larger than the content rect.
  bool content_visible = desired_content_size.GetArea() > 0.f;

  // TODO(dtrainor): Improve these calculations to prune these layers out.
  bool toolbar_visible = show_toolbar && toolbar_alpha > 0.f;

  //----------------------------------------------------------------------------
  // Fix jaggies
  //----------------------------------------------------------------------------
  border_position.Offset(0.5f, 0.5f);
  border_inner_shadow_position.Offset(0.5f, 0.5f);
  shadow_position.Offset(0.5f, 0.5f);
  contour_position.Offset(0.5f, 0.5f);
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
      border_size, gfx::InsetsF::TLBR(1.f, 1.f, 1.f, 1.f)));

  front_border_inner_shadow_->SetUIResourceId(
      border_inner_shadow_resource->ui_resource()->id());
  front_border_inner_shadow_->SetAperture(
      border_inner_shadow_resource->aperture());
  front_border_inner_shadow_->SetBorder(border_inner_shadow_resource->Border(
      border_inner_shadow_size));

  // TODO(crbug.com/40219248): Remove FromColor and make all SkColor4f.
  side_padding_->SetBackgroundColor(
      SkColor4f::FromColor(default_background_color));
  bottom_padding_->SetBackgroundColor(
      SkColor4f::FromColor(default_background_color));

  gfx::Rect rounded_descaled_content_area(
      base::ClampRound(descaled_local_content_area.x()),
      base::ClampRound(descaled_local_content_area.y()),
      desired_content_size.width(), desired_content_size.height());

  content_->SetProperties(id, can_use_live_layer, static_to_view_blend, true,
                          alpha, saturation, true,
                          rounded_descaled_content_area);

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
    toolbar_layer_->layer()->SetTransformOrigin(gfx::PointF(0.f, 0.f));
    toolbar_layer_->layer()->SetTransform(transform);
    toolbar_layer_->SetOpacity(toolbar_alpha);

    toolbar_layer_->layer()->SetMasksToBounds(
        toolbar_layer_->layer()->bounds() != toolbar_size);
    toolbar_layer_->layer()->SetBounds(toolbar_size);
  }

  if (content_visible) {
    {
      gfx::Transform transform;
      transform.Scale(content_scale, content_scale);
      transform.Translate(content_position.x(), content_position.y());
      transform.Translate(descaled_local_content_area.x(),
                          descaled_local_content_area.y());

      content_->layer()->SetHideLayerAndSubtree(false);
      content_->layer()->SetTransformOrigin(gfx::PointF(0.f, 0.f));
      content_->layer()->SetTransform(transform);
    }

    {
      // padding_ Transform
      gfx::Size content_bounds = content_->ComputeSize(id);
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
    side_padding_->SetHideLayerAndSubtree(true);
    bottom_padding_->SetHideLayerAndSubtree(true);
    content_->layer()->SetHideLayerAndSubtree(true);
  }

  {
    // Global Transform

    gfx::Transform transform;
    // Translate to correct position on the screen
    transform.Translate(x, y);
    transform.Scale(border_scale, border_scale);
    layer_->SetTransform(transform);
  }
}

scoped_refptr<cc::slim::Layer> TabLayer::layer() {
  return layer_;
}

TabLayer::TabLayer(bool incognito,
                   ui::ResourceManager* resource_manager,
                   TabContentManager* tab_content_manager)
    : incognito_(incognito),
      resource_manager_(resource_manager),
      tab_content_manager_(tab_content_manager),
      layer_(cc::slim::Layer::Create()),
      toolbar_layer_(ToolbarLayer::Create(resource_manager)),
      content_(ContentLayer::Create(tab_content_manager)),
      side_padding_(cc::slim::SolidColorLayer::Create()),
      bottom_padding_(cc::slim::SolidColorLayer::Create()),
      front_border_(cc::slim::NinePatchLayer::Create()),
      front_border_inner_shadow_(cc::slim::NinePatchLayer::Create()),
      contour_shadow_(cc::slim::NinePatchLayer::Create()),
      shadow_(cc::slim::NinePatchLayer::Create()) {
  layer_->AddChild(shadow_);
  layer_->AddChild(contour_shadow_);
  layer_->AddChild(side_padding_);
  layer_->AddChild(bottom_padding_);
  layer_->AddChild(content_->layer());
  layer_->AddChild(front_border_inner_shadow_);
  layer_->AddChild(front_border_);
  layer_->AddChild(toolbar_layer_->layer());

  contour_shadow_->SetIsDrawable(true);
  side_padding_->SetIsDrawable(true);
  bottom_padding_->SetIsDrawable(true);
  front_border_->SetIsDrawable(true);
  front_border_inner_shadow_->SetIsDrawable(true);
  shadow_->SetIsDrawable(true);

  front_border_->SetFillCenter(false);
}

TabLayer::~TabLayer() {
}

}  //  namespace android
