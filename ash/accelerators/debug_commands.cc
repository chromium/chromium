// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/debug_commands.h"

#include <string>
#include <utility>

#include "ash/accelerators/accelerator_commands.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/hud_display/hud_display.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/debug_utils.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/color_palette_controller.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/style_viewer/system_ui_components_style_viewer_view.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/system/video_conference/video_conference_common.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ash/touch/touch_devices_controller.h"
#include "ash/virtual_trackpad/virtual_trackpad_view.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/display/manager/display_manager.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace debug {
namespace {

void HandlePrintLayerHierarchy() {
  std::ostringstream out;
  PrintLayerHierarchy(&out);
  LOG(ERROR) << out.str();
}

void HandlePrintViewHierarchy() {
  std::ostringstream out;
  PrintViewHierarchy(&out);
  LOG(ERROR) << out.str();
}

void HandlePrintWindowHierarchy() {
  std::ostringstream out;
  PrintWindowHierarchy(&out, /*scrub_data=*/false);
  LOG(ERROR) << out.str();
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
  return gfx::ImageSkia::CreateFromBitmap(std::move(bitmap), 1.f);
}

void HandleToggleWallpaperMode() {
  static int index = 0;
  auto* wallpaper_controller = Shell::Get()->wallpaper_controller();
  WallpaperInfo info("", WALLPAPER_LAYOUT_STRETCH, WallpaperType::kDefault,
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

void HandleToggleDarkMode() {
  // Toggling dark mode requires that the active user session has started
  // since the feature is backed by user preferences.
  if (auto* controller = Shell::Get()->session_controller();
      !(controller && controller->IsActiveUserSessionStarted())) {
    return;
  }

  if (auto* controller = DarkLightModeControllerImpl::Get())
    controller->ToggleColorMode();
}

void HandleToggleDynamicColor() {
  if (!chromeos::features::IsJellyEnabled()) {
    // Only toggle colors when Dynamic Colors are enabled.
    return;
  }
  static int index = 0;
  SkColor color;
  switch (++index % 2) {
    case 0:
      color = SK_ColorGREEN;
      break;
    case 1:
      color = SK_ColorRED;
      break;
  }

  // This behavior is similar to the way that color changes in production, but
  // it may not match exactly.
  auto* theme = ui::NativeTheme::GetInstanceForNativeUi();
  theme->set_user_color(color);
  theme->NotifyOnNativeThemeUpdated();
}

// TODO(b/292584649): Remove this shortcut after testing is complete.
void HandleClearKMeansPref() {
  if (auto* controller = Shell::Get()->session_controller();
      !(controller && controller->IsActiveUserSessionStarted())) {
    return;
  }

  const UserSession* session =
      Shell::Get()->session_controller()->GetUserSession(/*index=*/0);
  const AccountId& account_id = session->user_info.account_id;
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetUserPrefServiceForUser(account_id);
  pref_service->ClearPref(prefs::kDynamicColorUseKMeans);

  // Setting the color scheme is a visual indicator that the pref has been
  // cleared. Tonal spot is the default color scheme, which is necessary to see
  // the k means color.
  Shell::Get()->color_palette_controller()->SetColorScheme(
      ColorScheme::kTonalSpot, account_id, base::DoNothing());
}

void HandleToggleGlanceables() {
  if (!features::AreGlanceablesEnabled())
    return;
  auto* controller = Shell::Get()->glanceables_controller();
  DCHECK(controller);
  if (controller->IsShowing())
    controller->DestroyUi();
  else
    controller->CreateUi();
}

void HandleTogglePowerButtonMenu() {
  auto* controller = Shell::Get()->power_button_controller();
  controller->ShowMenuOnDebugAccelerator();
}

void HandleToggleKeyboardBacklight() {
  if (ash::features::IsKeyboardBacklightToggleEnabled()) {
    base::RecordAction(base::UserMetricsAction("Accel_Keyboard_Backlight"));
    accelerators::ToggleKeyboardBacklight();
  }
}

void HandleToggleMicrophoneMute() {
  base::RecordAction(base::UserMetricsAction("Accel_Microphone_Mute"));
  accelerators::MicrophoneMuteToggle();
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

void HandleToggleVideoConferenceCameraTrayIcon() {
  if (!ash::features::IsVideoConferenceEnabled()) {
    return;
  }

  // Update media state to toggle video conference tray visibility.
  const bool vc_tray_visible = Shell::Get()
                                   ->GetPrimaryRootWindowController()
                                   ->GetStatusAreaWidget()
                                   ->video_conference_tray()
                                   ->GetVisible();

  VideoConferenceMediaState state;
  state.has_media_app = !vc_tray_visible;
  state.has_camera_permission = !vc_tray_visible;
  state.has_microphone_permission = !vc_tray_visible;
  state.is_capturing_screen = !vc_tray_visible;
  VideoConferenceTrayController::Get()->UpdateWithMediaState(state);
}

void HandleTriggerCrash() {
  LOG(FATAL) << "Intentional crash via debug accelerator.";
}

void HandleTriggerHUDDisplay() {
  hud_display::HUDDisplayView::Toggle();
}

void HandleToggleVirtualTrackpad() {
  VirtualTrackpadView::Toggle();
}

// Toast debug shortcut constants.
const std::u16string oneline_toast_text = u"SystemUI toast text string";
const std::u16string multiline_toast_text =
    u"SystemUI toast text string that breaks to two lines due to accomodate "
    u"long strings or translations. The text container has a max-width of "
    u"512px.";

void HandleShowToast() {
  // Iterates through all toast variations, which are a combination of having
  // multi-line text, dismiss button, and a leading icon.
  // `has_multiline_text` changes value every 4 iterations.
  // `has_dismiss_button` changes value every 2 iterations.
  // `has_leading_icon` changes value every iteration.
  static int index = 0;
  bool has_multiline_text = (index / 4) % 2;
  bool has_dismiss_button = (index / 2) % 2;
  bool has_leading_icon = index % 2;
  index++;

  Shell::Get()->toast_manager()->Show(ToastData(
      /*id=*/"id", ToastCatalogName::kDebugCommand,
      has_multiline_text ? multiline_toast_text : oneline_toast_text,
      ToastData::kDefaultToastDuration,
      /*visible_on_lock_screen=*/true, has_dismiss_button,
      /*custom_dismiss_text=*/u"Button",
      /*dismiss_callback=*/base::RepeatingClosure(),
      has_leading_icon ? kSystemMenuBusinessIcon : gfx::kNoneIcon));
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
    case AcceleratorAction::kDebugKeyboardBacklightToggle:
      HandleToggleKeyboardBacklight();
      break;
    case AcceleratorAction::kDebugMicrophoneMuteToggle:
      HandleToggleMicrophoneMute();
      break;
    case AcceleratorAction::kDebugPrintLayerHierarchy:
      HandlePrintLayerHierarchy();
      break;
    case AcceleratorAction::kDebugPrintViewHierarchy:
      HandlePrintViewHierarchy();
      break;
    case AcceleratorAction::kDebugPrintWindowHierarchy:
      HandlePrintWindowHierarchy();
      break;
    case AcceleratorAction::kDebugShowToast:
      HandleShowToast();
      break;
    case AcceleratorAction::kDebugSystemUiStyleViewer:
      SystemUIComponentsStyleViewerView::CreateAndShowWidget();
      break;
    case AcceleratorAction::kDebugToggleDarkMode:
      HandleToggleDarkMode();
      break;
    case AcceleratorAction::kDebugToggleDynamicColor:
      HandleToggleDynamicColor();
      break;
    case AcceleratorAction::kDebugClearUseKMeansPref:
      HandleClearKMeansPref();
      break;
    case AcceleratorAction::kDebugToggleGlanceables:
      HandleToggleGlanceables();
      break;
    case AcceleratorAction::kDebugTogglePowerButtonMenu:
      HandleTogglePowerButtonMenu();
      break;
    case AcceleratorAction::kDebugToggleTouchPad:
      HandleToggleTouchpad();
      break;
    case AcceleratorAction::kDebugToggleTouchScreen:
      HandleToggleTouchscreen();
      break;
    case AcceleratorAction::kDebugToggleTabletMode:
      HandleToggleTabletMode();
      break;
    case AcceleratorAction::kDebugToggleWallpaperMode:
      HandleToggleWallpaperMode();
      break;
    case AcceleratorAction::kDebugTriggerCrash:
      HandleTriggerCrash();
      break;
    case AcceleratorAction::kDebugToggleHudDisplay:
      HandleTriggerHUDDisplay();
      break;
    case AcceleratorAction::kDebugToggleVirtualTrackpad:
      HandleToggleVirtualTrackpad();
      break;
    case AcceleratorAction::kDebugToggleVideoConferenceCameraTrayIcon:
      HandleToggleVideoConferenceCameraTrayIcon();
      break;
    default:
      break;
  }
}

}  // namespace debug
}  // namespace ash
