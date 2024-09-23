// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROOT_WINDOW_CONTROLLER_H_
#define ASH_ROOT_WINDOW_CONTROLLER_H_

#include <memory>
#include <optional>
#include <vector>

#include "ash/ash_export.h"
#include "ash/style/ash_color_provider_source.h"
#include "ash/wm/overview/overview_metrics.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/wm_metrics.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_tree_host.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Point;
}  // namespace gfx

namespace ui {
class SimpleMenuModel;
}  // namespace ui

namespace views {
class MenuRunner;
class Widget;
}  // namespace views

namespace wm {
class ScopedCaptureClient;
}

namespace ash {

class AccessibilityPanelLayoutManager;
class AlwaysOnTopController;
class AppMenuModelAdapter;
class AshWindowTreeHost;
class LockScreenActionBackgroundController;
class RootWindowLayoutManager;
class ScreenRotationAnimator;
class Shelf;
class ShelfLayoutManager;
class SplitViewController;
class SplitViewOverviewSession;
class StatusAreaWidget;
class SystemModalContainerLayoutManager;
class TouchExplorationManager;
class TouchHudDebug;
class TouchHudProjection;
class WallpaperWidgetController;
class WindowParentingController;
class WorkAreaInsets;
enum class LoginStatus;
enum class SplitViewOverviewSessionExitPoint;

namespace curtain {
class SecurityCurtainWidgetController;
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

  RootWindowController(const RootWindowController&) = delete;
  RootWindowController& operator=(const RootWindowController&) = delete;

  ~RootWindowController();

  // Creates and Initialize the RootWindowController for primary display.
  // Returns a pointer to the newly created controller.
  static RootWindowController* CreateForPrimaryDisplay(AshWindowTreeHost* host);

  // Creates and Initialize the RootWindowController for secondary displays.
  // Returns a pointer to the newly created controller.
  static RootWindowController* CreateForSecondaryDisplay(
      AshWindowTreeHost* host);

  // Returns a RootWindowController of the window's root window.
  static RootWindowController* ForWindow(const aura::Window* window);

  // Returns the RootWindowController of the target root window.
  static RootWindowController* ForTargetRootWindow();

  static std::vector<RootWindowController*> root_window_controllers() {
    return root_window_controllers_ ? *root_window_controllers_
                                    : std::vector<RootWindowController*>();
  }

  AshWindowTreeHost* ash_host() { return ash_host_.get(); }
  const AshWindowTreeHost* ash_host() const { return ash_host_.get(); }

  aura::WindowTreeHost* GetHost();
  const aura::WindowTreeHost* GetHost() const;
  aura::Window* GetRootWindow();
  const aura::Window* GetRootWindow() const;

  SplitViewController* split_view_controller() {
    return split_view_controller_.get();
  }
  SplitViewOverviewSession* split_view_overview_session() {
    return split_view_overview_session_.get();
  }

  Shelf* shelf() const { return shelf_.get(); }

  TouchHudDebug* touch_hud_debug() const { return touch_hud_debug_; }
  TouchHudProjection* touch_hud_projection() const {
    return touch_hud_projection_;
  }

  // Set touch HUDs for this root window controller. The root window controller
  // will not own the HUDs; their lifetimes are managed by themselves. Whenever
  // the widget showing a HUD is being destroyed (e.g. because of detaching a
  // display), the HUD deletes itself.
  void set_touch_hud_debug(TouchHudDebug* hud) { touch_hud_debug_ = hud; }
  void set_touch_hud_projection(TouchHudProjection* hud) {
    touch_hud_projection_ = hud;
  }

  RootWindowLayoutManager* root_window_layout_manager() {
    return root_window_layout_manager_;
  }

  bool is_shutting_down() const { return is_shutting_down_; }

  // Returns parameters of the work area associated with this root window.
  WorkAreaInsets* work_area_insets() { return work_area_insets_.get(); }

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

  AshColorProviderSource* color_provider_source() {
    return color_provider_source_.get();
  }

  // Returns the rotation animator associated with the root window of this
  // controller. It creates it lazily if it had never been created, unless
  // `Shutdown()` had been called, and in this case, it returns nullptr.
  ScreenRotationAnimator* GetScreenRotationAnimator();

  // Deletes associated objects and clears the state, but doesn't delete
  // the root window yet. This is used to delete a secondary displays'
  // root window safely when the display disconnect signal is received,
  // which may come while we're in the nested run loop. Child windows of the
  // root window of this controller will be moved to `destination_root` if
  // provided.
  void Shutdown(aura::Window* destination_root);

  // Deletes all child windows and performs necessary cleanup.
  void CloseChildWindows();

  // Initialize touch HUDs if necessary.
  void InitTouchHuds();

  // Returns the topmost window or one of its transient parents, if any of them
  // are in fullscreen mode.
  // TODO(afakhry): Rename this to imply getting the fullscreen window on the
  // currently active desk on this root.
  aura::Window* GetWindowForFullscreenMode();

  // Returns true if window is fulllscreen and the shelf is hidden.
  bool IsInFullscreenMode();

  // If touch exploration is enabled, update the touch exploration
  // controller so that synthesized touch events are anchored at this point.
  void SetTouchAccessibilityAnchorPoint(const gfx::Point& anchor_point);

  // Shows a context menu at the |location_in_screen|.
  void ShowContextMenu(const gfx::Point& location_in_screen,
                       ui::MenuSourceType source_type);

  // Called when the login status changes after login (such as lock/unlock).
  void UpdateAfterLoginStatusChange(LoginStatus status);

  void CreateAmbientWidget();
  void CloseAmbientWidget(bool immediately);
  bool HasAmbientWidget() const;

  views::Widget* ambient_widget_for_testing() { return ambient_widget_.get(); }
  AppMenuModelAdapter* menu_model_adapter_for_testing() {
    return root_window_menu_model_adapter_.get();
  }

  // Returns accessibility panel layout manager for this root window.
  AccessibilityPanelLayoutManager* GetAccessibilityPanelLayoutManagerForTest();

  void SetSecurityCurtainWidgetController(
      std::unique_ptr<curtain::SecurityCurtainWidgetController> controller);
  void ClearSecurityCurtainWidgetController();
  curtain::SecurityCurtainWidgetController*
  security_curtain_widget_controller();

  // Starts a split view overview session for this root window with `window`
  // snapped on one side and overview on the other side.
  void StartSplitViewOverviewSession(aura::Window* window,
                                     std::optional<OverviewStartAction> action,
                                     std::optional<OverviewEnterExitType> type,
                                     WindowSnapActionSource snap_action_source);

  // Ends the split view overview session and reports the uma metrics if it is
  // active.
  void EndSplitViewOverviewSession(
      SplitViewOverviewSessionExitPoint exit_point);

  void SetScreenRotationAnimatorForTest(
      std::unique_ptr<ScreenRotationAnimator> animator);

  bool IsContextMenuShownForTest() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(RootWindowControllerTest,
                           ContextMenuDisappearsInTabletMode);

  // Takes ownership of |ash_host|.
  explicit RootWindowController(AshWindowTreeHost* ash_host);

  // Moves child windows to `dest`.
  void MoveWindowsTo(aura::Window* dest);

  // Initializes the RootWindowController based on |root_window_type|.
  void Init(RootWindowType root_window_type);

  void InitLayoutManagers(
      std::unique_ptr<RootWindowLayoutManager> root_window_layout_manager);

  AccessibilityPanelLayoutManager* GetAccessibilityPanelLayoutManager() const;

  // Creates the containers (aura::Windows) used by the shell.
  void CreateContainers();

  // Creates a new window for use as a container.
  aura::Window* CreateContainer(int window_id,
                                const char* name,
                                aura::Window* parent);

  // Build a menu model adapter to configure birch bar in Overview.
  std::unique_ptr<AppMenuModelAdapter> BuildBirchMenuModelAdapter(
      ui::MenuSourceType source_type);

  // Build a menu model adapter to configure shelf.
  std::unique_ptr<AppMenuModelAdapter> BuildShelfMenuModelAdapter(
      ui::MenuSourceType source_type);

  // Callback for MenuRunner.
  void OnMenuClosed();

  // Passed as callback to |wallpaper_widget_controller_| - run when the
  // wallpaper widget is first set.
  void OnFirstWallpaperWidgetSet();

  std::unique_ptr<AshWindowTreeHost> ash_host_;
  // |ash_host_| as a WindowTreeHost.
  raw_ptr<aura::WindowTreeHost, DanglingUntriaged> window_tree_host_;

  // LayoutManagers are owned by the window they are installed on.
  raw_ptr<RootWindowLayoutManager, DanglingUntriaged>
      root_window_layout_manager_ = nullptr;

  std::unique_ptr<WallpaperWidgetController> wallpaper_widget_controller_;

  std::unique_ptr<AlwaysOnTopController> always_on_top_controller_;

  // Manages the context menu.
  std::unique_ptr<AppMenuModelAdapter> root_window_menu_model_adapter_;
  std::unique_ptr<ui::SimpleMenuModel> sort_apps_submenu_;

  std::unique_ptr<WindowParentingController> window_parenting_controller_;

  // The rotation animator of the root window controlled by `this`. It's created
  // lazily when `GetScreenRotationAnimator()` is called, unless it was called
  // after `Shutdown()` had begun.
  std::unique_ptr<ScreenRotationAnimator> screen_rotation_animator_;

  std::unique_ptr<SplitViewController> split_view_controller_;
  std::unique_ptr<SplitViewOverviewSession> split_view_overview_session_;

  // The shelf controller for this root window. Exists for the entire lifetime
  // of the RootWindowController so that it is safe for observers to be added
  // to it during construction of the shelf widget and status tray.
  std::unique_ptr<Shelf> shelf_;

  // Responsible for initializing TouchExplorationController when spoken
  // feedback is on.
  std::unique_ptr<TouchExplorationManager> touch_exploration_manager_;

  // Heads-up displays for touch events. These HUDs are not owned by the root
  // window controller and manage their own lifetimes.
  raw_ptr<TouchHudDebug, DanglingUntriaged> touch_hud_debug_ = nullptr;
  raw_ptr<TouchHudProjection, DanglingUntriaged> touch_hud_projection_ =
      nullptr;

  std::unique_ptr<::wm::ScopedCaptureClient> capture_client_;

  std::unique_ptr<LockScreenActionBackgroundController>
      lock_screen_action_background_controller_;

  std::unique_ptr<views::Widget> ambient_widget_;

  std::unique_ptr<curtain::SecurityCurtainWidgetController>
      security_curtain_widget_controller_;

  std::unique_ptr<AshColorProviderSource> color_provider_source_;

  // True if we are in the process of shutting down this controller.
  bool is_shutting_down_ = false;

  // Whether child windows have been closed during shutdown. Exists to avoid
  // calling related cleanup code more than once.
  bool did_close_child_windows_ = false;

  std::unique_ptr<WorkAreaInsets> work_area_insets_;

  static std::vector<RootWindowController*>* root_window_controllers_;
};

}  // namespace ash

#endif  // ASH_ROOT_WINDOW_CONTROLLER_H_
