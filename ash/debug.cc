// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/debug.h"

#include <memory>
#include <string>

#include "ash/public/cpp/debug_utils.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "ui/accessibility/aura/aura_window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/debug_utils.h"
#include "ui/compositor/layer.h"
#include "ui/views/debug_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace debug {

void PrintLayerHierarchy(std::ostringstream* out) {
  for (aura::Window* root : Shell::Get()->GetAllRootWindows()) {
    ui::Layer* layer = root->layer();
    if (layer) {
      ui::PrintLayerHierarchy(
          layer,
          RootWindowController::ForWindow(root)->GetLastMouseLocationInRoot(),
          out);
    }
  }
}

void PrintViewHierarchy(std::ostringstream* out) {
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window)
    return;
  views::Widget* widget = views::Widget::GetWidgetForNativeView(active_window);
  if (!widget)
    return;

  *out << "Host widget:\n";
  views::PrintWidgetInformation(*widget, /*detailed*/ true, out);
  views::PrintViewHierarchy(widget->GetRootView(), out);
}

void PrintWindowHierarchy(const aura::Window* active_window,
                          const aura::Window* focused_window,
                          const aura::Window* capture_window,
                          aura::Window* window,
                          int indent,
                          bool scrub_data,
                          std::vector<std::string>* out_window_titles,
                          std::ostringstream* out) {
  std::string indent_str(indent, ' ');
  std::string name(window->GetName());
  if (name.empty())
    name = "\"\"";
  const gfx::Vector2dF& subpixel_position_offset =
      window->layer()->GetSubpixelOffset();
  *out << indent_str;
  *out << " [window]";
  *out << " " << name << " (" << window << ")"
       << " type=" << window->GetType();
  int window_id = window->GetId();
  if (window_id != aura::Window::kInitialId)
    *out << " id=" << window_id;
  if (window->GetProperty(kWindowStateKey))
    *out << " " << WindowState::Get(window)->GetStateType();
  *out << ((window == active_window) ? " [active]" : "")
       << ((window == focused_window) ? " [focused]" : "")
       << ((window == capture_window) ? " [capture]" : "")
       << (window->GetTransparent() ? " [transparent]" : "")
       << (window->IsVisible() ? " [visible]" : "") << " "
       << (window->GetOcclusionState() != aura::Window::OcclusionState::UNKNOWN
               ? base::UTF16ToUTF8(aura::Window::OcclusionStateToString(
                                       window->GetOcclusionState()))
                     .c_str()
               : "")
       << " " << window->bounds().ToString();
  if (!subpixel_position_offset.IsZero())
    *out << " subpixel offset=" + subpixel_position_offset.ToString();
  std::string* tree_id = window->GetProperty(ui::kChildAXTreeID);
  if (tree_id)
    *out << " ax_tree_id=" << *tree_id;

  std::u16string title(window->GetTitle());
  if (!title.empty()) {
    out_window_titles->push_back(base::UTF16ToUTF8(title));
    if (!scrub_data) {
      *out << " title=" << title;
    }
  }

  int app_type = window->GetProperty(aura::client::kAppType);
  *out << " app_type=" << app_type;
  std::string* pkg_name = window->GetProperty(ash::kArcPackageNameKey);
  if (pkg_name)
    *out << " pkg_name=" << *pkg_name;
  *out << '\n';

  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  if (widget) {
    *out << std::string(indent + 3, ' ');
    *out << " [widget]";
    views::PrintWidgetInformation(*widget, /*detailed*/ false, out);
  }

  for (aura::Window* child : window->children()) {
    PrintWindowHierarchy(active_window, focused_window, capture_window, child,
                         indent + 3, scrub_data, out_window_titles, out);
  }
}

std::vector<std::string> PrintWindowHierarchy(std::ostringstream* out,
                                              bool scrub_data) {
  aura::Window* active_window = window_util::GetActiveWindow();
  aura::Window* focused_window = window_util::GetFocusedWindow();
  aura::Window* capture_window = window_util::GetCaptureWindow();
  aura::Window::Windows roots = Shell::Get()->GetAllRootWindows();
  std::vector<std::string> window_titles;
  for (size_t i = 0; i < roots.size(); ++i) {
    *out << "RootWindow " << i << ":\n";
    PrintWindowHierarchy(active_window, focused_window, capture_window,
                         roots[i], 0, scrub_data, &window_titles, out);
  }
  return window_titles;
}

void ToggleShowDebugBorders() {
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  std::unique_ptr<cc::DebugBorderTypes> value;
  for (auto* window : root_windows) {
    ui::Compositor* compositor = window->GetHost()->compositor();
    cc::LayerTreeDebugState state = compositor->GetLayerTreeDebugState();
    if (!value.get())
      value = std::make_unique<cc::DebugBorderTypes>(
          state.show_debug_borders.flip());
    state.show_debug_borders = *value.get();
    compositor->SetLayerTreeDebugState(state);
  }
}

void ToggleShowFpsCounter() {
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  std::unique_ptr<bool> value;
  for (auto* window : root_windows) {
    ui::Compositor* compositor = window->GetHost()->compositor();
    cc::LayerTreeDebugState state = compositor->GetLayerTreeDebugState();
    if (!value.get())
      value = std::make_unique<bool>(!state.show_fps_counter);
    state.show_fps_counter = *value.get();
    compositor->SetLayerTreeDebugState(state);
  }
}

void ToggleShowPaintRects() {
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  std::unique_ptr<bool> value;
  for (auto* window : root_windows) {
    ui::Compositor* compositor = window->GetHost()->compositor();
    cc::LayerTreeDebugState state = compositor->GetLayerTreeDebugState();
    if (!value.get())
      value = std::make_unique<bool>(!state.show_paint_rects);
    state.show_paint_rects = *value.get();
    compositor->SetLayerTreeDebugState(state);
  }
}

}  // namespace debug
}  // namespace ash
