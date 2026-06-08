// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/tab_strip_scene_layer.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/android/token_android.h"
#include "cc/slim/layer.h"
#include "cc/slim/nine_patch_layer.h"
#include "cc/slim/solid_color_layer.h"
#include "cc/slim/ui_resource_layer.h"
#include "chrome/browser/android/compositor/layer/group_indicator_layer.h"
#include "chrome/browser/android/compositor/layer/tab_handle_layer.h"
#include "chrome/browser/android/compositor/layer_title_cache.h"
#include "ui/android/resources/nine_patch_resource.h"
#include "ui/android/resources/resource_manager_impl.h"
#include "ui/base/l10n/l10n_util_android.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TabStripSceneLayer_jni.h"

using base::android::JavaRef;

namespace android {

TabStripSceneLayer::TabStripSceneLayer(JNIEnv* env,
                                       const JavaRef<jobject>& jobj)
    : SceneLayer(env, jobj),
      background_layer_(cc::slim::SolidColorLayer::Create()),
      tab_strip_layer_(cc::slim::SolidColorLayer::Create()),
      group_ui_parent_layer_(cc::slim::Layer::Create()),
      tab_ui_parent_layer_(cc::slim::Layer::Create()),
      foreground_layer_(cc::slim::Layer::Create()),
      foreground_tabs_(cc::slim::Layer::Create()),
      pinned_tabs_layer_(cc::slim::Layer::Create()),
      foreground_group_titles_(cc::slim::Layer::Create()),
      new_tab_button_(cc::slim::UIResourceLayer::Create()),
      new_tab_button_background_(cc::slim::UIResourceLayer::Create()),
      new_tab_button_keyboard_focus_ring_(cc::slim::UIResourceLayer::Create()),
      left_fade_(cc::slim::SolidColorLayer::Create()),
      right_fade_(cc::slim::SolidColorLayer::Create()),
      left_padding_layer_(cc::slim::SolidColorLayer::Create()),
      right_padding_layer_(cc::slim::SolidColorLayer::Create()),
      glic_button_container_(cc::slim::Layer::Create()),
      glic_button_(cc::slim::UIResourceLayer::Create()),
      glic_button_background_(cc::slim::SolidColorLayer::Create()),
      glic_button_text_(cc::slim::UIResourceLayer::Create()),
      glic_dismiss_nudge_button_(cc::slim::UIResourceLayer::Create()),
      glic_dismiss_nudge_button_keyboard_focus_ring_(
          cc::slim::UIResourceLayer::Create()),
      glic_button_keyboard_focus_ring_(cc::slim::NinePatchLayer::Create()),
      glic_actor_button_container_(cc::slim::Layer::Create()),
      glic_actor_button_(cc::slim::UIResourceLayer::Create()),
      glic_actor_button_background_(cc::slim::SolidColorLayer::Create()),
      glic_actor_button_text_(cc::slim::UIResourceLayer::Create()),
      glic_actor_button_keyboard_focus_ring_(
          cc::slim::NinePatchLayer::Create()),
      model_selector_button_(cc::slim::UIResourceLayer::Create()),
      model_selector_button_background_(cc::slim::UIResourceLayer::Create()),
      model_selector_button_keyboard_focus_ring_(
          cc::slim::UIResourceLayer::Create()),
      scrim_layer_(cc::slim::SolidColorLayer::Create()),
      content_tree_(nullptr) {
  new_tab_button_->SetIsDrawable(true);
  new_tab_button_background_->SetIsDrawable(true);
  glic_button_->SetIsDrawable(true);
  glic_button_background_->SetIsDrawable(true);
  glic_button_text_->SetIsDrawable(true);
  glic_dismiss_nudge_button_->SetIsDrawable(true);
  glic_actor_button_->SetIsDrawable(true);
  glic_actor_button_background_->SetIsDrawable(true);
  glic_actor_button_text_->SetIsDrawable(true);
  model_selector_button_->SetIsDrawable(true);
  model_selector_button_background_->SetIsDrawable(true);

  left_fade_->SetIsDrawable(true);
  right_fade_->SetIsDrawable(true);
  scrim_layer_->SetIsDrawable(true);
  left_padding_layer_->SetIsDrawable(true);
  right_padding_layer_->SetIsDrawable(true);

  // When the ScrollingStripStacker is used, the new tab button and tabs scroll,
  // while the incognito button and left/right fade stay fixed. Put the new tab
  // button and tabs in a separate layer placed visually below the others. Put
  // tab group indicators in a separate layer placed visually below the tabs.
  group_ui_parent_layer_->SetIsDrawable(true);
  tab_ui_parent_layer_->SetIsDrawable(true);
  foreground_layer_->SetIsDrawable(true);
  background_layer_->SetIsDrawable(true);
  foreground_tabs_->SetIsDrawable(true);
  foreground_group_titles_->SetIsDrawable(true);
  tab_strip_layer_->SetIsDrawable(true);
  pinned_tabs_layer_->SetIsDrawable(true);

  background_layer_->AddChild(tab_strip_layer_);
  background_layer_->AddChild(scrim_layer_);

  tab_strip_layer_->AddChild(group_ui_parent_layer_);
  tab_strip_layer_->AddChild(tab_ui_parent_layer_);
  foreground_layer_->AddChild(foreground_group_titles_);
  foreground_layer_->AddChild(foreground_tabs_);

  // Z-order matters: left_padding_layer_(right in rtl) acts as the background
  // for pinned tabs, so insert pinned_tabs_layer_ (and later foreground_layer_)
  // after padding_layer_. Then insert the opposite side padding after
  // pinned_tabs_layer_ and foreground_layer_, because the new tab button used
  // the fade layer as its background, and the minimize button(desktop) uses
  // the padding layer. Pinned tabs must render beneath these backgrounds to
  // avoid overlapping the buttons.
  if (l10n_util::IsLayoutRtl()) {
    tab_strip_layer_->AddChild(right_fade_);
    tab_strip_layer_->AddChild(right_padding_layer_);

    tab_strip_layer_->AddChild(pinned_tabs_layer_);
    tab_strip_layer_->AddChild(foreground_layer_);

    tab_strip_layer_->AddChild(left_fade_);
    tab_strip_layer_->AddChild(left_padding_layer_);
  } else {
    tab_strip_layer_->AddChild(left_fade_);
    tab_strip_layer_->AddChild(left_padding_layer_);

    tab_strip_layer_->AddChild(pinned_tabs_layer_);
    tab_strip_layer_->AddChild(foreground_layer_);

    tab_strip_layer_->AddChild(right_fade_);
    tab_strip_layer_->AddChild(right_padding_layer_);
  }
  tab_strip_layer_->AddChild(new_tab_button_background_);
  tab_strip_layer_->AddChild(new_tab_button_);
  tab_strip_layer_->AddChild(new_tab_button_keyboard_focus_ring_);

  glic_button_container_->SetMasksToBounds(true);
  glic_button_container_->AddChild(glic_button_background_);
  glic_button_container_->AddChild(glic_button_);
  glic_button_container_->AddChild(glic_button_text_);
  glic_button_container_->AddChild(glic_dismiss_nudge_button_);
  glic_button_container_->AddChild(
      glic_dismiss_nudge_button_keyboard_focus_ring_);
  tab_strip_layer_->AddChild(glic_button_container_);
  tab_strip_layer_->AddChild(glic_button_keyboard_focus_ring_);

  glic_actor_button_container_->SetMasksToBounds(true);
  glic_actor_button_container_->AddChild(glic_actor_button_background_);
  glic_actor_button_container_->AddChild(glic_actor_button_);
  glic_actor_button_container_->AddChild(glic_actor_button_text_);
  tab_strip_layer_->AddChild(glic_actor_button_container_);
  tab_strip_layer_->AddChild(glic_actor_button_keyboard_focus_ring_);

  tab_strip_layer_->AddChild(model_selector_button_background_);
  tab_strip_layer_->AddChild(model_selector_button_);
  tab_strip_layer_->AddChild(model_selector_button_keyboard_focus_ring_);

  layer()->AddChild(background_layer_);
}

TabStripSceneLayer::~TabStripSceneLayer() = default;

void TabStripSceneLayer::SetConstants(JNIEnv* env,
                                      int32_t reorder_background_top_margin,
                                      int32_t reorder_background_bottom_margin,
                                      int32_t reorder_background_padding_short,
                                      int32_t reorder_background_padding_long,
                                      int32_t reorder_background_corner_radius,
                                      float tab_underline_thickness,
                                      float tab_underline_corner_radius,
                                      float tab_underline_bottom_margin) {
  GroupIndicatorLayer::SetConstants(
      reorder_background_top_margin, reorder_background_bottom_margin,
      reorder_background_padding_short, reorder_background_padding_long,
      reorder_background_corner_radius);

  TabHandleLayer::SetConstants(tab_underline_thickness,
                               tab_underline_corner_radius,
                               tab_underline_bottom_margin);
}

void TabStripSceneLayer::SetContentTree(JNIEnv* env,
                                        const JavaRef<jobject>& jcontent_tree) {
  SceneLayer* content_tree = FromJavaObject(env, jcontent_tree);
  if (content_tree_ &&
      (!content_tree_->layer()->parent() ||
       content_tree_->layer()->parent()->id() != layer()->id())) {
    content_tree_ = nullptr;
  }

  if (content_tree != content_tree_) {
    if (content_tree_) {
      content_tree_->layer()->RemoveFromParent();
    }
    content_tree_ = content_tree;
    if (content_tree) {
      layer()->InsertChild(content_tree->layer(), 0);
      content_tree->layer()->SetPosition(
          gfx::PointF(0, -layer()->position().y()));
    }
  }
}

void TabStripSceneLayer::BeginBuildingFrame(
    JNIEnv* env,
    bool visible,
    const JavaRef<jobject>& jresource_manager,
    const JavaRef<jobject>& jlayer_title_cache) {
  write_index_ = 0;
  group_write_index_ = 0;
  background_layer_->SetHideLayerAndSubtree(!visible);
  resource_manager_ =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  layer_title_cache_ = LayerTitleCache::FromJavaObject(jlayer_title_cache);
}

void TabStripSceneLayer::FinishBuildingFrame(JNIEnv* env) {
  resource_manager_ = nullptr;
  layer_title_cache_ = nullptr;
  if (background_layer_->hide_layer_and_subtree()) {
    return;
  }

  for (unsigned i = write_index_; i < tab_handle_layers_.size(); ++i) {
    tab_handle_layers_[i]->layer()->RemoveFromParent();
  }
  for (unsigned i = group_write_index_; i < group_title_layers_.size(); ++i) {
    group_title_layers_[i]->layer()->RemoveFromParent();
  }

  tab_handle_layers_.erase(tab_handle_layers_.begin() + write_index_,
                           tab_handle_layers_.end());
  group_title_layers_.erase(group_title_layers_.begin() + group_write_index_,
                            group_title_layers_.end());
}

void TabStripSceneLayer::UpdateOffsetTag(JNIEnv* env,
                                         const JavaRef<jobject>& joffset_tag) {
  viz::OffsetTag tag = cc::android::FromJavaOffsetTag(env, joffset_tag);
  layer()->SetOffsetTag(tag);
}

void TabStripSceneLayer::UpdateTabStripLayer(JNIEnv* env,
                                             int32_t width,
                                             int32_t height,
                                             float y_offset,
                                             int32_t background_color,
                                             int32_t scrim_color,
                                             float scrim_opacity,
                                             float left_padding,
                                             float right_padding,
                                             float top_padding) {
  gfx::RectF content(0, y_offset, width, height);
  layer()->SetPosition(gfx::PointF(0, y_offset));
  background_layer_->SetBounds(gfx::Size(width, height));
  background_layer_->SetBackgroundColor(SkColor4f::FromColor(background_color));

  float scrollable_strip_height = height - top_padding;
  tab_strip_layer_->SetBounds(gfx::Size(width, scrollable_strip_height));
  tab_strip_layer_->SetPosition(gfx::PointF(0, top_padding));

  // Content tree should not be affected by tab strip scene layer visibility.
  if (content_tree_) {
    content_tree_->layer()->SetPosition(gfx::PointF(0, -y_offset));
  }

  // Update left and right padding layers as required.
  if (left_padding == 0) {
    left_padding_layer_->SetHideLayerAndSubtree(true);
  } else {
    left_padding_layer_->SetHideLayerAndSubtree(false);
    left_padding_layer_->SetBounds(gfx::Size(left_padding, height));
    left_padding_layer_->SetBackgroundColor(
        SkColor4f::FromColor(background_color));
  }

  if (right_padding == 0) {
    right_padding_layer_->SetHideLayerAndSubtree(true);
  } else {
    right_padding_layer_->SetHideLayerAndSubtree(false);
    right_padding_layer_->SetBounds(gfx::Size(right_padding, height));
    right_padding_layer_->SetPosition(gfx::PointF(width - right_padding, 0));
    right_padding_layer_->SetBackgroundColor(
        SkColor4f::FromColor(background_color));
  }

  // Hide scrim layer if it's not visible.
  if (scrim_opacity == 0.f) {
    scrim_layer_->SetHideLayerAndSubtree(true);
    return;
  }

  // Set opacity and color
  scrim_layer_->SetOpacity(scrim_opacity);
  scrim_layer_->SetBounds(background_layer_->bounds());
  scrim_layer_->SetBackgroundColor(SkColor4f::FromColor(scrim_color));

  // Ensure layer is visible.
  scrim_layer_->SetHideLayerAndSubtree(false);
}

void TabStripSceneLayer::UpdateNewTabButton(
    JNIEnv* env,
    int32_t resource_id,
    int32_t bg_resource_id,
    float x,
    float y,
    float touch_target_offset,
    bool visible,
    bool should_apply_hover_highlight,
    int32_t tint,
    int32_t background_tint,
    float button_alpha,
    bool is_keyboard_focused,
    int32_t keyboard_focus_ring_resource_id,
    int32_t keyboard_focus_ring_color) {
  DCHECK(resource_manager_);
  ui::Resource* button_resource =
      resource_manager_->GetStaticResourceWithTint(resource_id, tint);
  ui::Resource* background_resource =
      resource_manager_->GetStaticResourceWithTint(bg_resource_id,
                                                   background_tint, true);
  ui::Resource* keyboard_focus_ring_drawable =
      resource_manager_->GetStaticResourceWithTint(
          keyboard_focus_ring_resource_id, keyboard_focus_ring_color, true);

  x += touch_target_offset;

  UpdateCompositorButton(new_tab_button_, new_tab_button_background_,
                         button_resource, background_resource, x, y, visible,
                         should_apply_hover_highlight, button_alpha,
                         new_tab_button_keyboard_focus_ring_,
                         is_keyboard_focused, keyboard_focus_ring_drawable);
}

// The Glic button layer can be constructed with the following dynamic layout:
//
//   <----------------------- e ----------------------->
//   ┌─────────────────────────────────────────────────┐
// ^ │                                                 │
// │ │       ┌────┐       ┌──────┐       ┌────┐        │
// d │ <-a-> │Icon│ <-b-> │ Text │ <-b-> │Btn │ <-c->  │
// │ │       └────┘       └──────┘       └────┘        │
// v │                                                 │
//   └─────────────────────────────────────────────────┘
//
// Where the values are:
//   * a = button_start_padding: The distance from the button's leading edge to
//         the icon.
//   * b = icon_text_padding: The distance between the icon/btn and the text.
//   * c = button_end_padding: The distance from the last visible child
//         layer's end to the button's end. This is implicitly handled by the
//         total `button_width`.
//   * d = button_height: The total height of the button.
//   * e = button_width: The total dynamic width of the button, calculated to
//         wrap the icons, text, and paddings.
void TabStripSceneLayer::UpdateGlicButton(
    JNIEnv* env,
    int32_t resource_id,
    float x,
    float y,
    float button_width,
    float button_height,
    bool visible,
    bool should_apply_hover_highlight,
    int32_t tint,
    bool should_tint,
    int32_t background_tint,
    float button_alpha,
    bool is_keyboard_focused,
    int32_t keyboard_focus_ring_resource_id,
    int32_t keyboard_focus_ring_color,
    float keyboard_focus_ring_offset,
    int32_t text_texture_id,
    float button_start_padding,
    float icon_text_padding,
    float corner_radius_outer,
    float corner_radius_inner,
    int32_t dismiss_resource_id,
    float dismiss_x,
    float dismiss_y,
    bool dismiss_visible,
    int32_t dismiss_tint,
    bool dismiss_is_keyboard_focused,
    int32_t dismiss_keyboard_focus_ring_resource_id,
    int32_t dismiss_keyboard_focus_ring_color) {
  DCHECK(resource_manager_);

  // Glic button outer edge is left in LTR (matches base asset), right in RTL.
  bool should_flip_keyboard_focus_ring = l10n_util::IsLayoutRtl();

  UpdateGlicButtonInternal(
      glic_button_container_, glic_button_background_, glic_button_,
      glic_button_text_, glic_button_keyboard_focus_ring_, resource_id, x, y,
      button_width, button_height, visible, tint, should_tint, background_tint,
      button_alpha, is_keyboard_focused, keyboard_focus_ring_resource_id,
      keyboard_focus_ring_color, keyboard_focus_ring_offset,
      should_flip_keyboard_focus_ring, text_texture_id, button_start_padding,
      icon_text_padding, corner_radius_outer, corner_radius_inner);

  // Dismiss Button (positions are relative to the parent container coordinates
  ui::Resource* dismiss_icon_resource =
      resource_manager_->GetStaticResourceWithTint(dismiss_resource_id,
                                                   dismiss_tint);
  ui::Resource* dismiss_focus_ring_drawable =
      resource_manager_->GetStaticResourceWithTint(
          dismiss_keyboard_focus_ring_resource_id,
          dismiss_keyboard_focus_ring_color, true);

  UpdateCompositorButton(
      glic_dismiss_nudge_button_, nullptr, dismiss_icon_resource, nullptr,
      dismiss_x - x, dismiss_y - y, dismiss_visible, false, button_alpha,
      glic_dismiss_nudge_button_keyboard_focus_ring_,
      dismiss_is_keyboard_focused, dismiss_focus_ring_drawable);
}

void TabStripSceneLayer::UpdateModelSelectorButton(
    JNIEnv* env,
    int32_t resource_id,
    int32_t bg_resource_id,
    float x,
    float y,
    bool visible,
    bool should_apply_hover_highlight,
    int32_t tint,
    int32_t background_tint,
    float button_alpha,
    bool is_keyboard_focused,
    int32_t keyboard_focus_ring_resource_id,
    int32_t keyboard_focus_ring_color) {
  DCHECK(resource_manager_);
  ui::Resource* button_resource =
      resource_manager_->GetStaticResourceWithTint(resource_id, tint);
  ui::Resource* background_resource =
      resource_manager_->GetStaticResourceWithTint(bg_resource_id,
                                                   background_tint, true);
  ui::Resource* keyboard_focus_ring_drawable =
      resource_manager_->GetStaticResourceWithTint(
          keyboard_focus_ring_resource_id, keyboard_focus_ring_color, true);

  UpdateCompositorButton(model_selector_button_,
                         model_selector_button_background_, button_resource,
                         background_resource, x, y, visible,
                         should_apply_hover_highlight, button_alpha,
                         model_selector_button_keyboard_focus_ring_,
                         is_keyboard_focused, keyboard_focus_ring_drawable);
}

void TabStripSceneLayer::UpdateGlicActorButton(
    JNIEnv* env,
    int32_t resource_id,
    float x,
    float y,
    float button_width,
    float button_height,
    bool visible,
    bool should_apply_hover_highlight,
    int32_t tint,
    bool should_tint,
    int32_t background_tint,
    float button_alpha,
    bool is_keyboard_focused,
    int32_t keyboard_focus_ring_resource_id,
    int32_t keyboard_focus_ring_color,
    float keyboard_focus_ring_offset,
    int32_t text_texture_id,
    float button_start_padding,
    float icon_text_padding,
    float corner_radius_outer,
    float corner_radius_inner) {
  // Actor button outer edge is right in LTR (needs flip), left in RTL.
  bool should_flip_keyboard_focus_ring = !l10n_util::IsLayoutRtl();

  UpdateGlicButtonInternal(
      glic_actor_button_container_, glic_actor_button_background_,
      glic_actor_button_, glic_actor_button_text_,
      glic_actor_button_keyboard_focus_ring_, resource_id, x, y, button_width,
      button_height, visible, tint, should_tint, background_tint, button_alpha,
      is_keyboard_focused, keyboard_focus_ring_resource_id,
      keyboard_focus_ring_color, keyboard_focus_ring_offset,
      should_flip_keyboard_focus_ring, text_texture_id, button_start_padding,
      icon_text_padding, corner_radius_outer, corner_radius_inner);
}

void TabStripSceneLayer::UpdateGlicButtonInternal(
    scoped_refptr<cc::slim::Layer> container_layer,
    scoped_refptr<cc::slim::SolidColorLayer> background_layer,
    scoped_refptr<cc::slim::UIResourceLayer> icon_layer,
    scoped_refptr<cc::slim::UIResourceLayer> text_layer,
    scoped_refptr<cc::slim::NinePatchLayer> focus_ring_layer,
    int32_t resource_id,
    float x,
    float y,
    float button_width,
    float button_height,
    bool visible,
    int32_t tint,
    bool should_tint,
    int32_t background_tint,
    float button_alpha,
    bool is_keyboard_focused,
    int32_t keyboard_focus_ring_resource_id,
    int32_t keyboard_focus_ring_color,
    float keyboard_focus_ring_offset,
    bool should_flip_keyboard_focus_ring,
    int32_t text_texture_id,
    float button_start_padding,
    float icon_text_padding,
    float corner_radius_outer,
    float corner_radius_inner) {
  DCHECK(resource_manager_);
  ui::Resource* icon_resource;
  if (should_tint) {
    icon_resource =
        resource_manager_->GetStaticResourceWithTint(resource_id, tint);
  } else {
    icon_resource = resource_manager_->GetResource(
        ui::ANDROID_RESOURCE_TYPE_STATIC, resource_id);
  }


  gfx::Size background_size(std::round(button_width),
                            std::round(button_height));
  gfx::Size icon_size = icon_resource->size();

  // 0. Parent Nesting Container
  container_layer->SetPosition(gfx::PointF(std::round(x), std::round(y)));
  container_layer->SetBounds(background_size);
  container_layer->SetHideLayerAndSubtree(!visible);
  container_layer->SetOpacity(button_alpha);

  // 1. Background
  background_layer->SetBackgroundColor(SkColor4f::FromColor(background_tint));
  background_layer->SetBounds(background_size);
  background_layer->SetPosition(gfx::PointF(0, 0));

  float computed_corner_radius_l =
      l10n_util::IsLayoutRtl() ? corner_radius_inner : corner_radius_outer;
  float computed_corner_radius_r =
      l10n_util::IsLayoutRtl() ? corner_radius_outer : corner_radius_inner;

  background_layer->SetRoundedCorner(
      gfx::RoundedCornersF(computed_corner_radius_l, computed_corner_radius_r,
                           computed_corner_radius_r, computed_corner_radius_l));

  // 2. Icon
  float icon_x_pos;
  float icon_y_offset = (background_size.height() - icon_size.height()) / 2;

  ui::Resource* text_resource = nullptr;
  if (text_texture_id != 0 && text_layer) {
    text_resource = resource_manager_->GetResource(
        ui::ANDROID_RESOURCE_TYPE_DYNAMIC, text_texture_id);
  }
  gfx::Size text_size = text_resource ? text_resource->size() : gfx::Size();
  bool has_text = text_resource && !text_size.IsEmpty();

  if (has_text) {
    icon_x_pos = l10n_util::IsLayoutRtl()
                     ? (button_width - button_start_padding - icon_size.width())
                     : (button_start_padding);
  } else {
    icon_x_pos = (button_width - icon_size.width()) / 2;
  }

  icon_layer->SetUIResourceId(icon_resource->ui_resource()->id());
  icon_layer->SetBounds(icon_size);
  icon_layer->SetPosition(
      gfx::PointF(std::round(icon_x_pos), std::round(icon_y_offset)));

  // 3. Text
  if (text_layer) {
    if (has_text) {
      text_layer->SetUIResourceId(text_resource->ui_resource()->id());
      text_layer->SetBounds(text_size);

      float text_y_offset = (background_size.height() - text_size.height()) / 2;
      float text_x_pos =
          l10n_util::IsLayoutRtl()
              ? (button_width - button_start_padding - icon_text_padding -
                 text_size.width())
              : (button_start_padding + icon_size.width() + icon_text_padding);

      text_layer->SetPosition(
          gfx::PointF(std::round(text_x_pos), std::round(text_y_offset)));
      text_layer->SetHideLayerAndSubtree(false);
    } else {
      text_layer->SetHideLayerAndSubtree(true);
    }
  }

  // 4. Focus Ring
  if (is_keyboard_focused && visible) {
    ui::NinePatchResource* keyboard_focus_ring_drawable =
        ui::NinePatchResource::From(
            resource_manager_->GetStaticResourceWithTint(
                keyboard_focus_ring_resource_id, keyboard_focus_ring_color,
                true));
    focus_ring_layer->SetIsDrawable(true);
    focus_ring_layer->SetUIResourceId(
        keyboard_focus_ring_drawable->ui_resource()->id());
    focus_ring_layer->SetAperture(keyboard_focus_ring_drawable->aperture());
    focus_ring_layer->SetPosition(
        gfx::PointF(std::round(x - keyboard_focus_ring_offset),
                    std::round(y - keyboard_focus_ring_offset)));
    focus_ring_layer->SetBounds(
        gfx::Size(std::round(button_width + keyboard_focus_ring_offset * 2),
                  std::round(button_height + keyboard_focus_ring_offset * 2)));
    focus_ring_layer->SetFillCenter(true);
    focus_ring_layer->SetBorder(
        keyboard_focus_ring_drawable->Border(focus_ring_layer->bounds()));

    // Flip the focus ring texture horizontally to swap rounded and straight
    // edges.
    if (should_flip_keyboard_focus_ring) {
      focus_ring_layer->SetTransformOrigin(
          gfx::PointF(focus_ring_layer->bounds().width() / 2.0f,
                      focus_ring_layer->bounds().height() / 2.0f));
      focus_ring_layer->SetTransform(gfx::Transform::MakeScale(-1.0f, 1.0f));
    } else {
      focus_ring_layer->SetTransform(gfx::Transform());
    }
  } else {
    focus_ring_layer->SetIsDrawable(false);
  }
}

void TabStripSceneLayer::UpdateCompositorButton(
    scoped_refptr<cc::slim::UIResourceLayer> button,
    scoped_refptr<cc::slim::UIResourceLayer> background,
    ui::Resource* button_resource,
    ui::Resource* background_resource,
    float x,
    float y,
    bool visible,
    bool should_apply_hover_highlight,
    float button_alpha,
    scoped_refptr<cc::slim::UIResourceLayer> keyboard_focus_ring_layer,
    bool is_keyboard_focused,
    ui::Resource* keyboard_focus_ring_drawable) {
  button->SetUIResourceId(button_resource->ui_resource()->id());
  button->SetBounds(button_resource->size());
  button->SetHideLayerAndSubtree(!visible);
  button->SetOpacity(button_alpha);

  gfx::Size button_size = button_resource->size();
  gfx::Size background_size =
      background_resource ? background_resource->size() : button_size;
  float x_offset = (background_size.width() - button_size.width()) / 2.f;
  float y_offset = (background_size.height() - button_size.height()) / 2.f;
  // Round this so that the keyboard focus ring looks centered with respect to
  // the rest of the button (see comment below).
  button->SetPosition(
      gfx::PointF(std::round(x + x_offset), std::round(y + y_offset)));

  if (background) {
    if (!should_apply_hover_highlight) {
      background->SetHideLayerAndSubtree(true);
    } else if (background_resource) {
      background->SetUIResourceId(background_resource->ui_resource()->id());
      // Round this so that the keyboard focus ring looks centered with respect
      // to the rest of the button (see comment below).
      background->SetPosition(gfx::PointF(std::round(x), std::round(y)));
      background->SetBounds(background_resource->size());
      background->SetHideLayerAndSubtree(!visible);
      background->SetOpacity(button_alpha);
    }
  }

  if (is_keyboard_focused && visible) {
    keyboard_focus_ring_layer->SetIsDrawable(true);
    keyboard_focus_ring_layer->SetUIResourceId(
        keyboard_focus_ring_drawable->ui_resource()->id());
    gfx::Size ring_size = keyboard_focus_ring_drawable->size();

    float ring_x_offset = (background_size.width() - ring_size.width()) / 2;
    float ring_y_offset = (background_size.height() - ring_size.height()) / 2;
    // Make sure that the focus ring is int-aligned. Otherwise, the bitmap will
    // look clipped/blurry.
    keyboard_focus_ring_layer->SetPosition(gfx::PointF(
        std::round(x + ring_x_offset), std::round(y + ring_y_offset)));
    keyboard_focus_ring_layer->SetBounds(ring_size);
  } else {
    // If the keyboard focus ring is already showing, make sure it stops
    // showing.
    keyboard_focus_ring_layer->SetIsDrawable(false);
  }
}

void TabStripSceneLayer::UpdateTabStripFade(JNIEnv* env,
                                            bool is_left,
                                            int32_t fade_color,
                                            float opacity,
                                            float gradient_width,
                                            float opaque_width,
                                            float padding) {
  // Act on the correct fade.
  cc::slim::SolidColorLayer& fade = is_left ? *left_fade_ : *right_fade_;

  // Hide layer if it's not visible.
  if (opacity == 0.f) {
    fade.SetHideLayerAndSubtree(true);
    return;
  }

  // Set opacity.
  fade.SetOpacity(opacity);

  // Set background color.
  fade.SetBackgroundColor(SkColor4f::FromColor(fade_color));

  // Set bounds.
  float width = opaque_width + gradient_width;
  float height = tab_strip_layer_->bounds().height();
  fade.SetBounds(gfx::Size(width, height));

  // Set position.
  int fade_x =
      is_left ? padding : tab_strip_layer_->bounds().width() - width - padding;
  fade.SetPosition(gfx::PointF(fade_x, 0));

  // Set gradient.
  gfx::LinearGradient gradient;
  gradient.AddStep(0.f, 255);
  gradient.AddStep(opaque_width / (opaque_width + gradient_width), 255);
  gradient.AddStep(1.f, 0);
  if (!is_left) {
    gradient.set_angle(180);
  }
  fade.SetContentsOpaque(false);
  fade.SetGradientMask(gradient);

  // Ensure layer is visible.
  fade.SetHideLayerAndSubtree(false);
}

void TabStripSceneLayer::PutStripTabLayer(
    JNIEnv* env,
    int32_t id,
    int32_t close_resource_id,
    int32_t close_hover_bg_resource_id,
    bool is_close_keyboard_focused,
    int32_t close_keyboard_focus_ring_resource_id,
    int32_t divider_resource_id,
    int32_t handle_resource_id,
    int32_t handle_outline_resource_id,
    int32_t close_tint,
    int32_t close_hover_bg_tint,
    int32_t divider_tint,
    int32_t handle_tint,
    int32_t handle_outline_tint,
    bool foreground,
    bool shouldShowTabOutline,
    bool close_pressed,
    bool should_hide_favicon,
    bool should_show_media_indicator,
    int32_t media_indicator_resource_id,
    int32_t media_indicator_tint,
    float media_indicator_width,
    float media_indicator_spacing,
    float media_indicator_internal_padding,
    float title_to_media_indicator_spacing,
    int32_t tab_indicator_overlay_resource_id,
    float tab_indicator_overlay_rotation,
    float tab_indicator_overlay_width,
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
    float width_to_hide_tab_title,
    bool is_start_divider_visible,
    bool is_end_divider_visible,
    bool is_loading,
    float spinner_rotation,
    float opacity,
    bool is_keyboard_focused,
    int32_t keyboard_focus_ring_resource_id,
    int32_t keyboard_focus_ring_color,
    int32_t keyboard_focus_ring_offset,
    int32_t stroke_width,
    float folio_foot_length,
    bool is_pinned,
    float pinned_icon_offset_x,
    bool is_underlined,
    int32_t underline_start_color,
    int32_t underline_end_color,
    int32_t underline_width_threshold) {
  DCHECK(layer_title_cache_);
  scoped_refptr<TabHandleLayer> layer = GetNextTabLayer(layer_title_cache_);

  if (foreground != layer->foreground() || is_pinned != layer->is_pinned()) {
    if (foreground != layer->foreground() && foreground) {
      foreground_tabs_->AddChild(layer->layer());
    } else if (is_pinned) {
      pinned_tabs_layer_->AddChild(layer->layer());
    } else {
      tab_ui_parent_layer_->AddChild(layer->layer());
    }
  }

  DCHECK(resource_manager_);
  ui::NinePatchResource* tab_handle_resource =
      ui::NinePatchResource::From(resource_manager_->GetStaticResourceWithTint(
          handle_resource_id, handle_tint, true));
  ui::NinePatchResource* tab_handle_outline_resource =
      ui::NinePatchResource::From(resource_manager_->GetStaticResourceWithTint(
          handle_outline_resource_id, handle_outline_tint));
  ui::Resource* close_button_resource =
      resource_manager_->GetStaticResourceWithTint(close_resource_id,
                                                   close_tint);
  ui::Resource* close_button_hover_resource =
      resource_manager_->GetStaticResourceWithTint(close_hover_bg_resource_id,
                                                   close_hover_bg_tint, true);
  ui::Resource* close_button_keyboard_focus_ring_resource =
      resource_manager_->GetStaticResourceWithTint(
          close_keyboard_focus_ring_resource_id, keyboard_focus_ring_color,
          true);
  ui::Resource* divider_resource = resource_manager_->GetStaticResourceWithTint(
      divider_resource_id, divider_tint, true);
  ui::NinePatchResource* keyboard_focus_ring_drawable =
      ui::NinePatchResource::From(resource_manager_->GetStaticResourceWithTint(
          keyboard_focus_ring_resource_id, keyboard_focus_ring_color));
  ui::Resource* media_indicator_drawable = nullptr;
  if (should_show_media_indicator) {
    media_indicator_drawable = resource_manager_->GetStaticResourceWithTint(
        media_indicator_resource_id, media_indicator_tint);
  }
  ui::Resource* media_indicator_overlay_drawable = nullptr;
  if (should_show_media_indicator && tab_indicator_overlay_resource_id != 0) {
    media_indicator_overlay_drawable =
        resource_manager_->GetStaticResourceWithTint(
            tab_indicator_overlay_resource_id, media_indicator_tint);
  }

  float media_indicator_opacity = 1.0f;
  if (media_indicator_tint == close_tint) {
    // Match close button opacity (0.7) if tints are the same, as the
    // media indicator is expected to look like the close button in such cases.
    media_indicator_opacity = 0.7f;
  }

  layer->SetProperties(
      id, close_button_resource, close_button_hover_resource,
      is_close_keyboard_focused, close_button_keyboard_focus_ring_resource,
      divider_resource, tab_handle_resource, tab_handle_outline_resource,
      foreground, is_pinned, shouldShowTabOutline, close_pressed,
      should_hide_favicon, should_show_media_indicator,
      media_indicator_drawable, media_indicator_width, media_indicator_spacing,
      media_indicator_internal_padding, title_to_media_indicator_spacing,
      media_indicator_opacity, media_indicator_overlay_drawable,
      tab_indicator_overlay_rotation, tab_indicator_overlay_width,
      toolbar_width, x, y, width, height, content_offset_y, divider_offset_x,
      bottom_margin, top_margin, close_button_padding, close_button_alpha,
      is_start_divider_visible, is_end_divider_visible, is_loading,
      spinner_rotation, opacity, is_keyboard_focused,
      keyboard_focus_ring_drawable, keyboard_focus_ring_offset, stroke_width,
      folio_foot_length, width_to_hide_tab_title, pinned_icon_offset_x,
      is_underlined, static_cast<SkColor>(underline_start_color),
      static_cast<SkColor>(underline_end_color), underline_width_threshold);
}

void TabStripSceneLayer::PutGroupIndicatorLayer(
    JNIEnv* env,
    bool incognito,
    bool foreground,
    bool collapsed,
    bool show_bubble,
    const base::android::JavaRef<jobject>& jgroup_token,
    int32_t tint,
    int32_t reorder_background_tint,
    int32_t bubble_tint,
    float x,
    float y,
    float width,
    float height,
    float title_start_padding,
    float title_end_padding,
    float corner_radius,
    float bottom_indicator_width,
    float bottom_indicator_height,
    float bubble_padding,
    float bubble_size,
    bool is_keyboard_focused,
    int32_t keyboard_focus_ring_resource_id,
    int32_t keyboard_focus_ring_color,
    int32_t keyboard_focus_ring_offset,
    int32_t keyboard_focus_ring_width) {
  DCHECK(layer_title_cache_);

  // Reuse existing layer if it exists.
  scoped_refptr<GroupIndicatorLayer> layer =
      GetNextGroupIndicatorLayer(layer_title_cache_);

  ui::NinePatchResource* keyboard_focus_ring_drawable =
      ui::NinePatchResource::From(resource_manager_->GetStaticResourceWithTint(
          keyboard_focus_ring_resource_id, keyboard_focus_ring_color));

  // Foreground if needed.
  if (foreground != layer->foreground()) {
    if (foreground) {
      foreground_group_titles_->AddChild(layer->layer());
    } else {
      group_ui_parent_layer_->AddChild(layer->layer());
    }
  }

  const tab_groups::TabGroupId& group_token =
      tab_groups::TabGroupId::FromRawToken(
          base::android::TokenAndroid::FromJavaToken(env, jgroup_token));
  layer->SetProperties(group_token, tint, reorder_background_tint, bubble_tint,
                       incognito, foreground, collapsed, show_bubble, x, y,
                       width, height, title_start_padding, title_end_padding,
                       corner_radius, bottom_indicator_width,
                       bottom_indicator_height, bubble_padding, bubble_size,
                       tab_strip_layer_->bounds().height(), is_keyboard_focused,
                       keyboard_focus_ring_drawable, keyboard_focus_ring_offset,
                       keyboard_focus_ring_width);
}

scoped_refptr<TabHandleLayer> TabStripSceneLayer::GetNextTabLayer(
    LayerTitleCache* layer_title_cache) {
  if (write_index_ < tab_handle_layers_.size()) {
    return tab_handle_layers_[write_index_++];
  }

  scoped_refptr<TabHandleLayer> layer_tree =
      TabHandleLayer::Create(layer_title_cache);
  tab_handle_layers_.push_back(layer_tree);
  tab_ui_parent_layer_->AddChild(layer_tree->layer());
  write_index_++;
  return layer_tree;
}

scoped_refptr<GroupIndicatorLayer>
TabStripSceneLayer::GetNextGroupIndicatorLayer(
    LayerTitleCache* layer_title_cache) {
  if (group_write_index_ < group_title_layers_.size()) {
    return group_title_layers_[group_write_index_++];
  }

  scoped_refptr<GroupIndicatorLayer> layer_tree =
      GroupIndicatorLayer::Create(layer_title_cache);
  group_title_layers_.push_back(layer_tree);
  group_ui_parent_layer_->AddChild(layer_tree->layer());
  group_write_index_++;
  return layer_tree;
}

bool TabStripSceneLayer::ShouldShowBackground() {
  if (content_tree_) {
    return content_tree_->ShouldShowBackground();
  }
  return SceneLayer::ShouldShowBackground();
}

SkColor TabStripSceneLayer::GetBackgroundColor() {
  if (content_tree_) {
    return content_tree_->GetBackgroundColor();
  }
  return SceneLayer::GetBackgroundColor();
}

static int64_t JNI_TabStripSceneLayer_Init(JNIEnv* env,
                                           const JavaRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  TabStripSceneLayer* scene_layer = new TabStripSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(scene_layer);
}

}  // namespace android

DEFINE_JNI(TabStripSceneLayer)
