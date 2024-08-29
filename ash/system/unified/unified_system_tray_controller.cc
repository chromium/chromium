// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_controller.h"

#include "ash/capture_mode/capture_mode_feature_pod_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/pagination/pagination_controller.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/public/cpp/update_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/accessibility/accessibility_feature_pod_controller.h"
#include "ash/system/accessibility/unified_accessibility_detailed_view_controller.h"
#include "ash/system/audio/unified_audio_detailed_view_controller.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/audio/unified_volume_view.h"
#include "ash/system/bluetooth/bluetooth_detailed_view_controller.h"
#include "ash/system/bluetooth/bluetooth_feature_pod_controller.h"
#include "ash/system/brightness/quick_settings_display_detailed_view_controller.h"
#include "ash/system/brightness/unified_brightness_slider_controller.h"
#include "ash/system/camera/autozoom_feature_pod_controller.h"
#include "ash/system/cast/cast_feature_pod_controller.h"
#include "ash/system/cast/unified_cast_detailed_view_controller.h"
#include "ash/system/dark_mode/dark_mode_feature_pod_controller.h"
#include "ash/system/focus_mode/focus_mode_detailed_view_controller.h"
#include "ash/system/focus_mode/focus_mode_feature_pod_controller.h"
#include "ash/system/hotspot/hotspot_detailed_view_controller.h"
#include "ash/system/hotspot/hotspot_feature_pod_controller.h"
#include "ash/system/ime/ime_feature_pod_controller.h"
#include "ash/system/ime/unified_ime_detailed_view_controller.h"
#include "ash/system/locale/locale_feature_pod_controller.h"
#include "ash/system/locale/unified_locale_detailed_view_controller.h"
#include "ash/system/media/media_tray.h"
#include "ash/system/media/unified_media_controls_detailed_view_controller.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/model/update_model.h"
#include "ash/system/nearby_share/nearby_share_detailed_view_controller.h"
#include "ash/system/nearby_share/nearby_share_feature_pod_controller.h"
#include "ash/system/network/network_detailed_view_controller.h"
#include "ash/system/network/network_feature_pod_controller.h"
#include "ash/system/network/unified_vpn_detailed_view_controller.h"
#include "ash/system/network/vpn_feature_pod_controller.h"
#include "ash/system/night_light/night_light_feature_pod_controller.h"
#include "ash/system/privacy_screen/privacy_screen_feature_pod_controller.h"
#include "ash/system/rotation/rotation_lock_feature_pod_controller.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/time/unified_calendar_view_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/deferred_update_dialog.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/feature_tiles_container_view.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/quick_settings_view.h"
#include "ash/system/unified/quiet_mode_feature_pod_controller.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/system/unified/user_chooser_detailed_view_controller.h"
#include "ash/wm/lock_state_controller.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "components/global_media_controls/public/constants.h"
#include "media/base/media_switches.h"
#include "media/capture/video/chromeos/video_capture_features_chromeos.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/compositor/compositor.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"

namespace ash {

// TODO(amehfooz): Add histograms for pagination metrics in system tray.
void RecordPageSwitcherSourceByEventType(ui::EventType type) {}

UnifiedSystemTrayController::UnifiedSystemTrayController(
    scoped_refptr<UnifiedSystemTrayModel> model,
    UnifiedSystemTrayBubble* bubble,
    views::View* owner_view)
    : model_(model), bubble_(bubble) {
  model_->pagination_model()->SetTransitionDurations(base::Milliseconds(250),
                                                     base::Milliseconds(50));

  pagination_controller_ = std::make_unique<PaginationController>(
      model_->pagination_model(), PaginationController::SCROLL_AXIS_HORIZONTAL,
      base::BindRepeating(&RecordPageSwitcherSourceByEventType));
}

UnifiedSystemTrayController::~UnifiedSystemTrayController() = default;

void UnifiedSystemTrayController::AddObserver(Observer* observer) {
  if (observer) {
    observers_.AddObserver(observer);
  }
}

void UnifiedSystemTrayController::RemoveObserver(Observer* observer) {
  if (observer) {
    observers_.RemoveObserver(observer);
  }
}

std::unique_ptr<QuickSettingsView>
UnifiedSystemTrayController::CreateQuickSettingsView(int max_height) {
  DCHECK(!quick_settings_view_);
  auto qs_view = std::make_unique<QuickSettingsView>(this);
  quick_settings_view_ = qs_view.get();

  if (!Shell::Get()->session_controller()->IsScreenLocked() &&
      !MediaTray::IsPinnedToShelf()) {
    media_view_controller_ =
        std::make_unique<QuickSettingsMediaViewController>(this);
    qs_view->AddMediaView(media_view_controller_->CreateView());
  }

  volume_slider_controller_ =
      std::make_unique<UnifiedVolumeSliderController>(this);
  unified_volume_view_ =
      qs_view->AddSliderView(volume_slider_controller_->CreateView());
  views::AsViewClass<QuickSettingsSlider>(
      views::AsViewClass<UnifiedVolumeView>(unified_volume_view_)->slider())
      ->set_is_toggleable_volume_slider(true);

  brightness_slider_controller_ =
      std::make_unique<UnifiedBrightnessSliderController>(
          model_, views::Button::PressedCallback(base::BindRepeating(
                      &UnifiedSystemTrayController::ShowDisplayDetailedView,
                      base::Unretained(this))));
  unified_brightness_view_ =
      qs_view->AddSliderView(brightness_slider_controller_->CreateView());

  qs_view->SetMaxHeight(max_height);

  // Feature Tiles are added last because the amount of rows depends on the
  // available height.
  InitFeatureTiles();

  return qs_view;
}

void UnifiedSystemTrayController::HandleSignOutAction() {
  base::RecordAction(base::UserMetricsAction("StatusArea_SignOut"));
  if (Shell::Get()->session_controller()->IsDemoSession()) {
    base::RecordAction(base::UserMetricsAction("DemoMode.ExitFromSystemTray"));
  }

  if (ShouldShowDeferredUpdateDialog()) {
    DeferredUpdateDialog::CreateDialog(
        DeferredUpdateDialog::Action::kSignOut,
        base::BindOnce(
            &LockStateController::RequestSignOut,
            base::Unretained(Shell::Get()->lock_state_controller())));
  } else {
    Shell::Get()->lock_state_controller()->RequestSignOut();
  }
}

void UnifiedSystemTrayController::HandleLockAction() {
  base::RecordAction(base::UserMetricsAction("Tray_LockScreen"));
  Shell::Get()->session_controller()->LockScreen();
}

void UnifiedSystemTrayController::HandleSettingsAction() {
  base::RecordAction(base::UserMetricsAction("Tray_Settings"));
  Shell::Get()->system_tray_model()->client()->ShowSettings(
      display::Screen::GetScreen()
          ->GetDisplayNearestView(
              quick_settings_view_->GetWidget()->GetNativeView())
          .id());
}

void UnifiedSystemTrayController::HandlePowerAction() {
  base::RecordAction(base::UserMetricsAction("Tray_ShutDown"));

  if (ShouldShowDeferredUpdateDialog()) {
    DeferredUpdateDialog::CreateDialog(
        DeferredUpdateDialog::Action::kShutDown,
        base::BindOnce(&LockStateController::RequestShutdown,
                       base::Unretained(Shell::Get()->lock_state_controller()),
                       ShutdownReason::TRAY_SHUT_DOWN_BUTTON));
  } else {
    Shell::Get()->lock_state_controller()->RequestShutdown(
        ShutdownReason::TRAY_SHUT_DOWN_BUTTON);
    CloseBubble();
  }
}

void UnifiedSystemTrayController::HandlePageSwitchAction(int page) {
  // TODO(amehfooz) Record Pagination Metrics here.
  model_->pagination_model()->SelectPage(page, true);
}

void UnifiedSystemTrayController::HandleOpenDateTimeSettingsAction() {
  ClockModel* model = Shell::Get()->system_tray_model()->clock();

  if (Shell::Get()->session_controller()->ShouldEnableSettings()) {
    model->ShowDateSettings();
  } else if (model->can_set_time()) {
    model->ShowSetTimeDialog();
  }
}

void UnifiedSystemTrayController::HandleOpenPowerSettingsAction() {
  ClockModel* model = Shell::Get()->system_tray_model()->clock();

  if (Shell::Get()->session_controller()->ShouldEnableSettings()) {
    model->ShowPowerSettings();
  }
}

void UnifiedSystemTrayController::HandleEnterpriseInfoAction() {
  Shell::Get()->system_tray_model()->client()->ShowEnterpriseInfo();
}

void UnifiedSystemTrayController::ShowUserChooserView() {
  if (!UserChooserDetailedViewController::IsUserChooserEnabled()) {
    return;
  }
  ShowDetailedView(std::make_unique<UserChooserDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowNearbyShareDetailedView() {
  ShowDetailedView(std::make_unique<NearbyShareDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowNetworkDetailedView() {
  base::RecordAction(base::UserMetricsAction("StatusArea_Network_Detailed"));
  ShowDetailedView(std::make_unique<NetworkDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowHotspotDetailedView() {
  ShowDetailedView(std::make_unique<HotspotDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowBluetoothDetailedView() {
  base::RecordAction(base::UserMetricsAction("StatusArea_Bluetooth_Detailed"));
  ShowDetailedView(std::make_unique<BluetoothDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowCastDetailedView() {
  base::RecordAction(base::UserMetricsAction("StatusArea_Cast_Detailed"));
  ShowDetailedView(std::make_unique<UnifiedCastDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowAccessibilityDetailedView() {
  base::RecordAction(
      base::UserMetricsAction("StatusArea_Accessability_DetailedView"));
  ShowDetailedView(
      std::make_unique<UnifiedAccessibilityDetailedViewController>(this));
  showing_accessibility_detailed_view_ = true;
}

void UnifiedSystemTrayController::ShowFocusModeDetailedView() {
  base::RecordAction(base::UserMetricsAction("StatusArea_FocusMode_Detailed"));
  ShowDetailedView(std::make_unique<FocusModeDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowVPNDetailedView() {
  base::RecordAction(base::UserMetricsAction("StatusArea_VPN_Detailed"));
  ShowDetailedView(std::make_unique<UnifiedVPNDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowIMEDetailedView() {
  ShowDetailedView(std::make_unique<UnifiedIMEDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowLocaleDetailedView() {
  ShowDetailedView(std::make_unique<UnifiedLocaleDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowAudioDetailedView() {
  ShowDetailedView(std::make_unique<UnifiedAudioDetailedViewController>(this));
  showing_audio_detailed_view_ = true;
}

void UnifiedSystemTrayController::ShowDisplayDetailedView() {
  ShowDetailedView(
      std::make_unique<QuickSettingsDisplayDetailedViewController>(this));
  showing_display_detailed_view_ = true;
}

void UnifiedSystemTrayController::ShowCalendarView(
    calendar_metrics::CalendarViewShowSource show_source,
    calendar_metrics::CalendarEventSource event_source) {
  calendar_metrics::RecordCalendarShowMetrics(show_source, event_source);
  ShowDetailedView(std::make_unique<UnifiedCalendarViewController>());

  showing_calendar_view_ = true;
  showing_accessibility_detailed_view_ = false;
  showing_audio_detailed_view_ = false;
  showing_display_detailed_view_ = false;

  for (auto& observer : observers_) {
    observer.OnOpeningCalendarView();
  }
}

void UnifiedSystemTrayController::ShowMediaControlsDetailedView(
    global_media_controls::GlobalMediaControlsEntryPoint entry_point,
    const std::string& show_devices_for_item_id) {
  ShowDetailedView(std::make_unique<UnifiedMediaControlsDetailedViewController>(
      this, entry_point, show_devices_for_item_id));
}

void UnifiedSystemTrayController::TransitionToMainView(bool restore_focus) {
  if (!detailed_view_controller_) {
    return;
  }

  if (showing_calendar_view_) {
    showing_calendar_view_ = false;
    for (auto& observer : observers_) {
      observer.OnTransitioningFromCalendarToMainView();
    }
  }

  showing_accessibility_detailed_view_ = false;
  showing_audio_detailed_view_ = false;
  showing_display_detailed_view_ = false;

  // Transfer `detailed_view_controller_` to a scoped object, which will be
  // destroyed once it's out of this method's scope (after resetting
  // `quick_settings_view_`'s `detailed_view_`). Because the detailed view has a
  // reference to its `detailed_view_controller_` which is used in shutdown.
  auto scoped_detailed_view_controller = std::move(detailed_view_controller_);

  bubble_->UpdateBubbleHeight(/*is_showing_detiled_view=*/false);
  quick_settings_view_->ResetDetailedView();
  if (restore_focus) {
    quick_settings_view_->RestoreFocus();
  }
  UpdateBubble();
}

void UnifiedSystemTrayController::CloseBubble() {
  if (quick_settings_view_->GetWidget()) {
    quick_settings_view_->GetWidget()->CloseNow();
  }
}

void UnifiedSystemTrayController::OnAudioSettingsButtonClicked() {
  ShowAudioDetailedView();
}

void UnifiedSystemTrayController::SetShowMediaView(bool show_media_view) {
  quick_settings_view_->SetShowMediaView(show_media_view);
}

void UnifiedSystemTrayController::InitFeatureTiles() {
  std::vector<std::unique_ptr<FeatureTile>> tiles;

  auto create_tile =
      [](ViewID tile_id, std::unique_ptr<FeaturePodControllerBase> controller,
         std::vector<std::unique_ptr<FeaturePodControllerBase>>& controllers,
         std::vector<std::unique_ptr<FeatureTile>>& tiles,
         bool compact = false) {
        std::unique_ptr<FeatureTile> tile = controller->CreateTile(compact);
        tile->SetID(static_cast<int>(tile_id));
        tiles.push_back(std::move(tile));
        controllers.push_back(std::move(controller));
      };

  create_tile(VIEW_ID_FEATURE_TILE_NETWORK,
              std::make_unique<NetworkFeaturePodController>(this),
              feature_pod_controllers_, tiles);

  // CaptureMode and QuietMode tiles will be compact if both are visible.
  bool capture_and_quiet_tiles_are_compact =
      CaptureModeFeaturePodController::CalculateButtonVisibility() &&
      QuietModeFeaturePodController::CalculateButtonVisibility();
  create_tile(VIEW_ID_FEATURE_TILE_SCREEN_CAPTURE,
              std::make_unique<CaptureModeFeaturePodController>(this),
              feature_pod_controllers_, tiles,
              capture_and_quiet_tiles_are_compact);
  create_tile(VIEW_ID_FEATURE_TILE_DND,
              std::make_unique<QuietModeFeaturePodController>(),
              feature_pod_controllers_, tiles,
              capture_and_quiet_tiles_are_compact);
  create_tile(VIEW_ID_FEATURE_TILE_BLUETOOTH,
              std::make_unique<BluetoothFeaturePodController>(this),
              feature_pod_controllers_, tiles);

  // Cast and RotationLock tiles will be compact if both are visible.
  bool cast_and_rotation_tiles_are_compact =
      CastFeaturePodController::CalculateButtonVisibility() &&
      RotationLockFeaturePodController::CalculateButtonVisibility();
  create_tile(VIEW_ID_FEATURE_TILE_CAST,
              std::make_unique<CastFeaturePodController>(this),
              feature_pod_controllers_, tiles,
              cast_and_rotation_tiles_are_compact);
  create_tile(VIEW_ID_FEATURE_TILE_AUTOROTATE,
              std::make_unique<RotationLockFeaturePodController>(),
              feature_pod_controllers_, tiles,
              cast_and_rotation_tiles_are_compact);
  create_tile(VIEW_ID_FEATURE_TILE_ACCESSIBILITY,
              std::make_unique<AccessibilityFeaturePodController>(this),
              feature_pod_controllers_, tiles);
  create_tile(VIEW_ID_FEATURE_TILE_HOTSPOT,
              std::make_unique<HotspotFeaturePodController>(this),
              feature_pod_controllers_, tiles);
  if (features::IsFocusModeEnabled()) {
    create_tile(VIEW_ID_FEATURE_TILE_FOCUS_MODE,
                std::make_unique<FocusModeFeaturePodController>(this),
                feature_pod_controllers_, tiles);
  }
  create_tile(VIEW_ID_FEATURE_TILE_NEARBY_SHARE,
              std::make_unique<NearbyShareFeaturePodController>(this),
              feature_pod_controllers_, tiles);
  create_tile(VIEW_ID_FEATURE_TILE_LOCALE,
              std::make_unique<LocaleFeaturePodController>(this),
              feature_pod_controllers_, tiles);
  create_tile(VIEW_ID_FEATURE_TILE_IME,
              std::make_unique<IMEFeaturePodController>(this),
              feature_pod_controllers_, tiles);
  if (media::ShouldEnableAutoFraming()) {
    create_tile(VIEW_ID_FEATURE_TILE_AUTOZOOM,
                std::make_unique<AutozoomFeaturePodController>(),
                feature_pod_controllers_, tiles);
  }
  create_tile(VIEW_ID_FEATURE_TILE_VPN,
              std::make_unique<VPNFeaturePodController>(this),
              feature_pod_controllers_, tiles);
  create_tile(VIEW_ID_FEATURE_TILE_PRIVACY_SCREEN,
              std::make_unique<PrivacyScreenFeaturePodController>(),
              feature_pod_controllers_, tiles);

  quick_settings_view_->AddTiles(std::move(tiles));

  quick_settings_metrics_util::RecordQsFeaturePodCount(
      quick_settings_view_->feature_tiles_container()
          ->GetVisibleFeatureTileCount(),
      display::Screen::GetScreen()->InTabletMode());
}

void UnifiedSystemTrayController::ShowDetailedView(
    std::unique_ptr<DetailedViewController> controller) {
  views::FocusManager* manager;
  quick_settings_view_->SaveFocus();
  manager = quick_settings_view_->GetFocusManager();

  if (manager && manager->GetFocusedView()) {
    manager->ClearFocus();
  }

  showing_accessibility_detailed_view_ = false;
  showing_audio_detailed_view_ = false;
  showing_display_detailed_view_ = false;
  bubble_->UpdateBubbleHeight(/*is_showing_detiled_view=*/true);

  ShutDownDetailedViewController();
  quick_settings_view_->SetDetailedView(controller->CreateView());

  detailed_view_controller_ = std::move(controller);

  // `bubble_` may be null in tests.
  if (bubble_) {
    UpdateBubble();
    // Notify accessibility features that a new view is showing.
    bubble_->NotifyAccessibilityEvent(ax::mojom::Event::kShow, true);
  }
}

bool UnifiedSystemTrayController::IsDetailedViewShown() const {
  if (quick_settings_view_) {
    return quick_settings_view_->IsDetailedViewShown();
  }
  return false;
}

void UnifiedSystemTrayController::UpdateBubble() {
  if (!bubble_) {
    return;
  }
  bubble_->UpdateBubble();
}

bool UnifiedSystemTrayController::ShouldShowDeferredUpdateDialog() const {
  return Shell::Get()->system_tray_model()->update_model()->update_deferred() ==
         DeferredUpdateState::kShowDialog;
}

void UnifiedSystemTrayController::ShutDownDetailedViewController() {
  if (detailed_view_controller_) {
    detailed_view_controller_->ShutDown();
  }
}

}  // namespace ash
