// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/tab_handle_layer.h"

#include "base/i18n/rtl.h"
#include "cc/layers/layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/resources/scoped_ui_resource.h"
#include "chrome/browser/android/compositor/decoration_title.h"
#include "chrome/browser/android/compositor/layer_title_cache.h"
#include "ui/android/resources/nine_patch_resource.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/base/l10n/l10n_util_android.h"

namespace android {

// static
scoped_refptr<TabHandleLayer> TabHandleLayer::Create(
    LayerTitleCache* layer_title_cache) {
  return base::WrapRefCounted(new TabHandleLayer(layer_title_cache));
}

void TabHandleLayer::SetProperties(
    int id,
    ui::Resource* close_button_resource,
    ui::NinePatchResource* tab_handle_resource,
    ui::NinePatchResource* tab_handle_outline_resource,
    bool foreground,
    bool close_pressed,
    float toolbar_width,
    float x,
    float y,
    float width,
    float height,
    float content_offset_x,
    float close_button_alpha,
    bool is_loading,
    float spinner_rotation,
    float brightness) {
  if (brightness != brightness_ || foreground != foreground_) {
    brightness_ = brightness;
    foreground_ = foreground;
    cc::FilterOperations filters;
    if (brightness_ != 1.0f && !foreground_)
      filters.Append(cc::FilterOperation::CreateBrightnessFilter(brightness_));
    layer_->SetFilters(filters);
  }

  float original_x = x;
  float original_y = y;
  if (foreground_) {
    x = 0;
    y = 0;
  }

  bool is_rtl = l10n_util::IsLayoutRtl();

  float margin_width = tab_handle_resource->size().width() -
                       tab_handle_resource->aperture().width();
  float margin_height = tab_handle_resource->size().height() -
                        tab_handle_resource->aperture().height();

  // In the inset case, the |decoration_tab_| nine-patch layer should not have a
  // margin that is greater than the content size of the layer.  This case can
  // happen at initialization.  The sizes used below are just temp values until
  // the real content sizes arrive.
  if (margin_width >= width) {
    // Shift the left edge over by the adjusted amount for more
    // aesthetic animations.
    x = x - (margin_width - width);
    width = margin_width;
  }
  if (margin_height >= height) {
    y = y - (margin_height - height);
    height = margin_height;
  }
  gfx::Size tab_bounds(width, height);

  layer_->SetPosition(gfx::PointF(x, y));
  DecorationTitle* title_layer = nullptr;
  if (layer_title_cache_)
    title_layer = layer_title_cache_->GetTitleLayer(id);

  if (title_layer) {
    title_layer->setOpacity(1.0f);
    unsigned expected_children = 4;
    title_layer_ = title_layer->layer();
    if (layer_->children().size() < expected_children) {
      layer_->AddChild(title_layer_);
    } else if (layer_->children()[expected_children - 1]->id() !=
               title_layer_->id()) {
      layer_->ReplaceChild((layer_->children()[expected_children - 1]).get(),
                           title_layer_);
    }
    title_layer->SetUIResourceIds();
  } else if (title_layer_.get()) {
    title_layer_->RemoveFromParent();
    title_layer_ = nullptr;
  }

  decoration_tab_->SetUIResourceId(tab_handle_resource->ui_resource()->id());
  decoration_tab_->SetAperture(tab_handle_resource->aperture());
  decoration_tab_->SetFillCenter(true);
  decoration_tab_->SetBounds(tab_bounds);
  decoration_tab_->SetBorder(
      tab_handle_resource->Border(decoration_tab_->bounds()));

  tab_outline_->SetUIResourceId(
      tab_handle_outline_resource->ui_resource()->id());
  tab_outline_->SetAperture(tab_handle_outline_resource->aperture());
  tab_outline_->SetFillCenter(true);
  tab_outline_->SetBounds(tab_bounds);
  tab_outline_->SetBorder(
      tab_handle_outline_resource->Border(tab_outline_->bounds()));

  if (foreground_) {
    decoration_tab_->SetPosition(gfx::PointF(original_x, original_y));
    tab_outline_->SetPosition(gfx::PointF(original_x, original_y));
  } else {
    decoration_tab_->SetPosition(gfx::PointF(0, 0));
    tab_outline_->SetPosition(gfx::PointF(0, 0));
  }

  close_button_->SetUIResourceId(close_button_resource->ui_resource()->id());
  close_button_->SetBounds(close_button_resource->size());
  const float padding_right = tab_handle_resource->size().width() -
                              tab_handle_resource->padding().right();
  const float padding_left = tab_handle_resource->padding().x();
  const float close_width = close_button_->bounds().width();
  if (title_layer) {
    int title_y = tab_handle_resource->padding().y() / 2 + height / 2 -
                  title_layer->size().height() / 2;
    int title_x = is_rtl ? padding_left + close_width : padding_left;
    title_x += is_rtl ? 0 : content_offset_x;
    title_layer->setBounds(gfx::Size(
        width - padding_right - padding_left - close_width - content_offset_x,
        height));
    if (foreground_) {
      title_x += original_x;
      title_y += original_y;
    }
    title_layer->layer()->SetPosition(gfx::PointF(title_x, title_y));
    if (is_loading) {
      title_layer->SetIsLoading(true);
      title_layer->SetSpinnerRotation(spinner_rotation);
    } else {
      title_layer->SetIsLoading(false);
    }
  }

  if (close_button_alpha == 0.f) {
    close_button_->SetIsDrawable(false);
  } else {
    close_button_->SetIsDrawable(true);
    const float close_max_width = close_button_->bounds().width();
    int close_y = (tab_handle_resource->padding().y() + height) / 2 -
                  close_button_->bounds().height() / 2;
    int close_x = is_rtl ? padding_left - close_max_width + close_width
                         : width - padding_right - close_width;
    if (foreground_) {
      close_y += original_y;
      close_x += original_x;
    }

    close_button_->SetPosition(gfx::PointF(close_x, close_y));
    close_button_->SetOpacity(close_button_alpha);
  }
}

scoped_refptr<cc::Layer> TabHandleLayer::layer() {
  return layer_;
}

TabHandleLayer::TabHandleLayer(LayerTitleCache* layer_title_cache)
    : layer_title_cache_(layer_title_cache),
      layer_(cc::Layer::Create()),
      close_button_(cc::UIResourceLayer::Create()),
      decoration_tab_(cc::NinePatchLayer::Create()),
      tab_outline_(cc::NinePatchLayer::Create()),
      brightness_(1.0f),
      foreground_(false) {
  decoration_tab_->SetIsDrawable(true);
  tab_outline_->SetIsDrawable(true);
  layer_->AddChild(decoration_tab_);
  layer_->AddChild(tab_outline_);
  layer_->AddChild(close_button_);
}

TabHandleLayer::~TabHandleLayer() {
}

}  // namespace android
