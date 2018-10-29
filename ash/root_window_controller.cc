// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"

#include <algorithm>
#include <memory>
#include <queue>
#include <vector>

#include "ash/accessibility/accessibility_panel_layout_manager.h"
#include "ash/accessibility/touch_exploration_controller.h"
#include "ash/accessibility/touch_exploration_manager.h"
#include "ash/focus_cycler.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/keyboard/arc/arc_virtual_keyboard_container_layout_manager.h"
#include "ash/keyboard/ash_keyboard_controller.h"
#include "ash/keyboard/virtual_keyboard_container_layout_manager.h"
#include "ash/lock_screen_action/lock_screen_action_background_controller.h"
#include "ash/login_status.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_settings.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_context_menu_model.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shelf/shelf_window_targeter.h"
#include "ash/shell.h"
#include "ash/shell_state.h"
#include "ash/system/status_area_layout_manager.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/touch/touch_observer_hud.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/window_factory.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/fullscreen_window_finder.h"
#include "ash/wm/lock_action_handler_layout_manager.h"
#include "ash/wm/lock_layout_manager.h"
#include "ash/wm/overlay_layout_manager.h"
#include "ash/wm/root_window_layout_manager.h"
#include "ash/wm/stacking_controller.h"
#include "ash/wm/switchable_windows.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/system_wallpaper_controller.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chromeos/chromeos_switches.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event_utils.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_layout_manager.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_model.h"
#include "ui/views/view_model_utils.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_properties.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/tooltip_client.h"

namespace ash {
namespace {

bool IsWindowAboveContainer(aura::Window* window,
                            aura::Window* blocking_container) {
  std::vector<aura::Window*> target_path;
  std::vector<aura::Window*> blocking_path;

  while (window) {
    target_path.push_back(window);
    window = window->parent();
  }

  while (blocking_container) {
    blocking_path.push_back(blocking_container);
    blocking_container = blocking_container->parent();
  }

  // The root window is put at the end so that we compare windows at
  // the same depth.
  while (!blocking_path.empty()) {
    if (target_path.empty())
      return false;

    aura::Window* target = target_path.back();
    target_path.pop_back();
    aura::Window* blocking = blocking_path.back();
    blocking_path.pop_back();

    // Still on the same path, continue.
    if (target == blocking)
      continue;

    // This can happen only if unparented window is passed because
    // first element must be the same root.
    if (!target->parent() || !blocking->parent())
      return false;

    aura::Window* common_parent = target->parent();
    DCHECK_EQ(common_parent, blocking->parent());
    const aura::Window::Windows& windows = common_parent->children();
    auto blocking_iter = std::find(windows.begin(), windows.end(), blocking);
    // If the target window is above blocking window, the window can handle
    // events.
    return std::find(blocking_iter, windows.end(), target) != windows.end();
  }

  return true;
}

// Scales |value| that is originally between 0 and |src_max| to be between
// 0 and |dst_max|.
float ToRelativeValue(int value, int src_max, int dst_max) {
  return static_cast<float>(value) / static_cast<float>(src_max) * dst_max;
}

// Uses ToRelativeValue() to scale the origin of |bounds_in_out|. The
// width/height are not changed.
void MoveOriginRelativeToSize(const gfx::Size& src_size,
                              const gfx::Size& dst_size,
                              gfx::Rect* bounds_in_out) {
  gfx::Point origin = bounds_in_out->origin();
  bounds_in_out->set_origin(gfx::Point(
      ToRelativeValue(origin.x(), src_size.width(), dst_size.width()),
      ToRelativeValue(origin.y(), src_size.height(), dst_size.height())));
}

// Reparents |window| to |new_parent|.
void ReparentWindow(aura::Window* window, aura::Window* new_parent) {
  const gfx::Size src_size = window->parent()->bounds().size();
  const gfx::Size dst_size = new_parent->bounds().size();
  // Update the restore bounds to make it relative to the display.
  wm::WindowState* state = wm::GetWindowState(window);
  gfx::Rect restore_bounds;
  const bool has_restore_bounds = state->HasRestoreBounds();

  const bool update_bounds = state->IsNormalOrSnapped() || state->IsMinimized();
  gfx::Rect work_area_in_new_parent =
      screen_util::GetDisplayWorkAreaBoundsInParent(new_parent);

  gfx::Rect local_bounds;
  if (update_bounds) {
    local_bounds = state->window()->bounds();
    MoveOriginRelativeToSize(src_size, dst_size, &local_bounds);
    local_bounds.AdjustToFit(work_area_in_new_parent);
  }

  if (has_restore_bounds) {
    restore_bounds = state->GetRestoreBoundsInParent();
    MoveOriginRelativeToSize(src_size, dst_size, &restore_bounds);
    restore_bounds.AdjustToFit(work_area_in_new_parent);
  }

  new_parent->AddChild(window);

  // Docked windows have bounds handled by the layout manager in AddChild().
  if (update_bounds)
    window->SetBounds(local_bounds);

  if (has_restore_bounds)
    state->SetRestoreBoundsInParent(restore_bounds);
}

// Reparents the appropriate set of windows from |src| to |dst|.
void ReparentAllWindows(aura::Window* src, aura::Window* dst) {
  // Set of windows to move.
  const int kContainerIdsToMove[] = {
      kShellWindowId_DefaultContainer,
      kShellWindowId_AlwaysOnTopContainer,
      kShellWindowId_SystemModalContainer,
      kShellWindowId_LockSystemModalContainer,
      kShellWindowId_UnparentedControlContainer,
      kShellWindowId_OverlayContainer,
      kShellWindowId_LockActionHandlerContainer,
  };
  const int kExtraContainerIdsToMoveInUnifiedMode[] = {
      kShellWindowId_LockScreenContainer,
  };
  std::vector<int> container_ids(
      kContainerIdsToMove,
      kContainerIdsToMove + arraysize(kContainerIdsToMove));
  // Check the display mode as this is also necessary when trasitioning between
  // mirror and unified mode.
  if (Shell::Get()->display_manager()->current_default_multi_display_mode() ==
      display::DisplayManager::UNIFIED) {
    for (int id : kExtraContainerIdsToMoveInUnifiedMode)
      container_ids.push_back(id);
  }

  for (int id : container_ids) {
    aura::Window* src_container = src->GetChildById(id);
    aura::Window* dst_container = dst->GetChildById(id);
    while (!src_container->children().empty()) {
      // Restart iteration from the source container windows each time as they
      // may change as a result of moving other windows.
      const aura::Window::Windows& src_container_children =
          src_container->children();
      auto iter = src_container_children.begin();
      while (iter != src_container_children.end() &&
             SystemModalContainerLayoutManager::IsModalBackground(*iter)) {
        ++iter;
      }
      // If the entire window list is modal background windows then stop.
      if (iter == src_container_children.end())
        break;
      ReparentWindow(*iter, dst_container);
    }
  }
}

// Creates a new window for use as a container.
aura::Window* CreateContainer(int window_id,
                              const char* name,
                              aura::Window* parent) {
  aura::Window* window =
      window_factory::NewWindow(nullptr, aura::client::WINDOW_TYPE_UNKNOWN)
          .release();
  window->Init(ui::LAYER_NOT_DRAWN);
  window->set_id(window_id);
  window->SetName(name);
  parent->AddChild(window);
  if (window_id != kShellWindowId_UnparentedControlContainer)
    window->Show();
  return window;
}

bool ShouldDestroyWindowInCloseChildWindows(aura::Window* window) {
  return window->owned_by_parent();
}

}  // namespace

// static
std::vector<RootWindowController*>*
    RootWindowController::root_window_controllers_ = nullptr;

RootWindowController::~RootWindowController() {
  Shutdown();
  DCHECK(!wallpaper_widget_controller_.get());
  ash_host_.reset();
  mus_window_tree_host_.reset();
  // The CaptureClient needs to be around for as long as the RootWindow is
  // valid.
  capture_client_.reset();
  root_window_controllers_->erase(std::find(root_window_controllers_->begin(),
                                            root_window_controllers_->end(),
                                            this));
}

void RootWindowController::CreateForPrimaryDisplay(AshWindowTreeHost* host) {
  RootWindowController* controller = new RootWindowController(host, nullptr);
  controller->Init(RootWindowType::PRIMARY);
}

void RootWindowController::CreateForSecondaryDisplay(AshWindowTreeHost* host) {
  RootWindowController* controller = new RootWindowController(host, nullptr);
  controller->Init(RootWindowType::SECONDARY);
}

// static
RootWindowController* RootWindowController::ForWindow(
    const aura::Window* window) {
  DCHECK(window);
  CHECK(Shell::HasInstance());
  return GetRootWindowSettings(window->GetRootWindow())->controller;
}

// static
RootWindowController* RootWindowController::ForTargetRootWindow() {
  CHECK(Shell::HasInstance());
  return ForWindow(Shell::GetRootWindowForNewWindows());
}

aura::WindowTreeHost* RootWindowController::GetHost() {
  return window_tree_host_;
}

const aura::WindowTreeHost* RootWindowController::GetHost() const {
  return window_tree_host_;
}

aura::Window* RootWindowController::GetRootWindow() {
  return GetHost()->window();
}

const aura::Window* RootWindowController::GetRootWindow() const {
  return GetHost()->window();
}

wm::WorkspaceWindowState RootWindowController::GetWorkspaceWindowState() {
  return workspace_controller_ ? workspace_controller()->GetWindowState()
                               : wm::WORKSPACE_WINDOW_STATE_DEFAULT;
}

void RootWindowController::InitializeShelf() {
  if (shelf_initialized_)
    return;
  shelf_initialized_ = true;

  shelf_->shelf_widget()->PostCreateShelf();
}

ShelfLayoutManager* RootWindowController::GetShelfLayoutManager() {
  return shelf_->shelf_layout_manager();
}

SystemModalContainerLayoutManager*
RootWindowController::GetSystemModalLayoutManager(aura::Window* window) {
  aura::Window* modal_container = nullptr;
  if (window) {
    aura::Window* window_container = wm::GetContainerForWindow(window);
    if (window_container &&
        window_container->id() >= kShellWindowId_LockScreenContainer) {
      modal_container = GetContainer(kShellWindowId_LockSystemModalContainer);
    } else {
      modal_container = GetContainer(kShellWindowId_SystemModalContainer);
    }
  } else {
    int modal_window_id =
        Shell::Get()->session_controller()->IsUserSessionBlocked()
            ? kShellWindowId_LockSystemModalContainer
            : kShellWindowId_SystemModalContainer;
    modal_container = GetContainer(modal_window_id);
  }
  return modal_container ? static_cast<SystemModalContainerLayoutManager*>(
                               modal_container->layout_manager())
                         : nullptr;
}

StatusAreaWidget* RootWindowController::GetStatusAreaWidget() {
  ShelfWidget* shelf_widget = shelf_->shelf_widget();
  return shelf_widget ? shelf_widget->status_area_widget() : nullptr;
}

SystemTray* RootWindowController::GetSystemTray() {
  // We assume in throughout the code that this will not return NULL. If code
  // triggers this for valid reasons, it should test status_area_widget first.
  CHECK(shelf_->shelf_widget()->status_area_widget());
  return shelf_->shelf_widget()->status_area_widget()->system_tray();
}

bool RootWindowController::IsSystemTrayVisible() {
  TrayBackgroundView* tray = GetStatusAreaWidget()->unified_system_tray();
  return tray && tray->GetWidget()->IsVisible() && tray->visible();
}

bool RootWindowController::CanWindowReceiveEvents(aura::Window* window) {
  if (GetRootWindow() != window->GetRootWindow())
    return false;

  aura::Window* blocking_container = nullptr;
  aura::Window* modal_container = nullptr;
  wm::GetBlockingContainersForRoot(GetRootWindow(), &blocking_container,
                                   &modal_container);
  SystemModalContainerLayoutManager* modal_layout_manager = nullptr;
  modal_layout_manager = static_cast<SystemModalContainerLayoutManager*>(
      modal_container->layout_manager());

  if (modal_layout_manager->has_window_dimmer())
    blocking_container = modal_container;
  else
    modal_container = nullptr;  // Don't check modal dialogs.

  // In normal session.
  if (!blocking_container)
    return true;

  if (!IsWindowAboveContainer(window, blocking_container))
    return false;

  // If the window is in the target modal container, only allow the top most
  // one.
  if (modal_container && modal_container->Contains(window))
    return modal_layout_manager->IsPartOfActiveModalWindow(window);

  return true;
}

aura::Window* RootWindowController::FindEventTarget(
    const gfx::Point& location_in_screen) {
  gfx::Point location_in_root(location_in_screen);
  aura::Window* root_window = GetRootWindow();
  ::wm::ConvertPointFromScreen(root_window, &location_in_root);
  ui::MouseEvent test_event(ui::ET_MOUSE_MOVED, location_in_root,
                            location_in_root, ui::EventTimeForNow(),
                            ui::EF_NONE, ui::EF_NONE);
  ui::EventTarget* event_handler =
      root_window->GetHost()
          ->dispatcher()
          ->GetDefaultEventTargeter()
          ->FindTargetForEvent(root_window, &test_event);
  return static_cast<aura::Window*>(event_handler);
}

gfx::Point RootWindowController::GetLastMouseLocationInRoot() {
  return window_tree_host_->dispatcher()->GetLastMouseLocationInRoot();
}

aura::Window* RootWindowController::GetContainer(int container_id) {
  return GetRootWindow()->GetChildById(container_id);
}

const aura::Window* RootWindowController::GetContainer(int container_id) const {
  return window_tree_host_->window()->GetChildById(container_id);
}

void RootWindowController::Shutdown() {
  touch_exploration_manager_.reset();

  ResetRootForNewWindowsIfNecessary();

  wallpaper_widget_controller_.reset();

  CloseChildWindows();
  aura::Window* root_window = GetRootWindow();
  GetRootWindowSettings(root_window)->controller = nullptr;
  // Forget with the display ID so that display lookup
  // ends up with invalid display.
  GetRootWindowSettings(root_window)->display_id = display::kInvalidDisplayId;
  if (ash_host_)
    ash_host_->PrepareForShutdown();

  system_wallpaper_.reset();
  lock_screen_action_background_controller_.reset();
  aura::client::SetScreenPositionClient(root_window, nullptr);
}

void RootWindowController::CloseChildWindows() {
  // Child windows can be closed by secondary monitor disconnection, Shell
  // shutdown, or both. Avoid running the related cleanup code twice.
  if (did_close_child_windows_)
    return;
  did_close_child_windows_ = true;

  // Deactivate keyboard container before closing child windows and shutting
  // down associated layout managers.
  auto* ash_keyboard_controller = Shell::Get()->ash_keyboard_controller();
  if (ash_keyboard_controller->keyboard_controller()->GetRootWindow() ==
      GetRootWindow()) {
    ash_keyboard_controller->DeactivateKeyboard();
  }

  shelf_->ShutdownShelfWidget();

  workspace_controller_.reset();

  // Explicitly destroy top level windows. We do this because such windows may
  // query the RootWindow for state.
  aura::WindowTracker non_toplevel_windows;
  aura::Window* root = GetRootWindow();
  non_toplevel_windows.Add(root);
  while (!non_toplevel_windows.windows().empty()) {
    aura::Window* non_toplevel_window = non_toplevel_windows.Pop();
    aura::WindowTracker toplevel_windows;
    for (aura::Window* child : non_toplevel_window->children()) {
      if (!ShouldDestroyWindowInCloseChildWindows(child))
        continue;
      if (child->delegate())
        toplevel_windows.Add(child);
      else
        non_toplevel_windows.Add(child);
    }
    while (!toplevel_windows.windows().empty())
      delete toplevel_windows.Pop();
  }

  // And then remove the containers.
  while (!root->children().empty()) {
    aura::Window* child = root->children()[0];
    if (ShouldDestroyWindowInCloseChildWindows(child))
      delete child;
    else
      root->RemoveChild(child);
  }

  // Removing the containers destroys ShelfLayoutManager. ShelfWidget outlives
  // ShelfLayoutManager because ShelfLayoutManager holds a pointer to it.
  shelf_->DestroyShelfWidget();

  ::wm::SetTooltipClient(GetRootWindow(), nullptr);
}

void RootWindowController::MoveWindowsTo(aura::Window* dst) {
  // Clear the workspace controller, so it doesn't incorrectly update the shelf.
  workspace_controller_.reset();
  ReparentAllWindows(GetRootWindow(), dst);
}

void RootWindowController::UpdateShelfVisibility() {
  shelf_->UpdateVisibilityState();
}

void RootWindowController::InitTouchHuds() {
  // Enable touch debugging features when each display is initialized.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAshTouchHud))
    set_touch_observer_hud(new TouchObserverHUD(GetRootWindow()));
}

aura::Window* RootWindowController::GetWindowForFullscreenMode() {
  return wm::GetWindowForFullscreenMode(GetRootWindow());
}

void RootWindowController::SetTouchAccessibilityAnchorPoint(
    const gfx::Point& anchor_point) {
  if (touch_exploration_manager_)
    touch_exploration_manager_->SetTouchAccessibilityAnchorPoint(anchor_point);
}

void RootWindowController::ShowContextMenu(const gfx::Point& location_in_screen,
                                           ui::MenuSourceType source_type) {
  // The wallpaper widget may not be set yet if the user clicked on the
  // status area before the initial animation completion. See crbug.com/222218
  if (!wallpaper_widget_controller()->GetWidget())
    return;

  const int64_t display_id = display::Screen::GetScreen()
                                 ->GetDisplayNearestWindow(GetRootWindow())
                                 .id();
  menu_model_ = std::make_unique<ShelfContextMenuModel>(
      std::vector<mojom::MenuItemPtr>(), nullptr, display_id);

  menu_model_->set_histogram_name("Apps.ContextMenuExecuteCommand.NotFromApp");
  UMA_HISTOGRAM_ENUMERATION("Apps.ContextMenuShowSource.Desktop", source_type,
                            ui::MENU_SOURCE_TYPE_LAST);

  int run_types = views::MenuRunner::CONTEXT_MENU;
  views::MenuAnchorPosition anchor_position = views::MENU_ANCHOR_TOPLEFT;
  if (::features::IsTouchableAppContextMenuEnabled()) {
    run_types |= views::MenuRunner::USE_TOUCHABLE_LAYOUT |
                 views::MenuRunner::FIXED_ANCHOR;
    anchor_position = views::MENU_ANCHOR_BUBBLE_TOUCHABLE_ABOVE;
  }
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), run_types,
      base::Bind(&RootWindowController::OnMenuClosed, base::Unretained(this),
                 base::TimeTicks::Now()));
  menu_runner_->RunMenuAt(wallpaper_widget_controller()->GetWidget(), nullptr,
                          gfx::Rect(location_in_screen, gfx::Size()),
                          anchor_position, source_type);
}

void RootWindowController::HideContextMenu() {
  if (menu_runner_)
    menu_runner_->Cancel();
}

bool RootWindowController::IsContextMenuShown() const {
  return menu_runner_ && menu_runner_->IsRunning();
}

void RootWindowController::UpdateAfterLoginStatusChange(LoginStatus status) {
  StatusAreaWidget* status_area_widget =
      shelf_->shelf_widget()->status_area_widget();
  if (status_area_widget)
    status_area_widget->UpdateAfterLoginStatusChange(status);
}

////////////////////////////////////////////////////////////////////////////////
// RootWindowController, private:

RootWindowController::RootWindowController(
    AshWindowTreeHost* ash_host,
    aura::WindowTreeHost* window_tree_host)
    : ash_host_(ash_host),
      mus_window_tree_host_(window_tree_host),
      window_tree_host_(ash_host ? ash_host->AsWindowTreeHost()
                                 : window_tree_host),
      shelf_(std::make_unique<Shelf>()),
      lock_screen_action_background_controller_(
          LockScreenActionBackgroundController::Create()) {
  DCHECK((ash_host && !window_tree_host) || (!ash_host && window_tree_host));

  if (!root_window_controllers_)
    root_window_controllers_ = new std::vector<RootWindowController*>;
  root_window_controllers_->push_back(this);

  aura::Window* root_window = GetRootWindow();
  GetRootWindowSettings(root_window)->controller = this;

  stacking_controller_.reset(new StackingController);
  aura::client::SetWindowParentingClient(root_window,
                                         stacking_controller_.get());
  capture_client_.reset(new ::wm::ScopedCaptureClient(root_window));

  wallpaper_widget_controller_ = std::make_unique<WallpaperWidgetController>(
      base::BindOnce(&RootWindowController::OnFirstWallpaperWidgetSet,
                     base::Unretained(this)));
}

void RootWindowController::Init(RootWindowType root_window_type) {
  aura::Window* root_window = GetRootWindow();
  Shell* shell = Shell::Get();
  shell->InitRootWindow(root_window);

  CreateContainers();
  CreateSystemWallpaper(root_window_type);

  InitLayoutManagers();
  InitTouchHuds();
  InitializeShelf();

  if (Shell::GetPrimaryRootWindowController()
          ->GetSystemModalLayoutManager(nullptr)
          ->has_window_dimmer()) {
    GetSystemModalLayoutManager(nullptr)->CreateModalBackground();
  }

  root_window_layout_manager_->OnWindowResized();
  if (root_window_type == RootWindowType::PRIMARY) {
    shell->EnableKeyboard();
  } else {
    window_tree_host_->Show();

    // Notify shell observers about new root window.
    shell->OnRootWindowAdded(root_window);
  }

  // TODO: TouchExplorationManager doesn't work with mash.
  // http://crbug.com/679782
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshDisableTouchExplorationMode)) {
    touch_exploration_manager_ =
        std::make_unique<TouchExplorationManager>(this);
  }
}

void RootWindowController::InitLayoutManagers() {
  // Create the shelf and status area widgets. Creates the ShelfLayoutManager
  // as a side-effect.
  DCHECK(!shelf_->shelf_widget());
  aura::Window* root = GetRootWindow();
  shelf_->CreateShelfWidget(root);

  root_window_layout_manager_ = new wm::RootWindowLayoutManager(root);
  root->SetLayoutManager(root_window_layout_manager_);

  aura::Window* default_container =
      GetContainer(kShellWindowId_DefaultContainer);
  // Installs WorkspaceLayoutManager on |default_container|.
  workspace_controller_.reset(new WorkspaceController(default_container));

  aura::Window* modal_container =
      GetContainer(kShellWindowId_SystemModalContainer);
  modal_container->SetLayoutManager(
      new SystemModalContainerLayoutManager(modal_container));

  aura::Window* lock_modal_container =
      GetContainer(kShellWindowId_LockSystemModalContainer);
  DCHECK(lock_modal_container);
  lock_modal_container->SetLayoutManager(
      new SystemModalContainerLayoutManager(lock_modal_container));

  aura::Window* lock_action_handler_container =
      GetContainer(kShellWindowId_LockActionHandlerContainer);
  DCHECK(lock_action_handler_container);
  lock_screen_action_background_controller_->SetParentWindow(
      lock_action_handler_container);
  lock_action_handler_container->SetLayoutManager(
      new LockActionHandlerLayoutManager(
          lock_action_handler_container, shelf_.get(),
          lock_screen_action_background_controller_.get()));

  aura::Window* lock_container =
      GetContainer(kShellWindowId_LockScreenContainer);
  DCHECK(lock_container);
  lock_container->SetLayoutManager(
      new LockLayoutManager(lock_container, shelf_.get()));

  aura::Window* always_on_top_container =
      GetContainer(kShellWindowId_AlwaysOnTopContainer);
  DCHECK(always_on_top_container);
  always_on_top_controller_ =
      std::make_unique<AlwaysOnTopController>(always_on_top_container);

  wm::WmSnapToPixelLayoutManager::InstallOnContainers(root);

  // Make it easier to resize windows that partially overlap the shelf. Must
  // occur after the ShelfLayoutManager is constructed by ShelfWidget.
  aura::Window* shelf_container = GetContainer(kShellWindowId_ShelfContainer);
  shelf_container->SetEventTargeter(
      std::make_unique<ShelfWindowTargeter>(shelf_container, shelf_.get()));
  aura::Window* status_container = GetContainer(kShellWindowId_StatusContainer);
  status_container->SetEventTargeter(
      std::make_unique<ShelfWindowTargeter>(status_container, shelf_.get()));
}

void RootWindowController::CreateContainers() {
  aura::Window* root = GetRootWindow();
  // For screen rotation animation: add a NOT_DRAWN layer in between the
  // root_window's layer and its current children so that we only need to
  // initiate two LayerAnimationSequences. One for the new layers and one for
  // the old layers.
  aura::Window* screen_rotation_container = CreateContainer(
      kShellWindowId_ScreenRotationContainer, "ScreenRotationContainer", root);

  // These containers are just used by PowerButtonController to animate groups
  // of containers simultaneously without messing up the current transformations
  // on those containers. These are direct children of the
  // screen_rotation_container window; all of the other containers are their
  // children.

  // The wallpaper container is not part of the lock animation, so it is not
  // included in those animate groups. When the screen is locked, the wallpaper
  // is moved to the lock screen wallpaper container (and moved back on unlock).
  // Ensure that there's an opaque layer occluding the non-lock-screen layers.
  aura::Window* wallpaper_container =
      CreateContainer(kShellWindowId_WallpaperContainer, "WallpaperContainer",
                      screen_rotation_container);
  ::wm::SetChildWindowVisibilityChangesAnimated(wallpaper_container);

  aura::Window* non_lock_screen_containers = CreateContainer(
      kShellWindowId_NonLockScreenContainersContainer,
      "NonLockScreenContainersContainer", screen_rotation_container);
  // Clip all windows inside this container, as half pixel of the window's
  // texture may become visible when the screen is scaled. crbug.com/368591.
  non_lock_screen_containers->layer()->SetMasksToBounds(true);

  aura::Window* lock_wallpaper_containers = CreateContainer(
      kShellWindowId_LockScreenWallpaperContainer,
      "LockScreenWallpaperContainer", screen_rotation_container);
  ::wm::SetChildWindowVisibilityChangesAnimated(lock_wallpaper_containers);

  aura::Window* lock_screen_containers = CreateContainer(
      kShellWindowId_LockScreenContainersContainer,
      "LockScreenContainersContainer", screen_rotation_container);
  aura::Window* lock_screen_related_containers = CreateContainer(
      kShellWindowId_LockScreenRelatedContainersContainer,
      "LockScreenRelatedContainersContainer", screen_rotation_container);

  aura::Window* app_list_tablet_mode_container =
      CreateContainer(kShellWindowId_AppListTabletModeContainer,
                      "AppListTabletModeContainer", non_lock_screen_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(app_list_tablet_mode_container);
  app_list_tablet_mode_container->SetProperty(::wm::kUsesScreenCoordinatesKey,
                                              true);

  CreateContainer(kShellWindowId_UnparentedControlContainer,
                  "UnparentedControlContainer", non_lock_screen_containers);

  aura::Window* default_container =
      CreateContainer(kShellWindowId_DefaultContainer, "DefaultContainer",
                      non_lock_screen_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(default_container);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(default_container);
  default_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);
  wm::SetChildrenUseExtendedHitRegionForWindow(default_container);

  aura::Window* always_on_top_container =
      CreateContainer(kShellWindowId_AlwaysOnTopContainer,
                      "AlwaysOnTopContainer", non_lock_screen_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(always_on_top_container);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(always_on_top_container);
  always_on_top_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);

  aura::Window* app_list_container =
      CreateContainer(kShellWindowId_AppListContainer, "AppListContainer",
                      non_lock_screen_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(app_list_container);
  app_list_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);

  aura::Window* arc_ime_parent_container = CreateContainer(
      kShellWindowId_ArcImeWindowParentContainer, "ArcImeWindowParentContainer",
      non_lock_screen_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(arc_ime_parent_container);
  arc_ime_parent_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);
  arc_ime_parent_container->SetLayoutManager(
      new ArcVirtualKeyboardContainerLayoutManager(arc_ime_parent_container));
  aura::Window* arc_vk_container =
      CreateContainer(kShellWindowId_ArcVirtualKeyboardContainer,
                      "ArcVirtualKeyboardContainer", arc_ime_parent_container);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(arc_vk_container);
  arc_vk_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);

  aura::Window* shelf_container_parent = lock_screen_related_containers;
  aura::Window* shelf_container = CreateContainer(
      kShellWindowId_ShelfContainer, "ShelfContainer", shelf_container_parent);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(shelf_container);
  shelf_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);
  shelf_container->SetProperty(kLockedToRootKey, true);

  aura::Window* shelf_bubble_container =
      CreateContainer(kShellWindowId_ShelfBubbleContainer,
                      "ShelfBubbleContainer", non_lock_screen_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(shelf_bubble_container);
  shelf_bubble_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);
  shelf_bubble_container->SetProperty(kLockedToRootKey, true);

  aura::Window* modal_container =
      CreateContainer(kShellWindowId_SystemModalContainer,
                      "SystemModalContainer", non_lock_screen_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(modal_container);
  ::wm::SetChildWindowVisibilityChangesAnimated(modal_container);
  modal_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);
  wm::SetChildrenUseExtendedHitRegionForWindow(modal_container);

  aura::Window* lock_container =
      CreateContainer(kShellWindowId_LockScreenContainer, "LockScreenContainer",
                      lock_screen_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(lock_container);
  lock_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);

  aura::Window* lock_action_handler_container =
      CreateContainer(kShellWindowId_LockActionHandlerContainer,
                      "LockActionHandlerContainer", lock_screen_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(lock_action_handler_container);
  ::wm::SetChildWindowVisibilityChangesAnimated(lock_action_handler_container);
  lock_action_handler_container->SetProperty(::wm::kUsesScreenCoordinatesKey,
                                             true);

  aura::Window* lock_modal_container =
      CreateContainer(kShellWindowId_LockSystemModalContainer,
                      "LockSystemModalContainer", lock_screen_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(lock_modal_container);
  ::wm::SetChildWindowVisibilityChangesAnimated(lock_modal_container);
  lock_modal_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);
  wm::SetChildrenUseExtendedHitRegionForWindow(lock_modal_container);

  aura::Window* status_container =
      CreateContainer(kShellWindowId_StatusContainer, "StatusContainer",
                      lock_screen_related_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(status_container);
  status_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);
  status_container->SetProperty(kLockedToRootKey, true);

  aura::Window* power_menu_container =
      CreateContainer(kShellWindowId_PowerMenuContainer, "PowerMenuContainer",
                      lock_screen_related_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(power_menu_container);
  power_menu_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);

  aura::Window* settings_bubble_container =
      CreateContainer(kShellWindowId_SettingBubbleContainer,
                      "SettingBubbleContainer", lock_screen_related_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(settings_bubble_container);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(settings_bubble_container);
  settings_bubble_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);
  settings_bubble_container->SetProperty(kLockedToRootKey, true);

  aura::Window* accessibility_panel_container = CreateContainer(
      kShellWindowId_AccessibilityPanelContainer, "AccessibilityPanelContainer",
      lock_screen_related_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(accessibility_panel_container);
  accessibility_panel_container->SetProperty(::wm::kUsesScreenCoordinatesKey,
                                             true);
  accessibility_panel_container->SetProperty(kLockedToRootKey, true);
  accessibility_panel_container->SetLayoutManager(
      new AccessibilityPanelLayoutManager());

  aura::Window* virtual_keyboard_parent_container = CreateContainer(
      kShellWindowId_ImeWindowParentContainer, "VirtualKeyboardParentContainer",
      lock_screen_related_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(
      virtual_keyboard_parent_container);
  virtual_keyboard_parent_container->SetProperty(
      ::wm::kUsesScreenCoordinatesKey, true);
  virtual_keyboard_parent_container->SetLayoutManager(
      new VirtualKeyboardContainerLayoutManager(
          virtual_keyboard_parent_container));
  aura::Window* virtual_keyboard_container = CreateContainer(
      kShellWindowId_VirtualKeyboardContainer, "VirtualKeyboardContainer",
      virtual_keyboard_parent_container);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(virtual_keyboard_container);
  virtual_keyboard_container->SetProperty(::wm::kUsesScreenCoordinatesKey,
                                          true);
  virtual_keyboard_container->SetLayoutManager(
      new keyboard::KeyboardLayoutManager(keyboard::KeyboardController::Get()));

  aura::Window* menu_container =
      CreateContainer(kShellWindowId_MenuContainer, "MenuContainer",
                      lock_screen_related_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(menu_container);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(menu_container);
  menu_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);

  aura::Window* drag_drop_container = CreateContainer(
      kShellWindowId_DragImageAndTooltipContainer,
      "DragImageAndTooltipContainer", lock_screen_related_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(drag_drop_container);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(drag_drop_container);
  drag_drop_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);

  aura::Window* overlay_container =
      CreateContainer(kShellWindowId_OverlayContainer, "OverlayContainer",
                      lock_screen_related_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(overlay_container);
  overlay_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);
  overlay_container->SetLayoutManager(
      new OverlayLayoutManager(overlay_container));  // Takes ownership.

  CreateContainer(kShellWindowId_DockedMagnifierContainer,
                  "DockedMagnifierContainer", lock_screen_related_containers);

  aura::Window* mouse_cursor_container =
      CreateContainer(kShellWindowId_MouseCursorContainer,
                      "MouseCursorContainer", screen_rotation_container);
  mouse_cursor_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);

  CreateContainer(kShellWindowId_PowerButtonAnimationContainer,
                  "PowerButtonAnimationContainer", screen_rotation_container);
}

void RootWindowController::CreateSystemWallpaper(
    RootWindowType root_window_type) {
  SkColor color = SK_ColorBLACK;
  // The splash screen appears on the primary display at boot. If this is a
  // secondary monitor (either connected at boot or connected later) or if the
  // browser restarted for a second login then don't use the boot color.
  const bool is_boot_splash_screen =
      root_window_type == RootWindowType::PRIMARY &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kFirstExecAfterBoot);
  if (is_boot_splash_screen)
    color = kChromeOsBootColor;
  system_wallpaper_.reset(
      new SystemWallpaperController(GetRootWindow(), color));
}

void RootWindowController::ResetRootForNewWindowsIfNecessary() {
  // Change the target root window before closing child windows. If any child
  // being removed triggers a relayout of the shelf it will try to build a
  // window list adding windows from the target root window's containers which
  // may have already gone away.
  aura::Window* root = GetRootWindow();
  if (Shell::GetRootWindowForNewWindows() == root) {
    // The root window for new windows is being destroyed. Switch to the primary
    // root window if possible.
    aura::Window* primary_root = Shell::GetPrimaryRootWindow();
    Shell::Get()->shell_state()->SetRootWindowForNewWindows(
        primary_root == root ? nullptr : primary_root);
  }
}

void RootWindowController::OnMenuClosed(
    const base::TimeTicks desktop_context_menu_show_time) {
  menu_runner_.reset();
  menu_model_.reset();
  shelf_->UpdateVisibilityState();
  UMA_HISTOGRAM_TIMES("Apps.ContextMenuUserJourneyTime.Desktop",
                      base::TimeTicks::Now() - desktop_context_menu_show_time);
}

void RootWindowController::OnFirstWallpaperWidgetSet() {
  DCHECK(system_wallpaper_.get());

  // Set the system wallpaper color once a wallpaper has been set to ensure the
  // wallpaper color that might have been set for the Chrome OS boot splash
  // screen is overriden.
  system_wallpaper_->SetColor(SK_ColorBLACK);
}

}  // namespace ash
