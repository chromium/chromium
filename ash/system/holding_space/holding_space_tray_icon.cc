// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray_icon.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/holding_space/holding_space_tray_icon_preview.h"
#include "ash/system/tray/tray_constants.h"
#include "base/barrier_closure.h"
#include "base/containers/adapters.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/stl_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

namespace {

// The preview are animated in and shifted with a delay that increases
// incrementally. This is the delay increment.
constexpr base::TimeDelta kPreviewItemUpdateDelayIncrement =
    base::TimeDelta::FromMilliseconds(50);

}  // namespace

// Animation for resizing the previews icon. The animation updates the icon
// view's preferred size, which causes the status area (and the shelf) to
// relayout.
class HoldingSpaceTrayIcon::ResizeAnimation
    : public views::AnimationDelegateViews {
 public:
  ResizeAnimation(HoldingSpaceTrayIcon* icon,
                  views::View* previews_container,
                  const gfx::Size& initial_size,
                  const gfx::Size& target_size)
      : views::AnimationDelegateViews(icon),
        icon_(icon),
        previews_container_(previews_container),
        initial_size_(initial_size),
        target_size_(target_size),
        animation_(this) {
    animation_.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    animation_.SetSlideDuration(
        ui::ScopedAnimationDurationScaleMode::duration_multiplier() *
        base::TimeDelta::FromMilliseconds(250));
  }
  ResizeAnimation(const ResizeAnimation&) = delete;
  ResizeAnimation operator=(const ResizeAnimation&) = delete;
  ~ResizeAnimation() override = default;

  // views::AnimationDelegateViews:
  void AnimationEnded(const gfx::Animation* animation) override {
    icon_->SetPreferredSize(target_size_);
    previews_container_->SetTransform(gfx::Transform());
  }

  void AnimationProgressed(const gfx::Animation* animation) override {
    const gfx::Size current_size = gfx::Tween::SizeValueBetween(
        animation->GetCurrentValue(), initial_size_, target_size_);

    // Bounds grow from start to the end by default - for holding space tray
    // icon, the bounds are expected to grow from end to start. To achieve
    // growth from end to start, the position of previews is adjusted for target
    // bounds, and the previews container is translated so previews position
    // remains constant relative to the end of the icon.
    const gfx::Vector2d offset(current_size.width() - target_size_.width(),
                               current_size.height() - target_size_.height());
    gfx::Transform transform;
    const int direction = base::i18n::IsRTL() ? -1 : 1;
    transform.Translate(direction * offset.x(), offset.y());
    previews_container_->SetTransform(transform);

    // This will update the shelf and status area layout.
    if (icon_->GetPreferredSize() != current_size)
      icon_->SetPreferredSize(current_size);
  }

  void Start() {
    animation_.Show();
    AnimationProgressed(&animation_);
  }

  void AdvanceToEnd() { animation_.End(); }

 private:
  HoldingSpaceTrayIcon* const icon_;
  views::View* const previews_container_;
  const gfx::Size initial_size_;
  const gfx::Size target_size_;

  gfx::SlideAnimation animation_;
};

// HoldingSpaceTrayIcon --------------------------------------------------------

HoldingSpaceTrayIcon::HoldingSpaceTrayIcon(Shelf* shelf) : shelf_(shelf) {
  SetID(kHoldingSpaceTrayPreviewsIconId);
  InitLayout();
  shell_observer_.Add(Shell::Get());
}

HoldingSpaceTrayIcon::~HoldingSpaceTrayIcon() = default;

void HoldingSpaceTrayIcon::Clear() {
  previews_update_weak_factory_.InvalidateWeakPtrs();
  previews_by_id_.clear();
  removed_previews_.clear();
}

int HoldingSpaceTrayIcon::GetHeightForWidth(int width) const {
  // The parent for this view (`TrayContainer`) uses a `BoxLayout` for its
  // `LayoutManager`. When the shelf orientation is vertical, the `BoxLayout`
  // will also have vertical orientation and will invoke `GetHeightForWidth()`
  // instead of `GetPreferredSize()` when determining preferred size.
  return GetPreferredSize().height();
}

gfx::Size HoldingSpaceTrayIcon::CalculatePreferredSize() const {
  const int num_visible_previews =
      std::min(kHoldingSpaceTrayIconMaxVisiblePreviews,
               static_cast<int>(previews_by_id_.size()));

  int primary_axis_size = kTrayItemSize;
  if (num_visible_previews > 1)
    primary_axis_size += (num_visible_previews - 1) * kTrayItemSize / 2;

  return shelf_->PrimaryAxisValue(
      /*horizontal=*/gfx::Size(primary_axis_size, kTrayItemSize),
      /*vertical=*/gfx::Size(kTrayItemSize, primary_axis_size));
}

void HoldingSpaceTrayIcon::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetPreferredSize(gfx::Size(kTrayItemSize, kTrayItemSize));

  SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  layer()->SetFillsBoundsOpaquely(false);
  const int radius = ShelfConfig::Get()->control_border_radius();
  gfx::RoundedCornersF rounded_corners(radius);
  layer()->SetRoundedCornerRadius(rounded_corners);
  layer()->SetIsFastRoundedCorner(true);

  previews_container_ = AddChildView(std::make_unique<views::View>());
  // As holding space items are added to the model, child layers will be added
  // to `previews_container_` view's layer to represent them.
  previews_container_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
}

void HoldingSpaceTrayIcon::UpdatePreviews(
    const std::vector<const HoldingSpaceItem*> items) {
  // Cancel any in progress updates.
  previews_update_weak_factory_.InvalidateWeakPtrs();

  item_ids_.clear();

  // Go over the new item list, create previews for new items, and assign new
  // indices to existing items.
  std::set<std::string> item_ids;
  for (size_t index = 0; index < items.size(); ++index) {
    const HoldingSpaceItem* item = items[index];
    DCHECK(item->IsFinalized());

    item_ids.insert(item->id());
    item_ids_.push_back(item->id());

    auto preview_it = previews_by_id_.find(item->id());
    if (preview_it != previews_by_id_.end()) {
      preview_it->second->set_pending_index(index);
      continue;
    }

    auto preview = std::make_unique<HoldingSpaceTrayIconPreview>(
        shelf_, previews_container_, item);
    preview->set_pending_index(index);
    previews_by_id_.emplace(item->id(), std::move(preview));
  }

  // Collect the list of items that should be removed.
  std::vector<std::string> items_to_remove;
  for (const auto& preview_pair : previews_by_id_) {
    if (!base::Contains(item_ids, preview_pair.first))
      items_to_remove.push_back(preview_pair.first);
  }

  if (items_to_remove.empty()) {
    OnOldItemsRemoved();
    return;
  }

  // Animate out all items that should be removed.
  base::RepeatingClosure items_removed_callback = base::BarrierClosure(
      items_to_remove.size(),
      base::BindOnce(&HoldingSpaceTrayIcon::OnOldItemsRemoved,
                     previews_update_weak_factory_.GetWeakPtr()));

  for (auto& item_id : items_to_remove) {
    auto preview_it = previews_by_id_.find(item_id);
    HoldingSpaceTrayIconPreview* preview_ptr = preview_it->second.get();
    removed_previews_.push_back(std::move(preview_it->second));
    previews_by_id_.erase(preview_it);

    preview_ptr->AnimateOut(base::BindOnce(
        &HoldingSpaceTrayIcon::OnOldItemAnimatedOut, base::Unretained(this),
        base::Unretained(preview_ptr), items_removed_callback));
  }
}

void HoldingSpaceTrayIcon::OnShelfAlignmentChanged(
    aura::Window* root_window,
    ShelfAlignment old_alignment) {
  removed_previews_.clear();
  for (const auto& preview : previews_by_id_)
    preview.second->OnShelfAlignmentChanged(old_alignment, shelf_->alignment());

  if (resize_animation_) {
    resize_animation_->AdvanceToEnd();
    resize_animation_.reset();
  }

  SetPreferredSize(CalculatePreferredSize());
  previews_container_->SetTransform(gfx::Transform());
}

void HoldingSpaceTrayIcon::OnOldItemAnimatedOut(
    HoldingSpaceTrayIconPreview* preview,
    const base::RepeatingClosure& callback) {
  base::EraseIf(removed_previews_, base::MatchesUniquePtr(preview));
  callback.Run();
}

void HoldingSpaceTrayIcon::OnOldItemsRemoved() {
  if (resize_animation_) {
    resize_animation_->AdvanceToEnd();
    resize_animation_.reset();
  }

  // Now that the old items have been removed, resize the icon, and update
  // previews position within the icon.
  const gfx::Size initial_size = size();
  const gfx::Size target_size = CalculatePreferredSize();

  if (initial_size != target_size) {
    // Changing icon bounds changes the relative position of existing item
    // layers within the icon (as the icon origin moves). Adjust the
    // position of existing items to maintain their position relative to the
    // "end" visible bounds.
    gfx::Vector2d adjustment(target_size.width() - initial_size.width(),
                             target_size.height() - initial_size.height());
    for (auto& preview_pair : previews_by_id_)
      preview_pair.second->AdjustTransformForContainerSizeChange(adjustment);

    resize_animation_ = std::make_unique<ResizeAnimation>(
        this, previews_container_, initial_size, target_size);
    resize_animation_->Start();
  }

  // Note: the order is important - `AnimateInNewItems()` will set the new item
  // indices, and `ShiftExistingItems()` depends on the preview index value to
  // detect whether an item is new.
  ShiftExistingItems();
  AnimateInNewItems();

  // Ensure that preview layers stacking matches their order in the item list.
  for (auto& item_id : item_ids_) {
    auto preview_it = previews_by_id_.find(item_id);
    HoldingSpaceTrayIconPreview* preview_ptr = preview_it->second.get();
    if (preview_ptr->layer())
      previews_container_->layer()->StackAtBottom(preview_ptr->layer());
  }
}

void HoldingSpaceTrayIcon::ShiftExistingItems() {
  // Items shifting should do so with an incremental delay. For items shifting
  // towards the end of the icon, the delay should decrease with the index. For
  // items shifting towards the start of the icon, the delay should increase
  // with the index. This ensures that items moving into the same direction do
  // not fly over each other.
  // Calculate the starting delay for items that will be shifting towards the
  // end of the icon.
  base::TimeDelta shift_out_delay;
  for (size_t i = 0; i < item_ids_.size(); ++i) {
    auto preview_it = previews_by_id_.find(item_ids_[i]);
    if (!preview_it->second->index().has_value())
      continue;
    DCHECK(preview_it->second->pending_index());

    if (*preview_it->second->index() <=
            kHoldingSpaceTrayIconMaxVisiblePreviews &&
        *preview_it->second->index() < *preview_it->second->pending_index()) {
      shift_out_delay += kPreviewItemUpdateDelayIncrement;
    }
  }

  base::TimeDelta shift_in_delay;
  for (auto& item_id : item_ids_) {
    auto preview_it = previews_by_id_.find(item_id);
    HoldingSpaceTrayIconPreview* preview_ptr = preview_it->second.get();

    // Existing items have current index set.
    if (preview_ptr->index().has_value()) {
      const bool shift_out =
          *preview_ptr->index() < *preview_ptr->pending_index();

      preview_ptr->AnimateShift(shift_out ? shift_out_delay : shift_in_delay);

      if (shift_out) {
        shift_out_delay -= kPreviewItemUpdateDelayIncrement;
        if (shift_out_delay < base::TimeDelta())
          shift_out_delay = base::TimeDelta();
      } else {
        shift_in_delay += kPreviewItemUpdateDelayIncrement;
      }
    }
  }
}

void HoldingSpaceTrayIcon::AnimateInNewItems() {
  // Items animating in should do so with a delay that increases in order of
  // addtion, which is reverse of order in `items_` - calculate the max delay,
  // which will be used for the first item.
  base::TimeDelta addition_delay;
  // NOTE: When animating in, an exta preview may be visible before items before
  // it drop into their position.
  for (size_t i = 0;
       i < kHoldingSpaceTrayIconMaxVisiblePreviews + 1 && i < item_ids_.size();
       ++i) {
    auto preview_it = previews_by_id_.find(item_ids_[i]);
    if (!preview_it->second->index().has_value())
      addition_delay += kPreviewItemUpdateDelayIncrement;
  }

  for (auto& item_id : item_ids_) {
    auto preview_it = previews_by_id_.find(item_id);
    HoldingSpaceTrayIconPreview* preview_ptr = preview_it->second.get();

    // New items do not have current index set.
    if (!preview_ptr->index().has_value()) {
      preview_ptr->AnimateIn(addition_delay);
      addition_delay -= kPreviewItemUpdateDelayIncrement;
      if (addition_delay < base::TimeDelta())
        addition_delay = base::TimeDelta();
    }
  }
}

BEGIN_METADATA(HoldingSpaceTrayIcon, views::View)
END_METADATA

}  // namespace ash
