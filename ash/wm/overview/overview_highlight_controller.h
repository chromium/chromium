// Copyright 2019 The Chromium Authors. All rights reserved.
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

// Manages highlighting items while in overview. Responsible for telling
// overview items to show or hide their focus ring borders, when tabbing through
// overview items with arrow keys and trackpad swipes, or when tab dragging.
class ASH_EXPORT OverviewHighlightController {
 public:
  // TestApi is used for tests to get internal implementation details.
  // TODO(dandersson): Move this class out.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(OverviewHighlightController* highlight_controller);
    ~TestApi();

    OverviewHighlightableView* GetHighlightView() const;

   private:
    OverviewHighlightController* const highlight_controller_;
  };

  explicit OverviewHighlightController(OverviewSession* overview_session);
  OverviewHighlightController(const OverviewHighlightController&) = delete;
  OverviewHighlightController& operator=(const OverviewHighlightController&) =
      delete;
  ~OverviewHighlightController();

  OverviewHighlightableView* highlighted_view() { return highlighted_view_; }

  // Moves the focus ring to the next traversable view.
  void MoveHighlight(bool reverse);

  // Moves the focus ring directly to |target_view|. |target_view| must be a
  // traversable view, i.e. one of the views returned by GetTraversableViews().
  // This should be used when a view requests focus directly so the overview
  // highlight can be in-sync with focus. Due to this expected use, this does
  // not trigger an accessibility event.
  void MoveHighlightToView(OverviewHighlightableView* target_view);

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
  bool MaybeCloseHighlightedView();

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

  // Hides or shows the tab dragging highlight.
  void HideTabDragHighlight();
  void ShowTabDragHighlight(OverviewHighlightableView* view);
  bool IsTabDragHighlightVisible() const;

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

  // The current view that is being tab dragged, if any.
  OverviewHighlightableView* tab_dragged_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_HIGHLIGHT_CONTROLLER_H_
