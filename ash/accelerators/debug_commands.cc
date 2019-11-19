// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/debug_commands.h"

#include <string>
#include <utility>

#include "ash/accelerators/accelerator_commands.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/toast_data.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/touch/touch_devices_controller.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/platform/aura_window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/compositor/debug_utils.h"
#include "ui/compositor/layer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/views/debug_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_properties.h"

namespace ash {
namespace debug {
namespace {

void HandlePrintLayerHierarchy() {
  for (aura::Window* root : Shell::Get()->GetAllRootWindows()) {
    ui::Layer* layer = root->layer();
    if (layer)
      ui::PrintLayerHierarchy(
          layer,
          RootWindowController::ForWindow(root)->GetLastMouseLocationInRoot());
  }
}

void HandlePrintViewHierarchy() {
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window)
    return;
  views::Widget* widget = views::Widget::GetWidgetForNativeView(active_window);
  if (!widget)
    return;
  views::PrintViewHierarchy(widget->GetRootView());
}

void PrintWindowHierarchy(const aura::Window* active_window,
                          const aura::Window* focused_window,
                          aura::Window* window,
                          int indent,
                          std::ostringstream* out) {
  std::string indent_str(indent, ' ');
  std::string name(window->GetName());
  if (name.empty())
    name = "\"\"";
  const gfx::Vector2dF& subpixel_position_offset =
      window->layer()->GetSubpixelOffset();
  *out << indent_str;
  *out << name << " (" << window << ")"
       << " type=" << window->type();
  int window_id = window->id();
  if (window_id != aura::Window::kInitialId)
    *out << " id=" << window_id;
  if (window->GetProperty(kWindowStateKey))
    *out << " " << WindowState::Get(window)->GetStateType();
  *out << ((window == active_window) ? " [active]" : "")
       << ((window == focused_window) ? " [focused]" : "")
       << (window->IsVisible() ? " visible" : "") << " "
       << (window->occlusion_state() != aura::Window::OcclusionState::UNKNOWN
               ? aura::Window::OcclusionStateToString(window->occlusion_state())
               : "")
       << " " << window->bounds().ToString();
  if (!subpixel_position_offset.IsZero())
    *out << " subpixel offset=" + subpixel_position_offset.ToString();
  std::string* tree_id = window->GetProperty(ui::kChildAXTreeID);
  if (tree_id)
    *out << " ax_tree_id=" << *tree_id;
  base::string16 title(window->GetTitle());
  if (!title.empty())
    *out << " title=" << title;
  int app_type = window->GetProperty(aura::client::kAppType);
  *out << " app_type=" << app_type;
  std::string* pkg_name = window->GetProperty(ash::kArcPackageNameKey);
  if (pkg_name)
    *out << " pkg_name=" << *pkg_name;
  *out << '\n';

  for (aura::Window* child : window->children())
    PrintWindowHierarchy(active_window, focused_window, child, indent + 3, out);
}

void HandlePrintWindowHierarchy() {
  aura::Window* active_window = window_util::GetActiveWindow();
  aura::Window* focused_window = window_util::GetFocusedWindow();
  aura::Window::Windows roots = Shell::Get()->GetAllRootWindows();
  for (size_t i = 0; i < roots.size(); ++i) {
    std::ostringstream out;
    out << "RootWindow " << i << ":\n";
    PrintWindowHierarchy(active_window, focused_window, roots[i], 0, &out);
    // Error so logs can be collected from end-users.
    LOG(ERROR) << out.str();
  }
}

gfx::ImageSkia CreateWallpaperImage(SkColor fill, SkColor rect) {
  // TODO(oshima): Consider adding a command line option to control wallpaper
  // images for testing. The size is randomly picked.
  gfx::Size image_size(1366, 768);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(image_size.width(), image_size.height(), true);
  SkCanvas canvas(bitmap);
  canvas.drawColor(fill);
  SkPaint paint;
  paint.setColor(rect);
  paint.setStrokeWidth(10);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setBlendMode(SkBlendMode::kSrcOver);
  canvas.drawRoundRect(gfx::RectToSkRect(gfx::Rect(image_size)), 100.f, 100.f,
                       paint);
  return gfx::ImageSkia(gfx::ImageSkiaRep(std::move(bitmap), 1.f));
}

void HandleToggleWallpaperMode() {
  static int index = 0;
  auto* wallpaper_controller = Shell::Get()->wallpaper_controller();
  WallpaperInfo info("", WALLPAPER_LAYOUT_STRETCH, DEFAULT,
                     base::Time::Now().LocalMidnight());
  switch (++index % 4) {
    case 0:
      wallpaper_controller->ShowDefaultWallpaperForTesting();
      break;
    case 1:
      wallpaper_controller->ShowWallpaperImage(
          CreateWallpaperImage(SK_ColorRED, SK_ColorBLUE), info,
          /*preview_mode=*/false, /*always_on_top=*/false);
      break;
    case 2:
      info.layout = WALLPAPER_LAYOUT_CENTER;
      wallpaper_controller->ShowWallpaperImage(
          CreateWallpaperImage(SK_ColorBLUE, SK_ColorGREEN), info,
          /*preview_mode=*/false, /*always_on_top=*/false);
      break;
    case 3:
      info.layout = WALLPAPER_LAYOUT_CENTER_CROPPED;
      wallpaper_controller->ShowWallpaperImage(
          CreateWallpaperImage(SK_ColorGREEN, SK_ColorRED), info,
          /*preview_mode=*/false, /*always_on_top=*/false);
      break;
  }
}

void HandleToggleTouchpad() {
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Touchpad"));
  Shell::Get()->touch_devices_controller()->ToggleTouchpad();
}

void HandleToggleTouchscreen() {
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Touchscreen"));
  TouchDevicesController* controller = Shell::Get()->touch_devices_controller();
  controller->SetTouchscreenEnabled(
      !controller->GetTouchscreenEnabled(TouchDeviceEnabledSource::USER_PREF),
      TouchDeviceEnabledSource::USER_PREF);
}

void HandleToggleTabletMode() {
  TabletModeController* controller = Shell::Get()->tablet_mode_controller();
  controller->SetEnabledForDev(!controller->InTabletMode());
}

void HandleTriggerCrash() {
  LOG(FATAL) << "Intentional crash via debug accelerator.";
}

}  // namespace

void PrintUIHierarchies() {
  // This is a separate command so the user only has to hit one key to generate
  // all the logs. Developers use the individual dumps repeatedly, so keep
  // those as separate commands to avoid spamming their logs.
  HandlePrintLayerHierarchy();
  HandlePrintWindowHierarchy();
  HandlePrintViewHierarchy();
}

bool DebugAcceleratorsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshDebugShortcuts);
}

bool DeveloperAcceleratorsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshDeveloperShortcuts);
}

void PerformDebugActionIfEnabled(AcceleratorAction action) {
  if (!DebugAcceleratorsEnabled())
    return;

  switch (action) {
    case DEBUG_PRINT_LAYER_HIERARCHY:
      HandlePrintLayerHierarchy();
      break;
    case DEBUG_PRINT_VIEW_HIERARCHY:
      HandlePrintViewHierarchy();
      break;
    case DEBUG_PRINT_WINDOW_HIERARCHY:
      HandlePrintWindowHierarchy();
      break;
    case DEBUG_SHOW_TOAST:
      Shell::Get()->toast_manager()->Show(
          ToastData("id", base::ASCIIToUTF16("Toast"), 5000 /* duration_ms */,
                    base::ASCIIToUTF16("Dismiss")));
      break;
    case DEBUG_TOGGLE_DEVICE_SCALE_FACTOR:
      Shell::Get()->display_manager()->ToggleDisplayScaleFactor();
      break;
    case DEBUG_TOGGLE_TOUCH_PAD:
      HandleToggleTouchpad();
      break;
    case DEBUG_TOGGLE_TOUCH_SCREEN:
      HandleToggleTouchscreen();
      break;
    case DEBUG_TOGGLE_TABLET_MODE:
      HandleToggleTabletMode();
      break;
    case DEBUG_TOGGLE_WALLPAPER_MODE:
      HandleToggleWallpaperMode();
      break;
    case DEBUG_TRIGGER_CRASH:
      HandleTriggerCrash();
      break;
    default:
      break;
  }
}

}  // namespace debug
}  // namespace ash
