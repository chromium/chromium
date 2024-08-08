// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_modal_container_layout_manager.h"

#include <cmath>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/window_dimmer.h"
#include "ash/wm/window_util.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

// The center point of the window can diverge this much from the center point
// of the container to be kept centered upon resizing operations.
const int kCenterPixelDelta = 32;

ui::mojom::ModalType GetModalType(aura::Window* window) {
  return window->GetProperty(aura::client::kModalKey);
}

bool HasTransientAncestor(const aura::Window* window,
                          const aura::Window* ancestor) {
  const aura::Window* transient_parent = ::wm::GetTransientParent(window);
  if (transient_parent == ancestor)
    return true;
  return transient_parent ? HasTransientAncestor(transient_parent, ancestor)
                          : false;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// SystemModalContainerLayoutManager, public:

SystemModalContainerLayoutManager::SystemModalContainerLayoutManager(
    aura::Window* container)
    : container_(container) {}

SystemModalContainerLayoutManager::~SystemModalContainerLayoutManager() {
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  if (keyboard_controller->HasObserver(this))
    keyboard_controller->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// SystemModalContainerLayoutManager, aura::LayoutManager implementation:

void SystemModalContainerLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  if (GetModalType(window) != ui::mojom::ModalType::kSystem) {
    return;
  }

  if (window->IsVisible()) {
    DCHECK(!base::Contains(modal_windows_, window));
    AddModalWindow(window);
  } else {
    if (RemoveModalWindow(window))
      OnModalWindowRemoved(window);
  }
}

void SystemModalContainerLayoutManager::OnWindowResized() {
  PositionDialogsAfterWorkAreaResize();
}

void SystemModalContainerLayoutManager::OnWindowAddedToLayout(
    aura::Window* child) {
  DCHECK(child->GetType() == aura::client::WINDOW_TYPE_NORMAL ||
         child->GetType() == aura::client::WINDOW_TYPE_POPUP);
  DCHECK(container_->GetId() != kShellWindowId_LockSystemModalContainer ||
         Shell::Get()->session_controller()->IsUserSessionBlocked());
  // Since this is for SystemModal, there is no good reason to add windows
  // other than ModalType::kNone or ModalType::kSystem. DCHECK to avoid
  // mistakes.
  DCHECK_NE(GetModalType(child), ui::mojom::ModalType::kChild);
  DCHECK_NE(GetModalType(child), ui::mojom::ModalType::kWindow);

  child->AddObserver(this);
  if (GetModalType(child) == ui::mojom::ModalType::kSystem &&
      child->IsVisible()) {
    AddModalWindow(child);
  }
}

void SystemModalContainerLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
  StopObservingWindow(child);
}

void SystemModalContainerLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  WmDefaultLayoutManager::SetChildBounds(child, requested_bounds);
  if (IsBoundsCentered(requested_bounds))
    windows_to_center_.insert(child);
  else
    windows_to_center_.erase(child);
}

////////////////////////////////////////////////////////////////////////////////
// SystemModalContainerLayoutManager, aura::WindowObserver implementation:

void SystemModalContainerLayoutManager::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (key != aura::client::kModalKey || !window->IsVisible())
    return;

  if (window->GetProperty(aura::client::kModalKey) ==
      ui::mojom::ModalType::kSystem) {
    if (base::Contains(modal_windows_, window))
      return;
    AddModalWindow(window);
  } else {
    if (RemoveModalWindow(window))
      OnModalWindowRemoved(window);
  }
}

void SystemModalContainerLayoutManager::OnWindowDestroying(
    aura::Window* window) {
  StopObservingWindow(window);
}

////////////////////////////////////////////////////////////////////////////////
// SystemModalContainerLayoutManager, Keyboard::KeyboardControllerObserver
// implementation:

void SystemModalContainerLayoutManager::OnKeyboardOccludedBoundsChanged(
    const gfx::Rect& new_bounds_in_screen) {
  PositionDialogsAfterWorkAreaResize();
}

bool SystemModalContainerLayoutManager::IsPartOfActiveModalWindow(
    aura::Window* window) {
  return modal_window() &&
         (modal_window()->Contains(window) ||
          HasTransientAncestor(::wm::GetToplevelWindow(window),
                               modal_window()));
}

bool SystemModalContainerLayoutManager::ActivateNextModalWindow() {
  if (modal_windows_.empty())
    return false;
  wm::ActivateWindow(modal_window());
  return true;
}

void SystemModalContainerLayoutManager::CreateModalBackground() {
  if (!window_dimmer_) {
    window_dimmer_ = std::make_unique<WindowDimmer>(container_);
    window_dimmer_->window()->SetName(
        "SystemModalContainerLayoutManager.ModalBackground");
    // The keyboard isn't always enabled.
    if (keyboard::KeyboardUIController::Get()->IsEnabled())
      keyboard::KeyboardUIController::Get()->AddObserver(this);
  }
  window_dimmer_->window()->Show();
}

void SystemModalContainerLayoutManager::DestroyModalBackground() {
  if (!window_dimmer_)
    return;

  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  if (keyboard_controller->HasObserver(this))
    keyboard_controller->RemoveObserver(this);
  window_dimmer_.reset();
}

// static
bool SystemModalContainerLayoutManager::IsModalBackground(
    aura::Window* window) {
  int id = window->parent()->GetId();
  if (id != kShellWindowId_SystemModalContainer &&
      id != kShellWindowId_LockSystemModalContainer)
    return false;
  SystemModalContainerLayoutManager* layout_manager =
      static_cast<SystemModalContainerLayoutManager*>(
          window->parent()->layout_manager());
  return layout_manager->window_dimmer_ &&
         layout_manager->window_dimmer_->window() == window;
}

void SystemModalContainerLayoutManager::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (display::Screen::GetScreen()->GetDisplayNearestWindow(container_).id() !=
      display.id()) {
    return;
  }

  if (changed_metrics & (display::DisplayObserver::DISPLAY_METRIC_BOUNDS |
                         display::DisplayObserver::DISPLAY_METRIC_WORK_AREA)) {
    PositionDialogsAfterWorkAreaResize();
  }
}

////////////////////////////////////////////////////////////////////////////////
// SystemModalContainerLayoutManager, private:

void SystemModalContainerLayoutManager::AddModalWindow(aura::Window* window) {
  if (modal_windows_.empty()) {
    aura::Window* capture_window = window_util::GetCaptureWindow();
    if (capture_window)
      capture_window->ReleaseCapture();
  }
  DCHECK(window->IsVisible());
  DCHECK(!base::Contains(modal_windows_, window));

  modal_windows_.push_back(window);
  // Create the modal background on all displays for |window|.
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    RootWindowController::ForWindow(root_window)
        ->GetSystemModalLayoutManager(window)
        ->CreateModalBackground();
  }
  window->parent()->StackChildAtTop(window);

  gfx::Rect target_bounds = window->bounds();
  target_bounds.AdjustToFit(GetUsableDialogArea());
  window->SetBounds(target_bounds);
}

bool SystemModalContainerLayoutManager::RemoveModalWindow(
    aura::Window* window) {
  auto it = base::ranges::find(modal_windows_, window);
  if (it == modal_windows_.end())
    return false;
  modal_windows_.erase(it);
  return true;
}

void SystemModalContainerLayoutManager::OnModalWindowRemoved(
    aura::Window* removed) {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  for (aura::Window* root_window : root_windows) {
    // system modal layout manager can be nullptr in some cases.
    auto* system_modal_layout_manager =
        RootWindowController::ForWindow(root_window)
            ->GetSystemModalLayoutManager(removed);
    if (system_modal_layout_manager &&
        system_modal_layout_manager->ActivateNextModalWindow()) {
      return;
    }
  }
  for (aura::Window* root_window : root_windows) {
    auto* system_modal_layout_manager =
        RootWindowController::ForWindow(root_window)
            ->GetSystemModalLayoutManager(removed);
    if (system_modal_layout_manager) {
      system_modal_layout_manager->DestroyModalBackground();
    }
  }
}

void SystemModalContainerLayoutManager::PositionDialogsAfterWorkAreaResize() {
  if (modal_windows_.empty())
    return;

  for (aura::Window* window : modal_windows_)
    window->SetBounds(GetCenteredAndOrFittedBounds(window));
}

gfx::Rect SystemModalContainerLayoutManager::GetUsableDialogArea() const {
  // Instead of resizing the system modal container, we move only the modal
  // windows. This way we avoid flashing lines upon resize animation and if the
  // keyboard will not fill left to right, the background is still covered.
  gfx::Rect valid_bounds = container_->bounds();
  const auto& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(container_);
  gfx::Rect work_area = display.work_area();
  // Convert work area in screen global coordinates to root local coordinates.
  wm::ConvertRectFromScreen(container_->GetRootWindow(), &work_area);

  // Similarly to NativeWidgetAura::CenterWindow, when centering window,
  // we take the intersection of the host and the container bounds.
  // The existing tests, SystemModalContainerLayoutManagerTest.KeepVisible and
  // KeepCentered, include the cases that container bounds are resizable.
  valid_bounds.Intersect(work_area);

  keyboard::KeyboardUIController* keyboard_controller =
      keyboard::KeyboardUIController::Get();
  if (keyboard_controller->IsEnabled()) {
    gfx::Rect bounds =
        keyboard_controller->GetWorkspaceOccludedBoundsInScreen();
    valid_bounds.set_height(
        std::max(0, valid_bounds.height() - bounds.height()));
  }
  return valid_bounds;
}

gfx::Rect SystemModalContainerLayoutManager::GetCenteredAndOrFittedBounds(
    const aura::Window* window) {
  gfx::Rect target_bounds;
  gfx::Rect usable_area = GetUsableDialogArea();
  if (windows_to_center_.count(window) > 0) {
    // Keep the dialog centered if it was centered before.
    target_bounds = usable_area;
    target_bounds.ClampToCenteredSize(window->bounds().size());
  } else {
    // Keep the dialog within the usable area.
    target_bounds = window->bounds();
    target_bounds.AdjustToFit(usable_area);
  }
  if (keyboard::KeyboardUIController::Get()->IsEnabled()) {
    // Don't clamp the dialog for the keyboard. Keep the size as it is but make
    // sure that the top remains visible.
    // TODO(skuhne): M37 should add over scroll functionality to address this.
    target_bounds.set_size(window->bounds().size());
  }
  return target_bounds;
}

bool SystemModalContainerLayoutManager::IsBoundsCentered(
    const gfx::Rect& bounds) const {
  gfx::Point window_center = bounds.CenterPoint();
  gfx::Point container_center = GetUsableDialogArea().CenterPoint();
  return std::abs(window_center.x() - container_center.x()) <
             kCenterPixelDelta &&
         std::abs(window_center.y() - container_center.y()) < kCenterPixelDelta;
}

void SystemModalContainerLayoutManager::StopObservingWindow(
    aura::Window* window) {
  window->RemoveObserver(this);
  windows_to_center_.erase(window);
  if (GetModalType(window) == ui::mojom::ModalType::kSystem &&
      RemoveModalWindow(window)) {
    OnModalWindowRemoved(window);
  }
}

}  // namespace ash
