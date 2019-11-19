// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_PRESENTER_DELEGATE_IMPL_H_
#define ASH_APP_LIST_APP_LIST_PRESENTER_DELEGATE_IMPL_H_

#include <stdint.h>

#include "ash/app_list/app_list_presenter_delegate.h"
#include "ash/ash_export.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_observer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"
#include "ui/events/event_handler.h"

namespace ui {
class LocatedEvent;
}  // namespace ui

namespace ash {
class AppListControllerImpl;
class AppListPresenterImpl;
class AppListView;
class AppListViewDelegate;

// Responsible for laying out the app list UI as well as updating the Shelf
// launch icon as the state of the app list changes. Listens to shell events
// and touches/mouse clicks outside the app list to auto dismiss the UI or
// update its layout as necessary.
class ASH_EXPORT AppListPresenterDelegateImpl : public AppListPresenterDelegate,
                                                public ui::EventHandler,
                                                public display::DisplayObserver,
                                                public ShelfObserver {
 public:
  explicit AppListPresenterDelegateImpl(AppListControllerImpl* controller);
  ~AppListPresenterDelegateImpl() override;

  // AppListPresenterDelegate:
  void SetPresenter(AppListPresenterImpl* presenter) override;
  void Init(AppListView* view, int64_t display_id) override;
  void ShowForDisplay(int64_t display_id) override;
  void OnClosing() override;
  void OnClosed() override;
  bool IsTabletMode() const override;
  AppListViewDelegate* GetAppListViewDelegate() override;
  bool GetOnScreenKeyboardShown() override;
  aura::Window* GetContainerForWindow(aura::Window* window) override;
  aura::Window* GetRootWindowForDisplayId(int64_t display_id) override;
  void OnVisibilityChanged(bool visible, int64_t display_id) override;
  void OnVisibilityWillChange(bool visible, int64_t display_id) override;
  bool IsVisible() override;
  // DisplayObserver overrides:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // ShelfObserver:
  void OnBackgroundTypeChanged(ShelfBackgroundType background_type,
                               AnimationChangeType change_type) override;

 private:
  void ProcessLocatedEvent(ui::LocatedEvent* event);

  // ui::EventHandler overrides:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;

  // Snaps the app list window bounds to fit the screen size. (See
  // https://crbug.com/884889).
  void SnapAppListBoundsToDisplayEdge();

  // Callback function which is run after a bounds animation on |view_| is
  // ended. Handles activation of |view_|'s widget.
  void OnViewBoundsChangedAnimationEnded();

  // Whether the app list is visible (or in the process of being shown).
  bool is_visible_ = false;

  // Not owned. Pointer is guaranteed to be valid while this object is alive.
  AppListPresenterImpl* presenter_;

  // Owned by its widget.
  AppListView* view_ = nullptr;

  // Not owned, owns this class.
  AppListControllerImpl* const controller_ = nullptr;

  // An observer that notifies AppListView when the display has changed.
  ScopedObserver<display::Screen, display::DisplayObserver> display_observer_{
      this};

  // An observer that notifies AppListView when the shelf state has changed.
  ScopedObserver<Shelf, ShelfObserver> shelf_observer_{this};

  base::WeakPtrFactory<AppListPresenterDelegateImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppListPresenterDelegateImpl);
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_PRESENTER_DELEGATE_IMPL_H_
