// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_PRESENTER_DELEGATE_H_
#define ASH_APP_LIST_APP_LIST_PRESENTER_DELEGATE_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "base/optional.h"

namespace ash {

class AppListPresenterImpl;
class AppListView;
class AppListViewDelegate;
enum class AppListViewState;

// Delegate of the app list presenter which allows customizing its behavior.
// The design of this interface was heavily influenced by the needs of Ash's
// app list implementation (see ash::AppListPresenterDelegateImpl).
class ASH_EXPORT AppListPresenterDelegate {
 public:
  virtual ~AppListPresenterDelegate() {}

  // Sets the owner presenter of this delegate
  virtual void SetPresenter(AppListPresenterImpl* presenter) = 0;

  // Called to initialize the layout of the app list.
  virtual void Init(AppListView* view, int64_t display_id) = 0;
  virtual void ShowForDisplay(AppListViewState preferred_state,
                              int64_t display_id) = 0;

  // Called when app list is closing.
  virtual void OnClosing() = 0;

  // Called when app list is closed.
  virtual void OnClosed() = 0;

  // Returns the view delegate, which will be passed into views so that views
  // can get access to Ash.
  virtual AppListViewDelegate* GetAppListViewDelegate() = 0;

  // Returns whether the on-screen keyboard is shown.
  virtual bool GetOnScreenKeyboardShown() = 0;

  // Called when the app list visibility changes.
  virtual void OnVisibilityChanged(bool visible, int64_t display_id) = 0;

  // Called when the app list target visibility changes.
  virtual void OnVisibilityWillChange(bool visible, int64_t display_id) = 0;

  // Whether the AppList is visible.
  virtual bool IsVisible(const base::Optional<int64_t>& display_id) = 0;
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_PRESENTER_DELEGATE_H_
