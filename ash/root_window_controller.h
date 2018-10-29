// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROOT_WINDOW_CONTROLLER_H_
#define ASH_ROOT_WINDOW_CONTROLLER_H_

#include <map>
#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/wm/workspace/workspace_types.h"
#include "base/macros.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
}

namespace ui {
class SimpleMenuModel;
class WindowTreeHost;
}

namespace views {
class MenuRunner;
}

namespace wm {
class ScopedCaptureClient;
}

namespace ash {
class AlwaysOnTopController;
class AshWindowTreeHost;
class LockScreenActionBackgroundController;
enum class LoginStatus;
class Shelf;
class ShelfLayoutManager;
class StackingController;
class StatusAreaWidget;
class SystemModalContainerLayoutManager;
class SystemTray;
class SystemWallpaperController;
class TouchExplorationManager;
class TouchObserverHUD;
class WallpaperWidgetController;
class WindowManager;
class WorkspaceController;

namespace wm {
class RootWindowLayoutManager;
}

// This class maintains the per root window state for ash. This class
// owns the root window and other dependent objects that should be
// deleted upon the deletion of the root window. This object is
// indirectly owned and deleted by |WindowTreeHostManager|.
// The RootWindowController for particular root window is stored in
// its property (RootWindowSettings) and can be obtained using
// |RootWindowController::ForWindow(aura::Window*)| function.
class ASH_EXPORT RootWindowController {
 public:
  // Enumerates the type of display. If there is only a single display then
  // it is primary. In a multi-display environment one monitor is deemed the
  // PRIMARY and all others SECONDARY.
  enum class RootWindowType { PRIMARY, SECONDARY };

  ~RootWindowController();

  // Creates and Initialize the RootWindowController for primary display.
  static void CreateForPrimaryDisplay(AshWindowTreeHost* host);

  // Creates and Initialize the RootWindowController for secondary displays.
  static void CreateForSecondaryDisplay(AshWindowTreeHost* host);

  // Returns a RootWindowController of the window's root window.
  static RootWindowController* ForWindow(const aura::Window* window);

  // Returns the RootWindowController of the target root window.
  static RootWindowController* ForTargetRootWindow();

  static std::vector<RootWindowController*> root_window_controllers() {
    return root_window_controllers_ ? *root_window_controllers_
                                    : std::vector<RootWindowController*>();
  }

  // TODO(sky): move these to a separate class or use AshWindowTreeHost in
  // mash. http://crbug.com/671246.
  AshWindowTreeHost* ash_host() { return ash_host_.get(); }
  const AshWindowTreeHost* ash_host() const { return ash_host_.get(); }

  aura::WindowTreeHost* GetHost();
  const aura::WindowTreeHost* GetHost() const;
  aura::Window* GetRootWindow();
  const aura::Window* GetRootWindow() const;

  WorkspaceController* workspace_controller() {
    return workspace_controller_.get();
  }

  wm::WorkspaceWindowState GetWorkspaceWindowState();

  Shelf* shelf() const { return shelf_.get(); }

  TouchObserverHUD* touch_observer_hud() const { return touch_observer_hud_; }
  void set_touch_observer_hud(TouchObserverHUD* hud) {
    touch_observer_hud_ = hud;
  }

  wm::RootWindowLayoutManager* root_window_layout_manager() {
    return root_window_layout_manager_;
  }

  // Access the shelf layout manager associated with this root
  // window controller, NULL if no such shelf exists.
  ShelfLayoutManager* GetShelfLayoutManager();

  // Returns the layout manager for the appropriate modal-container. If the
  // window is inside the lockscreen modal container, then the layout manager
  // for that is returned. Otherwise the layout manager for the default modal
  // container is returned.
  // If no window is specified (i.e. |window| is null), then the lockscreen
  // modal container is used if the screen is currently locked. Otherwise, the
  // default modal container is used.
  SystemModalContainerLayoutManager* GetSystemModalLayoutManager(
      aura::Window* window);

  AlwaysOnTopController* always_on_top_controller() {
    return always_on_top_controller_.get();
  }

  // May return null, for example for a secondary monitor at the login screen.
  StatusAreaWidget* GetStatusAreaWidget();

  // Returns the system tray on this root window. Note that
  // calling this on the root window that doesn't have a shelf will
  // lead to a crash.
  SystemTray* GetSystemTray();

  // Returns if system tray and its widget is visible.
  bool IsSystemTrayVisible();

  // True if the window can receive events on this root window.
  bool CanWindowReceiveEvents(aura::Window* window);

  // Returns the window events will be targeted at for the specified location
  // (in screen coordinates).
  //
  // NOTE: the returned window may not contain the location as resize handles
  // may extend outside the bounds of the window.
  aura::Window* FindEventTarget(const gfx::Point& location_in_screen);

  // Gets the last location seen in a mouse event in this root window's
  // coordinates. This may return a point outside the root window's bounds.
  gfx::Point GetLastMouseLocationInRoot();

  aura::Window* GetContainer(int container_id);
  const aura::Window* GetContainer(int container_id) const;

  WallpaperWidgetController* wallpaper_widget_controller() {
    return wallpaper_widget_controller_.get();
  }

  LockScreenActionBackgroundController*
  lock_screen_action_background_controller() {
    return lock_screen_action_background_controller_.get();
  }

  // Deletes associated objects and clears the state, but doesn't delete
  // the root window yet. This is used to delete a secondary displays'
  // root window safely when the display disconnect signal is received,
  // which may come while we're in the nested run loop.
  void Shutdown();

  // Deletes all child windows and performs necessary cleanup.
  void CloseChildWindows();

  // Moves child windows to |dest|.
  void MoveWindowsTo(aura::Window* dest);

  // Force the shelf to query for it's current visibility state.
  void UpdateShelfVisibility();

  // Initialize touch HUDs if necessary.
  void InitTouchHuds();

  // Returns the topmost window or one of its transient parents, if any of them
  // are in fullscreen mode.
  aura::Window* GetWindowForFullscreenMode();

  // If touch exploration is enabled, update the touch exploration
  // controller so that synthesized touch events are anchored at this point.
  void SetTouchAccessibilityAnchorPoint(const gfx::Point& anchor_point);

  // Shows a context menu at the |location_in_screen|.
  void ShowContextMenu(const gfx::Point& location_in_screen,
                       ui::MenuSourceType source_type);
  void HideContextMenu();
  bool IsContextMenuShown() const;

  // Called when the login status changes after login (such as lock/unlock).
  void UpdateAfterLoginStatusChange(LoginStatus status);

 private:
  FRIEND_TEST_ALL_PREFIXES(RootWindowControllerTest,
                           ContextMenuDisappearsInTabletMode);

  // TODO(sky): remove this. Temporary during ash-mus unification.
  // http://crbug.com/671246.
  friend class WindowManager;

  // Creates a new RootWindowController with the specified host. Only one of
  // |ash_host| or |window_tree_host| should be specified. This takes ownership
  // of the supplied arguments.
  // TODO(sky): mash should create AshWindowTreeHost, http://crbug.com/671246.
  RootWindowController(AshWindowTreeHost* ash_host,
                       aura::WindowTreeHost* window_tree_host);

  // Initializes the RootWindowController based on |root_window_type|.
  void Init(RootWindowType root_window_type);

  void InitLayoutManagers();

  // Initializes the shelf for this root window and notifies observers.
  void InitializeShelf();

  // Creates the containers (aura::Windows) used by the shell.
  void CreateContainers();

  // Initializes |system_wallpaper_| and possibly also |boot_splash_screen_|.
  // The initial color is determined by the |root_window_type| and whether or
  // not this is the first boot.
  void CreateSystemWallpaper(RootWindowType root_window_type);

  // Resets Shell::GetRootWindowForNewWindows() if appropriate. This is called
  // during shutdown to make sure GetRootWindowForNewWindows() isn't referencing
  // this.
  void ResetRootForNewWindowsIfNecessary();

  // Callback for MenuRunner.
  void OnMenuClosed(const base::TimeTicks desktop_context_menu_show_time);

  // Passed as callback to |wallpaper_widget_controller_| - run when the
  // wallpaper widget is first set.
  void OnFirstWallpaperWidgetSet();

  std::unique_ptr<AshWindowTreeHost> ash_host_;
  std::unique_ptr<aura::WindowTreeHost> mus_window_tree_host_;
  // This comes from |ash_host_| or |mus_window_tree_host_|.
  aura::WindowTreeHost* window_tree_host_;

  // LayoutManagers are owned by the window they are installed on.
  wm::RootWindowLayoutManager* root_window_layout_manager_ = nullptr;

  std::unique_ptr<WallpaperWidgetController> wallpaper_widget_controller_;
  std::unique_ptr<WorkspaceController> workspace_controller_;

  std::unique_ptr<AlwaysOnTopController> always_on_top_controller_;

  // Manages the context menu.
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  std::unique_ptr<StackingController> stacking_controller_;

  // The shelf controller for this root window. Exists for the entire lifetime
  // of the RootWindowController so that it is safe for observers to be added
  // to it during construction of the shelf widget and status tray.
  std::unique_ptr<Shelf> shelf_;

  // TODO(jamescook): Eliminate this. It is left over from legacy shelf code and
  // doesn't mean anything in particular.
  bool shelf_initialized_ = false;

  std::unique_ptr<SystemWallpaperController> system_wallpaper_;

  // Responsible for initializing TouchExplorationController when spoken
  // feedback is on.
  std::unique_ptr<TouchExplorationManager> touch_exploration_manager_;

  // Heads-up displays for touch events for this root. Not owned. Manages its
  // own lifetime. Whenever the widget showing a HUD is being destroyed (e.g.
  // because of detaching a display), the HUD deletes itself.
  TouchObserverHUD* touch_observer_hud_ = nullptr;

  std::unique_ptr<::wm::ScopedCaptureClient> capture_client_;

  std::unique_ptr<LockScreenActionBackgroundController>
      lock_screen_action_background_controller_;

  // Whether child windows have been closed during shutdown. Exists to avoid
  // calling related cleanup code more than once.
  bool did_close_child_windows_ = false;

  static std::vector<RootWindowController*>* root_window_controllers_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowController);
};

}  // namespace ash

#endif  // ASH_ROOT_WINDOW_CONTROLLER_H_
