// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_WIDGET_H_
#define ASH_SHELF_SHELF_WIDGET_H_

#include <memory>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/contextual_tooltip.h"
#include "ash/shelf/hotseat_transition_animator.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_background_animator.h"
#include "ash/shelf/shelf_component.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_observer.h"
#include "base/callback_helpers.h"
#include "ui/views/widget/widget.h"

namespace ash {
enum class AnimationChangeType;
class ApplicationDragAndDropHost;
class DragHandle;
class FocusCycler;
class HotseatWidget;
class LoginShelfView;
class LoginShelfGestureController;
class Shelf;
class ShelfLayoutManager;
class ShelfNavigationWidget;
class ShelfView;
class StatusAreaWidget;

// The ShelfWidget manages the shelf view (which contains the shelf icons) and
// the status area widget. There is one ShelfWidget per display. It is created
// early during RootWindowController initialization.
class ASH_EXPORT ShelfWidget : public SessionObserver,
                               public ShelfComponent,
                               public ShelfLayoutManagerObserver,
                               public ShelfObserver,
                               public views::Widget {
 public:
  explicit ShelfWidget(Shelf* shelf);

  ShelfWidget(const ShelfWidget&) = delete;
  ShelfWidget& operator=(const ShelfWidget&) = delete;

  ~ShelfWidget() override;

  // Sets the initial session state and show the UI. Not part of the constructor
  // because showing the UI triggers the accessibility checks in browser_tests,
  // which will crash unless the constructor returns, allowing the caller
  // to store the constructed widget.
  void Initialize(aura::Window* shelf_container);

  // Clean up prior to deletion.
  void Shutdown();

  void OnTabletModeChanged();

  ShelfBackgroundType GetBackgroundType() const;

  // Gets the alpha value of |background_type|.
  int GetBackgroundAlphaValue(ShelfBackgroundType background_type) const;

  const Shelf* shelf() const { return shelf_; }
  void RegisterHotseatWidget(HotseatWidget* hotseat_widget);
  ShelfLayoutManager* shelf_layout_manager() { return shelf_layout_manager_; }

  // TODO(manucornet): Remove these getters once all callers get the shelf
  // components from the shelf directly.
  ShelfNavigationWidget* navigation_widget() const {
    return shelf_->navigation_widget();
  }
  HotseatWidget* hotseat_widget() const { return shelf_->hotseat_widget(); }
  StatusAreaWidget* status_area_widget() const {
    return shelf_->status_area_widget();
  }
  void PostCreateShelf();

  bool IsShowingAppList() const;
  bool IsShowingMenu() const;

  // Sets the focus cycler. Also adds the shelf to the cycle.
  void SetFocusCycler(FocusCycler* focus_cycler);
  FocusCycler* GetFocusCycler();

  // See Shelf::GetScreenBoundsOfItemIconForWindow().
  gfx::Rect GetScreenBoundsOfItemIconForWindow(aura::Window* window);

  // Returns the bounds of the shelf on the screen. The returned rect does
  // not include portions of the shelf that extend beyond its own display,
  // as those are not visible to the user.
  gfx::Rect GetVisibleShelfBounds() const;

  // Returns the ApplicationDragAndDropHost for this shelf.
  ApplicationDragAndDropHost* GetDragAndDropHostForAppList();

  // Fetch the LoginShelfView instance.
  LoginShelfView* login_shelf_view() { return login_shelf_view_; }

  void set_default_last_focusable_child(bool default_last_focusable_child);

  // views::Widget:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  bool OnNativeWidgetActivationChanged(bool active) override;

  // ShelfComponent:
  void CalculateTargetBounds() override;
  gfx::Rect GetTargetBounds() const override;
  void UpdateLayout(bool animate) override;
  void UpdateTargetBoundsForGesture(int shelf_position) override;

  // Called when shelf layout manager detects a locale change.
  void HandleLocaleChange();

  // TODO(manucornet): Remove this method when all this widget's layout
  // logic is part of this class.
  void set_target_bounds(gfx::Rect target_bounds) {
    target_bounds_ = target_bounds;
  }

  // ShelfLayoutManagerObserver:
  void WillDeleteShelfLayoutManager() override;
  void OnHotseatStateChanged(HotseatState old_state,
                             HotseatState new_state) override;

  // ShelfObserver:
  void OnBackgroundTypeChanged(ShelfBackgroundType background_type,
                               AnimationChangeType change_type) override;

  // SessionObserver overrides:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnUserSessionAdded(const AccountId& account_id) override;

  SkColor GetShelfBackgroundColor() const;
  bool GetHitTestRects(aura::Window* target,
                       gfx::Rect* hit_test_rect_mouse,
                       gfx::Rect* hit_test_rect_touch);

  // Force to show hotseat in tablet mode. When the returned closure runner is
  // called or goes out of scope, it removes the caller as an instance to force
  // show hotseat. The hotseat will be shown as long as there is one
  // caller/instance force it to show.
  base::ScopedClosureRunner ForceShowHotseatInTabletMode();
  bool IsHotseatForcedShowInTabletMode() const;

  // Creates a login shelf gesture controller (which enabled login shelf gesture
  // detection). See ash/public/cpp/login_screen.h for more info.
  bool SetLoginShelfSwipeHandler(const std::u16string& nudge_text,
                                 const base::RepeatingClosure& fling_callback,
                                 base::OnceClosure exit_callback);

  // Resets a previously create login shelf gesture controller, if any.
  void ClearLoginShelfSwipeHandler();

  // Gets the layer used to draw the shelf background.
  ui::Layer* GetOpaqueBackground();

  // Gets the layer used to animate transitions between in-app and hotseat
  // background.
  ui::Layer* GetAnimatingBackground();

  // Gets the layer used to animate drag handle transitions between in-app and
  // home.
  ui::Layer* GetAnimatingDragHandle();

  // Gets the view used to display the drag handle on the in-app shelf.
  DragHandle* GetDragHandle();

  // Starts the animation to show the drag handle nudge.
  void ScheduleShowDragHandleNudge();

  // Starts the animation to hide the drag handle nudge.
  void HideDragHandleNudge(contextual_tooltip::DismissNudgeReason context);

  // Sets opacity of login shelf buttons to be consistent with shelf icons.
  void SetLoginShelfButtonOpacity(float target_opacity);

  // Handles shelf widget gesture events for login shelf, if login shelf view is
  // visible. Returns whether the gesture was handled (the gesture will not be
  // handled if the login shelf view is hidden).
  bool HandleLoginShelfGestureEvent(const ui::GestureEvent& event_in_screen);

  // Internal implementation detail. Do not expose outside of tests.
  ShelfView* shelf_view_for_testing() const {
    return hotseat_widget()->GetShelfView();
  }

  ShelfBackgroundAnimator* background_animator_for_testing() {
    return &background_animator_;
  }

  LoginShelfGestureController* login_shelf_gesture_controller_for_testing() {
    return login_shelf_gesture_controller_.get();
  }

  HotseatTransitionAnimator* hotseat_transition_animator() {
    return hotseat_transition_animator_.get();
  }

 private:
  class DelegateView;
  friend class DelegateView;

  // Hides shelf widget if IsVisible() returns true.
  void HideIfShown();

  // Shows shelf widget if IsVisible() returns false.
  void ShowIfHidden();

  ShelfView* GetShelfView();
  const ShelfView* GetShelfView() const;

  // Callback returned by ForceShowHotseatInTabletMode().
  void ResetForceShowHotseat();

  Shelf* shelf_;
  gfx::Rect target_bounds_;
  ShelfBackgroundAnimator background_animator_;

  // Owned by the shelf container's window.
  ShelfLayoutManager* shelf_layout_manager_;

  // Sets shelf opacity to 0 after all animations have completed.
  std::unique_ptr<ui::ImplicitAnimationObserver> hide_animation_observer_;

  // |delegate_view_| is the contents view of this widget and is cleaned up
  // during CloseChildWindows of the associated RootWindowController.
  DelegateView* delegate_view_;

  // Animates the shelf background to/from the hotseat background during hotseat
  // transitions.
  std::unique_ptr<HotseatTransitionAnimator> hotseat_transition_animator_;

  // View containing the shelf items for Login/Lock/OOBE/Add User screens.
  // Owned by the views hierarchy.
  LoginShelfView* login_shelf_view_;

  // Used to handle gestures on login shelf - created only if
  // SetLoginShelfSwipeHandler() gets called.
  std::unique_ptr<LoginShelfGestureController> login_shelf_gesture_controller_;

  ScopedSessionObserver scoped_session_observer_;

  size_t force_show_hotseat_count_ = 0;

  base::WeakPtrFactory<ShelfWidget> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_WIDGET_H_
