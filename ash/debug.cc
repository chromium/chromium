// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/debug.h"

#include <memory>
#include <string>

#include "ash/public/cpp/debug_utils.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/memory/raw_ptr.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "chromeos/ui/wm/debug_util.h"
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

namespace {

std::unique_ptr<DebugWindowHierarchyDelegate> instance = nullptr;

}  // namespace

void SetDebugWindowHierarchyDelegate(
    std::unique_ptr<DebugWindowHierarchyDelegate> delegate) {
  instance = std::move(delegate);
}

void PrintLayerHierarchy(std::ostringstream* out) {
  ui::DebugLayerChildCallback child_cb =
      instance ? base::BindRepeating(
                     &DebugWindowHierarchyDelegate::GetAdjustedLayerChildren,
                     base::Unretained(instance.get()))
               : ui::DebugLayerChildCallback();
  for (aura::Window* root : Shell::Get()->GetAllRootWindows()) {
    ui::Layer* layer = root->layer();
    if (layer) {
      ui::PrintLayerHierarchy(
          layer,
          RootWindowController::ForWindow(root)->GetLastMouseLocationInRoot(),
          out, child_cb);
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

std::vector<std::string> PrintWindowHierarchy(std::ostringstream* out,
                                              bool scrub_data) {
  auto children_callback = base::BindRepeating(
      [](aura::Window* window)
          -> std::vector<raw_ptr<aura::Window, VectorExperimental>> {
        return instance ? instance->GetAdjustedWindowChildren(window)
                        : window->children();
      });
  return chromeos::wm::PrintWindowHierarchy(Shell::Get()->GetAllRootWindows(),
                                            scrub_data, out, children_callback);
}

void ToggleShowDebugBorders() {
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  std::unique_ptr<cc::DebugBorderTypes> value;
  for (aura::Window* window : root_windows) {
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
  for (aura::Window* window : root_windows) {
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
  for (aura::Window* window : root_windows) {
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
