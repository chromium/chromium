// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/top_level_window_factory.h"

#include "ash/disconnected_app_handler.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/shell.h"
#include "ash/window_factory.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/non_client_frame_controller.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_state.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/ws/public/cpp/property_type_converters.h"
#include "services/ws/public/mojom/window_manager.mojom.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "services/ws/window_delegate_impl.h"
#include "services/ws/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/property_utils.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// Returns true if a fullscreen window was requested.
bool IsFullscreen(aura::PropertyConverter* property_converter,
                  const std::vector<uint8_t>& transport_data) {
  using ws::mojom::WindowManager;
  aura::PropertyConverter::PrimitiveType show_state = 0;
  return property_converter->GetPropertyValueFromTransportValue(
             WindowManager::kShowState_Property, transport_data, &show_state) &&
         (static_cast<ui::WindowShowState>(show_state) ==
          ui::SHOW_STATE_FULLSCREEN);
}

// Returns the RootWindowController where new top levels are created.
// |properties| is the properties supplied during window creation.
RootWindowController* GetRootWindowControllerForNewTopLevelWindow(
    std::map<std::string, std::vector<uint8_t>>* properties) {
  // If a specific display was requested, use it. If no display was requested,
  // choose one based on the requested initial bounds.
  int64_t display_id = GetInitialDisplayId(*properties);
  gfx::Rect requested_bounds;
  if (display_id == display::kInvalidDisplayId &&
      GetInitialBounds(*properties, &requested_bounds)) {
    display_id =
        display::Screen::GetScreen()->GetDisplayMatching(requested_bounds).id();
  }

  if (display_id != display::kInvalidDisplayId) {
    for (RootWindowController* root_window_controller :
         RootWindowController::root_window_controllers()) {
      if (GetRootWindowSettings(root_window_controller->GetRootWindow())
              ->display_id == display_id) {
        return root_window_controller;
      }
    }
  }

  return RootWindowController::ForWindow(Shell::GetRootWindowForNewWindows());
}

// Returns the bounds for the new window. If |container_window| is provided the
// bounds are local to the container, otherwise they are in screen coordinates.
gfx::Rect CalculateDefaultBounds(
    aura::Window* root_window,
    aura::Window* container_window,
    aura::PropertyConverter* property_converter,
    const std::map<std::string, std::vector<uint8_t>>* properties) {
  gfx::Rect requested_bounds;
  if (GetInitialBounds(*properties, &requested_bounds))
    return requested_bounds;

  const gfx::Size root_size = root_window->bounds().size();
  auto show_state_iter =
      properties->find(ws::mojom::WindowManager::kShowState_Property);
  if (show_state_iter != properties->end()) {
    if (IsFullscreen(property_converter, show_state_iter->second)) {
      gfx::Rect bounds(root_size);
      if (!container_window) {
        // Ensure the window is placed on the correct display.
        ::wm::ConvertRectToScreen(root_window, &bounds);
      }
      return bounds;
    }
  }

  gfx::Size window_size;
  if (GetWindowPreferredSize(*properties, &window_size) &&
      !window_size.IsEmpty()) {
    // TODO(sky): likely want to constrain more than root size.
    window_size.SetToMin(root_size);
  } else {
    // Pick a fixed default size. Most applications will immediately set the
    // bounds and/or center the window, so the user usually won't see this.
    window_size.SetSize(300, 200);
  }
  // TODO(sky): this should use code in chrome/browser/ui/window_sizer.
  static constexpr int kOriginOffset = 40;
  gfx::Rect bounds(gfx::Point(kOriginOffset, kOriginOffset), window_size);
  if (!container_window) {
    // Ensure the window is placed on the correct display.
    ::wm::ConvertRectToScreen(root_window, &bounds);
  }
  return bounds;
}

// Does the real work of CreateAndParentTopLevelWindow() once the appropriate
// RootWindowController was found.
aura::Window* CreateAndParentTopLevelWindowInRoot(
    RootWindowController* root_window_controller,
    ws::mojom::WindowType window_type,
    aura::PropertyConverter* property_converter,
    std::map<std::string, std::vector<uint8_t>>* properties) {
  // TODO(sky): constrain and validate properties.
  aura::Window* root_window = root_window_controller->GetRootWindow();

  int32_t container_id = kShellWindowId_Invalid;
  aura::Window* context = nullptr;
  aura::Window* container_window = nullptr;
  if (GetInitialContainerId(*properties, &container_id)) {
    container_window = root_window->GetChildById(container_id);
  } else {
    context = root_window;
  }

  gfx::Rect bounds = CalculateDefaultBounds(root_window, container_window,
                                            property_converter, properties);

  const bool provide_non_client_frame =
      window_type == ws::mojom::WindowType::WINDOW ||
      window_type == ws::mojom::WindowType::PANEL;
  if (provide_non_client_frame) {
    // See NonClientFrameController for details on lifetime.
    NonClientFrameController* non_client_frame_controller =
        new NonClientFrameController(container_window, context, bounds,
                                     window_type, property_converter,
                                     properties);
    return non_client_frame_controller->window();
  }

  // WindowDelegateImpl() deletes itself when the associated window is
  // destroyed.
  ws::WindowDelegateImpl* window_delegate = new ws::WindowDelegateImpl();
  aura::Window* window = window_factory::NewWindow(window_delegate).release();
  window_delegate->set_window(window);
  aura::SetWindowType(window, window_type);
  ApplyProperties(window, property_converter, *properties);
  window->Init(ui::LAYER_TEXTURED);

  if (container_window) {
    // |bounds| are in local coordinates.
    container_window->AddChild(window);
    window->SetBounds(bounds);
  } else {
    // |bounds| are in screen coordinates.
    aura::Window* parent = ash::wm::GetDefaultParent(window, bounds);
    parent->AddChild(window);
    gfx::Rect bounds_in_parent = bounds;
    ::wm::ConvertRectFromScreen(parent, &bounds_in_parent);
    window->SetBounds(bounds_in_parent);
  }
  return window;
}

}  // namespace

aura::Window* CreateAndParentTopLevelWindow(
    ws::mojom::WindowType window_type,
    aura::PropertyConverter* property_converter,
    std::map<std::string, std::vector<uint8_t>>* properties) {
  if (window_type == ws::mojom::WindowType::UNKNOWN)
    return nullptr;  // Clients must supply a valid type.

  RootWindowController* root_window_controller =
      GetRootWindowControllerForNewTopLevelWindow(properties);
  aura::Window* window = CreateAndParentTopLevelWindowInRoot(
      root_window_controller, window_type, property_converter, properties);
  DisconnectedAppHandler::Create(window);

  auto ignored_by_shelf_iter = properties->find(
      ws::mojom::WindowManager::kWindowIgnoredByShelf_InitProperty);
  if (ignored_by_shelf_iter != properties->end()) {
    wm::WindowState* window_state = wm::GetWindowState(window);
    window_state->set_ignored_by_shelf(
        mojo::ConvertTo<bool>(ignored_by_shelf_iter->second));
    // No need to persist this value.
    properties->erase(ignored_by_shelf_iter);
  }

  // TODO: kFocusable_InitProperty should be removed. http://crbug.com/837713.
  auto focusable_iter =
      properties->find(ws::mojom::WindowManager::kFocusable_InitProperty);
  if (focusable_iter != properties->end()) {
    bool can_focus = mojo::ConvertTo<bool>(focusable_iter->second);
    NonClientFrameController* non_client_frame_controller =
        NonClientFrameController::Get(window);
    window->SetProperty(ws::kCanFocus, can_focus);
    if (non_client_frame_controller)
      non_client_frame_controller->set_can_activate(can_focus);
    // No need to persist this value.
    properties->erase(focusable_iter);
  }

  auto translucent_iter =
      properties->find(ws::mojom::WindowManager::kTranslucent_InitProperty);
  if (translucent_iter != properties->end()) {
    bool translucent = mojo::ConvertTo<bool>(translucent_iter->second);
    window->SetTransparent(translucent);
    // No need to persist this value.
    properties->erase(translucent_iter);
  }

  return window;
}

}  // namespace ash
