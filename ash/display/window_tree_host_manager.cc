// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/window_tree_host_manager.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <utility>

#include "ash/display/cursor_window_controller.h"
#include "ash/display/mirror_window_controller.h"
#include "ash/display/root_window_transformers.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/host/root_window_transformer.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/shell.h"
#include "ash/shell_state.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/utility/transformer_util.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/class_property.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

// Primary display stored in global object as it can be
// accessed after Shell is deleted. A separate display instance is created
// during the shutdown instead of always keeping two display instances
// (one here and another one in display_manager) in sync, which is error prone.
// This is initialized in the constructor, and then in CreatePrimaryHost().
int64_t primary_display_id = -1;

// The default memory limit: 512mb.
const char kUICompositorDefaultMemoryLimitMB[] = "512";

display::DisplayManager* GetDisplayManager() {
  return Shell::Get()->display_manager();
}

void SetDisplayPropertiesOnHost(AshWindowTreeHost* ash_host,
                                const display::Display& display) {
  display::ManagedDisplayInfo info =
      GetDisplayManager()->GetDisplayInfo(display.id());
  const display::Display::Rotation effective_rotation =
      info.GetLogicalActiveRotation();
  aura::WindowTreeHost* host = ash_host->AsWindowTreeHost();
  ash_host->SetCursorConfig(display, effective_rotation);
  std::unique_ptr<RootWindowTransformer> transformer(
      CreateRootWindowTransformerForDisplay(display));
  ash_host->SetRootWindowTransformer(std::move(transformer));

  host->SetDisplayTransformHint(
      DisplayRotationToOverlayTransform(effective_rotation));

  // Just moving the display requires the full redraw.
  // chrome-os-partner:33558.
  host->compositor()->ScheduleFullRedraw();
}

void ClearDisplayPropertiesOnHost(AshWindowTreeHost* ash_host) {
  ash_host->ClearCursorConfig();
}

aura::Window* GetWindow(AshWindowTreeHost* ash_host) {
  CHECK(ash_host->AsWindowTreeHost());
  return ash_host->AsWindowTreeHost()->window();
}

}  // namespace

// A utility class to store/restore focused/active window
// when the display configuration has changed.
class FocusActivationStore {
 public:
  FocusActivationStore()
      : activation_client_(nullptr),
        capture_client_(nullptr),
        focus_client_(nullptr),
        focused_(nullptr),
        active_(nullptr) {}

  void Store(bool clear_focus) {
    if (!activation_client_) {
      aura::Window* root = Shell::GetPrimaryRootWindow();
      activation_client_ = ::wm::GetActivationClient(root);
      capture_client_ = aura::client::GetCaptureClient(root);
      focus_client_ = aura::client::GetFocusClient(root);
    }
    focused_ = focus_client_->GetFocusedWindow();
    if (focused_)
      tracker_.Add(focused_);
    active_ = activation_client_->GetActiveWindow();
    if (active_ && focused_ != active_)
      tracker_.Add(active_);

    // Deactivate the window to close menu / bubble windows. Deactivating by
    // setting active window to nullptr to avoid side effects of activating an
    // arbitrary window, such as covering |active_| before Restore().
    if (clear_focus && active_)
      activation_client_->ActivateWindow(nullptr);

    // Release capture if any.
    capture_client_->SetCapture(nullptr);

    // Clear the focused window if any. This is necessary because a
    // window may be deleted when losing focus (fullscreen flash for
    // example).  If the focused window is still alive after move, it'll
    // be re-focused below.
    if (clear_focus)
      focus_client_->FocusWindow(nullptr);
  }

  void Restore() {
    // Restore focused or active window if it's still alive.
    if (focused_ && tracker_.Contains(focused_)) {
      focus_client_->FocusWindow(focused_);
    } else if (active_ && tracker_.Contains(active_)) {
      activation_client_->ActivateWindow(active_);
    }
    if (focused_)
      tracker_.Remove(focused_);
    if (active_)
      tracker_.Remove(active_);
    focused_ = nullptr;
    active_ = nullptr;
  }

 private:
  ::wm::ActivationClient* activation_client_;
  aura::client::CaptureClient* capture_client_;
  aura::client::FocusClient* focus_client_;
  aura::WindowTracker tracker_;
  aura::Window* focused_;
  aura::Window* active_;

  DISALLOW_COPY_AND_ASSIGN(FocusActivationStore);
};

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostManager

WindowTreeHostManager::WindowTreeHostManager()
    : primary_tree_host_for_replace_(nullptr),
      focus_activation_store_(new FocusActivationStore()),
      cursor_window_controller_(new CursorWindowController()),
      mirror_window_controller_(new MirrorWindowController()),
      cursor_display_id_for_restore_(display::kInvalidDisplayId) {
  // Reset primary display to make sure that tests don't use
  // stale display info from previous tests.
  primary_display_id = display::kInvalidDisplayId;
}

WindowTreeHostManager::~WindowTreeHostManager() = default;

void WindowTreeHostManager::Start() {
  display::Screen::GetScreen()->AddObserver(this);
  Shell::Get()
      ->display_configurator()
      ->content_protection_manager()
      ->AddObserver(this);
  Shell::Get()->display_manager()->set_delegate(this);
}

void WindowTreeHostManager::Shutdown() {
  for (auto& observer : observers_)
    observer.OnWindowTreeHostManagerShutdown();

  // Unset the display manager's delegate here because
  // DisplayManager outlives WindowTreeHostManager.
  Shell::Get()->display_manager()->set_delegate(nullptr);

  cursor_window_controller_.reset();
  mirror_window_controller_.reset();

  Shell::Get()
      ->display_configurator()
      ->content_protection_manager()
      ->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);

  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();

  // Delete non primary root window controllers first, then
  // delete the primary root window controller.
  aura::Window::Windows root_windows =
      WindowTreeHostManager::GetAllRootWindows();
  std::vector<RootWindowController*> to_delete;
  RootWindowController* primary_rwc = nullptr;
  for (aura::Window::Windows::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    RootWindowController* rwc = RootWindowController::ForWindow(*iter);
    if (GetRootWindowSettings(*iter)->display_id == primary_id)
      primary_rwc = rwc;
    else
      to_delete.push_back(rwc);
  }
  CHECK(primary_rwc);

  Shell::Get()->shell_state()->SetRootWindowForNewWindows(nullptr);
  for (auto* rwc : to_delete)
    delete rwc;
  delete primary_rwc;
}

void WindowTreeHostManager::CreatePrimaryHost(
    const AshWindowTreeHostInitParams& init_params) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(
          switches::kUiCompositorMemoryLimitWhenVisibleMB)) {
    command_line->AppendSwitchASCII(
        switches::kUiCompositorMemoryLimitWhenVisibleMB,
        kUICompositorDefaultMemoryLimitMB);
  }

  const display::Display& primary_candidate =
      GetDisplayManager()->GetPrimaryDisplayCandidate();
  primary_display_id = primary_candidate.id();
  CHECK_NE(display::kInvalidDisplayId, primary_display_id);
  AddWindowTreeHostForDisplay(primary_candidate, init_params);
}

void WindowTreeHostManager::InitHosts() {
  RootWindowController::CreateForPrimaryDisplay(
      window_tree_hosts_[primary_display_id]);
  display::DisplayManager* display_manager = GetDisplayManager();
  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    const display::Display& display = display_manager->GetDisplayAt(i);
    if (primary_display_id != display.id()) {
      AshWindowTreeHost* ash_host =
          AddWindowTreeHostForDisplay(display, AshWindowTreeHostInitParams());
      RootWindowController::CreateForSecondaryDisplay(ash_host);
    }
  }

  // Record display zoom for the primary display for https://crbug.com/955071.
  // This can be removed after M79.
  const display::ManagedDisplayInfo& display_info =
      display_manager->GetDisplayInfo(primary_display_id);
  int zoom_percent = std::round(display_info.zoom_factor() * 100);
  constexpr int kMaxValue = 300;
  constexpr int kBucketSize = 5;
  constexpr int kBucketCount = kMaxValue / kBucketSize + 1;
  base::LinearHistogram::FactoryGet(
      "Ash.Display.PrimaryDisplayZoomAtStartup", kBucketSize, kMaxValue,
      kBucketCount, base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(zoom_percent);

  for (auto& observer : observers_)
    observer.OnDisplaysInitialized();
}

void WindowTreeHostManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void WindowTreeHostManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
int64_t WindowTreeHostManager::GetPrimaryDisplayId() {
  CHECK_NE(display::kInvalidDisplayId, primary_display_id);
  return primary_display_id;
}

// static
bool WindowTreeHostManager::HasValidPrimaryDisplayId() {
  return primary_display_id != display::kInvalidDisplayId;
}

aura::Window* WindowTreeHostManager::GetPrimaryRootWindow() {
  // If |primary_tree_host_for_replace_| is set, it means |primary_display_id|
  // is kInvalidDisplayId.
  if (primary_tree_host_for_replace_)
    return GetWindow(primary_tree_host_for_replace_);
  return GetRootWindowForDisplayId(primary_display_id);
}

aura::Window* WindowTreeHostManager::GetRootWindowForDisplayId(int64_t id) {
  AshWindowTreeHost* host = GetAshWindowTreeHostForDisplayId(id);
  return host ? GetWindow(host) : nullptr;
}

AshWindowTreeHost* WindowTreeHostManager::GetAshWindowTreeHostForDisplayId(
    int64_t display_id) {
  const auto host = window_tree_hosts_.find(display_id);
  if (host != window_tree_hosts_.end())
    return host->second;
  return mirror_window_controller_->GetAshWindowTreeHostForDisplayId(
      display_id);
}

aura::Window::Windows WindowTreeHostManager::GetAllRootWindows() {
  aura::Window::Windows windows;
  for (WindowTreeHostMap::const_iterator it = window_tree_hosts_.begin();
       it != window_tree_hosts_.end(); ++it) {
    DCHECK(it->second);
    if (RootWindowController::ForWindow(GetWindow(it->second)))
      windows.push_back(GetWindow(it->second));
  }
  return windows;
}

gfx::Insets WindowTreeHostManager::GetOverscanInsets(int64_t display_id) const {
  return GetDisplayManager()->GetOverscanInsets(display_id);
}

void WindowTreeHostManager::SetOverscanInsets(
    int64_t display_id,
    const gfx::Insets& insets_in_dip) {
  GetDisplayManager()->SetOverscanInsets(display_id, insets_in_dip);
}

std::vector<RootWindowController*>
WindowTreeHostManager::GetAllRootWindowControllers() {
  std::vector<RootWindowController*> controllers;
  for (WindowTreeHostMap::const_iterator it = window_tree_hosts_.begin();
       it != window_tree_hosts_.end(); ++it) {
    RootWindowController* controller =
        RootWindowController::ForWindow(GetWindow(it->second));
    if (controller)
      controllers.push_back(controller);
  }
  return controllers;
}

void WindowTreeHostManager::UpdateMouseLocationAfterDisplayChange() {
  // If the mouse is currently on a display in native location,
  // use the same native location. Otherwise find the display closest
  // to the current cursor location in screen coordinates.

  gfx::Point point_in_screen =
      display::Screen::GetScreen()->GetCursorScreenPoint();
  gfx::Point target_location_in_native;
  int64_t closest_distance_squared = -1;
  display::DisplayManager* display_manager = GetDisplayManager();

  aura::Window* dst_root_window = nullptr;
  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    const display::Display& display = display_manager->GetDisplayAt(i);
    const display::ManagedDisplayInfo display_info =
        display_manager->GetDisplayInfo(display.id());
    aura::Window* root_window = GetRootWindowForDisplayId(display.id());
    if (display_info.bounds_in_native().Contains(
            cursor_location_in_native_coords_for_restore_)) {
      dst_root_window = root_window;
      target_location_in_native = cursor_location_in_native_coords_for_restore_;
      break;
    }
    gfx::Point center = display.bounds().CenterPoint();
    // Use the distance squared from the center of the dislay. This is not
    // exactly "closest" display, but good enough to pick one
    // appropriate (and there are at most two displays).
    // We don't care about actual distance, only relative to other displays, so
    // using the LengthSquared() is cheaper than Length().

    int64_t distance_squared = (center - point_in_screen).LengthSquared();
    if (closest_distance_squared < 0 ||
        closest_distance_squared > distance_squared) {
      aura::Window* root_window = GetRootWindowForDisplayId(display.id());
      ::wm::ConvertPointFromScreen(root_window, &center);
      root_window->GetHost()->ConvertDIPToScreenInPixels(&center);
      dst_root_window = root_window;
      target_location_in_native = center;
      closest_distance_squared = distance_squared;
    }
  }

  gfx::Point target_location_in_root = target_location_in_native;
  dst_root_window->GetHost()->ConvertScreenInPixelsToDIP(
      &target_location_in_root);

  gfx::Point target_location_in_screen = target_location_in_root;
  ::wm::ConvertPointToScreen(dst_root_window, &target_location_in_screen);
  const display::Display& target_display =
      display_manager->FindDisplayContainingPoint(target_location_in_screen);
  // If the original location isn't on any of new display, let ozone move
  // the cursor.
  if (!target_display.is_valid())
    return;
  int64_t target_display_id = target_display.id();

  // Do not move the cursor if the cursor's location did not change. This avoids
  // moving (and showing) the cursor:
  // - At startup.
  // - When the device is rotated in tablet mode.
  // |cursor_display_id_for_restore_| is checked to ensure that the cursor is
  // moved when the cursor's native position does not change but the display
  // that it is on has changed. This occurs when swapping the primary display.
  if (target_location_in_native !=
          cursor_location_in_native_coords_for_restore_ ||
      target_display_id != cursor_display_id_for_restore_) {
    if (Shell::Get()->cursor_manager()) {
      if (Shell::Get()->cursor_manager()->IsCursorVisible()) {
        dst_root_window->MoveCursorTo(target_location_in_root);
      } else if (target_display_id != cursor_display_id_for_restore_) {
        Shell::Get()->cursor_manager()->SetDisplay(target_display);
      }
    }

  } else if (target_location_in_screen !=
             cursor_location_in_screen_coords_for_restore_) {
    // The cursor's native position did not change but its screen position did
    // change. This occurs when the scale factor or the rotation of the display
    // that the cursor is on changes.
    // TODO: conditional should not be necessary. http://crbug.com/631103.
    if (Shell::Get()->cursor_manager())
      Shell::Get()->cursor_manager()->SetDisplay(target_display);

    // Update the cursor's root location. This ends up dispatching a synthetic
    // mouse move. The synthetic mouse move updates the composited cursor's
    // location and hover effects. Synthetic mouse moves do not affect the
    // cursor's visibility.
    dst_root_window->GetHost()->dispatcher()->OnCursorMovedToRootLocation(
        target_location_in_root);
  }
}

bool WindowTreeHostManager::UpdateWorkAreaOfDisplayNearestWindow(
    const aura::Window* window,
    const gfx::Insets& insets) {
  const aura::Window* root_window = window->GetRootWindow();
  int64_t id = GetRootWindowSettings(root_window)->display_id;
  // if id is |kInvaildDisplayID|, it's being deleted.
  DCHECK(id != display::kInvalidDisplayId);
  return GetDisplayManager()->UpdateWorkAreaOfDisplay(id, insets);
}

void WindowTreeHostManager::OnDisplayAdded(const display::Display& display) {
  // If we're switching from/to offscreen WTH, we need to
  // create new WTH for primary display instead of reusing.
  if (primary_tree_host_for_replace_ &&
      (GetRootWindowSettings(GetWindow(primary_tree_host_for_replace_))
               ->display_id == display::kUnifiedDisplayId ||
       display.id() == display::kUnifiedDisplayId)) {
    DCHECK_EQ(display::kInvalidDisplayId, primary_display_id);
    primary_display_id = display.id();

    AshWindowTreeHost* ash_host =
        AddWindowTreeHostForDisplay(display, AshWindowTreeHostInitParams());
    RootWindowController* new_root_window_controller =
        RootWindowController::CreateForSecondaryDisplay(ash_host);

    // Magnifier controllers keep pointers to the current root window.
    // Update them here to avoid accessing them later.
    Shell::Get()->magnification_controller()->SwitchTargetRootWindow(
        ash_host->AsWindowTreeHost()->window(), false);
    Shell::Get()
        ->partial_magnification_controller()
        ->SwitchTargetRootWindowIfNeeded(
            ash_host->AsWindowTreeHost()->window());

    AshWindowTreeHost* to_delete = primary_tree_host_for_replace_;

    // Show the shelf if the original WTH had a visible system
    // tray. It may or may not be visible depending on OOBE state.
    RootWindowController* old_root_window_controller =
        RootWindowController::ForWindow(
            to_delete->AsWindowTreeHost()->window());
    TrayBackgroundView* old_tray =
        old_root_window_controller->GetStatusAreaWidget()
            ->unified_system_tray();
    TrayBackgroundView* new_tray =
        new_root_window_controller->GetStatusAreaWidget()
            ->unified_system_tray();
    if (old_tray->GetWidget()->IsVisible()) {
      new_tray->SetVisiblePreferred(true);
      new_tray->GetWidget()->Show();
    }

    // |to_delete| has already been removed from |window_tree_hosts_|.
    DCHECK(std::none_of(
        window_tree_hosts_.cbegin(), window_tree_hosts_.cend(),
        [to_delete](const std::pair<int64_t, AshWindowTreeHost*>& pair) {
          return pair.second == to_delete;
        }));

    DeleteHost(to_delete);
    DCHECK(!primary_tree_host_for_replace_);
  } else if (primary_tree_host_for_replace_) {
    // TODO(oshima): It should be possible to consolidate logic for
    // unified and non unified, but I'm keeping them separated to minimize
    // the risk in M44. I'll consolidate this in M45.
    DCHECK(window_tree_hosts_.empty());
    AshWindowTreeHost* ash_host = primary_tree_host_for_replace_;
    primary_tree_host_for_replace_ = nullptr;
    primary_display_id = display.id();
    window_tree_hosts_[display.id()] = ash_host;
    GetRootWindowSettings(GetWindow(ash_host))->display_id = display.id();
    const display::ManagedDisplayInfo& display_info =
        GetDisplayManager()->GetDisplayInfo(display.id());
    ash_host->AsWindowTreeHost()->SetBoundsInPixels(
        display_info.bounds_in_native());
    SetDisplayPropertiesOnHost(ash_host, display);
  } else {
    if (primary_display_id == display::kInvalidDisplayId)
      primary_display_id = display.id();
    DCHECK(!window_tree_hosts_.empty());
    AshWindowTreeHost* ash_host =
        AddWindowTreeHostForDisplay(display, AshWindowTreeHostInitParams());
    RootWindowController::CreateForSecondaryDisplay(ash_host);
  }
}

void WindowTreeHostManager::DeleteHost(AshWindowTreeHost* host_to_delete) {
  ClearDisplayPropertiesOnHost(host_to_delete);
  aura::Window* root_being_deleted = GetWindow(host_to_delete);
  RootWindowController* controller =
      RootWindowController::ForWindow(root_being_deleted);
  DCHECK(controller);
  aura::Window* primary_root_after_host_deletion =
      GetRootWindowForDisplayId(GetPrimaryDisplayId());
  controller->MoveWindowsTo(primary_root_after_host_deletion);
  // Delete most of root window related objects, but don't delete
  // root window itself yet because the stack may be using it.
  controller->Shutdown();
  if (primary_tree_host_for_replace_ == host_to_delete)
    primary_tree_host_for_replace_ = nullptr;
  DCHECK_EQ(primary_root_after_host_deletion, Shell::GetPrimaryRootWindow());
  if (Shell::GetRootWindowForNewWindows() == root_being_deleted) {
    Shell::Get()->shell_state()->SetRootWindowForNewWindows(
        primary_root_after_host_deletion);
  }
  // NOTE: ShelfWidget is gone, but Shelf still exists until this task runs.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, controller);
}

void WindowTreeHostManager::OnDisplayRemoved(const display::Display& display) {
  AshWindowTreeHost* host_to_delete = window_tree_hosts_[display.id()];
  CHECK(host_to_delete) << display.ToString();

  // When the primary root window's display is removed, move the primary
  // root to the other display.
  if (primary_display_id == display.id()) {
    // Temporarily store the primary root window in
    // |primary_root_window_for_replace_| when replacing the display.
    if (window_tree_hosts_.size() == 1) {
      primary_display_id = display::kInvalidDisplayId;
      primary_tree_host_for_replace_ = host_to_delete;
      // Display for root window will be deleted when the Primary RootWindow
      // is deleted by the Shell.
      window_tree_hosts_.erase(display.id());
      return;
    }
    for (const auto& pair : window_tree_hosts_) {
      if (pair.first != display.id()) {
        primary_display_id = pair.first;
        break;
      }
    }
    CHECK_NE(display::kInvalidDisplayId, primary_display_id);

    AshWindowTreeHost* primary_host = host_to_delete;
    // Delete the other host instead.
    host_to_delete = window_tree_hosts_[primary_display_id];
    GetRootWindowSettings(GetWindow(host_to_delete))->display_id = display.id();

    // Setup primary root.
    window_tree_hosts_[primary_display_id] = primary_host;
    GetRootWindowSettings(GetWindow(primary_host))->display_id =
        primary_display_id;

    OnDisplayMetricsChanged(
        GetDisplayManager()->GetDisplayForId(primary_display_id),
        DISPLAY_METRIC_BOUNDS);
  }

  DeleteHost(host_to_delete);

  // The window tree host should be erased at last because some handlers can
  // access to the host through GetRootWindowForDisplayId() during
  // MoveWindowsTo(). See http://crbug.com/415222
  window_tree_hosts_.erase(display.id());
}

void WindowTreeHostManager::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  if (!(metrics & (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_ROTATION |
                   DISPLAY_METRIC_DEVICE_SCALE_FACTOR))) {
    return;
  }

  const display::ManagedDisplayInfo& display_info =
      GetDisplayManager()->GetDisplayInfo(display.id());
  DCHECK(!display_info.bounds_in_native().IsEmpty());
  AshWindowTreeHost* ash_host = window_tree_hosts_[display.id()];
  ash_host->AsWindowTreeHost()->SetBoundsInPixels(
      display_info.bounds_in_native());
  SetDisplayPropertiesOnHost(ash_host, display);
}

void WindowTreeHostManager::OnHostResized(aura::WindowTreeHost* host) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(host->window());

  display::DisplayManager* display_manager = GetDisplayManager();
  if (display_manager->UpdateDisplayBounds(display.id(),
                                           host->GetBoundsInPixels())) {
    mirror_window_controller_->UpdateWindow();
    cursor_window_controller_->UpdateContainer();
  }
}

void WindowTreeHostManager::OnDisplaySecurityChanged(int64_t display_id,
                                                     bool secure) {
  AshWindowTreeHost* host = GetAshWindowTreeHostForDisplayId(display_id);
  // No host for internal display in docked mode.
  if (!host)
    return;

  ui::Compositor* compositor = host->AsWindowTreeHost()->compositor();
  compositor->SetOutputIsSecure(secure);
  compositor->ScheduleFullRedraw();
}

void WindowTreeHostManager::CreateOrUpdateMirroringDisplay(
    const display::DisplayInfoList& info_list) {
  if (GetDisplayManager()->IsInMirrorMode() ||
      GetDisplayManager()->IsInUnifiedMode()) {
    mirror_window_controller_->UpdateWindow(info_list);
    cursor_window_controller_->UpdateContainer();
  } else {
    NOTREACHED();
  }
}

void WindowTreeHostManager::CloseMirroringDisplayIfNotNecessary() {
  mirror_window_controller_->CloseIfNotNecessary();
  // If cursor_compositing is enabled for large cursor, the cursor window is
  // always on the desktop display (the visible cursor on the non-desktop
  // display is drawn through compositor mirroring). Therefore, it's unnecessary
  // to handle the cursor_window at all. See: http://crbug.com/412910
  if (!cursor_window_controller_->is_cursor_compositing_enabled())
    cursor_window_controller_->UpdateContainer();
}

void WindowTreeHostManager::PreDisplayConfigurationChange(bool clear_focus) {
  for (auto& observer : observers_)
    observer.OnDisplayConfigurationChanging();
  focus_activation_store_->Store(clear_focus);
  display::Screen* screen = display::Screen::GetScreen();
  gfx::Point point_in_screen = screen->GetCursorScreenPoint();
  cursor_location_in_screen_coords_for_restore_ = point_in_screen;

  display::Display display = screen->GetDisplayNearestPoint(point_in_screen);
  cursor_display_id_for_restore_ = display.id();

  gfx::Point point_in_native = point_in_screen;
  aura::Window* root_window = GetRootWindowForDisplayId(display.id());
  ::wm::ConvertPointFromScreen(root_window, &point_in_native);
  root_window->GetHost()->ConvertDIPToScreenInPixels(&point_in_native);
  cursor_location_in_native_coords_for_restore_ = point_in_native;
}

void WindowTreeHostManager::SetPrimaryDisplayId(int64_t id) {
  // TODO(oshima): Move primary display management to DisplayManager.
  DCHECK_NE(display::kInvalidDisplayId, id);
  if (id == display::kInvalidDisplayId || primary_display_id == id ||
      window_tree_hosts_.size() < 2) {
    return;
  }

  const display::Display& new_primary_display =
      GetDisplayManager()->GetDisplayForId(id);
  if (!new_primary_display.is_valid()) {
    LOG(ERROR) << "Invalid or non-existent display is requested:"
               << new_primary_display.ToString();
    return;
  }

  display::DisplayManager* display_manager = GetDisplayManager();
  DCHECK(new_primary_display.is_valid());
  DCHECK(display_manager->GetDisplayForId(new_primary_display.id()).is_valid());

  AshWindowTreeHost* non_primary_host =
      window_tree_hosts_[new_primary_display.id()];
  LOG_IF(ERROR, !non_primary_host)
      << "Unknown display is requested in SetPrimaryDisplay: id="
      << new_primary_display.id();
  if (!non_primary_host)
    return;

  display::Display old_primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  DCHECK_EQ(old_primary_display.id(), primary_display_id);

  // Swap root windows between current and new primary display.
  AshWindowTreeHost* primary_host = window_tree_hosts_[primary_display_id];
  CHECK(primary_host);
  CHECK_NE(primary_host, non_primary_host);

  aura::Window* primary_window = GetWindow(primary_host);
  aura::Window* non_primary_window = GetWindow(non_primary_host);
  window_tree_hosts_[new_primary_display.id()] = primary_host;
  GetRootWindowSettings(primary_window)->display_id = new_primary_display.id();

  window_tree_hosts_[old_primary_display.id()] = non_primary_host;
  GetRootWindowSettings(non_primary_window)->display_id =
      old_primary_display.id();

  base::string16 old_primary_title = primary_window->GetTitle();
  primary_window->SetTitle(non_primary_window->GetTitle());
  non_primary_window->SetTitle(old_primary_title);

  const display::DisplayLayout& layout =
      GetDisplayManager()->GetCurrentDisplayLayout();
  // The requested primary id can be same as one in the stored layout
  // when the primary id is set after new displays are connected.
  // Only update the layout if it is requested to swap primary display.
  if (layout.primary_id != new_primary_display.id()) {
    std::unique_ptr<display::DisplayLayout> swapped_layout = layout.Copy();
    swapped_layout->SwapPrimaryDisplay(new_primary_display.id());
    display::DisplayIdList list = display_manager->GetCurrentDisplayIdList();
    GetDisplayManager()->layout_store()->RegisterLayoutForDisplayIdList(
        list, std::move(swapped_layout));
  }

  primary_display_id = new_primary_display.id();

  UpdateWorkAreaOfDisplayNearestWindow(GetWindow(primary_host),
                                       old_primary_display.GetWorkAreaInsets());
  UpdateWorkAreaOfDisplayNearestWindow(GetWindow(non_primary_host),
                                       new_primary_display.GetWorkAreaInsets());

  // Update the dispay manager with new display info.
  GetDisplayManager()->set_force_bounds_changed(true);
  GetDisplayManager()->UpdateDisplays();
  GetDisplayManager()->set_force_bounds_changed(false);
}

void WindowTreeHostManager::PostDisplayConfigurationChange() {
  focus_activation_store_->Restore();

  for (auto& observer : observers_)
    observer.OnDisplayConfigurationChanged();
  UpdateMouseLocationAfterDisplayChange();

  // Enable cursor compositing, so that cursor could be mirrored to destination
  // displays along with other display content through reflector.
  Shell::Get()->UpdateCursorCompositingEnabled();
}

ui::EventDispatchDetails WindowTreeHostManager::DispatchKeyEventPostIME(
    ui::KeyEvent* event) {
  aura::Window* root_window = nullptr;
  if (event->target()) {
    root_window = static_cast<aura::Window*>(event->target())->GetRootWindow();
    DCHECK(root_window);
  } else {
    // Getting the active root window to dispatch the event. This isn't
    // significant as the event will be sent to the window resolved by
    // aura::client::FocusClient which is FocusController in ash.
    aura::Window* active_window = window_util::GetActiveWindow();
    root_window = active_window ? active_window->GetRootWindow()
                                : Shell::GetPrimaryRootWindow();
  }
  return root_window->GetHost()->DispatchKeyEventPostIME(event);
}

AshWindowTreeHost* WindowTreeHostManager::AddWindowTreeHostForDisplay(
    const display::Display& display,
    const AshWindowTreeHostInitParams& init_params) {
  static int host_count = 0;
  const display::ManagedDisplayInfo& display_info =
      GetDisplayManager()->GetDisplayInfo(display.id());
  AshWindowTreeHostInitParams params_with_bounds(init_params);
  params_with_bounds.initial_bounds = display_info.bounds_in_native();
  if (display.id() == display::kUnifiedDisplayId) {
    params_with_bounds.offscreen = true;
    params_with_bounds.mirroring_delegate = mirror_window_controller();
  }
  params_with_bounds.display_id = display.id();
  params_with_bounds.device_scale_factor = display.device_scale_factor();
  // The AshWindowTreeHost ends up owned by the RootWindowControllers created
  // by this class.
  AshWindowTreeHost* ash_host =
      AshWindowTreeHost::Create(params_with_bounds).release();
  aura::WindowTreeHost* host = ash_host->AsWindowTreeHost();
  DCHECK(!host->has_input_method());
  if (!input_method_) {  // Singleton input method instance for Ash.
    input_method_ = ui::CreateInputMethod(this, host->GetAcceleratedWidget());
    // Makes sure the input method is focused by default when created, because
    // Ash uses singleton InputMethod and it won't call OnFocus/OnBlur when
    // the active window changed.
    input_method_->OnFocus();
  }
  host->SetSharedInputMethod(input_method_.get());

  host->window()->SetName(base::StringPrintf(
      "%sRootWindow-%d", params_with_bounds.offscreen ? "Offscreen" : "",
      host_count++));
  host->window()->SetTitle(base::UTF8ToUTF16(display_info.name()));
  host->compositor()->SetBackgroundColor(SK_ColorBLACK);
  // No need to remove our observer observer because the WindowTreeHostManager
  // outlives the host.
  host->AddObserver(this);
  InitRootWindowSettings(host->window())->display_id = display.id();
  host->InitHost();
  host->window()->Show();

  window_tree_hosts_[display.id()] = ash_host;
  SetDisplayPropertiesOnHost(ash_host, display);

  ash_host->ConfineCursorToRootWindow();

  return ash_host;
}

}  // namespace ash
