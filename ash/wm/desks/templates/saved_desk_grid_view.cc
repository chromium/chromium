// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_grid_view.h"

#include <algorithm>
#include <memory>

#include "ash/public/cpp/desk_template.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/desks/templates/saved_desk_animations.h"
#include "ash/wm/desks/templates/saved_desk_item_view.h"
#include "ash/wm/desks/templates/saved_desk_name_view.h"
#include "ash/wm/desks/templates/saved_desk_presenter.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "base/i18n/string_compare.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

constexpr int kLandscapeMaxColumns = 3;
constexpr int kPortraitMaxColumns = 2;

constexpr int kGridPaddingDp = 24;

// This is the maximum number of templates we will show in the grid. This
// constant is used instead of the Desk model `GetMaxEntryCount()` because that
// takes into consideration the number of `policy_entries_`, which can cause it
// to exceed 6 items.
// Note: Because we are only showing a maximum number of templates, there are
// cases that not all existing templates will be displayed, such as when a user
// has more than the maximum count. Since we also don't update the grid whenever
// there is a change, deleting a template may result in existing templates not
// being shown as well, if the user originally exceeded the max template count
// when the grid was first shown.
constexpr std::size_t kMaxTemplateCount = 6u;

constexpr gfx::Transform kEndTransform;

// Scale for adding/deleting grid items.
constexpr float kAddOrDeleteItemScale = 0.75f;

constexpr base::TimeDelta kBoundsChangeAnimationDuration =
    base::Milliseconds(300);

constexpr base::TimeDelta kTemplateViewsScaleAndFadeDuration =
    base::Milliseconds(50);

// Gets the scale transform for `view`. It returns a transform with a scale of
// `kAddOrDeleteItemScale`. The pivot of the scale animation will be the center
// point of the view.
gfx::Transform GetScaleTransformForView(views::View* view) {
  gfx::Transform scale_transform;
  scale_transform.Scale(kAddOrDeleteItemScale, kAddOrDeleteItemScale);
  return gfx::TransformAboutPivot(view->GetLocalBounds().CenterPoint(),
                                  scale_transform);
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

void SavedDeskGridView::PopulateGridUI(
    const std::vector<const DeskTemplate*>& desk_templates,
    const base::GUID& last_saved_template_uuid) {
  DCHECK(grid_items_.empty());

  // TODO(richui|sammiequon): See if this can be removed as this function should
  // only be called once per overview session.
  if (desk_templates.empty()) {
    RemoveAllChildViews();
    grid_items_.clear();
    return;
  }

  AddOrUpdateTemplates(desk_templates,
                       /*initializing_grid_view=*/true,
                       last_saved_template_uuid);
}

void SavedDeskGridView::SortTemplateGridItems(
    const base::GUID& last_saved_template_uuid) {
  // Sort the `grid_items_` into alphabetical order based on template name.
  // Note that this doesn't update the order of the child views, but just sorts
  // the vector. `Layout` is responsible for placing the views in the correct
  // locations in the grid.
  UErrorCode error_code = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(
      icu::Collator::createInstance(error_code));  // Use current ICU locale.
  DCHECK(U_SUCCESS(error_code));
  // If there is a newly saved template, move that template to the front of the
  // grid, and sort the rest of the templates after it.
  std::sort(grid_items_.begin(), grid_items_.end(),
            [&collator, last_saved_template_uuid](const SavedDeskItemView* a,
                                                  const SavedDeskItemView* b) {
              if (last_saved_template_uuid.is_valid() &&
                  a->uuid() == last_saved_template_uuid) {
                return true;
              }
              if (last_saved_template_uuid.is_valid() &&
                  b->uuid() == last_saved_template_uuid) {
                return false;
              }
              return base::i18n::CompareString16WithCollator(
                         *collator, a->name_view()->GetAccessibleName(),
                         b->name_view()->GetAccessibleName()) < 0;
            });

  // A11y traverses views based on the order of the children, so we need to
  // manually reorder the child views to match the order that they are
  // displayed, which is the alphabetically sorted `grid_items_` order. If
  // there was a newly saved template, the first template in the grid will
  // be the new template, while the rest will be sorted alphabetically.
  for (size_t i = 0; i < grid_items_.size(); i++)
    ReorderChildView(grid_items_[i], i);
  NotifyAccessibilityEvent(ax::mojom::Event::kTreeChanged, true);

  if (bounds_animator_.IsAnimating())
    bounds_animator_.Cancel();
  Layout();
}

void SavedDeskGridView::AddOrUpdateTemplates(
    const std::vector<const DeskTemplate*>& entries,
    bool initializing_grid_view,
    const base::GUID& last_saved_template_uuid) {
  std::vector<SavedDeskItemView*> new_grid_items;

  for (const DeskTemplate* entry : entries) {
    auto iter = std::find_if(grid_items_.begin(), grid_items_.end(),
                             [entry](SavedDeskItemView* grid_item) {
                               return entry->uuid() == grid_item->uuid();
                             });

    if (iter != grid_items_.end()) {
      (*iter)->UpdateTemplate(*entry);
    } else if (grid_items_.size() < kMaxTemplateCount) {
      SavedDeskItemView* grid_item =
          AddChildView(std::make_unique<SavedDeskItemView>(entry));
      grid_items_.push_back(grid_item);
      if (!initializing_grid_view)
        new_grid_items.push_back(grid_item);
    }
  }

  // Sort the `grid_items_` into alphabetical order based on template name. If a
  // given uuid is valid, it'll push that template item to the front of the grid
  // and sort the remaining templates after it.
  SortTemplateGridItems(last_saved_template_uuid);

  if (!initializing_grid_view)
    AnimateGridItems(new_grid_items);
}

void SavedDeskGridView::DeleteTemplates(const std::vector<std::string>& uuids) {
  OverviewHighlightController* highlight_controller =
      Shell::Get()
          ->overview_controller()
          ->overview_session()
          ->highlight_controller();
  DCHECK(highlight_controller);

  for (const std::string& uuid : uuids) {
    auto iter =
        std::find_if(grid_items_.begin(), grid_items_.end(),
                     [uuid](SavedDeskItemView* grid_item) {
                       return uuid == grid_item->uuid().AsLowercaseString();
                     });

    if (iter == grid_items_.end())
      continue;

    SavedDeskItemView* grid_item = *iter;
    highlight_controller->OnViewDestroyingOrDisabling(grid_item);
    highlight_controller->OnViewDestroyingOrDisabling(grid_item->name_view());

    // Performs an animation of changing the deleted grid item opacity
    // from 1 to 0 and scales down to `kAddOrDeleteItemScale`. `old_layer_tree`
    // will be deleted when the animation is complete.
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
        .SetOpacity(old_grid_item_layer_tree_root, 0.f)
        .SetDuration(kTemplateViewsScaleAndFadeDuration);

    RemoveChildViewT(grid_item);
    grid_items_.erase(iter);
  }

  AnimateGridItems(/*new_grid_items=*/{});
  NotifyAccessibilityEvent(ax::mojom::Event::kTreeChanged, true);
}

bool SavedDeskGridView::IsTemplateNameBeingModified() const {
  if (!GetWidget()->IsActive())
    return false;

  for (auto* grid_item : grid_items_) {
    if (grid_item->IsNameBeingModified())
      return true;
  }
  return false;
}

gfx::Size SavedDeskGridView::CalculatePreferredSize() const {
  if (grid_items_.empty())
    return gfx::Size();

  const size_t cols = GetMaxColumns();
  const size_t rows = (grid_items_.size() + cols - 1) / cols;
  DCHECK_GT(cols, 0u);
  DCHECK_GT(rows, 0u);

  const int item_width = SavedDeskItemView::kPreferredSize.width();
  const int item_height = SavedDeskItemView::kPreferredSize.height();

  return gfx::Size(cols * item_width + (cols - 1) * kGridPaddingDp,
                   rows * item_height + (rows - 1) * kGridPaddingDp);
}

void SavedDeskGridView::Layout() {
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

SavedDeskItemView* SavedDeskGridView::GetItemForUUID(const base::GUID& uuid) {
  if (!uuid.is_valid())
    return nullptr;

  auto it = std::find_if(grid_items_.begin(), grid_items_.end(),
                         [&uuid](SavedDeskItemView* item_view) {
                           return uuid == item_view->desk_template()->uuid();
                         });
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
      y += grid_item_size.height() + kGridPaddingDp;
    }

    positions.emplace_back(gfx::Point(x, y), grid_item_size);

    x += grid_item_size.width() + kGridPaddingDp;
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
                  kTemplateViewsScaleAndFadeDuration)
          .SetTransform(layer, kEndTransform)
          .SetOpacity(layer, 1.f)
          .SetDuration(kTemplateViewsScaleAndFadeDuration);
      continue;
    }

    bounds_animator_.AnimateViewTo(grid_item, target_bounds);
  }
}

BEGIN_METADATA(SavedDeskGridView, views::View)
END_METADATA

}  // namespace ash
