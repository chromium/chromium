// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_grid_view.h"

#include <memory>

#include "ash/public/cpp/desk_template.h"
#include "ash/shell.h"
#include "ash/wm/desks/templates/saved_desk_constants.h"
#include "ash/wm/desks/templates/saved_desk_item_view.h"
#include "ash/wm/desks/templates/saved_desk_name_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "base/i18n/string_compare.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

constexpr int kLandscapeMaxColumns = 3;
constexpr int kPortraitMaxColumns = 2;

// This is the maximum number of saved desks we will show in the grid. This
// constant is used instead of the Desk model `GetMaxEntryCount()` because that
// takes into consideration the number of `policy_entries_`, which can cause it
// to exceed 6 items.
// Note: Because we are only showing a maximum number of saved desks, there are
// cases that not all existing saved desks will be displayed, such as when a
// user has more than the maximum count. Since we also don't update the grid
// whenever there is a change, deleting a saved desk may result in existing
// saved desks not being shown as well, if the user originally exceeded the max
// saved desk item count when the grid was first shown.
constexpr std::size_t kMaxItemCount = 6u;

constexpr gfx::Transform kEndTransform;

// Scale for adding/deleting grid items.
constexpr float kAddOrDeleteItemScale = 0.75f;

constexpr base::TimeDelta kBoundsChangeAnimationDuration =
    base::Milliseconds(300);

constexpr base::TimeDelta kItemViewsScaleAndFadeDuration =
    base::Milliseconds(50);

// Gets the scale transform for `view`. It returns a transform with a scale of
// `kAddOrDeleteItemScale`. The pivot of the scale animation will be the center
// point of the view.
gfx::Transform GetScaleTransformForView(views::View* view) {
  gfx::Transform scale_transform;
  scale_transform.Scale(kAddOrDeleteItemScale, kAddOrDeleteItemScale);
  return gfx::TransformAboutPivot(
      gfx::RectF(view->GetLocalBounds()).CenterPoint(), scale_transform);
}

}  // namespace

SavedDeskGridView::SavedDeskGridView()
    : bounds_animator_(this, /*use_transforms=*/true) {
  // Bounds animator is unaffected by debug tools such as "--ui-slow-animations"
  // flag, so manually multiply the duration here.
  bounds_animator_.SetAnimationDuration(
      ui::ScopedAnimationDurationScaleMode::duration_multiplier() *
      kBoundsChangeAnimationDuration);
  bounds_animator_.set_tween_type(gfx::Tween::LINEAR);
}

SavedDeskGridView::~SavedDeskGridView() = default;

void SavedDeskGridView::SortEntries(const base::Uuid& order_first_uuid) {
  // Sort the `grid_items_` into alphabetical order based on saved desk name.
  // Note that this doesn't update the order of the child views, but just sorts
  // the vector. `Layout` is responsible for placing the views in the correct
  // locations in the grid.
  UErrorCode error_code = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(
      icu::Collator::createInstance(error_code));  // Use current ICU locale.
  DCHECK(U_SUCCESS(error_code));

  // If there is a uuid that is to be placed first, move that saved desk to the
  // front of the grid, and sort the rest of the entries after it.
  auto rest = base::ranges::partition(
      grid_items_,
      [&order_first_uuid](const base::Uuid& uuid) {
        return uuid == order_first_uuid;
      },
      &SavedDeskItemView::uuid);

  std::sort(
      rest, grid_items_.end(),
      [&collator](const SavedDeskItemView* a, const SavedDeskItemView* b) {
        return base::i18n::CompareString16WithCollator(
                   *collator, a->name_view()->GetText(),
                   b->name_view()->GetText()) < 0;
      });

  // A11y traverses views based on the order of the children, so we need to
  // manually reorder the child views to match the order that they are
  // displayed, which is the alphabetically sorted `grid_items_` order. If
  // there was a newly saved desk item, the first item in the grid will
  // be the new item, while the rest will be sorted alphabetically.
  for (size_t i = 0; i < grid_items_.size(); i++)
    ReorderChildView(grid_items_[i], i);
  NotifyAccessibilityEvent(ax::mojom::Event::kTreeChanged, true);

  if (bounds_animator_.IsAnimating())
    bounds_animator_.Cancel();
  DeprecatedLayoutImmediately();
}

void SavedDeskGridView::AddOrUpdateEntries(
    const std::vector<raw_ptr<const DeskTemplate, VectorExperimental>>& entries,
    const base::Uuid& order_first_uuid,
    bool animate) {
  std::vector<SavedDeskItemView*> new_grid_items;

  for (const DeskTemplate* entry : entries) {
    auto iter = base::ranges::find(grid_items_, entry->uuid(),
                                   &SavedDeskItemView::uuid);

    if (iter != grid_items_.end()) {
      (*iter)->UpdateSavedDesk(*entry);
    } else if (grid_items_.size() < kMaxItemCount) {
      SavedDeskItemView* grid_item =
          AddChildView(std::make_unique<SavedDeskItemView>(entry->Clone()));
      grid_items_.push_back(grid_item);
      if (animate)
        new_grid_items.push_back(grid_item);
    }
  }

  SortEntries(order_first_uuid);

  // The preferred size of `SavedDeskGridView` is related to the number of
  // items. Here our quantities may have changed which means the preferred size
  // has period.
  PreferredSizeChanged();

  if (animate)
    AnimateGridItems(new_grid_items);
}

void SavedDeskGridView::DeleteEntries(const std::vector<base::Uuid>& uuids,
                                      bool delete_animation) {
  for (const base::Uuid& uuid : uuids) {
    auto iter = base::ranges::find(grid_items_, uuid, &SavedDeskItemView::uuid);

    if (iter == grid_items_.end())
      continue;

    SavedDeskItemView* grid_item = *iter;

    // Performs an animation of changing the deleted grid item opacity
    // from 1 to 0 and scales down to `kAddOrDeleteItemScale`. `old_layer_tree`
    // will be deleted when the animation is complete.
    if (delete_animation) {
      auto old_grid_item_layer_tree = wm::RecreateLayers(grid_item);
      auto* old_grid_item_layer_tree_root = old_grid_item_layer_tree->root();
      GetWidget()->GetLayer()->Add(old_grid_item_layer_tree_root);

      views::AnimationBuilder()
          .OnEnded(base::BindOnce(
              [](std::unique_ptr<ui::LayerTreeOwner> layer_tree_owner) {},
              std::move(old_grid_item_layer_tree)))
          .Once()
          .SetTransform(old_grid_item_layer_tree_root,
                        GetScaleTransformForView(grid_item))
          .SetOpacity(old_grid_item_layer_tree_root, 1.f)
          .SetDuration(kItemViewsScaleAndFadeDuration);
    }

    RemoveChildViewT(grid_item);
    grid_items_.erase(iter);
  }

  AnimateGridItems(/*new_grid_items=*/{});
  NotifyAccessibilityEvent(ax::mojom::Event::kTreeChanged, true);
}

bool SavedDeskGridView::IsSavedDeskNameBeingModified() const {
  if (!GetWidget()->IsActive())
    return false;

  for (ash::SavedDeskItemView* grid_item : grid_items_) {
    if (grid_item->IsNameBeingModified())
      return true;
  }
  return false;
}

gfx::Size SavedDeskGridView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (grid_items_.empty())
    return gfx::Size();

  const size_t cols = GetMaxColumns();
  const size_t rows = (grid_items_.size() + cols - 1) / cols;
  DCHECK_GT(cols, 0u);
  DCHECK_GT(rows, 0u);

  const int item_width = SavedDeskItemView::kPreferredSize.width();
  const int item_height = SavedDeskItemView::kPreferredSize.height();

  return gfx::Size(cols * item_width + (cols - 1) * kSaveDeskPaddingDp,
                   rows * item_height + (rows - 1) * kSaveDeskPaddingDp);
}

void SavedDeskGridView::Layout(PassKey) {
  if (grid_items_.empty())
    return;

  if (bounds_animator_.IsAnimating())
    return;

  const std::vector<gfx::Rect> positions = CalculateGridItemPositions();
  for (size_t i = 0; i < grid_items_.size(); i++)
    grid_items_[i]->SetBoundsRect(positions[i]);
}

void SavedDeskGridView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // In the event where the bounds change while an animation is in progress
  // (i.e. screen rotation), we need to ensure that we stop the current
  // animation. This is because we block layouts while an animation is in
  // progress.
  if (bounds_animator_.IsAnimating())
    bounds_animator_.Cancel();
}

bool SavedDeskGridView::IsAnimating() const {
  return bounds_animator_.IsAnimating();
}

SavedDeskItemView* SavedDeskGridView::GetItemForUUID(const base::Uuid& uuid) {
  if (!uuid.is_valid())
    return nullptr;

  auto it = base::ranges::find(grid_items_, uuid, &SavedDeskItemView::uuid);
  return it == grid_items_.end() ? nullptr : *it;
}

size_t SavedDeskGridView::GetMaxColumns() const {
  return layout_mode_ == LayoutMode::LANDSCAPE ? kLandscapeMaxColumns
                                               : kPortraitMaxColumns;
}

std::vector<gfx::Rect> SavedDeskGridView::CalculateGridItemPositions() const {
  std::vector<gfx::Rect> positions;

  if (grid_items_.empty())
    return positions;

  const size_t count = grid_items_.size();
  const gfx::Size grid_item_size = grid_items_[0]->GetPreferredSize();
  const size_t max_column_count = GetMaxColumns();
  const size_t column_count = std::min(count, max_column_count);

  int x = 0;
  int y = 0;

  for (size_t i = 0; i < count; i++) {
    if (i != 0 && i % column_count == 0) {
      // Move the position to the start of the next row.
      x = 0;
      y += grid_item_size.height() + kSaveDeskPaddingDp;
    }

    positions.emplace_back(gfx::Point(x, y), grid_item_size);

    x += grid_item_size.width() + kSaveDeskPaddingDp;
  }

  DCHECK_EQ(positions.size(), grid_items_.size());

  return positions;
}

void SavedDeskGridView::AnimateGridItems(
    const std::vector<SavedDeskItemView*>& new_grid_items) {
  const std::vector<gfx::Rect> positions = CalculateGridItemPositions();
  for (size_t i = 0; i < grid_items_.size(); i++) {
    SavedDeskItemView* grid_item = grid_items_[i];
    const gfx::Rect target_bounds = positions[i];
    if (bounds_animator_.GetTargetBounds(grid_item) == target_bounds)
      continue;

    // This is a new grid_item, so do the scale up to identity and fade in
    // animation. The animation is delayed to sync up with the
    // `bounds_animator_` animation.
    if (base::Contains(new_grid_items, grid_item)) {
      grid_item->SetBoundsRect(target_bounds);

      ui::Layer* layer = grid_item->layer();
      layer->SetTransform(GetScaleTransformForView(grid_item));
      layer->SetOpacity(0.f);

      views::AnimationBuilder()
          .Once()
          .Offset(kBoundsChangeAnimationDuration -
                  kItemViewsScaleAndFadeDuration)
          .SetTransform(layer, kEndTransform)
          .SetOpacity(layer, 1.f)
          .SetDuration(kItemViewsScaleAndFadeDuration);
      continue;
    }

    bounds_animator_.AnimateViewTo(grid_item, target_bounds);
  }
}

BEGIN_METADATA(SavedDeskGridView)
END_METADATA

}  // namespace ash
