// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_H_
#define ASH_SHELF_SHELF_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/desk_button_widget.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_locking_manager.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace ui {
class GestureEvent;
class MouseWheelEvent;
class MouseEvent;
class ScrollEvent;
}  // namespace ui

namespace ash {

enum class AnimationChangeType;
class HotseatWidget;
class HotseatWidgetAnimationMetricsReporter;
class NavigationWidgetAnimationMetricsReporter;
class ShelfFocusCycler;
class LoginShelfWidget;
class ShelfLayoutManager;
class ShelfLayoutManagerTest;
class ShelfLockingManager;
class ShelfNavigationWidget;
class ShelfView;
class ShelfWidget;
class StatusAreaWidget;
class ShelfObserver;
class WorkAreaInsets;
class ShelfTooltipManager;

// TODO(oshima) : move to .cc

// Returns a value based on shelf alignment.
template <typename T>
T SelectValueByShelfAlignment(ShelfAlignment alignment,
                              T bottom,
                              T left,
                              T right) {
  switch (alignment) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      return bottom;
    case ShelfAlignment::kLeft:
      return left;
    case ShelfAlignment::kRight:
      return right;
  }
  NOTREACHED();
}

bool IsHorizontalAlignment(ShelfAlignment alignment);

// Returns |horizontal| if shelf is horizontal, otherwise |vertical|.
template <typename T>
T PrimaryAxisValueByShelfAlignment(ShelfAlignment alignment,
                                   T horizontal,
                                   T vertical) {
  return IsHorizontalAlignment(alignment) ? horizontal : vertical;
}

// Controller for the shelf state. One per display, because each display might
// have different shelf alignment, autohide, etc. Exists for the lifetime of the
// root window controller.
class ASH_EXPORT Shelf : public ShelfLayoutManagerObserver {
 public:
  // Used to maintain a lock for the auto-hide shelf. If lock, then we should
  // not update the state of the auto-hide shelf.
  class ScopedAutoHideLock {
   public:
    explicit ScopedAutoHideLock(Shelf* shelf) : shelf_(shelf) {
      ++shelf_->auto_hide_lock_;
    }

    ScopedAutoHideLock(const ScopedAutoHideLock&) = delete;
    ScopedAutoHideLock& operator=(const ScopedAutoHideLock&) = delete;

    ~ScopedAutoHideLock() {
      --shelf_->auto_hide_lock_;
      DCHECK_GE(shelf_->auto_hide_lock_, 0);
    }

   private:
    raw_ptr<Shelf> shelf_;
  };

  // Used to disable auto-hide shelf behavior while in scope. Note that
  // disabling auto-hide behavior is of lower precedence than auto-hide behavior
  // based on locks and session state, so it is not guaranteed to show the shelf
  // in all cases.
  class ScopedDisableAutoHide {
   public:
    explicit ScopedDisableAutoHide(Shelf* shelf);
    ScopedDisableAutoHide(const ScopedDisableAutoHide&) = delete;
    ScopedDisableAutoHide& operator=(const ScopedDisableAutoHide&) = delete;
    ~ScopedDisableAutoHide();

    Shelf* weak_shelf() { return shelf_.get(); }

   private:
    // Save a `base::WeakPtr` to avoid a crash if `shelf_` is deallocated due to
    // monitor disconnect.
    base::WeakPtr<Shelf> const shelf_;
  };

  Shelf();

  Shelf(const Shelf&) = delete;
  Shelf& operator=(const Shelf&) = delete;

  ~Shelf() override;

  // Returns the shelf for the display that |window| is on. Note that the shelf
  // widget may not exist, or the shelf may not be visible.
  static Shelf* ForWindow(aura::Window* window);

  // Launch a 0-indexed shelf item in the shelf. A negative index launches the
  // last shelf item in the shelf.
  static void LaunchShelfItem(int item_index);

  // Activates the shelf item specified by the index in the list of shelf items.
  static void ActivateShelfItem(int item_index);

  // Activates the shelf item specified by the index in the list of shelf items
  // on the display identified by |display_id|.
  static void ActivateShelfItemOnDisplay(int item_index, int64_t display_id);

  // Updates the shelf visibility on all displays. This method exists for
  // historical reasons. If a display or shelf instance is available, prefer
  // Shelf::UpdateVisibilityState() below.
  static void UpdateShelfVisibility();

  void CreateNavigationWidget(aura::Window* container);
  void CreateDeskButtonWidget(aura::Window* container);
  void CreateHotseatWidget(aura::Window* container);
  void CreateStatusAreaWidget(aura::Window* status_container);
  void CreateShelfWidget(aura::Window* root);

  // Begins shutdown of the ShelfWidget and all child widgets.
  void ShutdownShelfWidget();

  // Resets `shelf_widget_`.
  void DestroyShelfWidget();

  // Returns true if the shelf is visible. Shelf can be visible in 1)
  // SHELF_VISIBLE or 2) SHELF_AUTO_HIDE but in SHELF_AUTO_HIDE_SHOWN. See
  // details in ShelfLayoutManager::IsVisible.
  bool IsVisible() const;

  // Returns the window showing the shelf.
  const aura::Window* GetWindow() const;
  aura::Window* GetWindow();

  void SetAlignment(ShelfAlignment alignment);

  // Returns true if the shelf alignment is horizontal (i.e. at the bottom).
  bool IsHorizontalAlignment() const;

  // Returns a value based on shelf alignment.
  template <typename T>
  T SelectValueForShelfAlignment(T bottom, T left, T right) const {
    return SelectValueByShelfAlignment(alignment_, bottom, left, right);
  }

  // Returns |horizontal| if shelf is horizontal, otherwise |vertical|.
  template <typename T>
  T PrimaryAxisValue(T horizontal, T vertical) const {
    return IsHorizontalAlignment() ? horizontal : vertical;
  }

  void SetAutoHideBehavior(ShelfAutoHideBehavior behavior);

  ShelfAutoHideState GetAutoHideState() const;

  // Invoke when the auto-hide state may have changed (for example, when the
  // system tray bubble opens it should force the shelf to be visible).
  void UpdateAutoHideState();

  ShelfBackgroundType GetBackgroundType() const;

  void UpdateVisibilityState();

  void MaybeUpdateShelfBackground();

  ShelfVisibilityState GetVisibilityState() const;

  gfx::Rect GetShelfBoundsInScreen() const;

  // Returns the ideal bounds of the shelf assuming it is visible.
  gfx::Rect GetIdealBounds() const;

  // Returns the ideal bounds of the shelf, but in tablet mode always returns
  // the bounds of the in-app shelf.
  gfx::Rect GetIdealBoundsForWorkAreaCalculation();

  // Returns the screen bounds of the item for the specified window. If there is
  // no item for the specified window an empty rect is returned.
  gfx::Rect GetScreenBoundsOfItemIconForWindow(aura::Window* window);

  // Handles a gesture |event| coming from a source outside the shelf widget
  // (e.g. the status area widget). Allows support for behaviors like toggling
  // auto-hide with a swipe, even if that gesture event hits another window.
  // Returns true if the event was handled.
  bool ProcessGestureEvent(const ui::GestureEvent& event);

  // Handles a mouse |event| coming from the Shelf.
  void ProcessMouseEvent(const ui::MouseEvent& event);

  // Handles a scroll |event| coming from the Shelf.
  void ProcessScrollEvent(ui::ScrollEvent* event);

  // Handles a mousewheel scroll event coming from the shelf.
  void ProcessMouseWheelEvent(ui::MouseWheelEvent* event);

  void AddObserver(ShelfObserver* observer);
  void RemoveObserver(ShelfObserver* observer);

  void NotifyShelfIconPositionsChanged();

  StatusAreaWidget* GetStatusAreaWidget() const;

  // Get the anchor rect that the system tray bubble and the notification center
  // bubble will be anchored.
  // x() and y() designates anchor point, but width() and height() are dummy.
  // See also: BubbleDialogDelegateView::GetBubbleBounds()
  gfx::Rect GetSystemTrayAnchorRect() const;

  // Returns whether this shelf should be hidden on secondary display in a given
  // |state|.
  bool ShouldHideOnSecondaryDisplay(session_manager::SessionState state);

  void SetVirtualKeyboardBoundsForTesting(const gfx::Rect& bounds);
  ShelfLockingManager* GetShelfLockingManagerForTesting();
  ShelfView* GetShelfViewForTesting();

  ShelfLayoutManager* shelf_layout_manager() const {
    return shelf_layout_manager_;
  }

  // Getters for the various shelf components.
  ShelfWidget* shelf_widget() const { return shelf_widget_.get(); }
  ShelfNavigationWidget* navigation_widget() const {
    return navigation_widget_.get();
  }
  DeskButtonWidget* desk_button_widget() const {
    return desk_button_widget_.get();
  }
  HotseatWidget* hotseat_widget() const { return hotseat_widget_.get(); }
  StatusAreaWidget* status_area_widget() const {
    return status_area_widget_.get();
  }
  LoginShelfWidget* login_shelf_widget() { return login_shelf_widget_.get(); }

  ShelfAlignment alignment() const { return alignment_; }
  ShelfAutoHideBehavior auto_hide_behavior() const {
    return auto_hide_behavior_;
  }

  ShelfAlignment in_session_alignment() const {
    return shelf_locking_manager_.in_session_alignment();
  }

  ShelfAutoHideBehavior in_session_auto_hide_behavior() const {
    return shelf_locking_manager_.in_session_auto_hide_behavior();
  }

  ShelfFocusCycler* shelf_focus_cycler() { return shelf_focus_cycler_.get(); }

  int auto_hide_lock() const { return auto_hide_lock_; }
  int disable_auto_hide() const { return disable_auto_hide_; }

  ShelfTooltipManager* tooltip() { return tooltip_.get(); }

  // |target_state| is the hotseat state after hotseat transition animation.
  metrics_util::ReportCallback GetHotseatTransitionReportCallback(
      HotseatState target_state);
  metrics_util::ReportCallback GetTranslucentBackgroundReportCallback(
      HotseatState target_state);

  metrics_util::ReportCallback GetNavigationWidgetAnimationReportCallback(
      HotseatState target_hotseat_state);

 protected:
  // ShelfLayoutManagerObserver:
  void WillDeleteShelfLayoutManager() override;
  void OnShelfVisibilityStateChanged(ShelfVisibilityState new_state) override;
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;
  void OnBackgroundUpdated(ShelfBackgroundType background_type,
                           AnimationChangeType change_type) override;
  void OnHotseatStateChanged(HotseatState old_state,
                             HotseatState new_state) override;
  void OnWorkAreaInsetsChanged() override;

 private:
  class AutoDimEventHandler;
  class AutoHideEventHandler;
  friend class DimShelfLayoutManagerTestBase;
  friend class ShelfLayoutManagerTest;

  // Uses Auto Dim Event Handler to update the shelf dim state.
  void DimShelf();
  void UndimShelf();
  bool HasDimShelfTimer();

  // Returns work area insets object for the window with this shelf.
  WorkAreaInsets* GetWorkAreaInsets() const;

  base::WeakPtr<Shelf> GetWeakPtr();

  // Layout manager for the shelf container window. Instances are constructed by
  // ShelfWidget and lifetimes are managed by the container windows themselves.
  raw_ptr<ShelfLayoutManager> shelf_layout_manager_ = nullptr;

  // Pointers to shelf components.
  std::unique_ptr<ShelfNavigationWidget> navigation_widget_;
  std::unique_ptr<DeskButtonWidget> desk_button_widget_;
  std::unique_ptr<HotseatWidget> hotseat_widget_;
  std::unique_ptr<StatusAreaWidget> status_area_widget_;
  // Null during display teardown, see WindowTreeHostManager::DeleteHost() and
  // RootWindowController::CloseAllChildWindows().
  std::unique_ptr<ShelfWidget> shelf_widget_;
  std::unique_ptr<LoginShelfWidget> login_shelf_widget_;

  // These initial values hide the shelf until user preferences are available.
  ShelfAlignment alignment_ = ShelfAlignment::kBottomLocked;
  ShelfAutoHideBehavior auto_hide_behavior_ =
      ShelfAutoHideBehavior::kAlwaysHidden;

  // Sets shelf alignment to bottom during login and screen lock.
  ShelfLockingManager shelf_locking_manager_;

  base::ObserverList<ShelfObserver>::Unchecked observers_;

  // Forwards mouse and gesture events to ShelfLayoutManager for auto-hide.
  std::unique_ptr<AutoHideEventHandler> auto_hide_event_handler_;

  // Forwards mouse and gesture events to ShelfLayoutManager for auto-dim.
  std::unique_ptr<AutoDimEventHandler> auto_dim_event_handler_;

  // Hands focus off to different parts of the shelf.
  std::unique_ptr<ShelfFocusCycler> shelf_focus_cycler_;

  // Animation metrics reporter for hotseat animations. Owned by the Shelf to
  // ensure it outlives the Hotseat Widget.
  std::unique_ptr<HotseatWidgetAnimationMetricsReporter>
      hotseat_transition_metrics_reporter_;

  // Metrics reporter for animations of the traslucent background in the
  // hotseat. Owned by the Shelf to ensure it outlives the Hotseat Widget.
  std::unique_ptr<HotseatWidgetAnimationMetricsReporter>
      translucent_background_metrics_reporter_;

  // Animation metrics reporter for navigation widget animations. Owned by the
  // Shelf to ensure it outlives the Navigation Widget.
  std::unique_ptr<NavigationWidgetAnimationMetricsReporter>
      navigation_widget_metrics_reporter_;

  // Used by ScopedAutoHideLock to maintain the state of the lock for auto-hide
  // shelf.
  int auto_hide_lock_ = 0;

  // Used by `ScopedDisableAutoHide` to disable auto-hide shelf behavior.
  int disable_auto_hide_ = 0;

  std::unique_ptr<ShelfTooltipManager> tooltip_;

  base::WeakPtrFactory<Shelf> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_H_
