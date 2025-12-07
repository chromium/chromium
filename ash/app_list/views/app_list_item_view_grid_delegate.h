// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_ITEM_VIEW_GRID_DELEGATE_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_ITEM_VIEW_GRID_DELEGATE_H_

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"

namespace ui {
class Event;
}  // namespace ui

namespace ash {
class AppListItemView;

// The parent apps grid (AppsGridView) or a stub. Not named "Delegate" to
// differentiate it from AppListViewDelegate.
class ASH_EXPORT AppListItemViewGridDelegate {
 public:
  virtual ~AppListItemViewGridDelegate() = default;

  // Whether the parent apps grid (if any) is a folder.
  virtual bool IsInFolder() const = 0;

  // Methods for keyboard selection.
  virtual void SetSelectedView(AppListItemView* view) = 0;
  virtual void ClearSelectedView() = 0;
  virtual bool IsSelectedView(const AppListItemView* view) const = 0;

  // Provides a callback to interrupt an ongoing drag operation.
  virtual void EndDrag(bool cancel) = 0;

  // Provided as a callback for AppListItemView to notify of activation via
  // press/click/return key.
  virtual void OnAppListItemViewActivated(AppListItemView* pressed_item_view,
                                          const ui::Event& event) = 0;

  // Whether the app list item view would be visible in the default apps grid
  // view state. For example, for scrollable apps grid view, an item view would
  // be above the fold if it were visible when the apps grid view is not
  // scrolled.
  virtual bool IsAboveTheFold(AppListItemView* item_view) = 0;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_ITEM_VIEW_GRID_DELEGATE_H_
