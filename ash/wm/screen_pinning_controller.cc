// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/screen_pinning_controller.h"

#include <algorithm>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/window_user_data.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/window_dimmer.h"
#include "ash/wm/window_state.h"
#include "base/auto_reset.h"
#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

// Returns a list of Windows corresponding to SystemModalContainers,
// except ones whose root is shared with |pinned_window|.
aura::Window::Windows GetSystemModalWindowsExceptPinned(
    aura::Window* pinned_window) {
  aura::Window* pinned_root = pinned_window->GetRootWindow();

  aura::Window::Windows result;
  for (aura::Window* system_modal :
       GetContainersForAllRootWindows(kShellWindowId_SystemModalContainer)) {
    if (system_modal->GetRootWindow() == pinned_root)
      continue;
    result.push_back(system_modal);
  }
  return result;
}

void AddObserverToChildren(aura::Window* container,
                           aura::WindowObserver* observer) {
  for (aura::Window* child : container->children())
    child->AddObserver(observer);
}

void RemoveObserverFromChildren(aura::Window* container,
                                aura::WindowObserver* observer) {
  for (aura::Window* child : container->children())
    child->RemoveObserver(observer);
}

// Returns true if the aura::Window from a WindowDimmer is visible.
bool IsWindowDimmerWindowVisible(WindowDimmer* window_dimmer) {
  return window_dimmer->window()->layer()->GetTargetVisibility();
}

}  // namespace

// Adapter to fire OnPinnedContainerWindowStackingChanged().
// TODO(oshima): Consider using aura::clinet::WindowStakingClient instead.
class ScreenPinningController::PinnedContainerChildWindowObserver
    : public aura::WindowObserver {
 public:
  explicit PinnedContainerChildWindowObserver(
      ScreenPinningController* controller)
      : controller_(controller) {}

  PinnedContainerChildWindowObserver(
      const PinnedContainerChildWindowObserver&) = delete;
  PinnedContainerChildWindowObserver& operator=(
      const PinnedContainerChildWindowObserver&) = delete;

  void OnWindowStackingChanged(aura::Window* window) override {
    controller_->OnPinnedContainerWindowStackingChanged(window);
  }

 private:
  raw_ptr<ScreenPinningController> controller_;
};

// Adapter to translate OnWindowAdded/OnWillRemoveWindow for the container
// containing the pinned window, to the corresponding controller's methods.
class ScreenPinningController::PinnedContainerWindowObserver
    : public aura::WindowObserver {
 public:
  explicit PinnedContainerWindowObserver(ScreenPinningController* controller)
      : controller_(controller) {}

  PinnedContainerWindowObserver(const PinnedContainerWindowObserver&) = delete;
  PinnedContainerWindowObserver& operator=(
      const PinnedContainerWindowObserver&) = delete;

  void OnWindowAdded(aura::Window* new_window) override {
    controller_->OnWindowAddedToPinnedContainer(new_window);
  }
  void OnWillRemoveWindow(aura::Window* window) override {
    controller_->OnWillRemoveWindowFromPinnedContainer(window);
  }
  void OnWindowDestroying(aura::Window* window) override {
    controller_->OnContainerDestroying(window);
  }

 private:
  raw_ptr<ScreenPinningController> controller_;
};

// Adapter to fire OnSystemModalContainerWindowStackingChanged().
class ScreenPinningController::SystemModalContainerChildWindowObserver
    : public aura::WindowObserver {
 public:
  explicit SystemModalContainerChildWindowObserver(
      ScreenPinningController* controller)
      : controller_(controller) {}

  SystemModalContainerChildWindowObserver(
      const SystemModalContainerChildWindowObserver&) = delete;
  SystemModalContainerChildWindowObserver& operator=(
      const SystemModalContainerChildWindowObserver&) = delete;

  void OnWindowStackingChanged(aura::Window* window) override {
    controller_->OnSystemModalContainerWindowStackingChanged(window);
  }

 private:
  raw_ptr<ScreenPinningController> controller_;
};

// Adapter to translate OnWindowAdded/OnWillRemoveWindow for the
// SystemModalContainer to the corresponding controller's methods.
class ScreenPinningController::SystemModalContainerWindowObserver
    : public aura::WindowObserver {
 public:
  explicit SystemModalContainerWindowObserver(
      ScreenPinningController* controller)
      : controller_(controller) {}

  SystemModalContainerWindowObserver(
      const SystemModalContainerWindowObserver&) = delete;
  SystemModalContainerWindowObserver& operator=(
      const SystemModalContainerWindowObserver&) = delete;

  void OnWindowAdded(aura::Window* new_window) override {
    controller_->OnWindowAddedToSystemModalContainer(new_window);
  }
  void OnWillRemoveWindow(aura::Window* window) override {
    controller_->OnWillRemoveWindowFromSystemModalContainer(window);
  }
  void OnWindowDestroying(aura::Window* window) override {
    // Just in case. There is nothing we can do here.
    window->RemoveObserver(this);
  }

 private:
  raw_ptr<ScreenPinningController> controller_;
};

ScreenPinningController::ScreenPinningController()
    : window_dimmers_(std::make_unique<WindowUserData<WindowDimmer>>()),
      pinned_container_window_observer_(
          std::make_unique<PinnedContainerWindowObserver>(this)),
      pinned_container_child_window_observer_(
          std::make_unique<PinnedContainerChildWindowObserver>(this)),
      system_modal_container_window_observer_(
          std::make_unique<SystemModalContainerWindowObserver>(this)),
      system_modal_container_child_window_observer_(
          std::make_unique<SystemModalContainerChildWindowObserver>(this)) {
  Shell::Get()->display_manager()->AddDisplayManagerObserver(this);
}

ScreenPinningController::~ScreenPinningController() {
  Shell::Get()->display_manager()->RemoveDisplayManagerObserver(this);
  if (pinned_window_)
    pinned_window_->RemoveObserver(this);
  pinned_window_ = nullptr;
}

bool ScreenPinningController::IsPinned() const {
  return pinned_window_ != nullptr;
}

void ScreenPinningController::SetPinnedWindow(aura::Window* pinned_window) {
  if (WindowState::Get(pinned_window)->IsPinned()) {
    if (pinned_window_) {
      LOG(DFATAL) << "Pinned mode is enabled, while it is already in "
                  << "the pinned mode";
      return;
    }

    container_ = pinned_window->parent();
    aura::Window::Windows system_modal_containers =
        GetSystemModalWindowsExceptPinned(pinned_window);

    // Set up the container which has the pinned window.
    pinned_window_ = pinned_window;
    // To monitor destruction.
    pinned_window_->AddObserver(this);
    AlwaysOnTopController::SetDisallowReparent(pinned_window);
    container_->StackChildAtTop(pinned_window);
    container_->StackChildBelow(CreateWindowDimmer(container_), pinned_window);

    // Set the dim windows to the system containers, other than the one which
    // the root window of the pinned window holds.
    for (aura::Window* system_modal : system_modal_containers)
      system_modal->StackChildAtBottom(CreateWindowDimmer(system_modal));

    // Set observers.
    container_->AddObserver(pinned_container_window_observer_.get());
    AddObserverToChildren(container_,
                          pinned_container_child_window_observer_.get());
    for (aura::Window* system_modal : system_modal_containers) {
      system_modal->AddObserver(system_modal_container_window_observer_.get());
      AddObserverToChildren(
          system_modal, system_modal_container_child_window_observer_.get());
    }
  } else {
    if (pinned_window != pinned_window_) {
      LOG(DFATAL) << "Pinned mode is being disabled, but for the different "
                  << "target window.";
      return;
    }

    ResetWindowPinningState();
  }

  Shell::Get()->NotifyPinnedStateChanged(pinned_window);
}

void ScreenPinningController::OnWindowAddedToPinnedContainer(
    aura::Window* new_window) {
  KeepPinnedWindowOnTop();
  new_window->AddObserver(pinned_container_child_window_observer_.get());
}

void ScreenPinningController::OnWillRemoveWindowFromPinnedContainer(
    aura::Window* window) {
  window->RemoveObserver(pinned_container_child_window_observer_.get());
}

void ScreenPinningController::OnPinnedContainerWindowStackingChanged(
    aura::Window* window) {
  KeepPinnedWindowOnTop();
}

void ScreenPinningController::OnWindowAddedToSystemModalContainer(
    aura::Window* new_window) {
  KeepDimWindowAtBottom(new_window->parent());
  new_window->AddObserver(system_modal_container_child_window_observer_.get());
}

void ScreenPinningController::OnWillRemoveWindowFromSystemModalContainer(
    aura::Window* window) {
  window->RemoveObserver(system_modal_container_child_window_observer_.get());
}

void ScreenPinningController::OnSystemModalContainerWindowStackingChanged(
    aura::Window* window) {
  KeepDimWindowAtBottom(window->parent());
}

void ScreenPinningController::OnContainerDestroying(aura::Window* container) {
  if (container_ != nullptr && container_ == container) {
    container->RemoveObserver(pinned_container_window_observer_.get());
    container_ = nullptr;
  }
}

aura::Window* ScreenPinningController::CreateWindowDimmer(
    aura::Window* container) {
  std::unique_ptr<WindowDimmer> window_dimmer =
      std::make_unique<WindowDimmer>(container);
  window_dimmer->SetDimOpacity(1);  // Fully opaque.
  AlwaysOnTopController::SetDisallowReparent(window_dimmer->window());
  ::wm::SetWindowFullscreen(window_dimmer->window(), true);
  window_dimmer->window()->Show();
  aura::Window* window = window_dimmer->window();
  window_dimmers_->Set(container, std::move(window_dimmer));
  return window;
}

void ScreenPinningController::ResetWindowPinningState() {
  aura::Window::Windows system_modal_containers =
      GetSystemModalWindowsExceptPinned(pinned_window_);

  // Unset observers.
  for (aura::Window* system_modal :
       GetSystemModalWindowsExceptPinned(pinned_window_)) {
    RemoveObserverFromChildren(
        system_modal, system_modal_container_child_window_observer_.get());
    system_modal->RemoveObserver(system_modal_container_window_observer_.get());
  }

  if (container_) {
    RemoveObserverFromChildren(container_,
                               pinned_container_child_window_observer_.get());
    container_->RemoveObserver(pinned_container_window_observer_.get());
    container_ = nullptr;
  }

  window_dimmers_->clear();
  pinned_window_->RemoveObserver(this);
  pinned_window_ = nullptr;
}

void ScreenPinningController::OnDidApplyDisplayChanges() {
  // Note: this is called on display attached or detached.
  if (!IsPinned())
    return;

  // On display detaching, all necessary windows are transferred to the
  // primary display's tree, and called this.
  // So, delete WindowDimmers which are not a part of target system modal
  // container.
  // On display attaching, the new system modal container does not have the
  // WindowDimmer. So create it.

  // First, delete unnecessary WindowDimmers.
  for (aura::Window* container : window_dimmers_->GetWindows()) {
    if (container != pinned_window_->parent() &&
        !IsWindowDimmerWindowVisible(window_dimmers_->Get(container))) {
      window_dimmers_->Set(container, nullptr);
    }
  }

  // Then, create missing WindowDimmers.
  aura::Window::Windows system_modal_containers =
      GetSystemModalWindowsExceptPinned(pinned_window_);
  for (aura::Window* system_modal : system_modal_containers) {
    if (window_dimmers_->Get(system_modal)) {
      // |system_modal| already has a WindowDimmer.
      continue;
    }

    // This is the new system modal dialog.
    system_modal->StackChildAtBottom(CreateWindowDimmer(system_modal));

    // Set observers to the tree.
    system_modal->AddObserver(system_modal_container_window_observer_.get());
    AddObserverToChildren(system_modal,
                          system_modal_container_child_window_observer_.get());
  }
}

void ScreenPinningController::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(pinned_window_, window);
  WindowState::Get(window)->Restore();

  // |pinned_window_| isn't cleared, which means the call to restore window
  // didn't unpin itself. This is possible because the window is being
  // destroyed and some requests are ignored, but we still want to restore
  // the internal state of |ScreenPinningController| so that other windows
  // can be pinned again.
  if (pinned_window_)
    ResetWindowPinningState();
}

void ScreenPinningController::KeepPinnedWindowOnTop() {
  if (in_restacking_ || allow_window_stacking_with_pinned_window_) {
    return;
  }

  base::AutoReset<bool> auto_reset(&in_restacking_, true);
  base::AutoReset<bool> auto_reset_pinned_window_stacking(
      &allow_window_stacking_with_pinned_window_, false);
  aura::Window* container = pinned_window_->parent();
  container->StackChildAtTop(pinned_window_);
  WindowDimmer* pinned_window_dimmer = window_dimmers_->Get(container);
  if (pinned_window_dimmer && pinned_window_dimmer->window())
    container->StackChildBelow(pinned_window_dimmer->window(), pinned_window_);
}

void ScreenPinningController::KeepDimWindowAtBottom(aura::Window* container) {
  if (in_restacking_)
    return;

  WindowDimmer* window_dimmer = window_dimmers_->Get(container);
  if (window_dimmer) {
    base::AutoReset<bool> auto_reset(&in_restacking_, true);
    container->StackChildAtBottom(window_dimmer->window());
  }
}

}  // namespace ash
