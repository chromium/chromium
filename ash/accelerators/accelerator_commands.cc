// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands.h"

#include "accelerator_notifications.h"
#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/display/privacy_screen_controller.h"
#include "ash/focus_cycler.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/media/media_controller_impl.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/window_rotation.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/floating_accessibility_controller.h"
#include "ash/system/brightness_control_delegate.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/keyboard_brightness_control_delegate.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/time/calendar_model.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/touch/touch_hud_debug.h"
#include "ash/wm/desks/desks_animations.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/wm/desks/chromeos_desks_histogram_enums.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/widget.h"

// Keep the functions in this file in alphabetical order.
namespace ash {
namespace accelerators {

namespace {

using ::base::UserMetricsAction;

// Percent by which the volume should be changed when a volume key is pressed.
constexpr double kStepPercentage = 4.0;
constexpr char kVirtualDesksToastId[] = "virtual_desks_toast";

views::Widget* FindPipWidget() {
  return Shell::Get()->focus_cycler()->FindWidget(
      base::BindRepeating([](views::Widget* widget) {
        return WindowState::Get(widget->GetNativeWindow())->IsPip();
      }));
}

PaletteTray* GetPaletteTray() {
  return Shelf::ForWindow(Shell::GetRootWindowForNewWindows())
      ->GetStatusAreaWidget()
      ->palette_tray();
}

// Return false if the accessibility shortcuts have been disabled, or if
// the accessibility feature itself associated with |accessibility_pref_name|
// is being enforced by the administrator.
bool IsAccessibilityShortcutEnabled(
    const std::string& accessibility_pref_name) {
  Shell* shell = Shell::Get();
  return shell->accessibility_controller()->accessibility_shortcuts_enabled() &&
         !shell->session_controller()
              ->GetActivePrefService()
              ->IsManagedPreference(accessibility_pref_name);
}

void RecordWindowSnapAcceleratorAction(WindowSnapAcceleratorAction action) {
  UMA_HISTOGRAM_ENUMERATION(kAccelWindowSnap, action);
}

void SetDockedMagnifierEnabled(bool enabled) {
  Shell* shell = Shell::Get();
  // Check that the attempt to change the value of the accessibility feature
  // will be done only when the accessibility shortcuts are enabled, and
  // the feature isn't being enforced by the administrator.
  DCHECK(IsAccessibilityShortcutEnabled(prefs::kDockedMagnifierEnabled));

  shell->docked_magnifier_controller()->SetEnabled(enabled);

  RemoveDockedMagnifierNotification();
  if (shell->docked_magnifier_controller()->GetEnabled()) {
    ShowDockedMagnifierNotification();
  }
}

void SetFullscreenMagnifierEnabled(bool enabled) {
  // TODO (afakhry): Move the below into a single call (crbug/817157).
  // Necessary to make magnification controller in ash observe changes to the
  // prefs itself.
  Shell* shell = Shell::Get();
  // Check that the attempt to change the value of the accessibility feature
  // will be done only when the accessibility shortcuts are enabled, and
  // the feature isn't being enforced by the administrator.
  DCHECK(IsAccessibilityShortcutEnabled(
      prefs::kAccessibilityScreenMagnifierEnabled));

  shell->accessibility_controller()->fullscreen_magnifier().SetEnabled(enabled);

  RemoveFullscreenMagnifierNotification();
  if (shell->fullscreen_magnifier_controller()->IsEnabled()) {
    ShowFullscreenMagnifierNotification();
  }
}

void SetHighContrastEnabled(bool enabled) {
  Shell* shell = Shell::Get();
  // Check that the attempt to change the value of the accessibility feature
  // will be done only when the accessibility shortcuts are enabled, and
  // the feature isn't being enforced by the administrator.
  DCHECK(
      IsAccessibilityShortcutEnabled(prefs::kAccessibilityHighContrastEnabled));

  shell->accessibility_controller()->high_contrast().SetEnabled(enabled);

  RemoveHighContrastNotification();
  if (shell->accessibility_controller()->high_contrast().enabled()) {
    ShowHighContrastNotification();
  }
}

void ShowToast(const std::string& id,
               ToastCatalogName catalog_name,
               const std::u16string& text) {
  ToastData toast(id, catalog_name, text, ToastData::kDefaultToastDuration,
                  /*visible_on_lock_screen=*/true);
  Shell::Get()->toast_manager()->Show(toast);
}

void HandleToggleSystemTrayBubbleInternal(bool focus_message_center) {
  aura::Window* target_root = Shell::GetRootWindowForNewWindows();
  UnifiedSystemTray* tray = RootWindowController::ForWindow(target_root)
                                ->GetStatusAreaWidget()
                                ->unified_system_tray();
  if (tray->IsBubbleShown()) {
    tray->CloseBubble();
  } else {
    tray->ShowBubble();
    tray->ActivateBubble();

    if (focus_message_center)
      tray->FocusMessageCenter(false, true);
  }
}

// Enters capture mode image type with |source|.
void EnterImageCaptureMode(CaptureModeSource source,
                           CaptureModeEntryType entry_type) {
  auto* capture_mode_controller = CaptureModeController::Get();
  capture_mode_controller->SetSource(source);
  capture_mode_controller->SetType(CaptureModeType::kImage);
  capture_mode_controller->Start(entry_type);
}

}  // namespace

void ActivateDeskAtIndex(AcceleratorAction action) {
  DCHECK_GE(action, DESKS_ACTIVATE_0);
  DCHECK_LE(action, DESKS_ACTIVATE_7);
  const size_t target_index = action - DESKS_ACTIVATE_0;
  auto* desks_controller = DesksController::Get();
  // Only 1 desk animation can occur at a time so ignore this action if there's
  // an ongoing desk animation.
  if (desks_controller->AreDesksBeingModified())
    return;

  const auto& desks = desks_controller->desks();
  if (target_index < desks.size()) {
    desks_controller->ActivateDesk(
        desks[target_index].get(),
        DesksSwitchSource::kIndexedDeskSwitchShortcut);
  } else {
    for (auto* root : Shell::GetAllRootWindows())
      desks_animations::PerformHitTheWallAnimation(root, /*going_left=*/false);
  }
}

void ActiveMagnifierZoom(int delta_index) {
  if (Shell::Get()->fullscreen_magnifier_controller()->IsEnabled()) {
    Shell::Get()->fullscreen_magnifier_controller()->StepToNextScaleValue(
        delta_index);
    return;
  }

  if (Shell::Get()->docked_magnifier_controller()->GetEnabled()) {
    Shell::Get()->docked_magnifier_controller()->StepToNextScaleValue(
        delta_index);
  }
}

void BrightnessDown() {
  BrightnessControlDelegate* delegate =
      Shell::Get()->brightness_control_delegate();
  if (delegate)
    delegate->HandleBrightnessDown();
}

void BrightnessUp() {
  BrightnessControlDelegate* delegate =
      Shell::Get()->brightness_control_delegate();
  if (delegate)
    delegate->HandleBrightnessUp();
}

void CycleUser(CycleUserDirection direction) {
  Shell::Get()->session_controller()->CycleActiveUser(direction);
}

void DumpCalendarModel() {
  Shell::Get()->system_tray_model()->calendar_model()->DebugDump();
}

void CycleBackwardMru() {
  Shell::Get()->window_cycle_controller()->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kBackward);
}

void FocusPip() {
  auto* widget = FindPipWidget();
  if (widget)
    Shell::Get()->focus_cycler()->FocusWidget(widget);
}

void CycleForwardMru() {
  Shell::Get()->window_cycle_controller()->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
}

void DisableCapsLock() {
  Shell::Get()->ime_controller()->SetCapsLockEnabled(false);
}

void FocusShelf() {
  if (Shell::Get()->session_controller()->IsRunningInAppMode()) {
    // If floating accessibility menu is shown, focus on it instead of the
    // shelf.
    FloatingAccessibilityController* floating_menu =
        Shell::Get()->accessibility_controller()->GetFloatingMenuController();
    if (floating_menu) {
      floating_menu->FocusOnMenu();
    }
    return;
  }

  // TODO(jamescook): Should this be GetRootWindowForNewWindows()?
  // Focus the home button.
  Shelf* shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());
  shelf->shelf_focus_cycler()->FocusNavigation(false /* lastElement */);
}

void FocusCameraPreview() {
  auto* camera_controller = CaptureModeController::Get()->camera_controller();
  DCHECK(camera_controller);
  camera_controller->PseudoFocusCameraPreview();
}

void KeyboardBrightnessDown() {
  KeyboardBrightnessControlDelegate* delegate =
      Shell::Get()->keyboard_brightness_control_delegate();
  if (delegate)
    delegate->HandleKeyboardBrightnessDown();
}

void KeyboardBrightnessUp() {
  KeyboardBrightnessControlDelegate* delegate =
      Shell::Get()->keyboard_brightness_control_delegate();
  if (delegate)
    delegate->HandleKeyboardBrightnessUp();
}

void LaunchAppN(int n) {
  Shelf::LaunchShelfItem(n);
}

void LaunchLastApp() {
  Shelf::LaunchShelfItem(-1);
}

void LockScreen() {
  Shell::Get()->session_controller()->LockScreen();
}

void MaybeTakePartialScreenshot() {
  // If a capture mode session is already running, this shortcut will be treated
  // as a no-op.
  if (CaptureModeController::Get()->IsActive())
    return;
  base::RecordAction(base::UserMetricsAction("Accel_Take_Partial_Screenshot"));
  EnterImageCaptureMode(CaptureModeSource::kRegion,
                        CaptureModeEntryType::kAccelTakePartialScreenshot);
}

void MaybeTakeWindowScreenshot() {
  // If a capture mode session is already running, this shortcut will be treated
  // as a no-op.
  if (CaptureModeController::Get()->IsActive())
    return;
  base::RecordAction(base::UserMetricsAction("Accel_Take_Window_Screenshot"));
  EnterImageCaptureMode(CaptureModeSource::kWindow,
                        CaptureModeEntryType::kAccelTakeWindowScreenshot);
}

void MediaFastForward() {
  Shell::Get()->media_controller()->HandleMediaSeekForward();
}

void MediaNextTrack() {
  Shell::Get()->media_controller()->HandleMediaNextTrack();
}

void MediaPause() {
  Shell::Get()->media_controller()->HandleMediaPause();
}

void MediaPlay() {
  Shell::Get()->media_controller()->HandleMediaPlay();
}

void MediaPlayPause() {
  Shell::Get()->media_controller()->HandleMediaPlayPause();
}

void MediaPrevTrack() {
  Shell::Get()->media_controller()->HandleMediaPrevTrack();
}
void MediaRewind() {
  Shell::Get()->media_controller()->HandleMediaSeekBackward();
}

void MediaStop() {
  Shell::Get()->media_controller()->HandleMediaStop();
}

void MicrophoneMuteToggle() {
  auto* const audio_handler = CrasAudioHandler::Get();
  const bool mute = !audio_handler->IsInputMuted();

  if (mute)
    base::RecordAction(base::UserMetricsAction("Keyboard_Microphone_Muted"));
  else
    base::RecordAction(base::UserMetricsAction("Keyboard_Microphone_Unmuted"));

  audio_handler->SetInputMute(mute);
}

void NewDesk() {
  auto* desks_controller = DesksController::Get();
  if (!desks_controller->CanCreateDesks()) {
    ShowToast(kVirtualDesksToastId, ToastCatalogName::kVirtualDesksLimitMax,
              l10n_util::GetStringUTF16(IDS_ASH_DESKS_MAX_NUM_REACHED));
    return;
  }

  if (desks_controller->AreDesksBeingModified())
    return;

  // Add a new desk and switch to it.
  const size_t new_desk_index = desks_controller->desks().size();
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  const Desk* desk = desks_controller->desks()[new_desk_index].get();
  desks_controller->ActivateDesk(desk, DesksSwitchSource::kNewDeskShortcut);
  base::RecordAction(base::UserMetricsAction("Accel_Desks_NewDesk"));
}

void NewIncognitoWindow() {
  NewWindowDelegate::GetPrimary()->NewWindow(
      /*is_incognito=*/true,
      /*should_trigger_session_restore=*/false);
}

void NewWindow() {
  NewWindowDelegate::GetPrimary()->NewWindow(
      /*is_incognito=*/false,
      /*should_trigger_session_restore=*/false);
}

void OpenCalculator() {
  NewWindowDelegate::GetInstance()->OpenCalculator();
}

void OpenCrosh() {
  NewWindowDelegate::GetInstance()->OpenCrosh();
}

void OpenDiagnostics() {
  NewWindowDelegate::GetInstance()->OpenDiagnostics();
}

void OpenFeedbackPage() {
  NewWindowDelegate::GetInstance()->OpenFeedbackPage();
}

void OpenFileManager() {
  NewWindowDelegate::GetInstance()->OpenFileManager();
}

void OpenHelp() {
  NewWindowDelegate::GetInstance()->OpenGetHelp();
}

void RemoveCurrentDesk() {
  if (window_util::IsAnyWindowDragged())
    return;

  auto* desks_controller = DesksController::Get();
  if (!desks_controller->CanRemoveDesks()) {
    ShowToast(kVirtualDesksToastId, ToastCatalogName::kVirtualDesksLimitMin,
              l10n_util::GetStringUTF16(IDS_ASH_DESKS_MIN_NUM_REACHED));
    return;
  }

  if (desks_controller->AreDesksBeingModified())
    return;

  // TODO(afakhry): Finalize the desk removal animation outside of overview with
  // UX. https://crbug.com/977434.
  desks_controller->RemoveDesk(desks_controller->active_desk(),
                               DesksCreationRemovalSource::kKeyboard,
                               DeskCloseType::kCombineDesks);
  base::RecordAction(base::UserMetricsAction("Accel_Desks_RemoveDesk"));
}

void ResetDisplayZoom() {
  base::RecordAction(base::UserMetricsAction("Accel_Scale_Ui_Reset"));
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  gfx::Point point = display::Screen::GetScreen()->GetCursorScreenPoint();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(point);
  display_manager->ResetDisplayZoom(display.id());
}

void RestoreTab() {
  NewWindowDelegate::GetPrimary()->RestoreTab();
}

void RotateActiveWindow() {
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window)
    return;
  // The rotation animation bases its target transform on the current
  // rotation and position. Since there could be an animation in progress
  // right now, queue this animation so when it starts it picks up a neutral
  // rotation and position. Use replace so we only enqueue one at a time.
  active_window->layer()->GetAnimator()->set_preemption_strategy(
      ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  active_window->layer()->GetAnimator()->StartAnimation(
      new ui::LayerAnimationSequence(
          std::make_unique<WindowRotation>(360, active_window->layer())));
}

void RotatePaneFocus(FocusCycler::Direction direction) {
  Shell::Get()->focus_cycler()->RotateFocus(direction);
}

void ShiftPrimaryDisplay() {
  display::DisplayManager* display_manager = Shell::Get()->display_manager();

  CHECK_GE(display_manager->GetNumDisplays(), 2U);

  const int64_t primary_display_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();

  const display::Displays& active_display_list =
      display_manager->active_display_list();

  auto primary_display_iter = base::ranges::find(
      active_display_list, primary_display_id, &display::Display::id);

  DCHECK(primary_display_iter != active_display_list.end());

  ++primary_display_iter;

  // If we've reach the end of |active_display_list|, wrap back around to the
  // front.
  if (primary_display_iter == active_display_list.end())
    primary_display_iter = active_display_list.begin();

  Shell::Get()->display_configuration_controller()->SetPrimaryDisplayId(
      primary_display_iter->id(), true /* throttle */);
}

void ShowEmojiPicker() {
  ui::ShowEmojiPanel();
}

void ShowKeyboardShortcutViewer() {
  NewWindowDelegate::GetInstance()->ShowKeyboardShortcutViewer();
}

void ShowStylusTools() {
  GetPaletteTray()->ShowBubble();
}

void ShowTaskManager() {
  NewWindowDelegate::GetInstance()->ShowTaskManager();
}

void Suspend() {
  chromeos::PowerManagerClient::Get()->RequestSuspend();
}

void ToggleAssignToAllDesk() {
  auto* active_window = window_util::GetActiveWindow();
  if (!active_window)
    return;

  // Only children of the desk container should have their assigned to all
  // desks state toggled to avoid interfering with special windows like
  // always-on-top windows, floated windows, etc.
  if (desks_util::IsActiveDeskContainer(active_window->parent())) {
    const bool is_already_visible_on_all_desks =
        desks_util::IsWindowVisibleOnAllWorkspaces(active_window);
    if (!is_already_visible_on_all_desks) {
      UMA_HISTOGRAM_ENUMERATION(
          chromeos::kDesksAssignToAllDesksSourceHistogramName,
          chromeos::DesksAssignToAllDesksSource::kKeyboardShortcut);
    }

    active_window->SetProperty(
        aura::client::kWindowWorkspaceKey,
        is_already_visible_on_all_desks
            ? aura::client::kWindowWorkspaceUnassignedWorkspace
            : aura::client::kWindowWorkspaceVisibleOnAllWorkspaces);
  }
}

void ToggleCalendar() {
  aura::Window* target_root = Shell::GetRootWindowForNewWindows();
  StatusAreaWidget* status_area_widget =
      RootWindowController::ForWindow(target_root)->GetStatusAreaWidget();
  UnifiedSystemTray* tray = status_area_widget->unified_system_tray();

  // If currently showing the calendar view, close it.
  if (tray->IsShowingCalendarView()) {
    tray->CloseBubble();
    return;
  }

  // If currently not showing the calendar view, show the bubble if needed then
  // show the calendar view.
  if (!tray->IsBubbleShown()) {
    // Set `DateTray` to be active prior to showing the bubble, this prevents
    // flashing of the status area. See crbug.com/1332603.
    status_area_widget->date_tray()->SetIsActive(true);
    tray->ShowBubble();
  }

  tray->bubble()->ShowCalendarView(
      calendar_metrics::CalendarViewShowSource::kAccelerator,
      calendar_metrics::CalendarEventSource::kKeyboard);
}

void ToggleCapsLock() {
  ImeControllerImpl* ime_controller = Shell::Get()->ime_controller();
  ime_controller->SetCapsLockEnabled(!ime_controller->IsCapsLockEnabled());
}

void ToggleClipboardHistory() {
  DCHECK(Shell::Get()->clipboard_history_controller());
  Shell::Get()->clipboard_history_controller()->ToggleMenuShownByAccelerator();
}

void ToggleDictation() {
  Shell::Get()->accessibility_controller()->ToggleDictationFromSource(
      DictationToggleSource::kKeyboard);
}

void ToggleDockedMagnifier() {
  const bool is_shortcut_enabled =
      IsAccessibilityShortcutEnabled(prefs::kDockedMagnifierEnabled);

  base::UmaHistogramBoolean(kAccessibilityDockedMagnifierShortcut,
                            is_shortcut_enabled);

  Shell* shell = Shell::Get();

  RemoveDockedMagnifierNotification();
  if (!is_shortcut_enabled) {
    ShowDockedMagnifierDisabledByAdminNotification(
        shell->docked_magnifier_controller()->GetEnabled());
    return;
  }

  DockedMagnifierController* docked_magnifier_controller =
      shell->docked_magnifier_controller();
  AccessibilityControllerImpl* accessibility_controller =
      shell->accessibility_controller();

  const bool current_enabled = docked_magnifier_controller->GetEnabled();
  const bool dialog_ever_accepted =
      accessibility_controller->docked_magnifier().WasDialogAccepted();

  if (!current_enabled && !dialog_ever_accepted) {
    shell->accelerator_controller()->MaybeShowConfirmationDialog(
        IDS_ASH_DOCKED_MAGNIFIER_TITLE, IDS_ASH_DOCKED_MAGNIFIER_BODY,
        base::BindOnce([]() {
          Shell::Get()
              ->accessibility_controller()
              ->docked_magnifier()
              .SetDialogAccepted();
          SetDockedMagnifierEnabled(true);
        }),
        base::DoNothing());
  } else {
    SetDockedMagnifierEnabled(!current_enabled);
  }
}

void ToggleFloating() {
  DCHECK(chromeos::wm::features::IsFloatWindowEnabled());
  aura::Window* window = window_util::GetActiveWindow();
  DCHECK(window);
  // TODO(sammiequon|shidi): Add some UI like a bounce if a window cannot be
  // floated.
  Shell::Get()->float_controller()->ToggleFloat(window);
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Floating"));
}

void ToggleFullscreen() {
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window)
    return;
  const WMEvent event(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(active_window)->OnWMEvent(&event);
}

void ToggleFullscreenMagnifier() {
  const bool is_shortcut_enabled = IsAccessibilityShortcutEnabled(
      prefs::kAccessibilityScreenMagnifierEnabled);

  base::UmaHistogramBoolean(kAccessibilityScreenMagnifierShortcut,
                            is_shortcut_enabled);

  Shell* shell = Shell::Get();

  RemoveFullscreenMagnifierNotification();
  if (!is_shortcut_enabled) {
    ShowFullscreenMagnifierDisabledByAdminNotification(
        shell->fullscreen_magnifier_controller()->IsEnabled());
    return;
  }

  FullscreenMagnifierController* magnification_controller =
      shell->fullscreen_magnifier_controller();
  AccessibilityControllerImpl* accessibility_controller =
      shell->accessibility_controller();

  const bool current_enabled = magnification_controller->IsEnabled();
  const bool dialog_ever_accepted =
      accessibility_controller->fullscreen_magnifier().WasDialogAccepted();

  if (!current_enabled && !dialog_ever_accepted) {
    shell->accelerator_controller()->MaybeShowConfirmationDialog(
        IDS_ASH_SCREEN_MAGNIFIER_TITLE, IDS_ASH_SCREEN_MAGNIFIER_BODY,
        base::BindOnce([]() {
          Shell::Get()
              ->accessibility_controller()
              ->fullscreen_magnifier()
              .SetDialogAccepted();
          SetFullscreenMagnifierEnabled(true);
        }),
        base::DoNothing());
  } else {
    SetFullscreenMagnifierEnabled(!current_enabled);
  }
}

void ToggleHighContrast() {
  const bool is_shortcut_enabled =
      IsAccessibilityShortcutEnabled(prefs::kAccessibilityHighContrastEnabled);

  base::UmaHistogramBoolean(kAccessibilityHighContrastShortcut,
                            is_shortcut_enabled);

  Shell* shell = Shell::Get();

  RemoveHighContrastNotification();
  if (!is_shortcut_enabled) {
    ShowHighContrastDisabledByAdminNotification(
        shell->accessibility_controller()->high_contrast().enabled());
    return;
  }

  AccessibilityControllerImpl* controller = shell->accessibility_controller();
  const bool current_enabled = controller->high_contrast().enabled();
  const bool dialog_ever_accepted =
      controller->high_contrast().WasDialogAccepted();

  if (!current_enabled && !dialog_ever_accepted) {
    shell->accelerator_controller()->MaybeShowConfirmationDialog(
        IDS_ASH_HIGH_CONTRAST_TITLE, IDS_ASH_HIGH_CONTRAST_BODY,
        base::BindOnce([]() {
          Shell::Get()
              ->accessibility_controller()
              ->high_contrast()
              .SetDialogAccepted();
          SetHighContrastEnabled(true);
        }),
        base::DoNothing());
  } else {
    SetHighContrastEnabled(!current_enabled);
  }
}

void ToggleSpokenFeedback() {
  const bool is_shortcut_enabled = IsAccessibilityShortcutEnabled(
      prefs::kAccessibilitySpokenFeedbackEnabled);

  base::UmaHistogramBoolean(kAccessibilitySpokenFeedbackShortcut,
                            is_shortcut_enabled);

  Shell* shell = Shell::Get();
  const bool old_value =
      shell->accessibility_controller()->spoken_feedback().enabled();

  RemoveSpokenFeedbackNotification();
  if (!is_shortcut_enabled) {
    ShowSpokenFeedbackDisabledByAdminNotification(old_value);
    return;
  }

  shell->accessibility_controller()->SetSpokenFeedbackEnabled(
      !old_value, A11Y_NOTIFICATION_SHOW);
}

void ToggleImeMenuBubble() {
  StatusAreaWidget* status_area_widget =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetStatusAreaWidget();
  if (status_area_widget) {
    ImeMenuTray* ime_menu_tray = status_area_widget->ime_menu_tray();
    if (!ime_menu_tray || !ime_menu_tray->GetVisible()) {
      // Do nothing when Ime tray is not being shown.
      return;
    }
    if (ime_menu_tray->GetBubbleView()) {
      ime_menu_tray->CloseBubble();
    } else {
      ime_menu_tray->ShowBubble();
    }
  }
}

void ToggleKeyboardBacklight() {
  KeyboardBrightnessControlDelegate* delegate =
      Shell::Get()->keyboard_brightness_control_delegate();
  delegate->HandleToggleKeyboardBacklight();
}

void ToggleMaximized() {
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window)
    return;
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Maximized"));
  WMEvent event(WM_EVENT_TOGGLE_MAXIMIZE);
  WindowState::Get(active_window)->OnWMEvent(&event);
}

bool ToggleMinimized() {
  aura::Window* window = window_util::GetActiveWindow();
  // Attempt to restore the window that would be cycled through next from
  // the launcher when there is no active window.
  if (!window) {
    // Do not unminimize a window on an inactive desk, since this will cause
    // desks to switch and that will be unintentional for the user.
    MruWindowTracker::WindowList mru_windows(
        Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk));
    if (!mru_windows.empty())
      WindowState::Get(mru_windows.front())->Activate();
    return true;
  }
  WindowState* window_state = WindowState::Get(window);
  if (!window_state->CanMinimize())
    return false;
  window_state->Minimize();
  return true;
}

void ToggleResizeLockMenu() {
  aura::Window* active_window = window_util::GetActiveWindow();
  auto* frame_view = ash::NonClientFrameViewAsh::Get(active_window);
  frame_view->GetToggleResizeLockMenuCallback().Run();
}

void ToggleMessageCenterBubble() {
  HandleToggleSystemTrayBubbleInternal(true /*focus_message_center*/);
}

void ToggleMirrorMode() {
  bool mirror = !Shell::Get()->display_manager()->IsInMirrorMode();
  Shell::Get()->display_configuration_controller()->SetMirrorMode(
      mirror, true /* throttle */);
}

void ToggleOverview() {
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller->InOverviewSession())
    overview_controller->EndOverview(OverviewEndAction::kAccelerator);
  else
    overview_controller->StartOverview(OverviewStartAction::kAccelerator);
}

void TogglePrivacyScreen() {
  PrivacyScreenController* controller =
      Shell::Get()->privacy_screen_controller();
  controller->SetEnabled(
      !controller->GetEnabled(),
      PrivacyScreenController::kToggleUISurfaceKeyboardShortcut);
}

void ToggleProjectorMarker() {
  auto* projector_controller = ProjectorController::Get();
  if (projector_controller) {
    projector_controller->ToggleAnnotationTray();
  }
}

void ToggleSystemTrayBubble() {
  HandleToggleSystemTrayBubbleInternal(false /*focus_message_center*/);
}

void ToggleUnifiedDesktop() {
  Shell::Get()->display_manager()->SetUnifiedDesktopEnabled(
      !Shell::Get()->display_manager()->unified_desktop_enabled());
}

void ToggleWifi() {
  Shell::Get()->system_tray_notifier()->NotifyRequestToggleWifi();
}

void TopWindowMinimizeOnBack() {
  WindowState::Get(window_util::GetTopWindow())->Minimize();
}

void TouchHudClear() {
  RootWindowController::ForTargetRootWindow()->touch_hud_debug()->Clear();
}

void TouchHudModeChange() {
  RootWindowController* controller =
      RootWindowController::ForTargetRootWindow();
  controller->touch_hud_debug()->ChangeToNextMode();
}

void UnpinWindow() {
  aura::Window* pinned_window =
      Shell::Get()->screen_pinning_controller()->pinned_window();
  if (pinned_window)
    WindowState::Get(pinned_window)->Restore();
}

void VolumeDown() {
  auto* audio_handler = CrasAudioHandler::Get();
  if (audio_handler->IsOutputMuted()) {
    audio_handler->SetOutputVolumePercent(0);
  } else {
    if (features::IsAudioPeripheralVolumeGranularityEnabled())
      audio_handler->DecreaseOutputVolumeByOneStep();
    else
      audio_handler->AdjustOutputVolumeByPercent(-kStepPercentage);

    if (audio_handler->IsOutputVolumeBelowDefaultMuteLevel())
      audio_handler->SetOutputMute(true);
    else
      AcceleratorController::PlayVolumeAdjustmentSound();
  }
}

void VolumeMute() {
  CrasAudioHandler::Get()->SetOutputMute(true);
}

void VolumeUp() {
  auto* audio_handler = CrasAudioHandler::Get();
  bool play_sound = false;
  if (audio_handler->IsOutputMuted()) {
    audio_handler->SetOutputMute(false);
    audio_handler->AdjustOutputVolumeToAudibleLevel();
    play_sound = true;
  } else {
    play_sound = audio_handler->GetOutputVolumePercent() != 100;
    if (features::IsAudioPeripheralVolumeGranularityEnabled())
      audio_handler->IncreaseOutputVolumeByOneStep();
    else
      audio_handler->AdjustOutputVolumeByPercent(kStepPercentage);
  }

  if (play_sound)
    AcceleratorController::PlayVolumeAdjustmentSound();
}

void WindowMinimize() {
  ToggleMinimized();
}

void WindowSnap(AcceleratorAction action) {
  Shell* shell = Shell::Get();
  const bool in_tablet = shell->tablet_mode_controller()->InTabletMode();
  const bool in_overview = shell->overview_controller()->InOverviewSession();
  if (action == WINDOW_CYCLE_SNAP_LEFT) {
    if (in_tablet) {
      RecordWindowSnapAcceleratorAction(
          WindowSnapAcceleratorAction::kCycleLeftSnapInTablet);
    } else if (in_overview) {
      RecordWindowSnapAcceleratorAction(
          WindowSnapAcceleratorAction::kCycleLeftSnapInClamshellOverview);
    } else {
      RecordWindowSnapAcceleratorAction(
          WindowSnapAcceleratorAction::kCycleLeftSnapInClamshellNoOverview);
    }
  } else {
    if (in_tablet) {
      RecordWindowSnapAcceleratorAction(
          WindowSnapAcceleratorAction::kCycleRightSnapInTablet);
    } else if (in_overview) {
      RecordWindowSnapAcceleratorAction(
          WindowSnapAcceleratorAction::kCycleRightSnapInClamshellOverview);
    } else {
      RecordWindowSnapAcceleratorAction(
          WindowSnapAcceleratorAction::kCycleRightSnapInClamshellNoOverview);
    }
  }

  const WindowSnapWMEvent event(action == WINDOW_CYCLE_SNAP_LEFT
                                    ? WM_EVENT_CYCLE_SNAP_PRIMARY
                                    : WM_EVENT_CYCLE_SNAP_SECONDARY);
  aura::Window* active_window = window_util::GetActiveWindow();
  DCHECK(active_window);

  auto* window_state = WindowState::Get(active_window);
  window_state->set_snap_action_source(
      WindowSnapActionSource::kKeyboardShortcutToSnap);
  window_state->OnWMEvent(&event);
}

bool ZoomDisplay(bool up) {
  if (up)
    base::RecordAction(base::UserMetricsAction("Accel_Scale_Ui_Up"));
  else
    base::RecordAction(base::UserMetricsAction("Accel_Scale_Ui_Down"));

  display::DisplayManager* display_manager = Shell::Get()->display_manager();

  gfx::Point point = display::Screen::GetScreen()->GetCursorScreenPoint();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(point);
  return display_manager->ZoomDisplay(display.id(), up);
}

}  // namespace accelerators
}  // namespace ash
