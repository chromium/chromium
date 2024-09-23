// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ash/accessibility/chromevox/touch_exploration_controller.h"
#include "ash/accessibility/chromevox/touch_exploration_manager.h"
#include "ash/accessibility/ui/accessibility_panel_layout_manager.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_menu/app_menu_model_adapter.h"
#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/curtain/security_curtain_widget_controller.h"
#include "ash/focus_cycler.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/keyboard/arc/arc_virtual_keyboard_container_layout_manager.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/keyboard_layout_manager.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/virtual_keyboard_container_layout_manager.h"
#include "ash/lock_screen_action/lock_screen_action_background_controller.h"
#include "ash/login_status.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_settings.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_context_menu_model.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shelf/shelf_window_targeter.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider_source.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/touch/touch_hud_debug.h"
#include "ash/touch/touch_hud_projection.h"
#include "ash/touch/touch_observer_hud.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/fullscreen_window_finder.h"
#include "ash/wm/lock_action_handler_layout_manager.h"
#include "ash/wm/lock_layout_manager.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overlay_layout_manager.h"
#include "ash/wm/overview/birch/birch_bar_context_menu_model.h"
#include "ash/wm/overview/birch/birch_bar_controller.h"
#include "ash/wm/overview/birch/birch_bar_menu_model_adapter.h"
#include "ash/wm/overview/birch/birch_privacy_nudge_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/root_window_layout_manager.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_overview_session.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/switchable_windows.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/window_parenting_controller.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_metrics.h"
#include "ash/wm/work_area_insets.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/null_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_properties.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/tooltip_client.h"

namespace ash {
namespace {

bool IsInShelfContainer(aura::Window* container) {
  if (!container) {
    return false;
  }
  int id = container->GetId();
  if (id == ash::kShellWindowId_ShelfContainer ||
      id == ash::kShellWindowId_ShelfBubbleContainer) {
    return true;
  }
  return IsInShelfContainer(container->parent());
}

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
    if (target_path.empty()) {
      return false;
    }

    aura::Window* target = target_path.back();
    target_path.pop_back();
    aura::Window* blocking = blocking_path.back();
    blocking_path.pop_back();

    // Still on the same path, continue.
    if (target == blocking) {
      continue;
    }

    // This can happen only if unparented window is passed because
    // first element must be the same root.
    if (!target->parent() || !blocking->parent()) {
      return false;
    }

    aura::Window* common_parent = target->parent();
    DCHECK_EQ(common_parent, blocking->parent());
    const aura::Window::Windows& windows = common_parent->children();
    auto blocking_iter = base::ranges::find(windows, blocking);
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
  WindowState* state = WindowState::Get(window);
  gfx::Rect restore_bounds;
  const bool has_restore_bounds = state && state->HasRestoreBounds();

  const bool update_bounds =
      state && (state->IsNormalStateType() || state->IsMinimized());
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

  // Snapped windows have bounds handled by the layout manager in AddChild().
  if (update_bounds) {
    window->SetBounds(local_bounds);
  }

  if (has_restore_bounds) {
    state->SetRestoreBoundsInParent(restore_bounds);
  }
}

// Reparents the appropriate set of windows from |src| to |dst|.
void ReparentAllWindows(aura::Window* src, aura::Window* dst) {
  // Set of windows to move.
  constexpr int kContainerIdsToMove[] = {
      kShellWindowId_UnparentedContainer,
      kShellWindowId_AlwaysOnTopContainer,
      kShellWindowId_FloatContainer,
      kShellWindowId_PipContainer,
      kShellWindowId_SystemModalContainer,
      kShellWindowId_LockActionHandlerContainer,
      kShellWindowId_LockSystemModalContainer,
      kShellWindowId_MenuContainer,
      kShellWindowId_LiveCaptionContainer,
      kShellWindowId_OverlayContainer,
  };
  constexpr int kExtraContainerIdsToMoveInUnifiedMode[] = {
      kShellWindowId_LockScreenContainer,
  };

  // Desk container ids are different depends on whether Bento feature is
  // enabled or not.
  std::vector<int> container_ids = desks_util::GetDesksContainersIds();
  for (const int id : kContainerIdsToMove) {
    container_ids.emplace_back(id);
  }

  // Check the display mode as this is also necessary when trasitioning between
  // mirror and unified mode.
  if (Shell::Get()->display_manager()->current_default_multi_display_mode() ==
      display::DisplayManager::UNIFIED) {
    for (const int id : kExtraContainerIdsToMoveInUnifiedMode) {
      container_ids.emplace_back(id);
    }
  }

  const std::vector<raw_ptr<aura::Window, VectorExperimental>> mru_list =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kAllDesks);
  for (int id : container_ids) {
    aura::Window* src_container = src->GetChildById(id);
    aura::Window* dst_container = dst->GetChildById(id);
    const bool switchable_container = IsSwitchableContainer(src_container);
    while (!src_container->children().empty()) {
      // Restart iteration from the source container windows each time as they
      // may change as a result of moving other windows.
      const aura::Window::Windows& src_container_children =
          src_container->children();
      auto iter = src_container_children.rbegin();
      while (iter != src_container_children.rend() &&
             SystemModalContainerLayoutManager::IsModalBackground(*iter)) {
        ++iter;
      }
      // If the entire window list is modal background windows then stop.
      if (iter == src_container_children.rend()) {
        break;
      }

      // |iter| is invalidated after ReparentWindow. Cache it to use afterwards.
      aura::Window* const window = *iter;
      ReparentWindow(window, dst_container);

      aura::Window* stacking_target = nullptr;
      if (switchable_container) {
        // Find the first window that comes after |window| in the MRU list that
        // shares the same parent.
        bool found_window = false;
        for (aura::Window* window_iter : mru_list) {
          // First determine the position of |window| in the |mru_list|.
          if (!found_window && window == window_iter) {
            found_window = true;
            continue;
          }

          if (!found_window || window_iter->parent() != dst_container) {
            continue;
          }

          // Once |window| is found, the next item in |mru_list| with the same
          // parent (container) is the stacking target.
          stacking_target = window_iter;
          break;
        }
      }

      // |stacking_target| may be null if |switchable_container| is false, which
      // means the children of that container wouldn't be in the MRU list or if
      // |window| was the last item in the MRU list with parent id |id|. In
      // this case stack |window| at the bottom.
      if (stacking_target) {
        dst_container->StackChildAbove(window, stacking_target);
      } else {
        dst_container->StackChildAtBottom(window);
      }
    }
  }
}

bool ShouldDestroyWindowInCloseChildWindows(aura::Window* window) {
  return window->owned_by_parent();
}

// Clears the workspace controllers from the properties of all virtual desks
// containers in |root|.
void ClearWorkspaceControllers(aura::Window* root) {
  for (auto* desk_container : desks_util::GetDesksContainers(root)) {
    SetWorkspaceController(desk_container, nullptr);
  }
  if (auto* float_controller = Shell::Get()->float_controller()) {
    float_controller->ClearWorkspaceEventHandler(root);
  }
}

class RootWindowTargeter : public aura::WindowTargeter {
 public:
  RootWindowTargeter() = default;

  RootWindowTargeter(const RootWindowTargeter&) = delete;
  RootWindowTargeter& operator=(const RootWindowTargeter&) = delete;

  ~RootWindowTargeter() override = default;

 protected:
  aura::Window* FindTargetForLocatedEvent(aura::Window* window,
                                          ui::LocatedEvent* event) override {
    if (!window->parent() && !window->bounds().Contains(event->location()) &&
        IsEventInsideDisplayForTelemetryHack(window, event)) {
      auto* dispatcher = window->GetHost()->dispatcher();
      bool has_capture_target = !!dispatcher->mouse_pressed_handler() ||
                                !!aura::client::GetCaptureWindow(window);

      // Make sure that event location is within the root window bounds if
      // 1) mouse event isn't captured.
      // 2) A mouse is clicked without movement and capture.
      //
      // The event can be outside on some scale factor due to rounding, or due
      // to not well calibrated a touch screen, or Detect this situation and
      // adjust the location.
      bool bounded_click = ShouldConstrainMouseClick(event, has_capture_target);
      if (!has_capture_target || bounded_click) {
        gfx::Point new_location =
            FitPointToBounds(event->location(), window->bounds());
        // Do not change |location_f|. It's used to compute pixel position and
        // such client should know what they're doing.
        event->set_location(new_location);
        event->set_root_location(new_location);
      }
    }
    return aura::WindowTargeter::FindTargetForLocatedEvent(window, event);
  }

  // Stop-gap workaround for telemetry tests that send events far outside of the
  // display (e.g. 512, -4711). Fix the test and remove this (crbgu.com/904623).
  bool IsEventInsideDisplayForTelemetryHack(aura::Window* window,
                                            ui::LocatedEvent* event) {
    constexpr int ExtraMarginForTelemetryTest = -10;
    gfx::Rect bounds = window->bounds();
    bounds.Inset(ExtraMarginForTelemetryTest);
    return bounds.Contains(event->location());
  }

 private:
  // Returns true if the mouse event should be constrainted.
  bool ShouldConstrainMouseClick(ui::LocatedEvent* event,
                                 bool has_capture_target) {
    if (event->type() == ui::EventType::kMousePressed && !has_capture_target) {
      last_mouse_event_type_ = ui::EventType::kMousePressed;
      return true;
    }
    if (last_mouse_event_type_ == ui::EventType::kMousePressed &&
        event->type() == ui::EventType::kMouseReleased && has_capture_target) {
      last_mouse_event_type_ = ui::EventType::kUnknown;
      return true;
    }
    // For other cases, reset the state
    if (event->type() != ui::EventType::kMouseCaptureChanged) {
      last_mouse_event_type_ = ui::EventType::kUnknown;
    }
    return false;
  }

  gfx::Point FitPointToBounds(const gfx::Point p, const gfx::Rect& bounds) {
    return gfx::Point(std::clamp(p.x(), bounds.x(), bounds.right() - 1),
                      std::clamp(p.y(), bounds.y(), bounds.bottom() - 1));
  }

  ui::EventType last_mouse_event_type_ = ui::EventType::kUnknown;
};

class ShelfMenuModelAdapter : public AppMenuModelAdapter {
 public:
  ShelfMenuModelAdapter(std::unique_ptr<ui::SimpleMenuModel> model,
                        views::Widget* widget_owner,
                        ui::MenuSourceType source_type,
                        base::OnceClosure on_menu_closed_callback,
                        bool is_tablet_mode)
      : AppMenuModelAdapter(std::string(),
                            std::move(model),
                            widget_owner,
                            source_type,
                            std::move(on_menu_closed_callback),
                            is_tablet_mode) {}

  ShelfMenuModelAdapter(const ShelfMenuModelAdapter&) = delete;
  ShelfMenuModelAdapter& operator=(const ShelfMenuModelAdapter&) = delete;

  ~ShelfMenuModelAdapter() override = default;

 private:
  // AppMenuModelAdapter overrides:
  void RecordHistogramOnMenuClosed() override {
    const base::TimeDelta user_journey_time =
        base::TimeTicks::Now() - menu_open_time();

    UMA_HISTOGRAM_TIMES("Apps.ContextMenuUserJourneyTimeV2.Desktop",
                        user_journey_time);
    UMA_HISTOGRAM_ENUMERATION("Apps.ContextMenuShowSourceV2.Desktop",
                              source_type(), ui::MENU_SOURCE_TYPE_LAST);
    if (is_tablet_mode()) {
      UMA_HISTOGRAM_TIMES(
          "Apps.ContextMenuUserJourneyTimeV2.Desktop.TabletMode",
          user_journey_time);
      UMA_HISTOGRAM_ENUMERATION(
          "Apps.ContextMenuShowSourceV2.Desktop.TabletMode", source_type(),
          ui::MENU_SOURCE_TYPE_LAST);
    } else {
      UMA_HISTOGRAM_TIMES(
          "Apps.ContextMenuUserJourneyTimeV2.Desktop.ClamshellMode",
          user_journey_time);
      UMA_HISTOGRAM_ENUMERATION(
          "Apps.ContextMenuShowSourceV2.Desktop.ClamshellMode", source_type(),
          ui::MENU_SOURCE_TYPE_LAST);
    }
  }
};

// A layout manager that fills its container when the child window's resize
// behavior is set to be maximizable.
class FillLayoutManager : public aura::LayoutManager {
 public:
  explicit FillLayoutManager(aura::Window* container) : container_(container) {}
  ~FillLayoutManager() override = default;
  FillLayoutManager(const FillLayoutManager&) = delete;
  FillLayoutManager& operator=(const FillLayoutManager&) = delete;

  // aura::LayoutManager:
  void OnWindowResized() override { Relayout(); }
  void OnWindowAddedToLayout(aura::Window* child) override { Relayout(); }
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {}

 private:
  void Relayout() {
    // Fill the window that is set to be maximizable.
    const gfx::Rect fullscreen(container_->bounds().size());
    for (aura::Window* child : container_->children()) {
      const int resize_behavior =
          child->GetProperty(aura::client::kResizeBehaviorKey);
      if (resize_behavior & aura::client::kResizeBehaviorCanMaximize) {
        SetChildBoundsDirect(child, fullscreen);
      }
    }
  }

  raw_ptr<aura::Window> container_;
};

}  // namespace

// static
std::vector<RootWindowController*>*
    RootWindowController::root_window_controllers_ = nullptr;

RootWindowController::~RootWindowController() {
  Shutdown(/*destination_root=*/nullptr);
  DCHECK(!wallpaper_widget_controller_.get());
  work_area_insets_.reset();
  ash_host_.reset();
  // The CaptureClient needs to be around for as long as the RootWindow is
  // valid.
  capture_client_.reset();
  root_window_controllers_->erase(
      base::ranges::find(*root_window_controllers_, this));
}

RootWindowController* RootWindowController::CreateForPrimaryDisplay(
    AshWindowTreeHost* host) {
  RootWindowController* controller = new RootWindowController(host);
  controller->Init(RootWindowType::PRIMARY);
  return controller;
}

RootWindowController* RootWindowController::CreateForSecondaryDisplay(
    AshWindowTreeHost* host) {
  RootWindowController* controller = new RootWindowController(host);
  controller->Init(RootWindowType::SECONDARY);
  return controller;
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

ShelfLayoutManager* RootWindowController::GetShelfLayoutManager() {
  return shelf_->shelf_layout_manager();
}

SystemModalContainerLayoutManager*
RootWindowController::GetSystemModalLayoutManager(aura::Window* window) {
  aura::Window* modal_container = nullptr;
  if (window) {
    aura::Window* window_container = GetContainerForWindow(window);
    if (window_container &&
        window_container->GetId() >= kShellWindowId_LockScreenContainer) {
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

bool RootWindowController::IsSystemTrayVisible() {
  TrayBackgroundView* tray = GetStatusAreaWidget()->unified_system_tray();
  return tray && tray->GetWidget()->IsVisible() && tray->GetVisible();
}

bool RootWindowController::CanWindowReceiveEvents(aura::Window* window) {
  if (GetRootWindow() != window->GetRootWindow()) {
    return false;
  }

  aura::Window* blocking_container = nullptr;
  aura::Window* modal_container = nullptr;
  window_util::GetBlockingContainersForRoot(
      GetRootWindow(), &blocking_container, &modal_container);
  SystemModalContainerLayoutManager* modal_layout_manager = nullptr;
  if (modal_container && modal_container->layout_manager()) {
    modal_layout_manager = static_cast<SystemModalContainerLayoutManager*>(
        modal_container->layout_manager());
  }

  if (modal_layout_manager && modal_layout_manager->has_window_dimmer()) {
    blocking_container = modal_container;
  } else {
    modal_container = nullptr;  // Don't check modal dialogs.
  }

  // In normal session.
  if (!blocking_container) {
    return true;
  }

  if (!IsWindowAboveContainer(window, blocking_container)) {
    return false;
  }

  if (modal_container) {
    // If the window is in the target modal container, only allow the top most
    // one.
    if (modal_container->Contains(window)) {
      return modal_layout_manager->IsPartOfActiveModalWindow(window);
    }
    // Don't allow shelf to process events if there is a visible modal dialog.
    if (IsInShelfContainer(window->parent())) {
      return false;
    }
  }
  return true;
}

aura::Window* RootWindowController::FindEventTarget(
    const gfx::Point& location_in_screen) {
  gfx::Point location_in_root(location_in_screen);
  aura::Window* root_window = GetRootWindow();
  ::wm::ConvertPointFromScreen(root_window, &location_in_root);
  ui::MouseEvent test_event(ui::EventType::kMouseMoved, location_in_root,
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

ScreenRotationAnimator* RootWindowController::GetScreenRotationAnimator() {
  if (is_shutting_down_) {
    return nullptr;
  }

  if (!screen_rotation_animator_) {
    screen_rotation_animator_ =
        std::make_unique<ScreenRotationAnimator>(GetRootWindow());
  }

  return screen_rotation_animator_.get();
}

void RootWindowController::Shutdown(aura::Window* destination_root) {
  is_shutting_down_ = true;

  // Moving root windows can cause an observer to be destroyed, i.e. if
  // `SplitViewController::OnWindowRemovingFromRootWindow()` ends overview, the
  // `screen_rotation_animator_` should be deleted first to avoid a dangling
  // raw_ptr. Destroy the `screen_rotation_animator_` now to avoid this and any
  // potential crashes if there's any ongoing animation. See http://b/293667233.
  screen_rotation_animator_.reset();

  if (destination_root) {
    MoveWindowsTo(destination_root);
  }

  aura::Window* root_window = GetRootWindow();
  auto targeter = root_window->SetEventTargeter(
      std::make_unique<aura::NullWindowTargeter>());

  touch_exploration_manager_.reset();
  wallpaper_widget_controller_.reset();
  EndSplitViewOverviewSession(SplitViewOverviewSessionExitPoint::kShutdown);
  CloseAmbientWidget(/*immediately=*/true);

  CloseChildWindows();
  GetRootWindowSettings(root_window)->controller = nullptr;
  // Forget with the display ID so that display lookup
  // ends up with invalid display.
  GetRootWindowSettings(root_window)->display_id = display::kInvalidDisplayId;
  if (ash_host_) {
    ash_host_->PrepareForShutdown();
  }
  window_parenting_controller_.reset();
  security_curtain_widget_controller_.reset();
  lock_screen_action_background_controller_.reset();
  aura::client::SetScreenPositionClient(root_window, nullptr);

  // The targeter may still on the stack, so delete it later.
  if (targeter) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(targeter));
  }
}

void RootWindowController::CloseChildWindows() {
  // Child windows can be closed by secondary monitor disconnection, Shell
  // shutdown, or both. Avoid running the related cleanup code twice.
  if (did_close_child_windows_) {
    return;
  }
  did_close_child_windows_ = true;

  aura::Window* root = GetRootWindow();

  Shell::Get()->desks_controller()->OnRootWindowClosing(root);

  // Notify the keyboard controller before closing child windows and shutting
  // down associated layout managers.
  Shell::Get()->keyboard_controller()->OnRootWindowClosing(root);

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller && overview_controller->InOverviewSession()) {
    overview_controller->overview_session()->OnRootWindowClosing(root);
  }

  shelf_->ShutdownShelfWidget();

  ClearWorkspaceControllers(root);
  always_on_top_controller_->ClearLayoutManagers();

  // Explicitly destroy top level windows. We do this because such windows may
  // query the RootWindow for state.
  aura::WindowTracker non_toplevel_windows;
  non_toplevel_windows.Add(root);
  while (!non_toplevel_windows.windows().empty()) {
    aura::Window* non_toplevel_window = non_toplevel_windows.Pop();
    aura::WindowTracker toplevel_windows;
    for (aura::Window* child : non_toplevel_window->children()) {
      if (!ShouldDestroyWindowInCloseChildWindows(child)) {
        continue;
      }
      if (child->delegate()) {
        toplevel_windows.Add(child);
      } else {
        non_toplevel_windows.Add(child);
      }
    }
    while (!toplevel_windows.windows().empty()) {
      aura::Window* toplevel_window = toplevel_windows.windows().back();
      // Complete in progress animations before deleting the window. This is
      // done as deleting the window implicitly cancels animations (as long
      // as the toplevel_window is the layer owner), which may delete the
      // window.
      if (toplevel_window->layer()->owner() == toplevel_window) {
        toplevel_window->layer()->CompleteAllAnimations();
        if (toplevel_windows.windows().empty() ||
            toplevel_windows.windows().back() != toplevel_window) {
          continue;
        }
      }
      delete toplevel_windows.Pop();
    }
  }

  // Reset layout manager so that it won't fire unnecessary layout evetns.
  root->SetLayoutManager(nullptr);
  // And then remove the containers.
  while (!root->children().empty()) {
    aura::Window* child = root->children()[0];
    if (ShouldDestroyWindowInCloseChildWindows(child)) {
      delete child;
    } else {
      root->RemoveChild(child);
    }
  }

  // Removing the containers destroys ShelfLayoutManager. ShelfWidget outlives
  // ShelfLayoutManager because ShelfLayoutManager holds a pointer to it.
  shelf_->DestroyShelfWidget();

  ::wm::SetTooltipClient(GetRootWindow(), nullptr);
}

void RootWindowController::InitTouchHuds() {
  // Enable touch debugging features when each display is initialized.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAshTouchHud)) {
    set_touch_hud_debug(new TouchHudDebug(GetRootWindow()));
  }

  // TouchHudProjection manages its own lifetime.
  if (command_line->HasSwitch(switches::kShowTaps)) {
    touch_hud_projection_ = new TouchHudProjection(GetRootWindow());
  }
}

aura::Window* RootWindowController::GetWindowForFullscreenMode() {
  return GetWindowForFullscreenModeInRoot(GetRootWindow());
}

bool RootWindowController::IsInFullscreenMode() {
  auto* window = GetWindowForFullscreenMode();
  return window && WindowState::Get(window)->GetHideShelfWhenFullscreen();
}

void RootWindowController::SetTouchAccessibilityAnchorPoint(
    const gfx::Point& anchor_point) {
  if (touch_exploration_manager_) {
    touch_exploration_manager_->SetTouchAccessibilityAnchorPoint(anchor_point);
  }
}

void RootWindowController::ShowContextMenu(const gfx::Point& location_in_screen,
                                           ui::MenuSourceType source_type) {
  // Show birch bar context menu for the primary user in clamshell mode Overview
  // without a partial split screen.
  if (features::IsForestFeatureEnabled() &&
      Shell::Get()->session_controller()->IsUserPrimary() &&
      OverviewController::Get()->InOverviewSession() &&
      !split_view_overview_session_) {
    root_window_menu_model_adapter_ = BuildBirchMenuModelAdapter(source_type);
    BirchPrivacyNudgeController::DidShowContextMenu();
  } else {
    root_window_menu_model_adapter_ = BuildShelfMenuModelAdapter(source_type);
  }

  root_window_menu_model_adapter_->Run(
      gfx::Rect(location_in_screen, gfx::Size()),
      views::MenuAnchorPosition::kBubbleRight,
      views::MenuRunner::CONTEXT_MENU |
          views::MenuRunner::USE_ASH_SYS_UI_LAYOUT |
          views::MenuRunner::FIXED_ANCHOR);
}

void RootWindowController::UpdateAfterLoginStatusChange(LoginStatus status) {
  StatusAreaWidget* status_area_widget =
      shelf_->shelf_widget()->status_area_widget();
  if (status_area_widget) {
    status_area_widget->UpdateAfterLoginStatusChange(status);
  }
}

void RootWindowController::CreateAmbientWidget() {
  DCHECK(!ambient_widget_);

  auto* ambient_controller = Shell::Get()->ambient_controller();
  if (ambient_controller) {
    ambient_widget_ = ambient_controller->CreateWidget(
        GetRootWindow()->GetChildById(kShellWindowId_AmbientModeContainer));
  }
}

void RootWindowController::CloseAmbientWidget(bool immediately) {
  if (ambient_widget_) {
    if (immediately) {
      ambient_widget_->CloseNow();
    } else {
      ambient_widget_->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
    }
  }

  ambient_widget_.reset();
}

bool RootWindowController::HasAmbientWidget() const {
  return !!ambient_widget_;
}

AccessibilityPanelLayoutManager*
RootWindowController::GetAccessibilityPanelLayoutManagerForTest() {
  return GetAccessibilityPanelLayoutManager();
}

void RootWindowController::SetSecurityCurtainWidgetController(
    std::unique_ptr<curtain::SecurityCurtainWidgetController> controller) {
  DCHECK(!security_curtain_widget_controller_);
  security_curtain_widget_controller_ = std::move(controller);
}

void RootWindowController::ClearSecurityCurtainWidgetController() {
  security_curtain_widget_controller_ = nullptr;
}

curtain::SecurityCurtainWidgetController*
RootWindowController::security_curtain_widget_controller() {
  return security_curtain_widget_controller_.get();
}

void RootWindowController::StartSplitViewOverviewSession(
    aura::Window* window,
    std::optional<OverviewStartAction> action,
    std::optional<OverviewEnterExitType> type,
    WindowSnapActionSource snap_action_source) {
  if (split_view_overview_session_) {
    return;
  }

  // TODO(michelefan): Remove the `StartOverview()` here, this is currently
  // added to limit `SplitViewOverviewSession` creation and usage to clamshell
  // only.
  if (Shell::Get()->IsInTabletMode()) {
    OverviewController::Get()->StartOverview(
        action.value_or(OverviewStartAction::kSplitView),
        type.value_or(OverviewEnterExitType::kNormal));
    return;
  }

  CHECK(OverviewController::Get()->CanEnterOverview());
  split_view_overview_session_ =
      std::make_unique<SplitViewOverviewSession>(window, snap_action_source);
  split_view_overview_session_->Init(action, type);
}

void RootWindowController::EndSplitViewOverviewSession(
    SplitViewOverviewSessionExitPoint exit_point) {
  if (!is_shutting_down_ && split_view_overview_session_) {
    split_view_overview_session_
        ->RecordSplitViewOverviewSessionExitPointMetrics(exit_point);
  }
  split_view_overview_session_.reset();
}

void RootWindowController::SetScreenRotationAnimatorForTest(
    std::unique_ptr<ScreenRotationAnimator> animator) {
  screen_rotation_animator_ = std::move(animator);
}

bool RootWindowController::IsContextMenuShownForTest() const {
  return root_window_menu_model_adapter_ &&
         root_window_menu_model_adapter_->IsShowingMenu();
}

////////////////////////////////////////////////////////////////////////////////
// RootWindowController, private:

RootWindowController::RootWindowController(AshWindowTreeHost* ash_host)
    : ash_host_(ash_host),
      window_tree_host_(ash_host->AsWindowTreeHost()),
      shelf_(std::make_unique<Shelf>()),
      lock_screen_action_background_controller_(
          LockScreenActionBackgroundController::Create()),
      work_area_insets_(std::make_unique<WorkAreaInsets>(this)) {
  DCHECK(ash_host_);
  DCHECK(window_tree_host_);

  if (!root_window_controllers_) {
    root_window_controllers_ = new std::vector<RootWindowController*>;
  }
  root_window_controllers_->push_back(this);

  aura::Window* root_window = GetRootWindow();
  GetRootWindowSettings(root_window)->controller = this;

  window_parenting_controller_ =
      std::make_unique<WindowParentingController>(root_window);
  capture_client_ = std::make_unique<::wm::ScopedCaptureClient>(root_window);
}

void RootWindowController::MoveWindowsTo(aura::Window* dst) {
  // Suspend unnecessary updates of the shelf visibility indefinitely since it
  // is going away.
  if (GetShelfLayoutManager()) {
    GetShelfLayoutManager()->SuspendVisibilityUpdateForShutdown();
  }

  // Clear the workspace controller to avoid a lot of unnecessary operations
  // when window are removed.
  // TODO(afakhry): Should we also clear the WorkspaceLayoutManagers of the pip,
  // always-on-top, and other containers?
  aura::Window* root = GetRootWindow();
  ClearWorkspaceControllers(root);

  ReparentAllWindows(root, dst);
}

void RootWindowController::Init(RootWindowType root_window_type) {
  aura::Window* root_window = GetRootWindow();
  // Create |split_view_controller_| for every display.
  split_view_controller_ = std::make_unique<SplitViewController>(root_window);
  Shell* shell = Shell::Get();
  shell->InitRootWindow(root_window);
  auto old_targeter =
      root_window->SetEventTargeter(std::make_unique<RootWindowTargeter>());
  DCHECK(!old_targeter);

  std::unique_ptr<RootWindowLayoutManager> root_window_layout_manager =
      std::make_unique<RootWindowLayoutManager>(root_window);
  root_window_layout_manager_ = root_window_layout_manager.get();

  CreateContainers();

  InitLayoutManagers(std::move(root_window_layout_manager));
  InitTouchHuds();

  // `shelf_` was created in the constructor.
  shelf_->shelf_widget()->PostCreateShelf();

  color_provider_source_ = std::make_unique<AshColorProviderSource>();
  if (Shell::GetPrimaryRootWindowController()
          ->GetSystemModalLayoutManager(nullptr)
          ->has_window_dimmer()) {
    GetSystemModalLayoutManager(nullptr)->CreateModalBackground();
  }

  wallpaper_widget_controller_ =
      std::make_unique<WallpaperWidgetController>(root_window);

  wallpaper_widget_controller_->Init(
      shell->session_controller()->IsUserSessionBlocked());
  root_window_layout_manager_->OnWindowResized();

  CreateAmbientWidget();

  // Explicitly update the desks controller before notifying the ShellObservers.
  // This is to make sure the desks' states are correct before clients are
  // updated.
  shell->desks_controller()->OnRootWindowAdded(root_window);

  if (root_window_type == RootWindowType::PRIMARY) {
    shell->keyboard_controller()->RebuildKeyboardIfEnabled();
  } else {
    window_tree_host_->Show();

    // Notify shell observers about new root window.
    shell->OnRootWindowAdded(root_window);
  }

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshDisableTouchExplorationMode)) {
    touch_exploration_manager_ =
        std::make_unique<TouchExplorationManager>(this);
  }
}

void RootWindowController::InitLayoutManagers(
    std::unique_ptr<RootWindowLayoutManager> root_window_layout_manager) {
  // Create the shelf and status area widgets. Creates the ShelfLayoutManager
  // as a side-effect.
  DCHECK(!shelf_->shelf_widget());
  aura::Window* root = GetRootWindow();
  shelf_->CreateShelfWidget(root);

  root->SetLayoutManager(std::move(root_window_layout_manager));

  for (auto* container : desks_util::GetDesksContainers(root)) {
    // Installs WorkspaceLayoutManager on the container.
    SetWorkspaceController(container,
                           std::make_unique<WorkspaceController>(container));
  }

  aura::Window* modal_container =
      GetContainer(kShellWindowId_SystemModalContainer);
  modal_container->SetLayoutManager(
      std::make_unique<SystemModalContainerLayoutManager>(modal_container));

  aura::Window* lock_modal_container =
      GetContainer(kShellWindowId_LockSystemModalContainer);
  DCHECK(lock_modal_container);
  lock_modal_container->SetLayoutManager(
      std::make_unique<SystemModalContainerLayoutManager>(
          lock_modal_container));

  aura::Window* lock_action_handler_container =
      GetContainer(kShellWindowId_LockActionHandlerContainer);
  DCHECK(lock_action_handler_container);
  lock_screen_action_background_controller_->SetParentWindow(
      lock_action_handler_container);
  lock_action_handler_container->SetLayoutManager(
      std::make_unique<LockActionHandlerLayoutManager>(
          lock_action_handler_container,
          lock_screen_action_background_controller_.get()));

  aura::Window* lock_container =
      GetContainer(kShellWindowId_LockScreenContainer);
  DCHECK(lock_container);
  lock_container->SetLayoutManager(
      std::make_unique<LockLayoutManager>(lock_container));

  aura::Window* always_on_top_container =
      GetContainer(kShellWindowId_AlwaysOnTopContainer);
  aura::Window* pip_container = GetContainer(kShellWindowId_PipContainer);
  DCHECK(always_on_top_container);
  DCHECK(pip_container);
  always_on_top_controller_ = std::make_unique<AlwaysOnTopController>(
      always_on_top_container, pip_container);

  // Make it easier to resize windows that partially overlap the shelf. Must
  // occur after the ShelfLayoutManager is constructed by ShelfWidget.
  aura::Window* shelf_container = GetContainer(kShellWindowId_ShelfContainer);
  shelf_container->SetEventTargeter(
      std::make_unique<ShelfWindowTargeter>(shelf_container, shelf_.get()));
}

void RootWindowController::CreateContainers() {
  // CreateContainer() depends on root_window_layout_manager_.
  DCHECK(root_window_layout_manager_);

  aura::Window* root = GetRootWindow();

  // Add a NOT_DRAWN layer in between the root_window's layer and its current
  // children so that we only need to initiate two LayerAnimationSequences for
  // fullscreen animations such as the screen rotation animation and the desk
  // switch animation. Those animations take a screenshot of this container,
  // stack it ontop and animate the screenshot instead of the individual
  // elements.
  aura::Window* screen_rotation_container =
      CreateContainer(kShellWindowId_ScreenAnimationContainer,
                      "ScreenAnimationContainer", root);

  // Everything that needs to be included in the docked magnifier, when enabled,
  // should be a descendant of MagnifiedContainer. The DockedMagnifierContainer
  // should not be a descendant of this container, otherwise there would be a
  // cycle (docked magnifier trying to magnify itself).
  aura::Window* magnified_container =
      CreateContainer(kShellWindowId_MagnifiedContainer, "MagnifiedContainer",
                      screen_rotation_container);

  CreateContainer(kShellWindowId_DockedMagnifierContainer,
                  "DockedMagnifierContainer", screen_rotation_container);

  // These containers are just used by PowerButtonController to animate groups
  // of containers simultaneously without messing up the current transformations
  // on those containers. These are direct children of the magnified_container
  // window; all of the other containers are their children.

  // The wallpaper container is not part of the lock animation, so it is not
  // included in those animate groups. When the screen is locked, the wallpaper
  // is moved to the lock screen wallpaper container (and moved back on unlock).
  // Ensure that there's an opaque layer occluding the non-lock-screen layers.
  aura::Window* wallpaper_container =
      CreateContainer(kShellWindowId_WallpaperContainer, "WallpaperContainer",
                      magnified_container);
  ::wm::SetChildWindowVisibilityChangesAnimated(wallpaper_container);
  wallpaper_container->SetLayoutManager(
      std::make_unique<FillLayoutManager>(wallpaper_container));

  aura::Window* non_lock_screen_containers =
      CreateContainer(kShellWindowId_NonLockScreenContainersContainer,
                      "NonLockScreenContainersContainer", magnified_container);
  // Clip all windows inside this container, as half pixel of the window's
  // texture may become visible when the screen is scaled. crbug.com/368591.
  non_lock_screen_containers->layer()->SetMasksToBounds(true);

  aura::Window* lock_wallpaper_container =
      CreateContainer(kShellWindowId_LockScreenWallpaperContainer,
                      "LockScreenWallpaperContainer", magnified_container);
  ::wm::SetChildWindowVisibilityChangesAnimated(lock_wallpaper_container);
  lock_wallpaper_container->SetLayoutManager(
      std::make_unique<FillLayoutManager>(lock_wallpaper_container));

  aura::Window* lock_screen_containers =
      CreateContainer(kShellWindowId_LockScreenContainersContainer,
                      "LockScreenContainersContainer", magnified_container);
  aura::Window* lock_screen_related_containers = CreateContainer(
      kShellWindowId_LockScreenRelatedContainersContainer,
      "LockScreenRelatedContainersContainer", magnified_container);

  aura::Window* app_list_tablet_mode_container =
      CreateContainer(kShellWindowId_HomeScreenContainer, "HomeScreenContainer",
                      non_lock_screen_containers);
  app_list_tablet_mode_container->SetProperty(::wm::kUsesScreenCoordinatesKey,
                                              true);

  CreateContainer(kShellWindowId_UnparentedContainer, "UnparentedContainer",
                  non_lock_screen_containers);

  aura::Window* shutdown_screenshot_container = non_lock_screen_containers;
  if (features::IsForestFeatureEnabled()) {
    shutdown_screenshot_container = CreateContainer(
        kShellWindowId_ShutdownScreenshotContainer,
        "ShutdownScreenshotContainer", non_lock_screen_containers);
  }

  for (const auto& id : desks_util::GetDesksContainersIds()) {
    aura::Window* container =
        CreateContainer(id, desks_util::GetDeskContainerName(id),
                        shutdown_screenshot_container);
    ::wm::SetChildWindowVisibilityChangesAnimated(container);
    container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);
    container->SetProperty(kForceVisibleInMiniViewKey, true);
    window_util::SetChildrenUseExtendedHitRegionForWindow(container);

    // Hide the non-active containers.
    if (id != desks_util::GetActiveDeskContainerId()) {
      container->Hide();
    }
  }

  aura::Window* always_on_top_container =
      CreateContainer(kShellWindowId_AlwaysOnTopContainer,
                      "AlwaysOnTopContainer", shutdown_screenshot_container);
  ::wm::SetChildWindowVisibilityChangesAnimated(always_on_top_container);
  always_on_top_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);

  aura::Window* float_container =
      CreateContainer(kShellWindowId_FloatContainer, "FloatContainer",
                      shutdown_screenshot_container);
  wm::SetChildWindowVisibilityChangesAnimated(float_container);
  float_container->SetProperty(wm::kUsesScreenCoordinatesKey, true);
  window_util::SetChildrenUseExtendedHitRegionForWindow(float_container);

  aura::Window* app_list_container =
      CreateContainer(kShellWindowId_AppListContainer, "AppListContainer",
                      non_lock_screen_containers);
  app_list_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);

  aura::Window* pip_container = CreateContainer(
      kShellWindowId_PipContainer, "PipContainer", non_lock_screen_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(pip_container);
  pip_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);

  aura::Window* arc_ime_parent_container = CreateContainer(
      kShellWindowId_ArcImeWindowParentContainer, "ArcImeWindowParentContainer",
      non_lock_screen_containers);
  arc_ime_parent_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);
  arc_ime_parent_container->SetLayoutManager(
      std::make_unique<ArcVirtualKeyboardContainerLayoutManager>(
          arc_ime_parent_container));
  aura::Window* arc_vk_container =
      CreateContainer(kShellWindowId_ArcVirtualKeyboardContainer,
                      "ArcVirtualKeyboardContainer", arc_ime_parent_container);
  arc_vk_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);

  aura::Window* shelf_container_parent = lock_screen_related_containers;
  aura::Window* shelf_container = CreateContainer(
      kShellWindowId_ShelfContainer, "ShelfContainer", shelf_container_parent);
  shelf_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);
  shelf_container->SetProperty(kLockedToRootKey, true);

  aura::Window* shelf_bubble_container =
      CreateContainer(kShellWindowId_ShelfBubbleContainer,
                      "ShelfBubbleContainer", non_lock_screen_containers);
  shelf_bubble_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);
  shelf_bubble_container->SetProperty(kLockedToRootKey, true);

  aura::Window* modal_container =
      CreateContainer(kShellWindowId_SystemModalContainer,
                      "SystemModalContainer", non_lock_screen_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(modal_container);
  modal_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);
  window_util::SetChildrenUseExtendedHitRegionForWindow(modal_container);

  aura::Window* lock_container =
      CreateContainer(kShellWindowId_LockScreenContainer, "LockScreenContainer",
                      lock_screen_containers);
  lock_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);

  aura::Window* lock_action_handler_container =
      CreateContainer(kShellWindowId_LockActionHandlerContainer,
                      "LockActionHandlerContainer", lock_screen_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(lock_action_handler_container);
  lock_action_handler_container->SetProperty(::wm::kUsesScreenCoordinatesKey,
                                             true);

  aura::Window* lock_modal_container =
      CreateContainer(kShellWindowId_LockSystemModalContainer,
                      "LockSystemModalContainer", lock_screen_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(lock_modal_container);
  lock_modal_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);
  window_util::SetChildrenUseExtendedHitRegionForWindow(lock_modal_container);

  aura::Window* power_menu_container =
      CreateContainer(kShellWindowId_PowerMenuContainer, "PowerMenuContainer",
                      GetPowerMenuContainerParent(GetRootWindow()));
  power_menu_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);

  aura::Window* settings_bubble_container =
      CreateContainer(kShellWindowId_SettingBubbleContainer,
                      "SettingBubbleContainer", lock_screen_related_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(settings_bubble_container);
  settings_bubble_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);
  settings_bubble_container->SetProperty(kLockedToRootKey, true);

  aura::Window* live_caption_container =
      CreateContainer(kShellWindowId_LiveCaptionContainer,
                      "LiveCaptionContainer", lock_screen_related_containers);
  live_caption_container->SetProperty(wm::kUsesScreenCoordinatesKey, true);

  aura::Window* help_bubble_container =
      CreateContainer(kShellWindowId_HelpBubbleContainer, "HelpBubbleContainer",
                      lock_screen_related_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(help_bubble_container);
  help_bubble_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);

  aura::Window* virtual_keyboard_parent_container = CreateContainer(
      kShellWindowId_ImeWindowParentContainer, "ImeWindowParentContainer",
      lock_screen_related_containers);
  virtual_keyboard_parent_container->SetProperty(
      ::wm::kUsesScreenCoordinatesKey, true);
  virtual_keyboard_parent_container->SetLayoutManager(
      std::make_unique<VirtualKeyboardContainerLayoutManager>(
          virtual_keyboard_parent_container));
  aura::Window* virtual_keyboard_container = CreateContainer(
      kShellWindowId_VirtualKeyboardContainer, "VirtualKeyboardContainer",
      virtual_keyboard_parent_container);
  virtual_keyboard_container->SetProperty(::wm::kUsesScreenCoordinatesKey,
                                          true);
  virtual_keyboard_container->SetLayoutManager(
      std::make_unique<keyboard::KeyboardLayoutManager>(
          keyboard::KeyboardUIController::Get()));

  aura::Window* accessibility_panel_container = CreateContainer(
      kShellWindowId_AccessibilityPanelContainer, "AccessibilityPanelContainer",
      lock_screen_related_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(accessibility_panel_container);
  accessibility_panel_container->SetProperty(::wm::kUsesScreenCoordinatesKey,
                                             true);
  accessibility_panel_container->SetProperty(kLockedToRootKey, true);
  accessibility_panel_container->SetLayoutManager(
      std::make_unique<AccessibilityPanelLayoutManager>());

  aura::Window* menu_container =
      CreateContainer(kShellWindowId_MenuContainer, "MenuContainer",
                      lock_screen_related_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(menu_container);
  menu_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);

  aura::Window* accessibility_bubble_container = CreateContainer(
      kShellWindowId_AccessibilityBubbleContainer,
      "AccessibilityBubbleContainer", lock_screen_related_containers);
  accessibility_bubble_container->SetProperty(::wm::kUsesScreenCoordinatesKey,
                                              true);

  aura::Window* drag_drop_container = CreateContainer(
      kShellWindowId_DragImageAndTooltipContainer,
      "DragImageAndTooltipContainer", lock_screen_related_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(drag_drop_container);
  drag_drop_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);

  aura::Window* overlay_container =
      CreateContainer(kShellWindowId_OverlayContainer, "OverlayContainer",
                      lock_screen_related_containers);
  overlay_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);
  overlay_container->SetLayoutManager(
      std::make_unique<OverlayLayoutManager>(overlay_container));

  aura::Window* ambient_container =
      CreateContainer(kShellWindowId_AmbientModeContainer,
                      "AmbientModeContainer", lock_screen_related_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(ambient_container);
  ambient_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);
  ambient_container->SetLayoutManager(
      std::make_unique<FillLayoutManager>(ambient_container));

  aura::Window* mouse_cursor_container =
      CreateContainer(kShellWindowId_MouseCursorContainer,
                      "MouseCursorContainer", magnified_container);
  mouse_cursor_container->SetProperty(::wm::kUsesScreenCoordinatesKey, true);

  aura::Window* always_on_top_wallpaper_container =
      CreateContainer(kShellWindowId_AlwaysOnTopWallpaperContainer,
                      "AlwaysOnTopWallpaperContainer", magnified_container);
  always_on_top_wallpaper_container->SetLayoutManager(
      std::make_unique<FillLayoutManager>(always_on_top_wallpaper_container));

  CreateContainer(kShellWindowId_PowerButtonAnimationContainer,
                  "PowerButtonAnimationContainer", magnified_container);

  // Make sure booting animation container is always on top of all other
  // siblings under the `magnified_container`.
  if (ash::features::IsBootAnimationEnabled()) {
    aura::Window* booting_animation_container =
        CreateContainer(kShellWindowId_BootingAnimationContainer,
                        "BootingAnimationContainer", magnified_container);
    booting_animation_container->SetLayoutManager(
        std::make_unique<FillLayoutManager>(booting_animation_container));
  }
}

aura::Window* RootWindowController::CreateContainer(int window_id,
                                                    const char* name,
                                                    aura::Window* parent) {
  aura::Window* window =
      new aura::Window(nullptr, aura::client::WINDOW_TYPE_UNKNOWN);
  window->Init(ui::LAYER_NOT_DRAWN);
  window->SetId(window_id);
  window->SetName(name);
  parent->AddChild(window);
  if (window_id != kShellWindowId_UnparentedContainer) {
    window->Show();
  }
  root_window_layout_manager_->AddContainer(window);
  return window;
}

AccessibilityPanelLayoutManager*
RootWindowController::GetAccessibilityPanelLayoutManager() const {
  aura::Window* container = const_cast<aura::Window*>(
      GetContainer(kShellWindowId_AccessibilityPanelContainer));
  auto* layout_manager = static_cast<AccessibilityPanelLayoutManager*>(
      container->layout_manager());
  return layout_manager;
}

std::unique_ptr<AppMenuModelAdapter>
RootWindowController::BuildBirchMenuModelAdapter(
    ui::MenuSourceType source_type) {
  const bool is_birch_bar_showing =
      BirchBarController::Get()->GetShowBirchSuggestions();

  return std::make_unique<BirchBarMenuModelAdapter>(
      std::make_unique<BirchBarContextMenuModel>(
          BirchBarController::Get(),
          is_birch_bar_showing
              ? BirchBarContextMenuModel::Type::kExpandedBarMenu
              : BirchBarContextMenuModel::Type::kCollapsedBarMenu),
      wallpaper_widget_controller()->GetWidget(), source_type,
      base::BindOnce(&RootWindowController::OnMenuClosed,
                     base::Unretained(this)),
      display::Screen::GetScreen()->InTabletMode(), /*for_chip_menu=*/false);
}

std::unique_ptr<AppMenuModelAdapter>
RootWindowController::BuildShelfMenuModelAdapter(
    ui::MenuSourceType source_type) {
  const bool tablet_mode = display::Screen::GetScreen()->InTabletMode();
  const int64_t display_id = display::Screen::GetScreen()
                                 ->GetDisplayNearestWindow(GetRootWindow())
                                 .id();
  auto shelf_menu_model_adapter = std::make_unique<ShelfMenuModelAdapter>(
      std::make_unique<ShelfContextMenuModel>(nullptr, display_id,
                                              /*menu_in_shelf=*/false),
      wallpaper_widget_controller()->GetWidget(), source_type,
      base::BindOnce(&RootWindowController::OnMenuClosed,
                     base::Unretained(this)),
      tablet_mode);

  // Appends the apps sort options in ShelfContextMenuModel in tablet mode.
  // Note that the launcher UI is fullscreen in tablet mode, so the whole root
  // window can be perceived by users to be part of the launcher.
  auto* const app_list_controller = Shell::Get()->app_list_controller();
  if (tablet_mode && app_list_controller->IsVisible(display_id) &&
      app_list_controller->GetCurrentAppListPage() ==
          AppListState::kStateApps) {
    ui::SimpleMenuModel* menu_model = shelf_menu_model_adapter->model();
    sort_apps_submenu_ = std::make_unique<ui::SimpleMenuModel>(
        static_cast<ShelfContextMenuModel*>(menu_model));
    sort_apps_submenu_->AddItemWithIcon(
        REORDER_BY_NAME_ALPHABETICAL,
        l10n_util::GetStringUTF16(
            IDS_ASH_LAUNCHER_APPS_GRID_CONTEXT_MENU_REORDER_BY_NAME),
        ui::ImageModel::FromVectorIcon(kSortAlphabeticalIcon,
                                       ui::kColorAshSystemUIMenuIcon));
    sort_apps_submenu_->AddItemWithIcon(
        REORDER_BY_COLOR,
        l10n_util::GetStringUTF16(
            IDS_ASH_LAUNCHER_APPS_GRID_CONTEXT_MENU_REORDER_BY_COLOR),
        ui::ImageModel::FromVectorIcon(kSortColorIcon,
                                       ui::kColorAshSystemUIMenuIcon));
    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model->AddSubMenuWithIcon(
        REORDER_SUBMENU,
        l10n_util::GetStringUTF16(
            IDS_ASH_LAUNCHER_APPS_GRID_CONTEXT_MENU_REORDER_TITLE),
        sort_apps_submenu_.get(),
        ui::ImageModel::FromVectorIcon(kReorderIcon,
                                       ui::kColorAshSystemUIMenuIcon));

    // Append the "Show all suggestions" / "Hide all suggestions" item.
    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
    if (app_list_controller->ShouldHideContinueSection()) {
      menu_model->AddItemWithIcon(
          ShelfContextMenuModel::MENU_SHOW_CONTINUE_SECTION,
          l10n_util::GetStringUTF16(IDS_ASH_LAUNCHER_SHOW_CONTINUE_SECTION),
          ui::ImageModel::FromVectorIcon(kLauncherShowContinueSectionIcon,
                                         ui::kColorAshSystemUIMenuIcon));
    } else {
      menu_model->AddItemWithIcon(
          ShelfContextMenuModel::MENU_HIDE_CONTINUE_SECTION,
          l10n_util::GetStringUTF16(IDS_ASH_LAUNCHER_HIDE_CONTINUE_SECTION),
          ui::ImageModel::FromVectorIcon(kLauncherHideContinueSectionIcon,
                                         ui::kColorAshSystemUIMenuIcon));
    }
  }
  return shelf_menu_model_adapter;
}

void RootWindowController::OnMenuClosed() {
  root_window_menu_model_adapter_.reset();
  sort_apps_submenu_.reset();
  shelf_->UpdateVisibilityState();
}

}  // namespace ash
