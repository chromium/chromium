// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_HIGHLIGHT_CONTROLLER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_HIGHLIGHT_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
class OverviewHighlightableView;
class OverviewItem;
class OverviewSession;
class ScopedA11yOverrideWindowSetter;

// Manages highlighting items while in overview. Responsible for telling
// highlightable items to show or hide their focus ring borders, or when tabbing
// through highlightable items with arrow keys and trackpad swipes. In this
// context, an highlightable item can represent anything focusable in overview
// mode such as a desk textfield, saved desk button and an `OverviewItem`. The
// idea behind the movement strategy is that it should be possible to access any
// highlightable view via keyboard by pressing the tab or arrow keys repeatedly.
// +-------+  +-------+  +-------+
// |   0   |  |   1   |  |   2   |
// +-------+  +-------+  +-------+
// +-------+  +-------+  +-------+
// |   3   |  |   4   |  |   5   |
// +-------+  +-------+  +-------+
// +-------+
// |   6   |
// +-------+
// Example sequences:
//  - Going right to left
//    0, 1, 2, 3, 4, 5, 6
// The highlight is switched to the next window grid (if available) or wrapped
// if it reaches the end of its movement sequence.
class ASH_EXPORT OverviewHighlightController {
 public:
  explicit OverviewHighlightController(OverviewSession* overview_session);
  OverviewHighlightController(const OverviewHighlightController&) = delete;
  OverviewHighlightController& operator=(const OverviewHighlightController&) =
      delete;
  ~OverviewHighlightController();

  OverviewHighlightableView* highlighted_view() { return highlighted_view_; }

  // Moves the focus ring to the next traversable view.
  void MoveHighlight(bool reverse);

  // Called when pressing two save desks buttons to show the saved desk grids
  // and focus on a saved desk name view.
  void UpdateA11yFocusWindow(OverviewHighlightableView* name_view);

  // Moves the focus ring directly to |target_view|. |target_view| must be a
  // traversable view, i.e. one of the views returned by GetTraversableViews().
  // This should be used when a view requests focus directly so the overview
  // highlight can be in-sync with focus. Due to this expected use, it should
  // not normally be necessary to trigger an accessibility event.
  void MoveHighlightToView(OverviewHighlightableView* target_view,
                           bool suppress_accessibility_event = true);

  // Called when a |view| that might be in the focus traversal rotation is about
  // to be deleted.
  // Note: When removing multiple highlightable views in one call, by calling
  // this function repeatedly, make sure to call it in reverse order (i.e. on
  // the views that come later in the highlight order first). This makes sure
  // that traversal continues correctly from where it was left off.
  void OnViewDestroyingOrDisabling(OverviewHighlightableView* view);

  // Sets and gets the visibility of |highlighted_view_|.
  void SetFocusHighlightVisibility(bool visible);
  bool IsFocusHighlightVisible() const;

  // Activates or closes the currently highlighted view (if any) if it supports
  // the activation or closing operations respectively.
  bool MaybeActivateHighlightedView();
  bool MaybeCloseHighlightedView(bool primary_action);

  // Swaps the currently highlighted view with its neighbor views.
  bool MaybeSwapHighlightedView(bool right);

  // Activates highlighted view when exiting overview mode.
  bool MaybeActivateHighlightedViewOnOverviewExit();

  // Tries to get the item that is currently highlighted. Returns null if there
  // is no highlight, or if the highlight is on a desk view.
  OverviewItem* GetHighlightedItem() const;

  // If `highlighted_view_` is not null, remove the highlight. The next tab will
  // start at the beginning of the tab order. This can be used when a lot of
  // views are getting removed or hidden, such as when the desks bar goes from
  // expanded to zero state.
  void ResetHighlightedView();

 private:
  // Returns a vector of views that can be traversed via overview tabbing.
  // Includes desk mini views, the new desk button and overview items.
  std::vector<OverviewHighlightableView*> GetTraversableViews() const;

  // Sets |highlighted_view_| to |view_to_be_highlighted| and updates the
  // highlight visibility for the previous |highlighted_view_|.
  // |suppress_accessibility_event| should be true if |view_to_be_highlighted|
  // will also request focus to avoid double emitting event.
  void UpdateHighlight(OverviewHighlightableView* view_to_be_highlighted,
                       bool suppress_accessibility_event = false);

  // The overview session which owns this object. Guaranteed to be non-null for
  // the lifetime of |this|.
  OverviewSession* const overview_session_;

  // If an item that is selected is deleted, store its index, so the next
  // traversal can pick up where it left off.
  absl::optional<int> deleted_index_ = absl::nullopt;

  // The current view that is being highlighted, if any.
  OverviewHighlightableView* highlighted_view_ = nullptr;

  // Helps to update the current a11y override window. And accessibility
  // features will focus on the window that is being set. Once `this` goes out
  // of scope, the a11y override window is set to nullptr.
  std::unique_ptr<ScopedA11yOverrideWindowSetter> scoped_a11y_overrider_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_HIGHLIGHT_CONTROLLER_H_
