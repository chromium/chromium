// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_HIGHLIGHT_CONTROLLER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_HIGHLIGHT_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace views {
class View;
}

namespace ash {
class OverviewItem;
class OverviewSession;

// Manages highlighting items while in overview. Responsible for telling
// overview items to show or hide their focus ring borders, when tabbing through
// overview items with arrow keys and trackpad swipes, or when tab dragging.
class ASH_EXPORT OverviewHighlightController {
 public:
  // An interface that must be implemented by classes that want to be
  // highlighted in overview.
  class OverviewHighlightableView {
   public:
    // Get the view class associated with |this|.
    virtual views::View* GetView() = 0;

    // Attempts to activate or close this view. Overriders may do nothing.
    virtual void MaybeActivateHighlightedView() = 0;
    virtual void MaybeCloseHighlightedView() = 0;

    void SetHighlightVisibility(bool visible);

    // Returns true if this is the current highlighted view.
    bool IsViewHighlighted() { return is_highlighted_; }

    // Returns the point the accessibility magnifiers should focus when this is
    // highlighted. If not overridden, this will return the centerpoint.
    virtual gfx::Point GetMagnifierFocusPointInScreen();

   protected:
    virtual ~OverviewHighlightableView() = default;

    // Highlights or unhighlights this view.
    virtual void OnViewHighlighted() = 0;
    virtual void OnViewUnhighlighted() = 0;

   private:
    bool is_highlighted_ = false;
  };

  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(OverviewHighlightController* highlight_controller);
    ~TestApi();

    OverviewHighlightableView* GetHighlightView() const;

   private:
    OverviewHighlightController* const highlight_controller_;
  };

  explicit OverviewHighlightController(OverviewSession* overview_session);
  ~OverviewHighlightController();

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

  // Tries to get the item that is currently highlighted. Returns null if there
  // is no highlight, or if the highlight is on a desk view.
  OverviewItem* GetHighlightedItem() const;

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
  base::Optional<int> deleted_index_ = base::nullopt;

  // The current view that is being highlighted, if any.
  OverviewHighlightableView* highlighted_view_ = nullptr;

  // The current view that is being tab dragged, if any.
  OverviewHighlightableView* tab_dragged_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(OverviewHighlightController);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_HIGHLIGHT_CONTROLLER_H_
