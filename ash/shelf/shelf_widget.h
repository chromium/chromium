// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_WIDGET_H_
#define ASH_SHELF_SHELF_WIDGET_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/session/session_observer.h"
#include "ash/shelf/shelf_background_animator.h"
#include "ash/shelf/shelf_background_animator_observer.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_observer.h"
#include "base/macros.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace app_list {
class ApplicationDragAndDropHost;
}

namespace ash {
enum class AnimationChangeType;
class AppListButton;
class BackButton;
class FocusCycler;
class LoginShelfView;
class Shelf;
class ShelfLayoutManager;
class ShelfView;
class StatusAreaWidget;

// The ShelfWidget manages the shelf view (which contains the shelf icons) and
// the status area widget. There is one ShelfWidget per display. It is created
// early during RootWindowController initialization.
class ASH_EXPORT ShelfWidget : public views::Widget,
                               public views::WidgetObserver,
                               public ShelfBackgroundAnimatorObserver,
                               public ShelfLayoutManagerObserver,
                               public ShelfObserver,
                               public SessionObserver {
 public:
  ShelfWidget(aura::Window* shelf_container, Shelf* shelf);
  ~ShelfWidget() override;

  // Sets the initial session state and show the UI. Not part of the constructor
  // because showing the UI triggers the accessibility checks in browser_tests,
  // which will crash unless the constructor returns, allowing the caller
  // to store the constructed widget.
  void Initialize();

  // Clean up prior to deletion.
  void Shutdown();

  // Returns true if the views-based shelf is being shown.
  static bool IsUsingViewsShelf();

  void CreateStatusAreaWidget(aura::Window* status_container);

  void OnShelfAlignmentChanged();

  void OnTabletModeChanged();

  // Sets the shelf's background type.
  void SetPaintsBackground(ShelfBackgroundType background_type,
                           AnimationChangeType change_type);
  ShelfBackgroundType GetBackgroundType() const;

  // Gets the alpha value of |background_type|.
  int GetBackgroundAlphaValue(ShelfBackgroundType background_type) const;

  const Shelf* shelf() const { return shelf_; }
  ShelfLayoutManager* shelf_layout_manager() { return shelf_layout_manager_; }
  StatusAreaWidget* status_area_widget() const {
    return status_area_widget_.get();
  }

  void PostCreateShelf();

  bool IsShowingAppList() const;
  bool IsShowingContextMenu() const;
  bool IsShowingOverflowBubble() const;

  // Sets the focus cycler.  Also adds the shelf to the cycle.
  void SetFocusCycler(FocusCycler* focus_cycler);
  FocusCycler* GetFocusCycler();

  // See Shelf::GetScreenBoundsOfItemIconForWindow().
  gfx::Rect GetScreenBoundsOfItemIconForWindow(aura::Window* window);

  // Returns the button that opens the app launcher.
  AppListButton* GetAppListButton() const;

  // Returns the browser back button.
  BackButton* GetBackButton() const;

  // Returns the ApplicationDragAndDropHost for this shelf.
  app_list::ApplicationDragAndDropHost* GetDragAndDropHostForAppList();

  // Fetch the LoginShelfView instance.
  LoginShelfView* login_shelf_view() { return login_shelf_view_; }

  void set_default_last_focusable_child(bool default_last_focusable_child);

  // Overridden from views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // ShelfBackgroundAnimatorObserver overrides:
  void UpdateShelfItemBackground(SkColor color) override;

  // ShelfLayoutManagerObserver overrides:
  void WillDeleteShelfLayoutManager() override;

  // ShelfObserver:
  void OnBackgroundTypeChanged(ShelfBackgroundType background_type,
                               AnimationChangeType change_type) override;

  // SessionObserver overrides:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // Internal implementation detail. Do not expose outside of tests.
  ShelfView* shelf_view_for_testing() const { return shelf_view_; }
  ShelfBackgroundAnimator* background_animator_for_testing() {
    return &background_animator_;
  }

  void set_activated_from_overflow_bubble(bool val) {
    activated_from_overflow_bubble_ = val;
  }

 private:
  class DelegateView;
  friend class DelegateView;

  // Hides shelf widget if IsVisible() returns true.
  void HideIfShown();

  // Shows shelf widget if IsVisible() returns false.
  void ShowIfHidden();

  // views::Widget:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  Shelf* shelf_;

  ShelfBackgroundAnimator background_animator_;

  // Owned by the shelf container's window.
  ShelfLayoutManager* shelf_layout_manager_;

  std::unique_ptr<StatusAreaWidget> status_area_widget_;

  // |delegate_view_| is the contents view of this widget and is cleaned up
  // during CloseChildWindows of the associated RootWindowController.
  DelegateView* delegate_view_;

  // View containing the shelf items within an active user session. Owned by
  // the views hierarchy.
  ShelfView* const shelf_view_;

  // View containing the shelf items for Login/Lock/OOBE/Add User screens.
  // Owned by the views hierarchy.
  LoginShelfView* const login_shelf_view_;

  // Set to true when the widget is activated from the shelf overflow bubble.
  // Do not focus the default element in this case. This should be set when
  // cycling focus from the overflow bubble to the main shelf.
  bool activated_from_overflow_bubble_ = false;

  ScopedSessionObserver scoped_session_observer_;

  DISALLOW_COPY_AND_ASSIGN(ShelfWidget);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_WIDGET_H_
