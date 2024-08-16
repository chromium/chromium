// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/tab_handle_layer.h"

#include <math.h>

#include <vector>

#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/metrics/field_trial_params.h"
#include "cc/resources/scoped_ui_resource.h"
#include "cc/slim/filter.h"
#include "cc/slim/layer.h"
#include "cc/slim/nine_patch_layer.h"
#include "cc/slim/solid_color_layer.h"
#include "chrome/browser/android/compositor/decoration_title.h"
#include "chrome/browser/android/compositor/layer_title_cache.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
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
    ui::Resource* close_button_background_resource,
    ui::Resource* divider_resource,
    ui::NinePatchResource* tab_handle_resource,
    ui::NinePatchResource* tab_handle_outline_resource,
    bool foreground,
    bool shouldShowTabOutline,
    bool close_pressed,
    float toolbar_width,
    float x,
    float y,
    float width,
    float height,
    float content_offset_y,
    float divider_offset_x,
    float bottom_margin,
    float top_margin,
    float close_button_padding,
    float close_button_alpha,
    bool is_start_divider_visible,
    bool is_end_divider_visible,
    bool is_loading,
    float spinner_rotation,
    float opacity) {
  if (foreground != foreground_ || opacity != opacity_) {
    foreground_ = foreground;
    opacity_ = opacity;
  }

  y += top_margin;
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
  height -= top_margin;
  height = ceil(height);
  gfx::Size tab_bounds(width, height - bottom_margin);

  layer_->SetPosition(gfx::PointF(x, y));

  DecorationTitle* title_layer = nullptr;
  // Only pull if tab id is valid.
  if (layer_title_cache_ && id != -1) {
    title_layer = layer_title_cache_->GetTitleLayer(id);
  }

  if (title_layer) {
    title_layer->setOpacity(1.0f);
    unsigned expected_children = 4;
    title_layer_ = title_layer->layer();
    if (tab_->children().size() < expected_children) {
      tab_->AddChild(title_layer_);
    } else if (tab_->children()[expected_children - 1]->id() !=
               title_layer_->id()) {
      tab_->ReplaceChild((tab_->children()[expected_children - 1]).get(),
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
  decoration_tab_->SetOpacity(opacity_);

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

  // Display the tab outline only for the currently selected tab in group when
  // TabGroupIndicator is enabled.
  if (shouldShowTabOutline) {
    tab_outline_->SetIsDrawable(true);
  } else {
    tab_outline_->SetIsDrawable(false);
  }

  close_button_->SetUIResourceId(close_button_resource->ui_resource()->id());
  close_button_->SetBounds(close_button_resource->size());

  close_button_hover_highlight_->SetUIResourceId(
      close_button_background_resource->ui_resource()->id());
  close_button_hover_highlight_->SetBounds(
      close_button_background_resource->size());

  const float padding_right = tab_handle_resource->size().width() -
                              tab_handle_resource->padding().right();
  const float padding_left = tab_handle_resource->padding().x();

  float close_width = close_button_->bounds().width() - close_button_padding;

  // If close button is not shown, fill
  // the remaining space with the title text
  if (close_button_alpha == 0.f) {
    close_width = 0.f;
  }

  int divider_y = content_offset_y;
  int divider_width = divider_resource->size().width();

  if (!is_start_divider_visible) {
    start_divider_->SetIsDrawable(false);
  } else {
    start_divider_->SetIsDrawable(true);
    start_divider_->SetUIResourceId(divider_resource->ui_resource()->id());
    start_divider_->SetBounds(divider_resource->size());
    int divider_x =
        is_rtl ? width - divider_width - divider_offset_x : divider_offset_x;
    if (foreground_) {
      divider_x += original_x;
    }
    start_divider_->SetPosition(gfx::PointF(divider_x, divider_y));
    start_divider_->SetOpacity(1.0f);
  }

  if (!is_end_divider_visible) {
    end_divider_->SetIsDrawable(false);
  } else {
    end_divider_->SetIsDrawable(true);
    end_divider_->SetUIResourceId(divider_resource->ui_resource()->id());
    end_divider_->SetBounds(divider_resource->size());
    int divider_x =
        is_rtl ? divider_offset_x : width - divider_width - divider_offset_x;
    if (foreground_) {
      divider_x += original_x;
    }
    end_divider_->SetPosition(gfx::PointF(divider_x, divider_y));
    end_divider_->SetOpacity(1.0f);
  }

  if (title_layer) {
    int title_y;
    float title_y_offset_mid = (tab_handle_resource->padding().y() + height -
                                title_layer->size().height()) /
                               2;
    // 8dp top padding for folio.
    title_y = std::min(content_offset_y, title_y_offset_mid);

    int title_x = is_rtl ? padding_left + close_width : padding_left;
    title_layer->setBounds(
        gfx::Size(width - padding_right - padding_left - close_width, height));
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
    close_button_hover_highlight_->SetIsDrawable(false);
  } else {
    close_button_->SetIsDrawable(true);
    close_button_hover_highlight_->SetIsDrawable(true);
    int close_y;

    // Close button image is larger than divider image, so close button will
    // appear slightly lower even the close_y are set in the same value as
    // divider_y. Thus need this offset to account for the effect of image
    // size difference has on close_y.
    int close_y_offset_tsr =
        std::max(0, (close_button_resource->size().height() -
                     divider_resource->size().height()) /
                        2);
    close_y = content_offset_y - std::abs(close_y_offset_tsr);

    int close_x = is_rtl ? padding_left - close_button_padding
                         : width - padding_right - close_width;
    if (foreground_) {
      close_x += original_x;
      close_y += original_y;
    }

    float background_left_offset =
        (close_button_background_resource->size().width() -
         close_button_resource->size().width()) /
        2;
    float background_top_offset =
        (close_button_background_resource->size().height() -
         close_button_resource->size().height()) /
        2;
    close_button_->SetPosition(
        gfx::PointF(background_left_offset, background_top_offset));
    close_button_->SetOpacity(close_button_alpha);
    close_button_hover_highlight_->SetPosition(gfx::PointF(close_x, close_y));
  }
}

scoped_refptr<cc::slim::Layer> TabHandleLayer::layer() {
  return layer_;
}

TabHandleLayer::TabHandleLayer(LayerTitleCache* layer_title_cache)
    : layer_title_cache_(layer_title_cache),
      layer_(cc::slim::Layer::Create()),
      tab_(cc::slim::Layer::Create()),
      close_button_(cc::slim::UIResourceLayer::Create()),
      close_button_hover_highlight_(cc::slim::UIResourceLayer::Create()),
      start_divider_(cc::slim::UIResourceLayer::Create()),
      end_divider_(cc::slim::UIResourceLayer::Create()),
      decoration_tab_(cc::slim::NinePatchLayer::Create()),
      tab_outline_(cc::slim::NinePatchLayer::Create()),
      brightness_(1.0f),
      foreground_(false) {
  decoration_tab_->SetIsDrawable(true);

  tab_->AddChild(decoration_tab_);
  tab_->AddChild(tab_outline_);
  tab_->AddChild(close_button_hover_highlight_);
  close_button_hover_highlight_->AddChild(close_button_);

  // The divider is added as a separate child so its opacity can be controlled
  // separately from the other tab items.
  layer_->AddChild(tab_);
  layer_->AddChild(start_divider_);
  layer_->AddChild(end_divider_);
}

TabHandleLayer::~TabHandleLayer() {
}

}  // namespace android
