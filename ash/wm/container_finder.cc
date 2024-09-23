// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/container_finder.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "chromeos/components/mahi/public/cpp/mahi_util.h"
#include "components/app_restore/window_properties.h"
#include "components/live_caption/views/caption_bubble.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

aura::Window* FindContainerRoot(aura::Window* root_window,
                                const gfx::Rect& bounds_in_screen) {
  if (bounds_in_screen == gfx::Rect()) {
    return Shell::GetRootWindowForNewWindows();
  }
  auto display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window);
  auto overlap = display.bounds();
  overlap.Intersect(bounds_in_screen);

  // If the window is nearly invisible on the current display, use matching
  // display.
  if (!display.bounds().Contains(bounds_in_screen) &&
      overlap.width() < kMinimumOnScreenArea &&
      overlap.height() < kMinimumOnScreenArea) {
    return window_util::GetRootWindowMatching(bounds_in_screen);
  }
  return root_window;
}

bool HasTransientParentWindow(const aura::Window* window) {
  const aura::Window* transient_parent = ::wm::GetTransientParent(window);
  return transient_parent &&
         transient_parent->GetType() != aura::client::WINDOW_TYPE_UNKNOWN;
}

aura::Window* GetSystemModalContainer(aura::Window* root,
                                      aura::Window* window) {
  DCHECK_EQ(ui::mojom::ModalType::kSystem,
            window->GetProperty(aura::client::kModalKey));

  // If |window| is already in a system modal container in |root|, re-use it.
  for (auto modal_container_id : kSystemModalContainerIds) {
    aura::Window* modal_container = root->GetChildById(modal_container_id);
    if (window->parent() == modal_container) {
      return modal_container;
    }
  }

  aura::Window* transient_parent = ::wm::GetTransientParent(window);

  // If screen lock is not active and user session is active,
  // all modal windows are placed into the normal modal container.
  // In case of missing transient parent (it could happen for alerts from
  // background pages) assume that the window belongs to user session.
  if (!Shell::Get()->session_controller()->IsUserSessionBlocked() ||
      !transient_parent) {
    return root->GetChildById(kShellWindowId_SystemModalContainer);
  }

  // Otherwise those that originate from LockScreen container and above are
  // placed in the screen lock modal container.
  int window_container_id = transient_parent->parent()->GetId();
  if (window_container_id < kShellWindowId_LockScreenContainer) {
    return root->GetChildById(kShellWindowId_SystemModalContainer);
  }
  return root->GetChildById(kShellWindowId_LockSystemModalContainer);
}

aura::Window* GetContainerFromAlwaysOnTopController(aura::Window* root,
                                                    aura::Window* window) {
  return RootWindowController::ForWindow(root)
      ->always_on_top_controller()
      ->GetContainer(window);
}

}  // namespace

aura::Window* GetContainerForWindow(aura::Window* window) {
  aura::Window* parent = window->parent();
  // The first parent with an explicit shell window ID is the container.
  while (parent && parent->GetId() == kShellWindowId_Invalid) {
    parent = parent->parent();
  }
  return parent;
}

aura::Window* GetDefaultParentForWindow(aura::Window* window,
                                        aura::Window* root_window,
                                        const gfx::Rect& bounds_in_screen) {
  aura::Window* target_root = nullptr;
  aura::Window* transient_parent = ::wm::GetTransientParent(window);
  if (transient_parent) {
    // Transient window should use the same root as its transient parent.
    target_root = transient_parent->GetRootWindow();
  } else {
    target_root = FindContainerRoot(root_window, bounds_in_screen);
  }

  // For window restore, the window may be created before the associated window
  // restore data can be retrieved. In this case, we will place it in a hidden
  // container and will move it to a desk container when the window restore data
  // can be retrieved. An example would be ARC windows, which can be created
  // before their associated tasks are, which are required to retrieve window
  // restore data.
  if (window->GetProperty(app_restore::kParentToHiddenContainerKey)) {
    return target_root->GetChildById(kShellWindowId_UnparentedContainer);
  }

  // Use kShellWindowId_DragImageAndTooltipContainer to host security surfaces
  // so that they are on top of other normal widgets (top-level windows, menus,
  // bubbles etc). See http://crbug.com/1317904.
  if (window->GetProperty(aura::client::kZOrderingKey) ==
      ui::ZOrderLevel::kSecuritySurface) {
    return target_root->GetChildById(
        kShellWindowId_DragImageAndTooltipContainer);
  }

  // Live caption bubble always goes into its dedicated container, above the
  // float, always-on-top and shelf containers for example.
  if (window->GetProperty(captions::kIsCaptionBubbleKey)) {
    return target_root->GetChildById(kShellWindowId_LiveCaptionContainer);
  }

  // The MahiMenu always goes into the settings bubble container, this ensures
  // that it is displayed on top of the MahiPanelWidget which can often
  // intersect with the MahiMenu.
  if (window->GetProperty(chromeos::mahi::kIsMahiMenuKey)) {
    return target_root->GetChildById(kShellWindowId_SettingBubbleContainer);
  }

  switch (window->GetType()) {
    case aura::client::WINDOW_TYPE_NORMAL:
    case aura::client::WINDOW_TYPE_POPUP:
      if (window->GetProperty(aura::client::kModalKey) ==
          ui::mojom::ModalType::kSystem) {
        return GetSystemModalContainer(target_root, window);
      }
      if (HasTransientParentWindow(window)) {
        return GetContainerForWindow(transient_parent);
      }
      return GetContainerFromAlwaysOnTopController(target_root, window);
    case aura::client::WINDOW_TYPE_CONTROL:
      return target_root->GetChildById(kShellWindowId_UnparentedContainer);
    case aura::client::WINDOW_TYPE_MENU:
      return target_root->GetChildById(kShellWindowId_MenuContainer);
    case aura::client::WINDOW_TYPE_TOOLTIP:
      return target_root->GetChildById(
          kShellWindowId_DragImageAndTooltipContainer);
    default:
      NOTREACHED() << "Window " << window->GetId() << " has unhandled type "
                   << window->GetType();
  }
}

aura::Window::Windows GetContainersForAllRootWindows(
    int container_id,
    aura::Window* priority_root) {
  aura::Window::Windows containers;
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    aura::Window* container = root->GetChildById(container_id);
    if (!container) {
      continue;
    }

    if (priority_root && priority_root->Contains(container)) {
      containers.insert(containers.begin(), container);
    } else {
      containers.push_back(container);
    }
  }
  return containers;
}

aura::Window* GetPowerMenuContainerParent(aura::Window* root_window) {
  return root_window->GetChildById(
      kShellWindowId_LockScreenRelatedContainersContainer);
}

}  // namespace ash
