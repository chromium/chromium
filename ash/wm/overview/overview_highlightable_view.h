// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_HIGHLIGHTABLE_VIEW_H_
#define ASH_WM_OVERVIEW_OVERVIEW_HIGHLIGHTABLE_VIEW_H_

#include "ash/ash_export.h"
#include "ui/gfx/geometry/point.h"

namespace views {
class View;
}

namespace ash {
class OverviewSession;

// An interface that must be implemented by classes that want to be
// highlighted in overview.
class ASH_EXPORT OverviewHighlightableView {
 public:
  // Get the view class associated with |this|.
  virtual views::View* GetView() = 0;

  // Attempts to activate or close this view. Overriders may do nothing. Closing
  // supports a primary action and a secondary action, as some overrides may
  // have two actions that are related to closing.
  virtual void MaybeActivateHighlightedView() = 0;
  virtual void MaybeCloseHighlightedView(bool primary_action) = 0;

  // Attempts to swap the view with its neighbor views. (Mainly used for
  // |DeskMiniView|).
  virtual void MaybeSwapHighlightedView(bool right) = 0;

  // Activates highlighted view when exiting overview. Currently, it is only
  // used for the case of exiting overview by using 3-finger vertical swipes.
  // Note that not all the highlighted views support this behavior. Return
  // true means the highlighted view is activated and the overview is exited.
  virtual bool MaybeActivateHighlightedViewOnOverviewExit(
      OverviewSession* overview_session);

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

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_HIGHLIGHTABLE_VIEW_H_
