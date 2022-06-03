// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APPS_GRID_VIEW_FOCUS_DELEGATE_H_
#define ASH_APP_LIST_VIEWS_APPS_GRID_VIEW_FOCUS_DELEGATE_H_

#include "ash/ash_export.h"

namespace ash {

// A delegate which allows an AppsGridView to request custom focus changes.
class ASH_EXPORT AppsGridViewFocusDelegate {
 public:
  // Requests that focus move up and out (usually to the recent apps list).
  // `column` is the column of the item that was focused in the grid.
  // The delegate should choose an appropriate item to focus. Returns true if
  // the delegate handled the move.
  virtual bool MoveFocusUpFromAppsGrid(int column) = 0;

 protected:
  virtual ~AppsGridViewFocusDelegate() = default;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APPS_GRID_VIEW_FOCUS_DELEGATE_H_
