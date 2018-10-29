// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_H_
#define ASH_SHELF_SHELF_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_locking_manager.h"
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
}

namespace ash {

enum class AnimationChangeType;
class ShelfBezelEventHandler;
class ShelfLayoutManager;
class ShelfLayoutManagerTest;
class ShelfLockingManager;
class ShelfView;
class ShelfWidget;
class StatusAreaWidget;
class ShelfObserver;
class TrayBackgroundView;

// Controller for the shelf state. One per display, because each display might
// have different shelf alignment, autohide, etc. Exists for the lifetime of the
// root window controller.
class ASH_EXPORT Shelf : public ShelfLayoutManagerObserver {
 public:
  Shelf();
  ~Shelf() override;

  // Returns the shelf for the display that |window| is on. Note that the shelf
  // widget may not exist, or the shelf may not be visible.
  static Shelf* ForWindow(aura::Window* window);

  void CreateShelfWidget(aura::Window* root);
  void ShutdownShelfWidget();
  void DestroyShelfWidget();

  ShelfLayoutManager* shelf_layout_manager() const {
    return shelf_layout_manager_;
  }

  ShelfWidget* shelf_widget() { return shelf_widget_.get(); }

  // Returns the window showing the shelf.
  aura::Window* GetWindow();

  ShelfAlignment alignment() const { return alignment_; }
  void SetAlignment(ShelfAlignment alignment);

  // Returns true if the shelf alignment is horizontal (i.e. at the bottom).
  bool IsHorizontalAlignment() const;

  // Returns a value based on shelf alignment.
  int SelectValueForShelfAlignment(int bottom, int left, int right) const;

  // Returns |horizontal| if shelf is horizontal, otherwise |vertical|.
  int PrimaryAxisValue(int horizontal, int vertical) const;

  ShelfAutoHideBehavior auto_hide_behavior() const {
    return auto_hide_behavior_;
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

  int GetAccessibilityPanelHeight() const;
  void SetAccessibilityPanelHeight(int height);

  // Returns the height of the Docked Magnifier viewport.
  int GetDockedMagnifierHeight() const;

  // Returns the ideal bounds of the shelf assuming it is visible.
  gfx::Rect GetIdealBounds() const;

  gfx::Rect GetUserWorkAreaBounds() const;

  // Returns the screen bounds of the item for the specified window. If there is
  // no item for the specified window an empty rect is returned.
  gfx::Rect GetScreenBoundsOfItemIconForWindow(aura::Window* window);

  // Launch a 0-indexed shelf item in the shelf. A negative index launches the
  // last shelf item in the shelf.
  static void LaunchShelfItem(int item_index);

  // Activates the shelf item specified by the index in the list of shelf items.
  static void ActivateShelfItem(int item_index);

  // Activates the shelf item specified by the index in the list of shelf items
  // on the display identified by |display_id|.
  static void ActivateShelfItemOnDisplay(int item_index, int64_t display_id);

  // Handles a gesture |event| coming from a source outside the shelf widget
  // (e.g. the status area widget). Allows support for behaviors like toggling
  // auto-hide with a swipe, even if that gesture event hits another window.
  // Returns true if the event was handled.
  bool ProcessGestureEvent(const ui::GestureEvent& event);

  // Handles a mousewheel scroll event coming from the shelf.
  void ProcessMouseWheelEvent(const ui::MouseWheelEvent& event);

  void AddObserver(ShelfObserver* observer);
  void RemoveObserver(ShelfObserver* observer);

  void NotifyShelfIconPositionsChanged();
  StatusAreaWidget* GetStatusAreaWidget() const;

  // Get the tray button that the system tray bubble and the notification center
  // bubble will be anchored. See also: StatusAreaWidget::GetSystemTrayAnchor()
  TrayBackgroundView* GetSystemTrayAnchorView() const;

  // Get the anchor rect that the system tray bubble and the notification center
  // bubble will be anchored.
  // x() and y() designates anchor point, but width() and height() are dummy.
  // See also: BubbleDialogDelegateView::GetBubbleBounds()
  gfx::Rect GetSystemTrayAnchorRect() const;

  void set_is_tablet_mode_animation_running(bool value) {
    is_tablet_mode_animation_running_ = value;
  }
  bool is_tablet_mode_animation_running() const {
    return is_tablet_mode_animation_running_;
  }

  // Returns whether this shelf should be hidden on secondary display in a given
  // |state|.
  bool ShouldHideOnSecondaryDisplay(session_manager::SessionState state);

  void SetVirtualKeyboardBoundsForTesting(const gfx::Rect& bounds);
  ShelfLockingManager* GetShelfLockingManagerForTesting();
  ShelfView* GetShelfViewForTesting();

 protected:
  // ShelfLayoutManagerObserver:
  void WillDeleteShelfLayoutManager() override;
  void WillChangeVisibilityState(ShelfVisibilityState new_state) override;
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;
  void OnBackgroundUpdated(ShelfBackgroundType background_type,
                           AnimationChangeType change_type) override;

 private:
  class AutoHideEventHandler;
  friend class ShelfLayoutManagerTest;

  // Layout manager for the shelf container window. Instances are constructed by
  // ShelfWidget and lifetimes are managed by the container windows themselves.
  ShelfLayoutManager* shelf_layout_manager_ = nullptr;

  std::unique_ptr<ShelfWidget> shelf_widget_;

  // These initial values hide the shelf until user preferences are available.
  ShelfAlignment alignment_ = SHELF_ALIGNMENT_BOTTOM_LOCKED;
  ShelfAutoHideBehavior auto_hide_behavior_ = SHELF_AUTO_HIDE_ALWAYS_HIDDEN;

  // Sets shelf alignment to bottom during login and screen lock.
  ShelfLockingManager shelf_locking_manager_;

  base::ObserverList<ShelfObserver>::Unchecked observers_;

  // Forwards mouse and gesture events to ShelfLayoutManager for auto-hide.
  std::unique_ptr<AutoHideEventHandler> auto_hide_event_handler_;

  // Forwards touch gestures on a bezel sensor to the shelf.
  std::unique_ptr<ShelfBezelEventHandler> bezel_event_handler_;

  // True while the animation to enter or exit tablet mode is running. Sometimes
  // this value is true when the shelf movements are not actually animating
  // (animation value = 0.0). This is because this is set to true when we
  // enter/exit tablet mode but the animation is not started until a shelf
  // OnBoundsChanged is called because of tablet mode. Use this value to sync
  // the animation for AppListButton.
  bool is_tablet_mode_animation_running_ = false;

  DISALLOW_COPY_AND_ASSIGN(Shelf);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_H_
