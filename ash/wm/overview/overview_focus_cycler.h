// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_FOCUS_CYCLER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_FOCUS_CYCLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
class OverviewFocusableView;
class OverviewItemBase;
class OverviewSession;
class ScopedA11yOverrideWindowSetter;

// Manages focusing items while in overview. Responsible for telling
// focusable items to show or hide their focus ring borders, or when tabbing
// through focusable items with arrow keys and trackpad swipes. In this
// context, an focusable item can represent anything focusable in overview
// mode such as a desk textfield, saved desk button and an `OverviewItem`. The
// idea behind the movement strategy is that it should be possible to access any
// focusable view via keyboard by pressing the tab or arrow keys repeatedly.
// Views API has built in support for focus traversal, but we don't use it here
// since we need to tab between different widgets, some of which may not be
// activatable.
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
// The focus ring is switched to the next window grid (if available) or wrapped
// if it reaches the end of its movement sequence.
class ASH_EXPORT OverviewFocusCycler {
 public:
  explicit OverviewFocusCycler(OverviewSession* overview_session);
  OverviewFocusCycler(const OverviewFocusCycler&) = delete;
  OverviewFocusCycler& operator=(const OverviewFocusCycler&) = delete;
  ~OverviewFocusCycler();

  OverviewFocusableView* focused_view() { return focused_view_; }

  // Moves the focus ring to the next traversable view.
  void MoveFocus(bool reverse);

  // Called when pressing two save desks buttons to show the saved desk grids
  // and focus on a saved desk name view.
  void UpdateA11yFocusWindow(OverviewFocusableView* name_view);

  // Moves the focus ring directly to `target_view`. `target_view` must be a
  // traversable view, i.e. one of the views returned by
  // `GetTraversableViews()`. This should be used when a view requests focus
  // directly so the overview focus ring can be in-sync with focus. Due to this
  // expected use, it should not normally be necessary to trigger an
  // accessibility event.
  void MoveFocusToView(OverviewFocusableView* target_view,
                       bool suppress_accessibility_event = true);

  // Called when a |view| that might be in the focus traversal rotation is about
  // to be deleted.
  // Note: When removing multiple focusable views in one call, by calling
  // this function repeatedly, make sure to call it in reverse order (i.e. on
  // the views that come later in the traversal order first). This makes sure
  // that traversal continues correctly from where it was left off.
  void OnViewDestroyingOrDisabling(OverviewFocusableView* view);

  // Hides the focus ring for `focused_view_` if it is not null, but keeps the
  // pointer. Used for dragging to temporarily hide the focus.
  void SetFocusVisibility(bool visible);
  bool IsFocusVisible() const;

  // Activates or closes the currently focused view (if any) if it supports the
  // activation or closing operations respectively.
  bool MaybeActivateFocusedView();
  bool MaybeCloseFocusedView(bool primary_action);

  // Swaps the currently focused view with its neighbor views.
  bool MaybeSwapFocusedView(bool right);

  // Activates focused view when exiting overview mode.
  bool MaybeActivateFocusedViewOnOverviewExit();

  // Tries to get the item that is currently focused. Returns null if there
  // is no focus, or if the focus is on a desk view.
  OverviewItemBase* GetFocusedItem() const;

  // If `focused_view_` is not null, remove focus. The next tab will start at
  // the beginning of the tab order. This can be used when a lot of views are
  // getting removed or hidden, such as when the desks bar goes from expanded to
  // zero state.
  void ResetFocusedView();

 private:
  // Returns a vector of views that can be traversed via overview tabbing.
  // Includes desk mini views, the new desk button, overview items, etc.
  std::vector<OverviewFocusableView*> GetTraversableViews() const;

  // Sets `focused_view_` to `view_to_be_focused` and updates the
  // focus ring visibility for the previous `focused_view_`.
  // `suppress_accessibility_event` should be true if `view_to_be_focused`
  // will also request focus to avoid double emitting event.
  void UpdateFocus(OverviewFocusableView* view_to_be_focused,
                   bool suppress_accessibility_event = false);

  // The overview session which owns this object. Guaranteed to be non-null for
  // the lifetime of `this`.
  const raw_ptr<OverviewSession> overview_session_;

  // If an item that is selected is deleted, store its index, so the next
  // traversal can pick up where it left off.
  absl::optional<int> deleted_index_ = absl::nullopt;

  // The current view that is being focused, if any.
  raw_ptr<OverviewFocusableView, LeakedDanglingUntriaged> focused_view_ =
      nullptr;

  // Helps to update the current a11y override window. And accessibility
  // features will focus on the window that is being set. Once `this` goes out
  // of scope, the a11y override window is set to nullptr.
  std::unique_ptr<ScopedA11yOverrideWindowSetter> scoped_a11y_overrider_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_FOCUS_CYCLER_H_
