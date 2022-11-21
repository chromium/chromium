// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/top_icon_animation_view.h"

#include <memory>
#include <utility>

#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/style/ash_color_id.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace ash {

TopIconAnimationView::TopIconAnimationView(AppsGridView* grid,
                                           const gfx::ImageSkia& icon,
                                           const std::u16string& title,
                                           const gfx::Rect& scaled_rect,
                                           bool open_folder,
                                           bool item_in_folder_icon)
    : grid_(grid),
      icon_(nullptr),
      title_(nullptr),
      scaled_rect_(scaled_rect),
      open_folder_(open_folder),
      item_in_folder_icon_(item_in_folder_icon) {
  icon_size_ = grid->app_list_config()->grid_icon_size();
  DCHECK(!icon.isNull());
  gfx::ImageSkia resized(gfx::ImageSkiaOperations::CreateResizedImage(
      icon, skia::ImageOperations::RESIZE_BEST, icon_size_));
  auto icon_image = std::make_unique<views::ImageView>();
  icon_image->SetImage(resized);
  icon_ = AddChildView(std::move(icon_image));

  auto title_label = std::make_unique<views::Label>();
  title_label->SetBackgroundColor(SK_ColorTRANSPARENT);
  title_label->SetAutoColorReadabilityEnabled(false);
  title_label->SetHandlesTooltips(false);
  title_label->SetFontList(grid_->app_list_config()->app_title_font());
  title_label->SetLineHeight(
      grid_->app_list_config()->app_title_max_line_height());
  title_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  title_label->SetEnabledColorId(cros_tokens::kTextColorPrimary);
  title_label->SetText(title);
  if (item_in_folder_icon_) {
    // The title's opacity of the item should be changed separately if it is in
    // the folder item's icon.
    title_label->SetPaintToLayer();
    title_label->layer()->SetFillsBoundsOpaquely(false);
  }
  title_ = AddChildView(std::move(title_label));

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

TopIconAnimationView::~TopIconAnimationView() {
  // Required due to RequiresNotificationWhenAnimatorDestroyed() returning true.
  // See ui::LayerAnimationObserver for details.
  StopObservingImplicitAnimations();
}

void TopIconAnimationView::AddObserver(TopIconAnimationObserver* observer) {
  observers_.AddObserver(observer);
}

void TopIconAnimationView::RemoveObserver(TopIconAnimationObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TopIconAnimationView::TransformView(base::TimeDelta duration) {
  // Transform used for scaling down the icon and move it back inside to the
  // original folder icon. The transform's origin is this view's origin.
  gfx::Transform transform;
  transform.Translate(scaled_rect_.x() - GetMirroredX(),
                      scaled_rect_.y() - bounds().y());
  transform.Scale(
      static_cast<double>(scaled_rect_.width()) / bounds().width(),
      static_cast<double>(scaled_rect_.height()) / bounds().height());
  if (open_folder_) {
    // Transform to a scaled down icon inside the original folder icon.
    layer()->SetTransform(transform);
  }

  if (!item_in_folder_icon_)
    layer()->SetOpacity(open_folder_ ? 0.0f : 1.0f);

  // Animate the icon to its target location and scale when opening or
  // closing a folder.
  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  settings.AddObserver(this);
  settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  settings.SetTransitionDuration(duration);
  layer()->SetTransform(open_folder_ ? gfx::Transform() : transform);
  if (!item_in_folder_icon_)
    layer()->SetOpacity(open_folder_ ? 1.0f : 0.0f);

  if (item_in_folder_icon_) {
    // Animate the opacity of the title.
    title_->layer()->SetOpacity(open_folder_ ? 0.0f : 1.0f);
    ui::ScopedLayerAnimationSettings title_settings(
        title_->layer()->GetAnimator());
    title_settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    title_settings.SetTransitionDuration(duration);
    title_->layer()->SetOpacity(open_folder_ ? 1.0f : 0.0f);
  }
}

const char* TopIconAnimationView::GetClassName() const {
  return "TopIconAnimationView";
}

gfx::Size TopIconAnimationView::CalculatePreferredSize() const {
  return gfx::Size(grid_->app_list_config()->grid_tile_width(),
                   grid_->app_list_config()->grid_tile_height());
}

void TopIconAnimationView::Layout() {
  // This view's layout should be the same as AppListItemView's.
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  icon_->SetBoundsRect(AppListItemView::GetIconBoundsForTargetViewBounds(
      grid_->app_list_config(), rect, icon_->GetImage().size(),
      /*icon_scale=*/1.0f));
  title_->SetBoundsRect(AppListItemView::GetTitleBoundsForTargetViewBounds(
      grid_->app_list_config(), rect, title_->GetPreferredSize(),
      /*icon_scale=*/1.0f));
}

void TopIconAnimationView::OnImplicitAnimationsCompleted() {
  SetVisible(false);
  for (auto& observer : observers_)
    observer.OnTopIconAnimationsComplete(this);
  DCHECK(parent());
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, parent()->RemoveChildViewT(this));
}

bool TopIconAnimationView::RequiresNotificationWhenAnimatorDestroyed() const {
  return true;
}

}  // namespace ash
