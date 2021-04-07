// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_PRESENTER_DELEGATE_H_
#define ASH_APP_LIST_APP_LIST_PRESENTER_DELEGATE_H_

#include <stdint.h>

#include "ash/ash_export.h"

namespace ash {

class AppListPresenterImpl;
class AppListView;
enum class AppListViewState;

// Delegate of the app list presenter which allows customizing its behavior.
// The design of this interface was heavily influenced by the needs of Ash's
// app list implementation (see ash::AppListPresenterDelegateImpl).
class ASH_EXPORT AppListPresenterDelegate {
 public:
  virtual ~AppListPresenterDelegate() {}

  // Sets the owner presenter of this delegate
  virtual void SetPresenter(AppListPresenterImpl* presenter) = 0;

  // Sets the |view| for this delegate. Must be called before ShowForDisplay().
  virtual void SetView(AppListView* view) = 0;

  // Called to show the app list on a given display.
  virtual void ShowForDisplay(AppListViewState preferred_state,
                              int64_t display_id) = 0;

  // Called when app list is closing.
  virtual void OnClosing() = 0;

  // Called when app list is closed.
  virtual void OnClosed() = 0;
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_PRESENTER_DELEGATE_H_
