// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_modal_container_layout_manager.h"

#include <cmath>
#include <memory>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/window_dimmer.h"
#include "ash/wm/window_util.h"
#include "base/stl_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

// The center point of the window can diverge this much from the center point
// of the container to be kept centered upon resizing operations.
const int kCenterPixelDelta = 32;

ui::ModalType GetModalType(aura::Window* window) {
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
}

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
  if (GetModalType(window) != ui::MODAL_TYPE_SYSTEM)
    return;

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
  DCHECK(child->type() == aura::client::WINDOW_TYPE_NORMAL ||
         child->type() == aura::client::WINDOW_TYPE_POPUP);
  DCHECK(container_->id() != kShellWindowId_LockSystemModalContainer ||
         Shell::Get()->session_controller()->IsUserSessionBlocked());
  // Since this is for SystemModal, there is no good reason to add windows
  // other than MODAL_TYPE_NONE or MODAL_TYPE_SYSTEM. DCHECK to avoid simple
  // mistake.
  DCHECK_NE(GetModalType(child), ui::MODAL_TYPE_CHILD);
  DCHECK_NE(GetModalType(child), ui::MODAL_TYPE_WINDOW);

  child->AddObserver(this);
  if (GetModalType(child) == ui::MODAL_TYPE_SYSTEM && child->IsVisible())
    AddModalWindow(child);
}

void SystemModalContainerLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
  child->RemoveObserver(this);
  windows_to_center_.erase(child);
  if (GetModalType(child) == ui::MODAL_TYPE_SYSTEM)
    RemoveModalWindow(child);
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

  if (window->GetProperty(aura::client::kModalKey) == ui::MODAL_TYPE_SYSTEM) {
    if (base::Contains(modal_windows_, window))
      return;
    AddModalWindow(window);
  } else {
    if (RemoveModalWindow(window))
      OnModalWindowRemoved(window);
  }
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
  int id = window->parent()->id();
  if (id != kShellWindowId_SystemModalContainer &&
      id != kShellWindowId_LockSystemModalContainer)
    return false;
  SystemModalContainerLayoutManager* layout_manager =
      static_cast<SystemModalContainerLayoutManager*>(
          window->parent()->layout_manager());
  return layout_manager->window_dimmer_ &&
         layout_manager->window_dimmer_->window() == window;
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
  auto it = std::find(modal_windows_.begin(), modal_windows_.end(), window);
  if (it == modal_windows_.end())
    return false;
  modal_windows_.erase(it);
  return true;
}

void SystemModalContainerLayoutManager::OnModalWindowRemoved(
    aura::Window* removed) {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  for (aura::Window* root_window : root_windows) {
    if (RootWindowController::ForWindow(root_window)
            ->GetSystemModalLayoutManager(removed)
            ->ActivateNextModalWindow()) {
      return;
    }
  }
  for (aura::Window* root_window : root_windows) {
    RootWindowController::ForWindow(root_window)
        ->GetSystemModalLayoutManager(removed)
        ->DestroyModalBackground();
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
  if (usable_area != container_->bounds()) {
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

}  // namespace ash
