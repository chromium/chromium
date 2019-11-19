// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_HIGHLIGHT_CONTROLLER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_HIGHLIGHT_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace aura {
class Window;
}

namespace views {
class View;
}

namespace ash {
class OverviewItem;
class OverviewSession;

// Manages highlighting items while in overview. Creates a semi transparent
// highlight when users try to traverse through overview items using arrow keys
// or tab keys, or when users are tab dragging.
class ASH_EXPORT OverviewHighlightController {
 public:
  class HighlightWidget;

  // An interface that must be implemented by classes that want to be
  // highlighted in overview.
  class OverviewHighlightableView {
   public:
    // Get the view class associated with |this|.
    virtual views::View* GetView() = 0;
    // Get the bounds of where the highlight should be for |this|, in screen
    // coordinates.
    virtual gfx::Rect GetHighlightBoundsInScreen() = 0;

    // Get the rounded corners the highlight should have when highlighting
    // |this|.
    virtual gfx::RoundedCornersF GetRoundedCornersRadii() const;

    // Attempts to activate or close this view. Overriders may do nothing.
    virtual void MaybeActivateHighlightedView() = 0;
    virtual void MaybeCloseHighlightedView() = 0;

    // Subclasses can override these if they wish to have custom behavior when
    // they're highlighted. They should return true if overridden, otherwise the
    // default highlight will show up.
    virtual bool OnViewHighlighted();
    virtual void OnViewUnhighlighted();

    // Returns true if this is the current highlighted view.
    bool IsViewHighlighted();

    // Returns the point the accessibility magnifiers should focus when this is
    // highlighted. If not overridden, this will return the centerpoint.
    virtual gfx::Point GetMagnifierFocusPointInScreen();

   protected:
    virtual ~OverviewHighlightableView() {}
  };

  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(OverviewHighlightController* highlight_controller);
    ~TestApi();

    gfx::Rect GetHighlightBoundsInScreen() const;
    OverviewHighlightableView* GetHighlightView() const;
    HighlightWidget* GetHighlightWidget() const;

   private:
    OverviewHighlightController* const highlight_controller_;
  };

  explicit OverviewHighlightController(OverviewSession* overview_session);
  ~OverviewHighlightController();

  // Moves the |highlight_widget_| to the next traversable view.
  void MoveHighlight(bool reverse);

  // Called when a |view| that might be in the focus traversal rotation is about
  // to be deleted.
  void OnViewDestroyingOrDisabling(OverviewHighlightableView* view);

  // Sets and gets the visibility of |highlight_widget_|.
  void SetFocusHighlightVisibility(bool visible);
  bool IsFocusHighlightVisible() const;

  // Activates or closes the currently highlighted view (if any) if it supports
  // the activation or closing operations respectively.
  bool MaybeActivateHighlightedView();
  bool MaybeCloseHighlightedView();

  // Tries to get the item that is currently highlighted. Returns null if there
  // is no highlight, or if the highlight is on a desk view.
  OverviewItem* GetHighlightedItem() const;

  // Clears, creates or repositions the tab dragging highlight.
  void ClearTabDragHighlight();
  void UpdateTabDragHighlight(aura::Window* root_window,
                              const gfx::Rect& bounds_in_screen);
  bool IsTabDragHighlightVisible() const;

  // Called when an overview grid repositions its windows. Moves the focus
  // highlight widget without animation.
  void OnWindowsRepositioned(aura::Window* root_window);

 private:
  // Returns a vector of views that can be traversed via overview tabbing.
  // Includes desk mini views, the new desk button and overview items.
  std::vector<OverviewHighlightableView*> GetTraversableViews() const;

  void UpdateFocusWidget(OverviewHighlightableView* view_to_be_highlighted,
                         bool reverse);

  // The overview session which owns this object. Guaranteed to be non-null for
  // the lifetime of |this|.
  OverviewSession* const overview_session_;

  // If an item that is selected is deleted, store its index, so the next
  // traversal can pick up where it left off.
  base::Optional<int> deleted_index_ = base::nullopt;

  // The current view that |highlight_widget_| is highlighting.
  OverviewHighlightableView* highlighted_view_ = nullptr;

  // A background highlight that shows up when using keyboard traversal with tab
  // or arrow keys. This may not exist if the current highlighted view has its
  // own highlighting override.
  std::unique_ptr<HighlightWidget> highlight_widget_;

  // A background highlight that shows up when dragging a tab towards a chrome
  // window overview item, signaling that we can drop the tab into that browser
  // window.
  std::unique_ptr<HighlightWidget> tab_drag_widget_;

  DISALLOW_COPY_AND_ASSIGN(OverviewHighlightController);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_HIGHLIGHT_CONTROLLER_H_
