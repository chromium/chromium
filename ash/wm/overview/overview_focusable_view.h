// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_FOCUSABLE_VIEW_H_
#define ASH_WM_OVERVIEW_OVERVIEW_FOCUSABLE_VIEW_H_

#include "ash/ash_export.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace views {
class View;
}  // namespace views

namespace ash {

class OverviewItemBase;
class OverviewSession;

// An interface that must be implemented by classes that want to be
// focused in overview.
class ASH_EXPORT OverviewFocusableView {
 public:
  // Returns the view class associated with `this`.
  virtual views::View* GetView() = 0;

  // Returns nullptr, unless `this` is associated with an `OverviewItem`.
  virtual OverviewItemBase* GetOverviewItem();

  // Attempts to activate or close this view. Overriders may do nothing. Closing
  // supports a primary action and a secondary action, as some overrides may
  // have two actions that are related to closing.
  virtual void MaybeActivateFocusedView() = 0;
  virtual void MaybeCloseFocusedView(bool primary_action) = 0;

  // Attempts to swap the view with its neighbor views. (Mainly used for
  // |DeskMiniView|).
  virtual void MaybeSwapFocusedView(bool right) = 0;

  // Activates focused view when exiting overview. Currently, it is only
  // used for the case of exiting overview by using 3-finger vertical swipes.
  // Note that not all the focused views support this behavior. Return
  // true means the focused view is activated and the overview is exited.
  virtual bool MaybeActivateFocusedViewOnOverviewExit(
      OverviewSession* overview_session);

  // Returns the point the accessibility magnifiers should focus when this is
  // focused. If not overridden, this will return the centerpoint.
  virtual gfx::Point GetMagnifierFocusPointInScreen();

  bool is_focused() const { return is_focused_; }

  void SetFocused(bool visible);

 protected:
  virtual ~OverviewFocusableView() = default;

  // Focuses or blurs this view.
  virtual void OnFocusableViewFocused() = 0;
  virtual void OnFocusableViewBlurred() = 0;

 private:
  bool is_focused_ = false;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_FOCUSABLE_VIEW_H_
