// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_PRESENTER_APP_LIST_PRESENTER_DELEGATE_H_
#define ASH_APP_LIST_PRESENTER_APP_LIST_PRESENTER_DELEGATE_H_

#include <stdint.h>

#include "ash/app_list/presenter/app_list_presenter_export.h"

namespace aura {
class Window;
}  // namespace aura

namespace base {
class TimeDelta;
}  // namespace base

namespace gfx {
class Vector2d;
}  // namespace gfx

namespace app_list {

class AppListPresenterImpl;
class AppListView;
class AppListViewDelegate;

// Delegate of the app list presenter which allows customizing its behavior.
// The design of this interface was heavily influenced by the needs of Ash's
// app list implementation (see ash::AppListPresenterDelegateImpl).
class APP_LIST_PRESENTER_EXPORT AppListPresenterDelegate {
 public:
  virtual ~AppListPresenterDelegate() {}

  // Sets the owner presenter of this delegate
  virtual void SetPresenter(AppListPresenterImpl* presenter) = 0;

  // Called to initialize the layout of the app list.
  virtual void Init(AppListView* view,
                    int64_t display_id,
                    int current_apps_page) = 0;

  // Called when app list is shown.
  virtual void OnShown(int64_t display_id) = 0;

  // Called when app list is closing.
  virtual void OnClosing() = 0;

  // Called when app list is closed.
  virtual void OnClosed() = 0;

  // Returns the offset vector by which the app list window should animate
  // when it gets shown or hidden.
  virtual gfx::Vector2d GetVisibilityAnimationOffset(
      aura::Window* root_window) = 0;

  // Returns the animation duration in ms the app list window should animate
  // when shown or hidden.
  virtual base::TimeDelta GetVisibilityAnimationDuration(
      aura::Window* root_window,
      bool is_visible) = 0;

  // Returns true if the home launcher is enabled in tablet mode.
  virtual bool IsHomeLauncherEnabledInTabletMode() = 0;

  // Returns the view delegate, which will be passed into views so that views
  // can get access to Ash.
  virtual AppListViewDelegate* GetAppListViewDelegate() = 0;

  // Returns whether the on-screen keyboard is shown.
  virtual bool GetOnScreenKeyboardShown() = 0;

  // Returns the root Window for the given display id. If there is no display
  // for |display_id| null is returned.
  virtual aura::Window* GetRootWindowForDisplayId(int64_t display_id) = 0;

  // Called when the app list visibility changes.
  virtual void OnVisibilityChanged(bool visible, aura::Window* root_window) = 0;

  // Called when the app list target visibility changes.
  virtual void OnTargetVisibilityChanged(bool visible) = 0;

 protected:
  // Gets the duration for the hide animation for the fullscreen version of
  // the app list in Ms.
  static base::TimeDelta GetAnimationDurationFullscreen(bool is_side_shelf,
                                                        bool is_fullscreen);

  // Offset in pixels to animation away/towards the shelf.
  static const int kAnimationOffset = 8;

  // Offset for the hide animation for the fullscreen app list in DIPs.
  static const int kAnimationOffsetFullscreen = 400;
};

}  // namespace app_list

#endif  // ASH_APP_LIST_PRESENTER_APP_LIST_PRESENTER_DELEGATE_H_
