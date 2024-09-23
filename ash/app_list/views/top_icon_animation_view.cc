// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/top_icon_animation_view.h"

#include <memory>
#include <utility>

#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

TopIconAnimationView::TopIconAnimationView(AppsGridView* grid,
                                           const gfx::ImageSkia& icon,
                                           const gfx::ImageSkia& badge_icon,
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
  const bool is_badged = !badge_icon.isNull();

  const AppListConfig* app_list_config = grid->app_list_config();
  // For badged icon, add the background view behind the icon to mimic
  // appearance of the main app shortcut icon.
  if (is_badged) {
    icon_background_ = AddChildView(std::make_unique<views::View>());
    if (item_in_folder_icon_) {
      icon_background_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
      icon_background_->layer()->SetFillsBoundsOpaquely(false);
    } else {
      const int background_diameter =
          app_list_config->GetShortcutBackgroundContainerDimension();
      const gfx::RoundedCornersF rounded_corners(
          background_diameter, background_diameter,
          app_list_config->GetShortcutHostBadgeIconContainerDimension() / 2,
          background_diameter);
      icon_background_->SetBackground(views::CreateThemedRoundedRectBackground(
          cros_tokens::kCrosSysSystemOnBaseOpaque, rounded_corners, 0));
    }
  }
  icon_size_ = is_badged ? app_list_config->GetShortcutIconSize()
                         : app_list_config->grid_icon_size();
  DCHECK(!icon.isNull());
  gfx::ImageSkia resized(gfx::ImageSkiaOperations::CreateResizedImage(
      icon, skia::ImageOperations::RESIZE_BEST, icon_size_));
  auto icon_image = std::make_unique<views::ImageView>();
  icon_image->SetImage(resized);
  icon_ = AddChildView(std::move(icon_image));
  if (icon_background_ && icon_background_->layer()) {
    icon_->SetPaintToLayer();
    icon_->layer()->SetFillsBoundsOpaquely(false);
  }

  // Add badge view if the item is badged.
  if (is_badged) {
    badge_container_ = AddChildView(std::make_unique<views::View>());
    badge_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        gfx::Insets(
            app_list_config->shortcut_host_badge_icon_border_margin())));
    badge_container_->SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSystemOnBaseOpaque,
        app_list_config->GetShortcutHostBadgeIconContainerDimension() / 2));
    if (item_in_folder_icon_) {
      badge_container_->SetPaintToLayer();
      badge_container_->layer()->SetFillsBoundsOpaquely(false);
    }
    auto* badge_icon_view =
        badge_container_->AddChildView(std::make_unique<views::ImageView>());
    const gfx::Size badge_icon_size =
        gfx::Size(app_list_config->shortcut_host_badge_icon_dimension(),
                  app_list_config->shortcut_host_badge_icon_dimension());
    badge_icon_view->SetImage(gfx::ImageSkiaOperations::CreateResizedImage(
        badge_icon, skia::ImageOperations::RESIZE_BEST, badge_icon_size));
  }

  auto title_label = std::make_unique<views::Label>();
  title_label->SetBackgroundColor(SK_ColorTRANSPARENT);
  title_label->SetAutoColorReadabilityEnabled(false);
  title_label->SetHandlesTooltips(false);
  title_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);

  TypographyProvider::Get()->StyleLabel(
      app_list_config->type() == AppListConfigType::kDense
          ? TypographyToken::kCrosAnnotation1
          : TypographyToken::kCrosButton2,
      *title_label);
  title_label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  title_label->SetLineHeight(app_list_config->app_title_max_line_height());
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

  // Animate the badge opacity - the item icon within the folder icon is not
  // badged, while the icon in the folder view is badged.
  if (item_in_folder_icon_ && badge_container_) {
    badge_container_->layer()->SetOpacity(open_folder_ ? 0.0f : 1.0f);
    ui::ScopedLayerAnimationSettings badge_settings(
        badge_container_->layer()->GetAnimator());
    badge_settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    badge_settings.SetTransitionDuration(duration);
    badge_container_->layer()->SetOpacity(open_folder_ ? 1.0f : 0.0f);
  }

  // Animate the background size and shape - in the folder view UI, the icon
  // background (set for badge items only) is tearshaped, with bottom right
  // rounded corner smaller than other corners, and is larger than the icon
  // itself. Within the folder icon, the item icon does not have a background.
  // The animation makes the background bounds and shape match the icon bounds
  // and shape.
  if (item_in_folder_icon_ && icon_background_) {
    const gfx::Size background_size = icon_background_->layer()->size();
    const gfx::Rect collapsed_background = gfx::Rect(
        gfx::Point(
            std::round((background_size.width() - icon_size_.width()) / 2.0f),
            std::round((background_size.height() - icon_size_.height()) /
                       2.0f)),
        icon_size_);
    const gfx::Rect expanded_background = gfx::Rect(background_size);
    icon_background_->layer()->SetClipRect(open_folder_ ? collapsed_background
                                                        : expanded_background);

    const int background_diameter = background_size.width() / 2;
    const gfx::RoundedCornersF collapsed_rounded_corners(background_diameter);
    const gfx::RoundedCornersF expanded_rounded_corners(
        background_diameter, background_diameter,
        grid_->app_list_config()->GetShortcutHostBadgeIconContainerDimension() /
            2,
        background_diameter);
    icon_background_->layer()->SetRoundedCornerRadius(
        open_folder_ ? collapsed_rounded_corners : expanded_rounded_corners);

    ui::ScopedLayerAnimationSettings background_settings(
        icon_background_->layer()->GetAnimator());
    background_settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    background_settings.SetTransitionDuration(duration);

    icon_background_->layer()->SetClipRect(open_folder_ ? expanded_background
                                                        : collapsed_background);
    icon_background_->layer()->SetRoundedCornerRadius(
        open_folder_ ? expanded_rounded_corners : collapsed_rounded_corners);
  }

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

gfx::Size TopIconAnimationView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(grid_->app_list_config()->grid_tile_width(),
                   grid_->app_list_config()->grid_tile_height());
}

void TopIconAnimationView::OnThemeChanged() {
  views::View::OnThemeChanged();

  if (icon_background_ && icon_background_->layer()) {
    icon_background_->layer()->SetColor(
        GetColorProvider()->GetColor(cros_tokens::kCrosSysSystemOnBaseOpaque));
  }
}

void TopIconAnimationView::Layout(PassKey) {
  // This view's layout should be the same as AppListItemView's.
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  icon_->SetBoundsRect(AppListItemView::GetIconBoundsForTargetViewBounds(
      grid_->app_list_config(), rect, icon_->GetImage().size(),
      /*icon_scale=*/1.0f));
  if (icon_background_) {
    const int background_diameter =
        grid_->app_list_config()->GetShortcutBackgroundContainerDimension();
    icon_background_->SetBoundsRect(
        AppListItemView::GetIconBoundsForTargetViewBounds(
            grid_->app_list_config(), rect,
            gfx::Size(background_diameter, background_diameter),
            /*icon_scale=*/1.0f));
  }
  if (badge_container_) {
    CHECK(icon_background_);
    const int badge_container_diameter =
        grid_->app_list_config()->GetShortcutHostBadgeIconContainerDimension();
    badge_container_->SetBoundsRect(gfx::Rect(
        icon_background_->bounds().CenterPoint(),
        gfx::Size(badge_container_diameter, badge_container_diameter)));
  }
  title_->SetBoundsRect(AppListItemView::GetTitleBoundsForTargetViewBounds(
      grid_->app_list_config(), rect,
      title_->GetPreferredSize(views::SizeBounds(title_->width(), {})),
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

BEGIN_METADATA(TopIconAnimationView)
END_METADATA

}  // namespace ash
