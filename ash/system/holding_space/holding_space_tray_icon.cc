// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray_icon.h"

#include <vector>

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/holding_space/holding_space_tray_icon_preview.h"
#include "ash/system/tray/tray_constants.h"
#include "base/barrier_closure.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// When in drop target state, previews are shifted to indices which are offset
// from their standard positions by this fixed amount.
constexpr int kPreviewIndexOffsetForDropTarget = 3;

// The previews are animated in and shifted with a delay that increases
// incrementally. This is the delay increment.
constexpr base::TimeDelta kPreviewItemUpdateDelayIncrement =
    base::Milliseconds(50);

// Helpers ---------------------------------------------------------------------

// Returns the size of previews given the current shelf configuration.
int GetPreviewSize() {
  ShelfConfig* const shelf_config = ShelfConfig::Get();
  return shelf_config->in_tablet_mode() && shelf_config->is_in_app()
             ? kHoldingSpaceTrayIconSmallPreviewSize
             : kHoldingSpaceTrayIconDefaultPreviewSize;
}

}  // namespace

// HoldingSpaceTrayIcon::ResizeAnimation ---------------------------------------

// Animation for resizing the previews icon. The animation updates the icon
// view's preferred size, which causes the status area (and the shelf) to
// re-layout.
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
        animation_(this),
        animation_throughput_tracker_(
            icon->GetWidget()->GetCompositor()->RequestNewThroughputTracker()) {
    animation_.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    animation_.SetSlideDuration(
        ui::ScopedAnimationDurationScaleMode::duration_multiplier() *
        base::Milliseconds(250));
  }
  ResizeAnimation(const ResizeAnimation&) = delete;
  ResizeAnimation operator=(const ResizeAnimation&) = delete;
  ~ResizeAnimation() override = default;

  // views::AnimationDelegateViews:
  void AnimationEnded(const gfx::Animation* animation) override {
    icon_->SetPreferredSize(target_size_);
    previews_container_->SetTransform(gfx::Transform());

    // Record animation smoothness.
    animation_throughput_tracker_.Stop();
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
    animation_throughput_tracker_.Start(
        metrics_util::ForSmoothnessV3(base::BindRepeating(
            holding_space_metrics::RecordPodResizeAnimationSmoothness)));

    animation_.Show();
    AnimationProgressed(&animation_);
  }

  void AdvanceToEnd() { animation_.End(); }

 private:
  const raw_ptr<HoldingSpaceTrayIcon> icon_;
  const raw_ptr<views::View> previews_container_;
  const gfx::Size initial_size_;
  const gfx::Size target_size_;

  gfx::SlideAnimation animation_;
  ui::ThroughputTracker animation_throughput_tracker_;
};

// HoldingSpaceTrayIcon --------------------------------------------------------

HoldingSpaceTrayIcon::HoldingSpaceTrayIcon(Shelf* shelf) : shelf_(shelf) {
  SetID(kHoldingSpaceTrayPreviewsIconId);
  InitLayout();

  shell_observer_.Observe(Shell::Get());
  shelf_config_observer_.Observe(ShelfConfig::Get());
}

HoldingSpaceTrayIcon::~HoldingSpaceTrayIcon() = default;

void HoldingSpaceTrayIcon::Clear() {
  previews_update_weak_factory_.InvalidateWeakPtrs();
  item_ids_.clear();
  previews_by_id_.clear();
  removed_previews_.clear();
  SetPreferredSize(CalculatePreferredSize({}));
}

gfx::Size HoldingSpaceTrayIcon::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int num_visible_previews =
      std::min(kHoldingSpaceTrayIconMaxVisiblePreviews,
               static_cast<int>(previews_by_id_.size()));
  const int preview_size = GetPreviewSize();

  int primary_axis_size = preview_size;
  if (num_visible_previews > 1)
    primary_axis_size += (num_visible_previews - 1) * preview_size / 2;

  return shelf_->PrimaryAxisValue(
      /*horizontal=*/gfx::Size(primary_axis_size, kTrayItemSize),
      /*vertical=*/gfx::Size(kTrayItemSize, primary_axis_size));
}

void HoldingSpaceTrayIcon::OnThemeChanged() {
  views::View::OnThemeChanged();

  for (auto& preview_by_id : previews_by_id_)
    preview_by_id.second->OnThemeChanged();

  for (auto& preview : removed_previews_)
    preview->OnThemeChanged();
}

void HoldingSpaceTrayIcon::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  const int preview_size = GetPreviewSize();
  SetPreferredSize(gfx::Size(preview_size, preview_size));

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

void HoldingSpaceTrayIcon::UpdateDropTargetState(bool is_drop_target,
                                                 bool did_drop_to_pin) {
  if (is_drop_target_ == is_drop_target)
    return;

  is_drop_target_ = is_drop_target;

  // If the user performed a drag-and-drop to pin action, no handling is needed
  // to transition the holding space tray icon out of drop target state. When
  // the model updates, an `UpdatePreviews()` event will follow which will
  // restore standard indexing to the new and existing previews.
  if (!is_drop_target_ && did_drop_to_pin)
    return;

  DCHECK(!did_drop_to_pin);

  for (size_t i = 0; i < item_ids_.size(); ++i) {
    auto* preview = previews_by_id_.find(item_ids_[i])->second.get();

    DCHECK(preview->index());
    DCHECK(!preview->pending_index());

    size_t pending_index = i;
    base::TimeDelta delay;

    if (is_drop_target_) {
      // When in drop target state, preview indices are offset from their
      // standard positions by a fixed amount.
      pending_index += kPreviewIndexOffsetForDropTarget;
    } else {
      // When transitioning into drop target state, all previews shift out in
      // sync. When transitioning out of drop target state, previews shift in
      // with incremental `delay`.
      delay = i * kPreviewItemUpdateDelayIncrement;
    }

    preview->set_pending_index(pending_index);
    preview->AnimateShift(delay);
  }

  EnsurePreviewLayerStackingOrder();
}

void HoldingSpaceTrayIcon::UpdatePreviews(
    const std::vector<const HoldingSpaceItem*> items) {
  // Cancel any in progress updates.
  previews_update_weak_factory_.InvalidateWeakPtrs();

  item_ids_.clear();

  // When in drop target state, indices are offset from their standard position.
  const int offset = is_drop_target_ ? kPreviewIndexOffsetForDropTarget : 0;

  // Go over the new item list, create previews for new items, and assign new
  // indices to existing items.
  std::set<std::string> item_ids;
  for (size_t index = 0; index < items.size(); ++index) {
    const HoldingSpaceItem* item = items[index];
    DCHECK(item->IsInitialized());

    item_ids.insert(item->id());
    item_ids_.push_back(item->id());

    auto preview_it = previews_by_id_.find(item->id());
    if (preview_it != previews_by_id_.end()) {
      preview_it->second->set_pending_index(index + offset);
      continue;
    }

    auto preview = std::make_unique<HoldingSpaceTrayIconPreview>(
        shelf_, previews_container_, item);
    preview->set_pending_index(index + offset);
    previews_by_id_.emplace(item->id(), std::move(preview));
  }

  // Collect the list of items that should be removed.
  std::vector<std::string> items_to_remove;
  for (const auto& preview_pair : previews_by_id_) {
    if (!base::Contains(item_ids, preview_pair.first))
      items_to_remove.push_back(preview_pair.first);
  }

  if (!should_animate_updates_) {
    for (auto& item_id : items_to_remove)
      previews_by_id_.erase(item_id);
    items_to_remove.clear();
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

void HoldingSpaceTrayIcon::OnShellDestroying() {
  shell_observer_.Reset();
}

void HoldingSpaceTrayIcon::OnShelfAlignmentChanged(
    aura::Window* root_window,
    ShelfAlignment old_alignment) {
  // Each display has its own shelf. The shelf undergoing an alignment change
  // may not be the `shelf_` associated with this holding space tray icon.
  if (shelf_ != Shelf::ForWindow(root_window))
    return;

  if (!removed_previews_.empty()) {
    removed_previews_.clear();
    OnOldItemsRemoved();
  }

  for (const auto& preview : previews_by_id_)
    preview.second->OnShelfAlignmentChanged(old_alignment, shelf_->alignment());

  if (resize_animation_) {
    resize_animation_->AdvanceToEnd();
    resize_animation_.reset();
  }

  SetPreferredSize(CalculatePreferredSize({}));
  previews_container_->SetTransform(gfx::Transform());
}

void HoldingSpaceTrayIcon::OnShelfConfigUpdated() {
  if (!removed_previews_.empty()) {
    removed_previews_.clear();
    OnOldItemsRemoved();
  }

  for (const auto& preview : previews_by_id_)
    preview.second->OnShelfConfigChanged();

  if (resize_animation_) {
    resize_animation_->AdvanceToEnd();
    resize_animation_.reset();
  }

  SetPreferredSize(CalculatePreferredSize({}));
  previews_container_->SetTransform(gfx::Transform());
}

void HoldingSpaceTrayIcon::OnOldItemAnimatedOut(
    HoldingSpaceTrayIconPreview* preview,
    const base::RepeatingClosure& callback) {
  std::erase_if(removed_previews_, base::MatchesUniquePtr(preview));
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
  const gfx::Size target_size = CalculatePreferredSize({});

  if (initial_size != target_size) {
    // Changing icon bounds changes the relative position of existing item
    // layers within the icon (as the icon origin moves). Adjust the
    // position of existing items to maintain their position relative to the
    // "end" visible bounds.
    gfx::Vector2d adjustment(target_size.width() - initial_size.width(),
                             target_size.height() - initial_size.height());
    for (auto& preview_pair : previews_by_id_)
      preview_pair.second->AdjustTransformForContainerSizeChange(adjustment);

    if (should_animate_updates_) {
      resize_animation_ = std::make_unique<ResizeAnimation>(
          this, previews_container_, initial_size, target_size);
      resize_animation_->Start();
    } else {
      SetPreferredSize(target_size);
    }
  }

  // Shift existing items to their new positions. Note that this must be done
  // *prior* to animating in new items as `CalculateAnimateShiftParams()` relies
  // on index being unset in order to distinguish new items from existing items.
  for (const PreviewAnimationParams& params : CalculateAnimateShiftParams()) {
    params.preview->AnimateShift(params.delay);
    if (!should_animate_updates_ && params.preview->layer())
      params.preview->layer()->GetAnimator()->StopAnimating();
  }

  // Add new items. Note that this must be done *after* animating existing items
  // as `CalculateAnimateInParams()` relies on index being unset in order to
  // distinguish new items from existing items.
  for (const PreviewAnimationParams& params : CalculateAnimateInParams()) {
    params.preview->AnimateIn(params.delay);
    if (!should_animate_updates_ && params.preview->layer())
      params.preview->layer()->GetAnimator()->StopAnimating();
  }

  EnsurePreviewLayerStackingOrder();
}

std::vector<HoldingSpaceTrayIcon::PreviewAnimationParams>
HoldingSpaceTrayIcon::CalculateAnimateShiftParams() {
  // Items shift with an incremental delay. For items shifting towards the end
  // of the icon, the delay should decrease with the index. For items shifting
  // towards the start of the icon, the delay should increase with the index.
  // This ensures items moving in the same direction do not fly over each other.
  std::vector<PreviewAnimationParams> animation_params;

  // Calculate the starting delay for items that will be shifting towards the
  // end of the icon.
  base::TimeDelta shift_out_delay;
  for (size_t i = 0; i < item_ids_.size(); ++i) {
    auto* preview = previews_by_id_.find(item_ids_[i])->second.get();
    if (!preview->index().has_value())
      continue;

    DCHECK(preview->pending_index());

    if (*preview->index() <= kHoldingSpaceTrayIconMaxVisiblePreviews &&
        *preview->index() < *preview->pending_index()) {
      shift_out_delay += kPreviewItemUpdateDelayIncrement;
    }
  }

  base::TimeDelta shift_in_delay;
  for (auto& item_id : item_ids_) {
    auto* preview = previews_by_id_.find(item_id)->second.get();

    // Existing items have current index set.
    if (preview->index().has_value()) {
      const bool shift_out = *preview->index() < *preview->pending_index();

      animation_params.emplace_back(PreviewAnimationParams{
          .preview = preview,
          .delay = shift_out ? shift_out_delay : shift_in_delay,
      });

      if (shift_out) {
        shift_out_delay -= kPreviewItemUpdateDelayIncrement;
        if (shift_out_delay < base::TimeDelta())
          shift_out_delay = base::TimeDelta();
      } else {
        shift_in_delay += kPreviewItemUpdateDelayIncrement;
      }
    }
  }

  return animation_params;
}

std::vector<HoldingSpaceTrayIcon::PreviewAnimationParams>
HoldingSpaceTrayIcon::CalculateAnimateInParams() {
  // Items animating in should do so with a delay that increases in order of
  // addition, which is reverse of order in `items_`.
  std::vector<PreviewAnimationParams> animation_params;

  // Calculate the max delay which will be used for the first item.
  // NOTE: When animating in, an extra preview may be visible before items
  // before it drops into their position.
  base::TimeDelta addition_delay;
  for (size_t i = 0;
       i < kHoldingSpaceTrayIconMaxVisiblePreviews + 1 && i < item_ids_.size();
       ++i) {
    auto* preview = previews_by_id_.find(item_ids_[i])->second.get();
    if (!preview->index().has_value())
      addition_delay += kPreviewItemUpdateDelayIncrement;
  }

  for (auto& item_id : item_ids_) {
    auto* preview = previews_by_id_.find(item_id)->second.get();

    // New items do not have current index set.
    if (!preview->index().has_value()) {
      animation_params.emplace_back(PreviewAnimationParams{
          .preview = preview,
          .delay = addition_delay,
      });

      addition_delay -= kPreviewItemUpdateDelayIncrement;
      if (addition_delay < base::TimeDelta())
        addition_delay = base::TimeDelta();
    }
  }

  return animation_params;
}

void HoldingSpaceTrayIcon::EnsurePreviewLayerStackingOrder() {
  for (const auto& item_id : item_ids_) {
    auto* preview = previews_by_id_.find(item_id)->second.get();
    if (preview->layer())
      previews_container_->layer()->StackAtBottom(preview->layer());
  }
}

BEGIN_METADATA(HoldingSpaceTrayIcon)
END_METADATA

}  // namespace ash
