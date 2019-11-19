// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_PRESENTER_DELEGATE_H_
#define ASH_APP_LIST_APP_LIST_PRESENTER_DELEGATE_H_

#include <stdint.h>

#include "ash/app_list/app_list_export.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class AppListPresenterImpl;
class AppListView;
class AppListViewDelegate;

// Delegate of the app list presenter which allows customizing its behavior.
// The design of this interface was heavily influenced by the needs of Ash's
// app list implementation (see ash::AppListPresenterDelegateImpl).
class APP_LIST_EXPORT AppListPresenterDelegate {
 public:
  virtual ~AppListPresenterDelegate() {}

  // Sets the owner presenter of this delegate
  virtual void SetPresenter(AppListPresenterImpl* presenter) = 0;

  // Called to initialize the layout of the app list.
  virtual void Init(AppListView* view, int64_t display_id) = 0;
  virtual void ShowForDisplay(int64_t display_id) = 0;

  // Called when app list is closing.
  virtual void OnClosing() = 0;

  // Called when app list is closed.
  virtual void OnClosed() = 0;

  // Returns true if tablet mode is enabled.
  virtual bool IsTabletMode() const = 0;

  // Returns the view delegate, which will be passed into views so that views
  // can get access to Ash.
  virtual AppListViewDelegate* GetAppListViewDelegate() = 0;

  // Returns whether the on-screen keyboard is shown.
  virtual bool GetOnScreenKeyboardShown() = 0;

  // Returns the container parent of the given window.
  virtual aura::Window* GetContainerForWindow(aura::Window* window) = 0;

  // Returns the root Window for the given display id. If there is no display
  // for |display_id| null is returned.
  virtual aura::Window* GetRootWindowForDisplayId(int64_t display_id) = 0;

  // Called when the app list visibility changes.
  virtual void OnVisibilityChanged(bool visible, int64_t display_id) = 0;

  // Called when the app list target visibility changes.
  virtual void OnVisibilityWillChange(bool visible, int64_t display_id) = 0;

  // Whether the AppList is visible.
  virtual bool IsVisible() = 0;
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_PRESENTER_DELEGATE_H_
