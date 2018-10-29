// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mojo_interface_factory.h"

#include <utility>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/accessibility_focus_ring_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/assistant/assistant_controller.h"
#include "ash/cast_config_controller.h"
#include "ash/display/ash_display_controller.h"
#include "ash/display/cros_display_config.h"
#include "ash/display/display_output_protection.h"
#include "ash/events/event_rewriter_controller.h"
#include "ash/first_run/first_run_helper.h"
#include "ash/highlighter/highlighter_controller.h"
#include "ash/ime/ime_controller.h"
#include "ash/keyboard/ash_keyboard_controller.h"
#include "ash/login/login_screen_controller.h"
#include "ash/magnifier/docked_magnifier_controller.h"
#include "ash/media_controller.h"
#include "ash/metrics/time_to_first_present_recorder.h"
#include "ash/new_window_controller.h"
#include "ash/note_taking_controller.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shutdown_controller.h"
#include "ash/system/locale/locale_notification_controller.h"
#include "ash/system/message_center/message_center_controller.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/vpn_list.h"
#include "ash/system/night_light/night_light_controller.h"
#include "ash/tray_action/tray_action.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/single_thread_task_runner.h"
#include "chromeos/chromeos_switches.h"
#include "ui/keyboard/keyboard_controller.h"

namespace ash {
namespace mojo_interface_factory {
namespace {

base::LazyInstance<RegisterInterfacesCallback>::Leaky
    g_register_interfaces_callback = LAZY_INSTANCE_INITIALIZER;

void BindAcceleratorControllerRequestOnMainThread(
    mojom::AcceleratorControllerRequest request) {
  Shell::Get()->accelerator_controller()->BindRequest(std::move(request));
}

void BindAccessibilityControllerRequestOnMainThread(
    mojom::AccessibilityControllerRequest request) {
  Shell::Get()->accessibility_controller()->BindRequest(std::move(request));
}

void BindAccessibilityFocusRingControllerRequestOnMainThread(
    mojom::AccessibilityFocusRingControllerRequest request) {
  Shell::Get()->accessibility_focus_ring_controller()->BindRequest(
      std::move(request));
}

void BindAppListControllerRequestOnMainThread(
    mojom::AppListControllerRequest request) {
  Shell::Get()->app_list_controller()->BindRequest(std::move(request));
}

void BindAshDisplayControllerRequestOnMainThread(
    mojom::AshDisplayControllerRequest request) {
  Shell::Get()->ash_display_controller()->BindRequest(std::move(request));
}

void BindAssistantControllerRequestOnMainThread(
    mojom::AssistantControllerRequest request) {
  Shell::Get()->assistant_controller()->BindRequest(std::move(request));
}

void BindAssistantVolumeControlRequestOnMainThread(
    mojom::AssistantVolumeControlRequest request) {
  Shell::Get()->assistant_controller()->BindRequest(std::move(request));
}

void BindCrosDisplayConfigControllerRequestOnMainThread(
    mojom::CrosDisplayConfigControllerRequest request) {
  Shell::Get()->cros_display_config()->BindRequest(std::move(request));
}

void BindAshMessageCenterControllerRequestOnMainThread(
    mojom::AshMessageCenterControllerRequest request) {
  Shell::Get()->message_center_controller()->BindRequest(std::move(request));
}

void BindCastConfigOnMainThread(mojom::CastConfigRequest request) {
  Shell::Get()->cast_config()->BindRequest(std::move(request));
}

void BindDisplayOutputProtectionRequestOnMainThread(
    mojom::DisplayOutputProtectionRequest request) {
  Shell::Get()->display_output_protection()->BindRequest(std::move(request));
}

void BindDockedMagnifierControllerRequestOnMainThread(
    mojom::DockedMagnifierControllerRequest request) {
  Shell::Get()->docked_magnifier_controller()->BindRequest(std::move(request));
}

void BindEventRewriterControllerRequestOnMainThread(
    mojom::EventRewriterControllerRequest request) {
  Shell::Get()->event_rewriter_controller()->BindRequest(std::move(request));
}

void BindFirstRunHelperRequestOnMainThread(
    mojom::FirstRunHelperRequest request) {
  Shell::Get()->first_run_helper()->BindRequest(std::move(request));
}

void BindHighlighterControllerRequestOnMainThread(
    mojom::HighlighterControllerRequest request) {
  Shell::Get()->highlighter_controller()->BindRequest(std::move(request));
}

void BindImeControllerRequestOnMainThread(mojom::ImeControllerRequest request) {
  Shell::Get()->ime_controller()->BindRequest(std::move(request));
}

void BindKeyboardControllerRequestOnMainThread(
    mojom::KeyboardControllerRequest request) {
  Shell::Get()->ash_keyboard_controller()->BindRequest(std::move(request));
}

void BindLocaleNotificationControllerOnMainThread(
    mojom::LocaleNotificationControllerRequest request) {
  Shell::Get()->locale_notification_controller()->BindRequest(
      std::move(request));
}

void BindLockScreenRequestOnMainThread(mojom::LoginScreenRequest request) {
  Shell::Get()->login_screen_controller()->BindRequest(std::move(request));
}

void BindMediaControllerRequestOnMainThread(
    mojom::MediaControllerRequest request) {
  Shell::Get()->media_controller()->BindRequest(std::move(request));
}

void BindNewWindowControllerRequestOnMainThread(
    mojom::NewWindowControllerRequest request) {
  Shell::Get()->new_window_controller()->BindRequest(std::move(request));
}

void BindNightLightControllerRequestOnMainThread(
    mojom::NightLightControllerRequest request) {
  Shell::Get()->night_light_controller()->BindRequest(std::move(request));
}

void BindNoteTakingControllerRequestOnMainThread(
    mojom::NoteTakingControllerRequest request) {
  Shell::Get()->note_taking_controller()->BindRequest(std::move(request));
}

void BindProcessCreationTimeRecorderOnMainThread(
    mojom::ProcessCreationTimeRecorderRequest request) {
  Shell::Get()->time_to_first_present_recorder()->Bind(std::move(request));
}

void BindSessionControllerRequestOnMainThread(
    mojom::SessionControllerRequest request) {
  Shell::Get()->session_controller()->BindRequest(std::move(request));
}

void BindShelfRequestOnMainThread(mojom::ShelfControllerRequest request) {
  Shell::Get()->shelf_controller()->BindRequest(std::move(request));
}

void BindShutdownControllerRequestOnMainThread(
    mojom::ShutdownControllerRequest request) {
  Shell::Get()->shutdown_controller()->BindRequest(std::move(request));
}

void BindSystemTrayRequestOnMainThread(mojom::SystemTrayRequest request) {
  Shell::Get()->system_tray_model()->BindRequest(std::move(request));
}

void BindTabletModeRequestOnMainThread(
    mojom::TabletModeControllerRequest request) {
  Shell::Get()->tablet_mode_controller()->BindRequest(std::move(request));
}

void BindTrayActionRequestOnMainThread(mojom::TrayActionRequest request) {
  Shell::Get()->tray_action()->BindRequest(std::move(request));
}

void BindVoiceInteractionControllerRequestOnMainThread(
    mojom::VoiceInteractionControllerRequest request) {
  Shell::Get()->voice_interaction_controller()->BindRequest(std::move(request));
}

void BindVpnListRequestOnMainThread(mojom::VpnListRequest request) {
  Shell::Get()->vpn_list()->BindRequest(std::move(request));
}

void BindWallpaperRequestOnMainThread(
    mojom::WallpaperControllerRequest request) {
  Shell::Get()->wallpaper_controller()->BindRequest(std::move(request));
}

void BindSplitViewRequestOnMainThread(
    mojom::SplitViewControllerRequest request) {
  Shell::Get()->split_view_controller()->BindRequest(std::move(request));
}

}  // namespace

void RegisterInterfaces(
    service_manager::BinderRegistry* registry,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner) {
  registry->AddInterface(
      base::BindRepeating(&BindAcceleratorControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindAccessibilityControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(
          &BindAccessibilityFocusRingControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindAppListControllerRequestOnMainThread),
      main_thread_task_runner);
  if (chromeos::switches::IsAssistantEnabled()) {
    registry->AddInterface(
        base::BindRepeating(&BindAssistantControllerRequestOnMainThread),
        main_thread_task_runner);
    registry->AddInterface(
        base::BindRepeating(&BindAssistantVolumeControlRequestOnMainThread),
        main_thread_task_runner);
  }
  registry->AddInterface(
      base::BindRepeating(&BindAshDisplayControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindCrosDisplayConfigControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindAshMessageCenterControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(base::BindRepeating(&BindCastConfigOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindDisplayOutputProtectionRequestOnMainThread),
      main_thread_task_runner);
  if (features::IsDockedMagnifierEnabled()) {
    registry->AddInterface(
        base::BindRepeating(&BindDockedMagnifierControllerRequestOnMainThread),
        main_thread_task_runner);
  }
  registry->AddInterface(
      base::BindRepeating(&BindEventRewriterControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindFirstRunHelperRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindHighlighterControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindImeControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindKeyboardControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindLocaleNotificationControllerOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindLockScreenRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindMediaControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindNewWindowControllerRequestOnMainThread),
      main_thread_task_runner);
  if (features::IsNightLightEnabled()) {
    registry->AddInterface(
        base::BindRepeating(&BindNightLightControllerRequestOnMainThread),
        main_thread_task_runner);
  }
  registry->AddInterface(
      base::BindRepeating(&BindNoteTakingControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindProcessCreationTimeRecorderOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindSessionControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(base::BindRepeating(&BindShelfRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindShutdownControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindSystemTrayRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindTabletModeRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindTrayActionRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindVoiceInteractionControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(base::BindRepeating(&BindVpnListRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::BindRepeating(&BindWallpaperRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::BindRepeating(&BindSplitViewRequestOnMainThread),
                         main_thread_task_runner);

  // Inject additional optional interfaces.
  if (g_register_interfaces_callback.Get()) {
    std::move(g_register_interfaces_callback.Get())
        .Run(registry, main_thread_task_runner);
  }
}

void SetRegisterInterfacesCallback(RegisterInterfacesCallback callback) {
  g_register_interfaces_callback.Get() = std::move(callback);
}

}  // namespace mojo_interface_factory

}  // namespace ash
