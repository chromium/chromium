// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_controller.h"

#include <algorithm>
#include <memory>

#include "ash/capture_mode/capture_mode_feature_pod_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/quick_settings_catalogs.h"
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
#include "ash/system/bluetooth/bluetooth_detailed_view_controller.h"
#include "ash/system/bluetooth/bluetooth_feature_pod_controller.h"
#include "ash/system/brightness/quick_settings_display_detailed_view_controller.h"
#include "ash/system/brightness/unified_brightness_slider_controller.h"
#include "ash/system/camera/autozoom_feature_pod_controller.h"
#include "ash/system/cast/cast_feature_pod_controller.h"
#include "ash/system/cast/unified_cast_detailed_view_controller.h"
#include "ash/system/dark_mode/dark_mode_feature_pod_controller.h"
#include "ash/system/hotspot/hotspot_detailed_view_controller.h"
#include "ash/system/hotspot/hotspot_feature_pod_controller.h"
#include "ash/system/ime/ime_feature_pod_controller.h"
#include "ash/system/ime/unified_ime_detailed_view_controller.h"
#include "ash/system/locale/locale_feature_pod_controller.h"
#include "ash/system/locale/unified_locale_detailed_view_controller.h"
#include "ash/system/media/media_tray.h"
#include "ash/system/media/quick_settings_media_view_controller.h"
#include "ash/system/media/unified_media_controls_detailed_view_controller.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/model/update_model.h"
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
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "ash/system/unified/feature_pods_container_view.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/feature_tiles_container_view.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/quick_settings_view.h"
#include "ash/system/unified/quiet_mode_feature_pod_controller.h"
#include "ash/system/unified/unified_notifier_settings_controller.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/system/unified/user_chooser_detailed_view_controller.h"
#include "ash/wm/focus_mode/focus_mode_feature_pod_controller.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "components/prefs/pref_service.h"
#include "media/base/media_switches.h"
#include "media/capture/video/chromeos/video_capture_features_chromeos.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/compositor/compositor.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/message_center/message_center.h"
#include "ui/views/widget/widget.h"

namespace ash {

// TODO(amehfooz): Add histograms for pagination metrics in system tray.
void RecordPageSwitcherSourceByEventType(ui::EventType type) {}

void ReportExpandAnimationSmoothness(int smoothness) {
  UMA_HISTOGRAM_PERCENTAGE(
      "ChromeOS.SystemTray.AnimationSmoothness."
      "TransitionToExpanded",
      smoothness);
}

void ReportCollapseAnimationSmoothness(int smoothness) {
  UMA_HISTOGRAM_PERCENTAGE(
      "ChromeOS.SystemTray.AnimationSmoothness."
      "TransitionToCollapsed",
      smoothness);
}

UnifiedSystemTrayController::UnifiedSystemTrayController(
    scoped_refptr<UnifiedSystemTrayModel> model,
    UnifiedSystemTrayBubble* bubble,
    views::View* owner_view)
    : views::AnimationDelegateViews(owner_view),
      model_(model),
      bubble_(bubble),
      active_user_prefs_(
          Shell::Get()->session_controller()->GetLastActiveUserPrefService()),
      animation_(std::make_unique<gfx::SlideAnimation>(this)) {
  LoadIsExpandedPref();

  const float animation_value = features::IsQsRevampEnabled()
                                    ? 1
                                    : (model_->IsExpandedOnOpen() ? 1.0 : 0.0);
  animation_->Reset(animation_value);
  animation_->SetSlideDuration(
      base::Milliseconds(kSystemMenuCollapseExpandAnimationDurationMs));
  animation_->SetTweenType(gfx::Tween::EASE_IN_OUT);

  model_->pagination_model()->SetTransitionDurations(base::Milliseconds(250),
                                                     base::Milliseconds(50));

  pagination_controller_ = std::make_unique<PaginationController>(
      model_->pagination_model(), PaginationController::SCROLL_AXIS_HORIZONTAL,
      base::BindRepeating(&RecordPageSwitcherSourceByEventType));
}

UnifiedSystemTrayController::~UnifiedSystemTrayController() {
  if (active_user_prefs_) {
    active_user_prefs_->SetBoolean(prefs::kSystemTrayExpanded,
                                   model_->IsExpandedOnOpen());
  }
}

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

// static
void UnifiedSystemTrayController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kSystemTrayExpanded,
                                /*default_value=*/true);
}

void UnifiedSystemTrayController::OnActiveUserPrefServiceChanged(
    PrefService* prefs) {
  active_user_prefs_ = prefs;
}

std::unique_ptr<UnifiedSystemTrayView>
UnifiedSystemTrayController::CreateUnifiedQuickSettingsView() {
  DCHECK(!unified_view_);
  auto unified_view =
      std::make_unique<UnifiedSystemTrayView>(this, model_->IsExpandedOnOpen());
  unified_view_ = unified_view.get();

  InitFeaturePods();

  if (!Shell::Get()->session_controller()->IsScreenLocked() &&
      !MediaTray::IsPinnedToShelf()) {
    media_controls_controller_ =
        std::make_unique<UnifiedMediaControlsController>(this);
    unified_view->AddMediaControlsView(
        media_controls_controller_->CreateView());
  }

  volume_slider_controller_ =
      std::make_unique<UnifiedVolumeSliderController>(this);
  unified_view->AddSliderView(volume_slider_controller_->CreateView());

  brightness_slider_controller_ =
      std::make_unique<UnifiedBrightnessSliderController>(
          model_, views::Button::PressedCallback(base::BindRepeating(
                      &UnifiedSystemTrayController::ShowDisplayDetailedView,
                      base::Unretained(this))));
  unified_view->AddSliderView(brightness_slider_controller_->CreateView());

  return unified_view;
}

std::unique_ptr<QuickSettingsView>
UnifiedSystemTrayController::CreateQuickSettingsView(int max_height) {
  DCHECK(!quick_settings_view_);
  auto qs_view = std::make_unique<QuickSettingsView>(this);
  quick_settings_view_ = qs_view.get();

  if (!Shell::Get()->session_controller()->IsScreenLocked() &&
      !MediaTray::IsPinnedToShelf()) {
    if (base::FeatureList::IsEnabled(
            media::kGlobalMediaControlsCrOSUpdatedUI)) {
      media_view_controller_ =
          std::make_unique<QuickSettingsMediaViewController>(this);
      qs_view->AddMediaView(media_view_controller_->CreateView());
    } else {
      media_controls_controller_ =
          std::make_unique<UnifiedMediaControlsController>(this);
      qs_view->AddMediaControlsView(media_controls_controller_->CreateView());
    }
  }

  volume_slider_controller_ =
      std::make_unique<UnifiedVolumeSliderController>(this);
  unified_volume_view_ =
      qs_view->AddSliderView(volume_slider_controller_->CreateView());

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
        base::BindOnce(&SessionControllerImpl::RequestSignOut,
                       base::Unretained(Shell::Get()->session_controller())));
  } else {
    Shell::Get()->session_controller()->RequestSignOut();
  }
}

void UnifiedSystemTrayController::HandleLockAction() {
  base::RecordAction(base::UserMetricsAction("Tray_LockScreen"));
  Shell::Get()->session_controller()->LockScreen();
}

void UnifiedSystemTrayController::HandleSettingsAction() {
  base::RecordAction(base::UserMetricsAction("Tray_Settings"));
  if (features::IsQsRevampEnabled()) {
    Shell::Get()->system_tray_model()->client()->ShowSettings(
        display::Screen::GetScreen()
            ->GetDisplayNearestView(
                quick_settings_view_->GetWidget()->GetNativeView())
            .id());
    return;
  }
  Shell::Get()->system_tray_model()->client()->ShowSettings(
      display::Screen::GetScreen()
          ->GetDisplayNearestView(unified_view_->GetWidget()->GetNativeView())
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

void UnifiedSystemTrayController::ToggleExpanded() {
  if (features::IsQsRevampEnabled()) {
    return;
  }

  if (animation_->is_animating()) {
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("ChromeOS.SystemTray.ToggleExpanded",
                            TOGGLE_EXPANDED_TYPE_BY_BUTTON,
                            TOGGLE_EXPANDED_TYPE_COUNT);
  if (IsExpanded()) {
    StartAnimation(/*expand=*/false);
    // Expand message center when quick settings is collapsed.
    if (bubble_) {
      bubble_->ExpandMessageCenter();
    }
  } else {
    // Collapse the message center if screen height is limited after expanding
    // the quick settings to its full height.
    if (IsMessageCenterCollapseRequired()) {
      bubble_->CollapseMessageCenter();
    }
    StartAnimation(/*expand=*/true);
  }
}

void UnifiedSystemTrayController::BeginDrag(const gfx::PointF& location) {
  if (features::IsQsRevampEnabled()) {
    return;
  }

  UpdateDragThreshold();
  // Ignore swipe collapsing when a detailed view is shown as it's confusing.
  if (detailed_view_controller_) {
    return;
  }
  drag_init_point_ = location;
  was_expanded_ = IsExpanded();
}

void UnifiedSystemTrayController::UpdateDrag(const gfx::PointF& location) {
  if (features::IsQsRevampEnabled()) {
    return;
  }

  // Ignore swipe collapsing when a detailed view is shown as it's confusing.
  if (detailed_view_controller_) {
    return;
  }
  double drag_expanded_amount = GetDragExpandedAmount(location);
  animation_->Reset(drag_expanded_amount);
  UpdateExpandedAmount();

  if (was_expanded_ &&
      drag_expanded_amount < kNotificationCenterDragExpandThreshold) {
    bubble_->ExpandMessageCenter();
  } else if (drag_expanded_amount >= kNotificationCenterDragExpandThreshold &&
             IsMessageCenterCollapseRequired()) {
    bubble_->CollapseMessageCenter();
  }
}

void UnifiedSystemTrayController::StartAnimation(bool expand) {
  // UnifiedSystemTrayControllerTest does not add `unified_view_` to a widget.
  if (features::IsQsRevampEnabled()) {
    return;
  }

  if (unified_view_->GetWidget()) {
    animation_tracker_.emplace(unified_view_->GetWidget()
                                   ->GetCompositor()
                                   ->RequestNewThroughputTracker());
    animation_tracker_->Start(metrics_util::ForSmoothness(
        expand ? base::BindRepeating(&ReportExpandAnimationSmoothness)
               : base::BindRepeating(&ReportCollapseAnimationSmoothness)));
  }

  if (expand) {
    animation_->Show();
  } else {
    // To animate to hidden state, first set SlideAnimation::IsShowing() to
    // true.
    animation_->Show();
    animation_->Hide();
  }
}

void UnifiedSystemTrayController::EndDrag(const gfx::PointF& location) {
  if (features::IsQsRevampEnabled()) {
    return;
  }

  // Ignore swipe collapsing when a detailed view is shown as it's confusing.
  if (detailed_view_controller_) {
    return;
  }
  if (animation_->is_animating()) {
    // Prevent overwriting the state right after fling event
    return;
  }
  bool expanded = GetDragExpandedAmount(location) > 0.5;
  if (was_expanded_ != expanded) {
    UMA_HISTOGRAM_ENUMERATION("ChromeOS.SystemTray.ToggleExpanded",
                              TOGGLE_EXPANDED_TYPE_BY_GESTURE,
                              TOGGLE_EXPANDED_TYPE_COUNT);
  }

  if (expanded && IsMessageCenterCollapseRequired()) {
    bubble_->CollapseMessageCenter();
  } else {
    bubble_->ExpandMessageCenter();
  }

  // If dragging is finished, animate to closer state.
  StartAnimation(expanded);
}

void UnifiedSystemTrayController::Fling(int velocity) {
  if (features::IsQsRevampEnabled()) {
    return;
  }

  // Ignore swipe collapsing when a detailed view is shown as it's confusing.
  if (detailed_view_controller_) {
    return;
  }
  // Expand when flinging up. Collapse otherwise.
  bool expand = (velocity < 0);

  if (expand && IsMessageCenterCollapseRequired()) {
    bubble_->CollapseMessageCenter();
  } else {
    bubble_->ExpandMessageCenter();
  }

  StartAnimation(expand);
}

void UnifiedSystemTrayController::ShowUserChooserView() {
  if (!UserChooserDetailedViewController::IsUserChooserEnabled()) {
    return;
  }
  animation_->Reset(1.0);
  UpdateExpandedAmount();
  ShowDetailedView(std::make_unique<UserChooserDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowNetworkDetailedView(bool force) {
  if (!force && !IsExpanded()) {
    return;
  }

  base::RecordAction(base::UserMetricsAction("StatusArea_Network_Detailed"));

  ShowDetailedView(std::make_unique<NetworkDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowHotspotDetailedView() {
  DCHECK(features::IsQsRevampEnabled());

  ShowDetailedView(std::make_unique<HotspotDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowBluetoothDetailedView() {
  // QSRevamp does not allow expand/collapse of the System Tray.
  if (!IsExpanded() && !features::IsQsRevampEnabled()) {
    return;
  }

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
}

void UnifiedSystemTrayController::ShowFocusModeDetailedView() {
  // TODO(b/286931532): Create the Focus Mode detailed view.
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

void UnifiedSystemTrayController::ShowNotifierSettingsView() {
  if (features::IsOsSettingsAppBadgingToggleEnabled()) {
    return;
  }

  DCHECK(Shell::Get()->session_controller()->ShouldShowNotificationTray());
  DCHECK(!Shell::Get()->session_controller()->IsScreenLocked());
  ShowDetailedView(std::make_unique<UnifiedNotifierSettingsController>(this));
}

void UnifiedSystemTrayController::ShowCalendarView(
    calendar_metrics::CalendarViewShowSource show_source,
    calendar_metrics::CalendarEventSource event_source) {
  calendar_metrics::RecordCalendarShowMetrics(show_source, event_source);
  ShowDetailedView(std::make_unique<UnifiedCalendarViewController>(this));

  showing_calendar_view_ = true;
  showing_audio_detailed_view_ = false;
  showing_display_detailed_view_ = false;

  for (auto& observer : observers_) {
    observer.OnOpeningCalendarView();
  }
}

void UnifiedSystemTrayController::ShowMediaControlsDetailedView() {
  ShowDetailedView(
      std::make_unique<UnifiedMediaControlsDetailedViewController>(this));
}

void UnifiedSystemTrayController::TransitionToMainView(bool restore_focus) {
  if (showing_calendar_view_) {
    showing_calendar_view_ = false;
    for (auto& observer : observers_) {
      observer.OnTransitioningFromCalendarToMainView();
    }
  }

  showing_audio_detailed_view_ = false;
  showing_display_detailed_view_ = false;

  // Transfer `detailed_view_controller_` to a scoped object, which will be
  // destroyed once it's out of this method's scope (after resetting
  // `quick_settings_view_`'s `detailed_view_`). Because the detailed view has a
  // reference to its `detailed_view_controller_` which is used in shutdown.
  auto scoped_detailed_view_controller = std::move(detailed_view_controller_);

  if (features::IsQsRevampEnabled()) {
    bubble_->UpdateBubbleHeight(/*is_showing_detiled_view=*/false);
    quick_settings_view_->ResetDetailedView();
    if (restore_focus) {
      quick_settings_view_->RestoreFocus();
    }
    UpdateBubble();
    return;
  }
  unified_view_->ResetDetailedView();
  if (restore_focus) {
    unified_view_->RestoreFocus();
  }
}

void UnifiedSystemTrayController::CloseBubble() {
  if (features::IsQsRevampEnabled()) {
    if (quick_settings_view_->GetWidget()) {
      quick_settings_view_->GetWidget()->CloseNow();
    }
    return;
  }
  if (unified_view_->GetWidget()) {
    unified_view_->GetWidget()->CloseNow();
  }
}

bool UnifiedSystemTrayController::FocusOut(bool reverse) {
  return bubble_->FocusOut(reverse);
}

void UnifiedSystemTrayController::EnsureCollapsed() {
  if (features::IsQsRevampEnabled()) {
    return;
  }

  if (IsExpanded()) {
    animation_->Hide();
  }
}

void UnifiedSystemTrayController::EnsureExpanded() {
  if (detailed_view_controller_) {
    // If a detailed view is showing, first transit to the main view.
    TransitionToMainView(false);
  }
  StartAnimation(true /*expand*/);

  if (IsMessageCenterCollapseRequired()) {
    bubble_->CollapseMessageCenter();
  }
}

void UnifiedSystemTrayController::AnimationEnded(
    const gfx::Animation* animation) {
  if (features::IsQsRevampEnabled()) {
    return;
  }

  if (animation_tracker_) {
    animation_tracker_->Stop();
    animation_tracker_.reset();
  }

  UpdateExpandedAmount();
}

void UnifiedSystemTrayController::AnimationProgressed(
    const gfx::Animation* animation) {
  if (features::IsQsRevampEnabled()) {
    return;
  }

  UpdateExpandedAmount();
}

void UnifiedSystemTrayController::AnimationCanceled(
    const gfx::Animation* animation) {
  if (features::IsQsRevampEnabled()) {
    return;
  }

  animation_->Reset(std::round(animation_->GetCurrentValue()));
  UpdateExpandedAmount();
}

void UnifiedSystemTrayController::OnAudioSettingsButtonClicked() {
  ShowAudioDetailedView();
}

void UnifiedSystemTrayController::ShowMediaControls() {
  if (features::IsQsRevampEnabled()) {
    quick_settings_view_->ShowMediaControls();
    return;
  }
  unified_view_->ShowMediaControls();
}

void UnifiedSystemTrayController::OnMediaControlsViewClicked() {
  ShowMediaControlsDetailedView();
}

void UnifiedSystemTrayController::SetShowMediaView(bool show_media_view) {
  quick_settings_view_->SetShowMediaView(show_media_view);
}

void UnifiedSystemTrayController::LoadIsExpandedPref() {
  if (active_user_prefs_ &&
      active_user_prefs_->HasPrefPath(prefs::kSystemTrayExpanded)) {
    model_->set_expanded_on_open(
        active_user_prefs_->GetBoolean(prefs::kSystemTrayExpanded)
            ? UnifiedSystemTrayModel::StateOnOpen::EXPANDED
            : UnifiedSystemTrayModel::StateOnOpen::COLLAPSED);
  }
}

void UnifiedSystemTrayController::InitFeaturePods() {
  AddFeaturePodItem(std::make_unique<NetworkFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<BluetoothFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<AccessibilityFeaturePodController>(this));
  if (base::FeatureList::IsEnabled(features::kFocusMode)) {
    AddFeaturePodItem(std::make_unique<FocusModeFeaturePodController>(this));
  }
  AddFeaturePodItem(std::make_unique<QuietModeFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<RotationLockFeaturePodController>());
  AddFeaturePodItem(std::make_unique<PrivacyScreenFeaturePodController>());
  AddFeaturePodItem(std::make_unique<CaptureModeFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<NearbyShareFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<NightLightFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<CastFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<VPNFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<IMEFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<LocaleFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<DarkModeFeaturePodController>(this));
  if (media::ShouldEnableAutoFraming()) {
    AddFeaturePodItem(std::make_unique<AutozoomFeaturePodController>());
  }
  // If you want to add a new feature pod item, add here.

  quick_settings_metrics_util::RecordQsFeaturePodCount(
      unified_view_->GetVisibleFeaturePodCount(),
      Shell::Get()->tablet_mode_controller()->InTabletMode());
}

void UnifiedSystemTrayController::InitFeatureTiles() {
  // TODO(b/252871301): Create each feature's tile.
  std::vector<std::unique_ptr<FeatureTile>> tiles;

  auto create_tile =
      [](std::unique_ptr<FeaturePodControllerBase> controller,
         std::vector<std::unique_ptr<FeaturePodControllerBase>>& controllers,
         std::vector<std::unique_ptr<FeatureTile>>& tiles,
         bool compact = false) {
        tiles.push_back(controller->CreateTile(compact));
        controllers.push_back(std::move(controller));
      };

  create_tile(std::make_unique<NetworkFeaturePodController>(this),
              feature_pod_controllers_, tiles);
  if (features::IsHotspotEnabled()) {
    create_tile(std::make_unique<HotspotFeaturePodController>(this),
                feature_pod_controllers_, tiles);
  }

  // CaptureMode and QuietMode tiles will be compact if both are visible.
  bool capture_and_quiet_tiles_are_compact =
      CaptureModeFeaturePodController::CalculateButtonVisibility() &&
      QuietModeFeaturePodController::CalculateButtonVisibility();
  create_tile(std::make_unique<CaptureModeFeaturePodController>(this),
              feature_pod_controllers_, tiles,
              capture_and_quiet_tiles_are_compact);
  create_tile(std::make_unique<QuietModeFeaturePodController>(this),
              feature_pod_controllers_, tiles,
              capture_and_quiet_tiles_are_compact);
  create_tile(std::make_unique<BluetoothFeaturePodController>(this),
              feature_pod_controllers_, tiles);

  // Cast and RotationLock tiles will be compact if both are visible.
  bool cast_and_rotation_tiles_are_compact =
      CastFeaturePodController::CalculateButtonVisibility() &&
      RotationLockFeaturePodController::CalculateButtonVisibility();
  create_tile(std::make_unique<CastFeaturePodController>(this),
              feature_pod_controllers_, tiles,
              cast_and_rotation_tiles_are_compact);
  create_tile(std::make_unique<RotationLockFeaturePodController>(),
              feature_pod_controllers_, tiles,
              cast_and_rotation_tiles_are_compact);
  create_tile(std::make_unique<AccessibilityFeaturePodController>(this),
              feature_pod_controllers_, tiles);
  if (base::FeatureList::IsEnabled(features::kFocusMode)) {
    create_tile(std::make_unique<FocusModeFeaturePodController>(this),
                feature_pod_controllers_, tiles);
  }
  create_tile(std::make_unique<NearbyShareFeaturePodController>(this),
              feature_pod_controllers_, tiles);
  create_tile(std::make_unique<LocaleFeaturePodController>(this),
              feature_pod_controllers_, tiles);
  create_tile(std::make_unique<IMEFeaturePodController>(this),
              feature_pod_controllers_, tiles);
  if (media::ShouldEnableAutoFraming()) {
    create_tile(std::make_unique<AutozoomFeaturePodController>(),
                feature_pod_controllers_, tiles);
  }
  create_tile(std::make_unique<VPNFeaturePodController>(this),
              feature_pod_controllers_, tiles);
  create_tile(std::make_unique<PrivacyScreenFeaturePodController>(),
              feature_pod_controllers_, tiles);

  quick_settings_view_->AddTiles(std::move(tiles));

  quick_settings_metrics_util::RecordQsFeaturePodCount(
      quick_settings_view_->feature_tiles_container()
          ->GetVisibleFeatureTileCount(),
      Shell::Get()->tablet_mode_controller()->InTabletMode());
}

void UnifiedSystemTrayController::AddFeaturePodItem(
    std::unique_ptr<FeaturePodControllerBase> controller) {
  DCHECK(unified_view_);
  FeaturePodButton* button = controller->CreateButton();
  button->SetExpandedAmount(IsExpanded() ? 1.0 : 0.0,
                            false /* fade_icon_button */);
  unified_view_->AddFeaturePodButton(button);
  feature_pod_controllers_.push_back(std::move(controller));
}

void UnifiedSystemTrayController::ShowDetailedView(
    std::unique_ptr<DetailedViewController> controller) {
  animation_->Reset(1.0);
  UpdateExpandedAmount();
  views::FocusManager* manager;
  if (features::IsQsRevampEnabled()) {
    quick_settings_view_->SaveFocus();
    manager = quick_settings_view_->GetFocusManager();
  } else {
    unified_view_->SaveFocus();
    manager = unified_view_->GetFocusManager();
  }

  if (manager && manager->GetFocusedView()) {
    manager->ClearFocus();
  }

  showing_audio_detailed_view_ = false;
  showing_display_detailed_view_ = false;
  if (features::IsQsRevampEnabled()) {
    bubble_->UpdateBubbleHeight(/*is_showing_detiled_view=*/true);
    quick_settings_view_->SetDetailedView(controller->CreateView());
  } else {
    unified_view_->SetDetailedView(controller->CreateView());
  }

  detailed_view_controller_ = std::move(controller);

  // `bubble_` may be null in tests.
  if (bubble_) {
    UpdateBubble();
    // Notify accessibility features that a new view is showing.
    bubble_->NotifyAccessibilityEvent(ax::mojom::Event::kShow, true);
  }
}

void UnifiedSystemTrayController::UpdateExpandedAmount() {
  if (quick_settings_view_) {
    return;
  }
  double expanded_amount = animation_->GetCurrentValue();
  unified_view_->SetExpandedAmount(expanded_amount);

  if (expanded_amount == 0.0 || expanded_amount == 1.0) {
    model_->set_expanded_on_open(
        expanded_amount == 1.0
            ? UnifiedSystemTrayModel::StateOnOpen::EXPANDED
            : UnifiedSystemTrayModel::StateOnOpen::COLLAPSED);
  }
}

void UnifiedSystemTrayController::ResetToCollapsedIfRequired() {
  if (quick_settings_view_) {
    return;
  }
  if (model_->IsExplicitlyExpanded()) {
    return;
  }

  if (unified_view_->feature_pods_container()->row_count() ==
      kUnifiedFeaturePodMinRows) {
    CollapseWithoutAnimating();
  }
}

void UnifiedSystemTrayController::CollapseWithoutAnimating() {
  if (features::IsQsRevampEnabled()) {
    return;
  }

  unified_view_->SetExpandedAmount(0.0);
  animation_->Reset(0);
}

bool UnifiedSystemTrayController::IsDetailedViewShown() const {
  if (quick_settings_view_) {
    return quick_settings_view_->IsDetailedViewShown();
  }
  if (unified_view_) {
    return unified_view_->IsDetailedViewShown();
  }
  return false;
}

void UnifiedSystemTrayController::UpdateDragThreshold() {
  if (features::IsQsRevampEnabled()) {
    return;
  }
  UnifiedSystemTrayView* unified_view = bubble_->unified_view();
  drag_threshold_ = unified_view->GetExpandedSystemTrayHeight() -
                    unified_view->GetCollapsedSystemTrayHeight();
}

double UnifiedSystemTrayController::GetDragExpandedAmount(
    const gfx::PointF& location) const {
  if (features::IsQsRevampEnabled()) {
    return 1.0;
  }

  double y_diff = (location - drag_init_point_).y();
  // If already expanded, only consider swiping down. Otherwise, only consider
  // swiping up.
  if (was_expanded_) {
    return std::clamp(1.0 - std::max(0.0, y_diff) / drag_threshold_, 0.0, 1.0);
  } else {
    return std::clamp(std::max(0.0, -y_diff) / drag_threshold_, 0.0, 1.0);
  }
}

bool UnifiedSystemTrayController::IsExpanded() const {
  return features::IsQsRevampEnabled() || animation_->IsShowing();
}

void UnifiedSystemTrayController::UpdateBubble() {
  if (!bubble_) {
    return;
  }
  bubble_->UpdateBubble();
}

bool UnifiedSystemTrayController::IsMessageCenterCollapseRequired() const {
  if (quick_settings_view_) {
    return false;
  }

  if (!bubble_) {
    return false;
  }

  // Note: This calculaton should be the same as
  // UnifiedMessageCenterBubble::CalculateAvailableHeight().
  auto available_height = CalculateMaxTrayBubbleHeight(
      bubble_->GetTray()->GetBubbleWindowContainer());
  available_height -= unified_view_->GetExpandedSystemTrayHeight();
  available_height -= kUnifiedMessageCenterBubbleSpacing;
  return available_height < kMessageCenterCollapseThreshold;
}

base::TimeDelta UnifiedSystemTrayController::GetAnimationDurationForReporting()
    const {
  return base::Milliseconds(kSystemMenuCollapseExpandAnimationDurationMs);
}

bool UnifiedSystemTrayController::ShouldShowDeferredUpdateDialog() const {
  return Shell::Get()->system_tray_model()->update_model()->update_deferred() ==
         DeferredUpdateState::kShowDialog;
}

}  // namespace ash
