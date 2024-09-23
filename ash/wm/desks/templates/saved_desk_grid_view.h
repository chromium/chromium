// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_GRID_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_GRID_VIEW_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/view.h"

namespace ash {

class DeskTemplate;
class SavedDeskItemView;

// A view that shows a grid of saved desks. Each saved desk is a
// `SavedDeskItemView`.
class SavedDeskGridView : public views::View {
  METADATA_HEADER(SavedDeskGridView, views::View)

 public:
  enum class LayoutMode {
    LANDSCAPE = 0,
    PORTRAIT,
  };

  SavedDeskGridView();
  SavedDeskGridView(const SavedDeskGridView&) = delete;
  SavedDeskGridView& operator=(const SavedDeskGridView&) = delete;
  ~SavedDeskGridView() override;

  const std::vector<raw_ptr<SavedDeskItemView, VectorExperimental>>&
  grid_items() const {
    return grid_items_;
  }

  // Sets the grid to show items in landscape or portrait mode.
  void set_layout_mode(LayoutMode layout_mode) { layout_mode_ = layout_mode; }

  // Sorts entries in alphabetical order. If `order_first_uuid` is valid, the
  // corresponding entry will be placed first.
  void SortEntries(const base::Uuid& order_first_uuid);

  // Updates existing saved desks and adds new saved desks to the grid. Also
  // sorts entries in alphabetical order. If `order_first_uuid` is valid, the
  // corresponding entry will be placed first. This will animate the entries to
  // their final positions if `animate` is true. Currently only allows a maximum
  // of 6 saved desks to be shown in the grid.
  void AddOrUpdateEntries(
      const std::vector<raw_ptr<const DeskTemplate, VectorExperimental>>&
          entries,
      const base::Uuid& order_first_uuid,
      bool animate);

  // Removes saved desks from the grid by UUID. Will trigger an animation to
  // shuffle `grid_items_` to their final positions. If `delete_animation` is
  // false, then deleted items will simply disappear (shuffled items will still
  // animate).
  void DeleteEntries(const std::vector<base::Uuid>& uuids,
                     bool delete_animation);

  // Returns true if a saved desk name is being modified using an item view's
  // `SavedDeskNameView` in this grid.
  bool IsSavedDeskNameBeingModified() const;

  // Returns the item view associated with `uuid`.
  SavedDeskItemView* GetItemForUUID(const base::Uuid& uuid);

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  bool IsAnimating() const;

  // Returns the size needed to lay out the grid in a given `width`.
  gfx::Size GetSizeForWidth(int width) const;

 private:
  friend class SavedDeskGridViewTestApi;

  // Returns the max columns that the grid can show based on `layout_mode_`.
  size_t GetMaxColumns() const;

  // Calculates the bounds for each grid item within the saved desks grid. The
  // indices of the returned vector directly correlate to those of `grid_items_`
  // (i.e. the Rect at index 1 of the returned vector should be applied to the
  // `SavedDeskItemView` found at index 1 of `grid_items_`).
  std::vector<gfx::Rect> CalculateGridItemPositions() const;

  // Animates the bounds for all the `grid_items_` (using `bounds_animator_`) to
  // their calculated position. `new_grid_items` contains a list of the
  // newly-created saved desk items and will be animated differently than the
  // existing views that are being shifted around.
  void AnimateGridItems(const std::vector<SavedDeskItemView*>& new_grid_items);

  // The views representing saved desks. They're owned by views hierarchy.
  std::vector<raw_ptr<SavedDeskItemView, VectorExperimental>> grid_items_;

  // Controls how the grid items are laid out.
  LayoutMode layout_mode_ = LayoutMode::LANDSCAPE;

  // Used to animate individual view positions.
  views::BoundsAnimator bounds_animator_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_GRID_VIEW_H_
