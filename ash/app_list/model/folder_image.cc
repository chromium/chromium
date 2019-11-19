// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/folder_image.h"

#include <memory>
#include <vector>

#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_item_list.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_config_provider.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/scoped_canvas.h"

namespace ash {

namespace {

// The shadow blur of icon.
constexpr int kIconShadowBlur = 5;

// The shadow color of icon.
constexpr SkColor kIconShadowColor = SkColorSetA(SK_ColorBLACK, 31);

// Generates the folder icon with the top 4 child item icons laid in 2x2 tile.
class FolderImageSource : public gfx::CanvasImageSource {
 public:
  typedef std::vector<gfx::ImageSkia> Icons;

  FolderImageSource(const AppListConfig& app_list_config,
                    const Icons& icons,
                    const gfx::Size& size);
  ~FolderImageSource() override;

 private:
  void DrawIcon(gfx::Canvas* canvas,
                const gfx::ImageSkia& icon,
                const gfx::Size icon_size,
                int x,
                int y);

  // gfx::CanvasImageSource overrides:
  void Draw(gfx::Canvas* canvas) override;

  const AppListConfig& app_list_config_;
  Icons icons_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(FolderImageSource);
};

FolderImageSource::FolderImageSource(const AppListConfig& app_list_config,
                                     const Icons& icons,
                                     const gfx::Size& size)
    : gfx::CanvasImageSource(size),
      app_list_config_(app_list_config),
      icons_(icons),
      size_(size) {
  DCHECK(icons.size() <= FolderImage::kNumFolderTopItems);
}

FolderImageSource::~FolderImageSource() = default;

void FolderImageSource::DrawIcon(gfx::Canvas* canvas,
                                 const gfx::ImageSkia& icon,
                                 const gfx::Size icon_size,
                                 int x,
                                 int y) {
  if (icon.isNull())
    return;

  const gfx::ImageSkia resized(gfx::ImageSkiaOperations::CreateResizedImage(
      icon, skia::ImageOperations::RESIZE_BEST, icon_size));

  // Draw a shadowed icon on the specified location.
  const gfx::ShadowValues shadow = {
      gfx::ShadowValue(gfx::Vector2d(), kIconShadowBlur, kIconShadowColor)};
  const gfx::ImageSkia shadowed(
      gfx::ImageSkiaOperations::CreateImageWithDropShadow(resized, shadow));

  // Offset the shadowed image so the actual image position matches its original
  // bounds. The offset has to be calculated in pixels, as the shadow margin
  // might not match the 1x margin scaled to the target scale factor (when the
  // drawing the shadow, the shadow margin is first scaled then rounded, which
  // might introduce different rounding error depending on the scale factor).
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();

  gfx::Insets shadow_margin =
      gfx::ShadowValue::GetMargin({shadow[0].Scale(scale)});
  const gfx::ImageSkiaRep& shadowed_rep = shadowed.GetRepresentation(scale);

  canvas->DrawImageIntInPixel(
      shadowed_rep, x * scale + shadow_margin.left(),
      y * scale + shadow_margin.top(), scale * shadowed.width(),
      scale * shadowed.height(), true, cc::PaintFlags());
}

void FolderImageSource::Draw(gfx::Canvas* canvas) {
  gfx::PointF bubble_center(size().width() / 2, size().height() / 2);
  bubble_center.Offset(0, -app_list_config_.folder_bubble_y_offset());

  // Draw circle for folder bubble.
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(app_list_config_.folder_bubble_color());
  canvas->DrawCircle(bubble_center, app_list_config_.folder_bubble_radius(),
                     flags);

  if (icons_.size() == 0)
    return;

  // Draw top items' icons.
  const size_t num_items =
      std::min(FolderImage::kNumFolderTopItems, icons_.size());
  std::vector<gfx::Rect> top_icon_bounds = FolderImage::GetTopIconsBounds(
      app_list_config_, gfx::Rect(size()), num_items);

  for (size_t i = 0; i < num_items; ++i) {
    DrawIcon(canvas, icons_[i],
             app_list_config_.item_icon_in_folder_icon_size(),
             top_icon_bounds[i].x(), top_icon_bounds[i].y());
  }
}

}  // namespace

// static
const size_t FolderImage::kNumFolderTopItems = 4;

FolderImage::FolderImage(const AppListConfig* app_list_config,
                         AppListItemList* item_list)
    : app_list_config_(app_list_config), item_list_(item_list) {
  DCHECK(app_list_config_);
  item_list_->AddObserver(this);
}

FolderImage::~FolderImage() {
  for (auto* item : top_items_)
    item->RemoveObserver(this);
  item_list_->RemoveObserver(this);
}

void FolderImage::UpdateIcon() {
  for (auto* item : top_items_)
    item->RemoveObserver(this);
  top_items_.clear();

  for (size_t i = 0;
       i < item_list_->item_count() && top_items_.size() < kNumFolderTopItems;
       ++i) {
    AppListItem* item = item_list_->item_at(i);
    // If this item is currently being dragged, pretend it has already left our
    // folder
    if (item == dragged_item_)
      continue;
    item->AddObserver(this);
    top_items_.push_back(item);
  }
  RedrawIconAndNotify();
}

void FolderImage::UpdateDraggedItem(const AppListItem* dragged_item) {
  DCHECK(dragged_item_ != dragged_item);
  dragged_item_ = dragged_item;
  UpdateIcon();
}

// static
std::vector<gfx::Rect> FolderImage::GetTopIconsBounds(
    const AppListConfig& app_list_config,
    const gfx::Rect& folder_icon_bounds,
    size_t num_items) {
  DCHECK_LE(num_items, kNumFolderTopItems);
  std::vector<gfx::Rect> top_icon_bounds;

  const AppListConfig& base_config =
      app_list_config.type() == ash::AppListConfigType::kShared
          ? AppListConfig::instance()
          : *AppListConfigProvider::Get().GetConfigForType(
                app_list_config.type(), true /*can_create*/);

  // The folder icons are generated as unclipped icons for default app list
  // config, and then scaled down to the required unclipped folder size as
  // needed (if clipped icon is needed, the unclipped icon bounds are clipped to
  // the target size).
  // This method goes through a similar flow:
  // 1.   Calculate the top icon bounds in the default unclipped folder icon.
  // 2.   Scale the bounds to the target config unclipped folder icon size.
  // 3.   Translate the bound to adjust for clipped bounds size (expected to be
  //      the |folder_icon_bounds| size).
  // 4.   Translate to bounds to adjust for the clipped bounds origin (expected
  //      to be the |folder_icon_bounds| origin).
  // Steps 2 - 4 are done using |scale_and_translate_bounds|.
  const int item_icon_dimension =
      base_config.item_icon_in_folder_icon_dimension();
  const int folder_unclipped_icon_dimension =
      base_config.folder_unclipped_icon_dimension();
  gfx::Point icon_center(folder_unclipped_icon_dimension / 2,
                         folder_unclipped_icon_dimension / 2);
  const gfx::Rect center_rect(icon_center.x() - item_icon_dimension / 2,
                              icon_center.y() - item_icon_dimension / 2,
                              item_icon_dimension, item_icon_dimension);

  const int origin_offset =
      (item_icon_dimension + base_config.item_icon_in_folder_icon_margin()) / 2;

  const int scaled_folder_unclipped_icon_dimension =
      app_list_config.folder_unclipped_icon_dimension();
  auto scale_and_translate_bounds = [folder_icon_bounds,
                                     folder_unclipped_icon_dimension,
                                     scaled_folder_unclipped_icon_dimension](
                                        const gfx::Rect& original) {
    const float scale =
        static_cast<float>(scaled_folder_unclipped_icon_dimension) /
        folder_unclipped_icon_dimension;
    gfx::Rect bounds = gfx::ScaleToRoundedRect(original, scale, scale);
    const int clipped_image_offset =
        (scaled_folder_unclipped_icon_dimension - folder_icon_bounds.width()) /
        2;
    bounds.Offset(-clipped_image_offset, -clipped_image_offset);
    bounds.Offset(folder_icon_bounds.x(), folder_icon_bounds.y());
    return bounds;
  };

  if (num_items == 1) {
    // Center icon bounds.
    top_icon_bounds.emplace_back(scale_and_translate_bounds(center_rect));
    return top_icon_bounds;
  }

  if (num_items == 2) {
    // Left icon bounds.
    gfx::Rect left_rect = center_rect;
    left_rect.Offset(-origin_offset, 0);
    top_icon_bounds.emplace_back(scale_and_translate_bounds(left_rect));

    // Right icon bounds.
    gfx::Rect right_rect = center_rect;
    right_rect.Offset(origin_offset, 0);
    top_icon_bounds.emplace_back(scale_and_translate_bounds(right_rect));
    return top_icon_bounds;
  }

  // Top left icon bounds.
  gfx::Rect top_left_rect = center_rect;
  top_left_rect.Offset(-origin_offset, -origin_offset);
  top_icon_bounds.emplace_back(scale_and_translate_bounds(top_left_rect));

  // Top right icon bounds.
  gfx::Rect top_right_rect = center_rect;
  top_right_rect.Offset(origin_offset, -origin_offset);
  top_icon_bounds.emplace_back(scale_and_translate_bounds(top_right_rect));

  if (num_items == 3) {
    // Bottom icon bounds.
    gfx::Rect bottom_rect = center_rect;
    bottom_rect.Offset(0, origin_offset);
    top_icon_bounds.emplace_back(scale_and_translate_bounds(bottom_rect));
    return top_icon_bounds;
  }

  // Bottom left icon bounds.
  gfx::Rect bottom_left_rect = center_rect;
  bottom_left_rect.Offset(-origin_offset, origin_offset);
  top_icon_bounds.emplace_back(scale_and_translate_bounds(bottom_left_rect));

  // Bottom right icon bounds.
  gfx::Rect bottom_right_rect = center_rect;
  bottom_right_rect.Offset(origin_offset, origin_offset);
  top_icon_bounds.emplace_back(scale_and_translate_bounds(bottom_right_rect));
  return top_icon_bounds;
}

gfx::Rect FolderImage::GetTargetIconRectInFolderForItem(
    const AppListConfig& app_list_config,
    AppListItem* item,
    const gfx::Rect& folder_icon_bounds) const {
  for (size_t i = 0; i < top_items_.size(); ++i) {
    if (item->id() == top_items_[i]->id()) {
      std::vector<gfx::Rect> rects = GetTopIconsBounds(
          app_list_config, folder_icon_bounds, top_items_.size());
      return rects[i];
    }
  }

  gfx::Rect target_rect(folder_icon_bounds);
  target_rect.ClampToCenteredSize(
      app_list_config.item_icon_in_folder_icon_size());
  return target_rect;
}

void FolderImage::AddObserver(FolderImageObserver* observer) {
  observers_.AddObserver(observer);
}

void FolderImage::RemoveObserver(FolderImageObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FolderImage::ItemIconChanged(ash::AppListConfigType config_type) {
  if (config_type != ash::AppListConfigType::kShared &&
      config_type != app_list_config_->type()) {
    return;
  }

  // Note: Must update the image only (cannot simply call UpdateIcon), because
  // UpdateIcon removes and re-adds the FolderImage as an observer of the
  // AppListItems, which causes the current iterator to call ItemIconChanged
  // again, and goes into an infinite loop.
  RedrawIconAndNotify();
}

void FolderImage::OnListItemAdded(size_t index, AppListItem* item) {
  if (index < kNumFolderTopItems)
    UpdateIcon();
}

void FolderImage::OnListItemRemoved(size_t index, AppListItem* item) {
  if (index < kNumFolderTopItems)
    UpdateIcon();
}

void FolderImage::OnListItemMoved(size_t from_index,
                                  size_t to_index,
                                  AppListItem* item) {
  if (from_index < kNumFolderTopItems || to_index < kNumFolderTopItems)
    UpdateIcon();
}

void FolderImage::RedrawIconAndNotify() {
  FolderImageSource::Icons top_icons;
  for (const auto* item : top_items_)
    top_icons.push_back(item->GetIcon(app_list_config_->type()));
  const gfx::Size icon_size = app_list_config_->folder_unclipped_icon_size();
  icon_ = gfx::ImageSkia(std::make_unique<FolderImageSource>(
                             *app_list_config_, top_icons, icon_size),
                         icon_size);

  for (auto& observer : observers_)
    observer.OnFolderImageUpdated(app_list_config_->type());
}

}  // namespace ash
