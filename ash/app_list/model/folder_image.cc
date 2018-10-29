// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/folder_image.h"

#include <memory>
#include <vector>

#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_item_list.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_constants.h"
#include "ash/public/cpp/app_list/app_list_features.h"
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

namespace app_list {

namespace {

// The margin of item icon in folder icon.
constexpr int kItemIconMargin = 2;

// The shadow blur of icon.
constexpr int kIconShadowBlur = 5;

// The shadow color of icon.
constexpr SkColor kIconShadowColor = SkColorSetA(SK_ColorBLACK, 31);

// Generates the folder icon with the top 4 child item icons laid in 2x2 tile.
class FolderImageSource : public gfx::CanvasImageSource {
 public:
  typedef std::vector<gfx::ImageSkia> Icons;

  FolderImageSource(const Icons& icons,
                    const gfx::Size& size,
                    bool draw_shadow);
  ~FolderImageSource() override;

 private:
  void DrawIcon(gfx::Canvas* canvas,
                const gfx::ImageSkia& icon,
                const gfx::Size icon_size,
                int x,
                int y);

  // gfx::CanvasImageSource overrides:
  void Draw(gfx::Canvas* canvas) override;

  Icons icons_;
  gfx::Size size_;
  bool draw_shadow_;  // True if |icons| have shadows.

  DISALLOW_COPY_AND_ASSIGN(FolderImageSource);
};

FolderImageSource::FolderImageSource(const Icons& icons,
                                     const gfx::Size& size,
                                     bool draw_shadow)
    : gfx::CanvasImageSource(size, false),
      icons_(icons),
      size_(size),
      draw_shadow_(draw_shadow) {
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
  if (!draw_shadow_) {
    canvas->DrawImageInt(resized, 0, 0, resized.width(), resized.height(), x, y,
                         resized.width(), resized.height(), true);
    return;
  }

  // Draw a shadowed icon on the specified location.
  const gfx::ImageSkia shadowed(
      gfx::ImageSkiaOperations::CreateImageWithDropShadow(
          resized, gfx::ShadowValues(
                       1, gfx::ShadowValue(gfx::Vector2d(), kIconShadowBlur,
                                           kIconShadowColor))));
  const gfx::Size shadow_size = shadowed.size();
  x -= (shadow_size.width() - icon_size.width()) / 2;
  y -= (shadow_size.height() - icon_size.height()) / 2;
  canvas->DrawImageInt(shadowed, 0, 0, shadow_size.width(),
                       shadow_size.height(), x, y, shadow_size.width(),
                       shadow_size.height(), true);
}

void FolderImageSource::Draw(gfx::Canvas* canvas) {
  gfx::PointF bubble_center(size().width() / 2, size().height() / 2);
  bubble_center.Offset(0, -AppListConfig::instance().folder_bubble_y_offset());

  // Draw circle for folder bubble.
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(AppListConfig::instance().folder_bubble_color());
  canvas->DrawCircle(bubble_center,
                     AppListConfig::instance().folder_bubble_radius(), flags);

  if (icons_.size() == 0)
    return;

  // Draw top items' icons.
  const size_t num_items =
      std::min(FolderImage::kNumFolderTopItems, icons_.size());
  std::vector<gfx::Rect> top_icon_bounds =
      FolderImage::GetTopIconsBounds(gfx::Rect(size()), num_items);

  for (size_t i = 0; i < num_items; ++i) {
    DrawIcon(canvas, icons_[i],
             AppListConfig::instance().item_icon_in_folder_icon_size(),
             top_icon_bounds[i].x(), top_icon_bounds[i].y());
  }
}

// Calculates and returns the top item icons' bounds inside |folder_icon_bounds|
// when new style launcher is not enabled.
std::vector<gfx::Rect> GetTopIconsBoundsLegacy(
    const gfx::Rect& folder_icon_bounds,
    size_t num_items) {
  DCHECK_LE(num_items, FolderImage::kNumFolderTopItems);
  const int delta_to_center = 1;
  const int item_icon_dimension =
      AppListConfig::instance().item_icon_in_folder_icon_dimension();
  gfx::Point icon_center = folder_icon_bounds.CenterPoint();
  std::vector<gfx::Rect> top_icon_bounds;

  // Get the top left icon bounds.
  int left_x = icon_center.x() - item_icon_dimension - delta_to_center;
  int top_y = icon_center.y() - item_icon_dimension - delta_to_center;
  gfx::Rect top_left(left_x, top_y, item_icon_dimension, item_icon_dimension);
  top_icon_bounds.emplace_back(top_left);

  // Get the top right icon bounds.
  int right_x = icon_center.x() + delta_to_center;
  gfx::Rect top_right(right_x, top_y, item_icon_dimension, item_icon_dimension);
  top_icon_bounds.emplace_back(top_right);

  // Get the bottom left icon bounds.
  int bottom_y = icon_center.y() + delta_to_center;
  gfx::Rect bottom_left(left_x, bottom_y, item_icon_dimension,
                        item_icon_dimension);
  top_icon_bounds.emplace_back(bottom_left);

  // Get the bottom right icon bounds.
  gfx::Rect bottom_right(right_x, bottom_y, item_icon_dimension,
                         item_icon_dimension);
  top_icon_bounds.emplace_back(bottom_right);

  return top_icon_bounds;
}

}  // namespace

// static
const size_t FolderImage::kNumFolderTopItems = 4;

FolderImage::FolderImage(AppListItemList* item_list)
    : item_list_(item_list),
      is_new_style_launcher_enabled_(
          app_list_features::IsNewStyleLauncherEnabled()) {
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
    const gfx::Rect& folder_icon_bounds,
    size_t num_items) {
  if (!app_list_features::IsNewStyleLauncherEnabled())
    return GetTopIconsBoundsLegacy(folder_icon_bounds, num_items);

  DCHECK_LE(num_items, kNumFolderTopItems);
  const int item_icon_dimension =
      AppListConfig::instance().item_icon_in_folder_icon_dimension();
  gfx::Point icon_center = folder_icon_bounds.CenterPoint();
  std::vector<gfx::Rect> top_icon_bounds;

  const gfx::Rect center_rect(icon_center.x() - item_icon_dimension / 2,
                              icon_center.y() - item_icon_dimension / 2,
                              item_icon_dimension, item_icon_dimension);
  const int origin_offset = (AppListConfig::instance().folder_icon_dimension() -
                             item_icon_dimension) /
                                2 -
                            kItemIconMargin;
  if (num_items == 1) {
    // Center icon bounds.
    top_icon_bounds.emplace_back(center_rect);
    return top_icon_bounds;
  }

  if (num_items == 2) {
    // Left icon bounds.
    gfx::Rect left_rect = center_rect;
    left_rect.Offset(-origin_offset, 0);
    top_icon_bounds.emplace_back(left_rect);

    // Right icon bounds.
    gfx::Rect right_rect = center_rect;
    right_rect.Offset(origin_offset, 0);
    top_icon_bounds.emplace_back(right_rect);
    return top_icon_bounds;
  }

  // Top left icon bounds.
  gfx::Rect top_left_rect = center_rect;
  top_left_rect.Offset(-origin_offset, -origin_offset);
  top_icon_bounds.emplace_back(top_left_rect);

  // Top right icon bounds.
  gfx::Rect top_right_rect = center_rect;
  top_right_rect.Offset(origin_offset, -origin_offset);
  top_icon_bounds.emplace_back(top_right_rect);

  if (num_items == 3) {
    // Bottom icon bounds.
    gfx::Rect bottom_rect = center_rect;
    bottom_rect.Offset(0, origin_offset);
    top_icon_bounds.emplace_back(bottom_rect);
    return top_icon_bounds;
  }

  // Bottom left icon bounds.
  gfx::Rect bottom_left_rect = center_rect;
  bottom_left_rect.Offset(-origin_offset, origin_offset);
  top_icon_bounds.emplace_back(bottom_left_rect);

  // Bottom right icon bounds.
  gfx::Rect bottom_right_rect = center_rect;
  bottom_right_rect.Offset(origin_offset, origin_offset);
  top_icon_bounds.emplace_back(bottom_right_rect);
  return top_icon_bounds;
}

gfx::Rect FolderImage::GetTargetIconRectInFolderForItem(
    AppListItem* item,
    const gfx::Rect& folder_icon_bounds) const {
  for (size_t i = 0; i < top_items_.size(); ++i) {
    if (item->id() == top_items_[i]->id()) {
      std::vector<gfx::Rect> rects =
          GetTopIconsBounds(folder_icon_bounds, top_items_.size());
      return rects[i];
    }
  }

  gfx::Rect target_rect(folder_icon_bounds);
  target_rect.ClampToCenteredSize(
      AppListConfig::instance().item_icon_in_folder_icon_size());
  return target_rect;
}

void FolderImage::AddObserver(FolderImageObserver* observer) {
  observers_.AddObserver(observer);
}

void FolderImage::RemoveObserver(FolderImageObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FolderImage::ItemIconChanged() {
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
    top_icons.push_back(item->icon());
  const gfx::Size icon_size =
      AppListConfig::instance().folder_unclipped_icon_size();
  icon_ = gfx::ImageSkia(std::make_unique<FolderImageSource>(
                             top_icons, icon_size,
                             is_new_style_launcher_enabled_ /* draw_shadow */),
                         icon_size);

  for (auto& observer : observers_)
    observer.OnFolderImageUpdated();
}

}  // namespace app_list
