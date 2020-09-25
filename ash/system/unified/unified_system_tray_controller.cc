// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_controller.h"

#include "ash/capture_mode/capture_mode_feature_pod_controller.h"
#include "ash/metrics/user_metrics_action.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/pagination/pagination_controller.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/accessibility/accessibility_feature_pod_controller.h"
#include "ash/system/accessibility/unified_accessibility_detailed_view_controller.h"
#include "ash/system/audio/unified_audio_detailed_view_controller.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/bluetooth/bluetooth_feature_pod_controller.h"
#include "ash/system/bluetooth/unified_bluetooth_detailed_view_controller.h"
#include "ash/system/brightness/unified_brightness_slider_controller.h"
#include "ash/system/cast/cast_feature_pod_controller.h"
#include "ash/system/cast/unified_cast_detailed_view_controller.h"
#include "ash/system/dark_mode/dark_mode_detailed_view_controller.h"
#include "ash/system/dark_mode/dark_mode_feature_pod_controller.h"
#include "ash/system/ime/ime_feature_pod_controller.h"
#include "ash/system/ime/unified_ime_detailed_view_controller.h"
#include "ash/system/locale/locale_feature_pod_controller.h"
#include "ash/system/locale/unified_locale_detailed_view_controller.h"
#include "ash/system/media/media_tray.h"
#include "ash/system/media/unified_media_controls_controller.h"
#include "ash/system/media/unified_media_controls_detailed_view_controller.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/nearby_share/nearby_share_feature_pod_controller.h"
#include "ash/system/network/network_feature_pod_controller.h"
#include "ash/system/network/unified_network_detailed_view_controller.h"
#include "ash/system/network/unified_vpn_detailed_view_controller.h"
#include "ash/system/network/vpn_feature_pod_controller.h"
#include "ash/system/night_light/night_light_feature_pod_controller.h"
#include "ash/system/privacy_screen/privacy_screen_feature_pod_controller.h"
#include "ash/system/rotation/rotation_lock_feature_pod_controller.h"
#include "ash/system/tray/system_tray_item_uma_type.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "ash/system/unified/feature_pods_container_view.h"
#include "ash/system/unified/quiet_mode_feature_pod_controller.h"
#include "ash/system/unified/unified_notifier_settings_controller.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/system/unified/user_chooser_detailed_view_controller.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/ranges.h"
#include "media/base/media_switches.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/compositor/animation_metrics_reporter.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/message_center/message_center.h"
#include "ui/views/widget/widget.h"

namespace ash {

// TODO(amehfooz): Add histograms for pagination metrics in system tray.
void RecordPageSwitcherSourceByEventType(ui::EventType type,
                                         bool is_tablet_mode) {}

class UnifiedSystemTrayController::SystemTrayTransitionAnimationMetricsReporter
    : public ui::AnimationMetricsReporter {
 public:
  SystemTrayTransitionAnimationMetricsReporter() = default;
  ~SystemTrayTransitionAnimationMetricsReporter() override = default;

  void set_target_expanded_state(bool expanded) { target_expanded_ = expanded; }

 private:
  // ui:AnimationMetricsReporter
  void Report(int value) override {
    if (target_expanded_) {
      UMA_HISTOGRAM_PERCENTAGE(
          "ChromeOS.SystemTray.AnimationSmoothness."
          "TransitionToExpanded",
          value);
    } else {
      UMA_HISTOGRAM_PERCENTAGE(
          "ChromeOS.SystemTray.AnimationSmoothness."
          "TransitionToCollapsed",
          value);
    }
  }

  bool target_expanded_;
};

UnifiedSystemTrayController::UnifiedSystemTrayController(
    UnifiedSystemTrayModel* model,
    UnifiedSystemTrayBubble* bubble,
    views::View* owner_view)
    : views::AnimationDelegateViews(owner_view),
      model_(model),
      bubble_(bubble),
      animation_(std::make_unique<gfx::SlideAnimation>(this)),
      animation_metrics_reporter_(
          std::make_unique<SystemTrayTransitionAnimationMetricsReporter>()) {
  animation_->Reset(model_->IsExpandedOnOpen() ? 1.0 : 0.0);
  animation_->SetSlideDuration(base::TimeDelta::FromMilliseconds(
      kSystemMenuCollapseExpandAnimationDurationMs));
  animation_->SetTweenType(gfx::Tween::EASE_IN_OUT);

  model_->pagination_model()->SetTransitionDurations(
      base::TimeDelta::FromMilliseconds(250),
      base::TimeDelta::FromMilliseconds(50));

  pagination_controller_ = std::make_unique<PaginationController>(
      model_->pagination_model(), PaginationController::SCROLL_AXIS_HORIZONTAL,
      base::BindRepeating(&RecordPageSwitcherSourceByEventType),
      Shell::Get()->tablet_mode_controller()->InTabletMode());

  Shell::Get()->metrics()->RecordUserMetricsAction(UMA_STATUS_AREA_MENU_OPENED);
  UMA_HISTOGRAM_BOOLEAN("ChromeOS.SystemTray.IsExpandedOnOpen",
                        model_->IsExpandedOnOpen());

  SetAnimationMetricsReporter(animation_metrics_reporter_.get());
}

UnifiedSystemTrayController::~UnifiedSystemTrayController() = default;

UnifiedSystemTrayView* UnifiedSystemTrayController::CreateView() {
  DCHECK(!unified_view_);
  unified_view_ = new UnifiedSystemTrayView(this, model_->IsExpandedOnOpen());
  InitFeaturePods();

  if (base::FeatureList::IsEnabled(media::kGlobalMediaControlsForChromeOS) &&
      !Shell::Get()->session_controller()->IsScreenLocked() &&
      !MediaTray::IsPinnedToShelf()) {
    media_controls_controller_ =
        std::make_unique<UnifiedMediaControlsController>(this);
    unified_view_->AddMediaControlsView(
        media_controls_controller_->CreateView());
  }

  volume_slider_controller_ =
      std::make_unique<UnifiedVolumeSliderController>(this);
  unified_view_->AddSliderView(volume_slider_controller_->CreateView());

  brightness_slider_controller_ =
      std::make_unique<UnifiedBrightnessSliderController>(model_);
  unified_view_->AddSliderView(brightness_slider_controller_->CreateView());

  return unified_view_;
}

void UnifiedSystemTrayController::HandleSignOutAction() {
  Shell::Get()->metrics()->RecordUserMetricsAction(UMA_STATUS_AREA_SIGN_OUT);
  if (Shell::Get()->session_controller()->IsDemoSession())
    base::RecordAction(base::UserMetricsAction("DemoMode.ExitFromSystemTray"));
  Shell::Get()->session_controller()->RequestSignOut();
}

void UnifiedSystemTrayController::HandleLockAction() {
  Shell::Get()->metrics()->RecordUserMetricsAction(UMA_TRAY_LOCK_SCREEN);
  Shell::Get()->session_controller()->LockScreen();
}

void UnifiedSystemTrayController::HandleSettingsAction() {
  Shell::Get()->metrics()->RecordUserMetricsAction(UMA_TRAY_SETTINGS);
  Shell::Get()->system_tray_model()->client()->ShowSettings();
}

void UnifiedSystemTrayController::HandlePowerAction() {
  Shell::Get()->metrics()->RecordUserMetricsAction(UMA_TRAY_SHUT_DOWN);
  Shell::Get()->lock_state_controller()->RequestShutdown(
      ShutdownReason::TRAY_SHUT_DOWN_BUTTON);
  CloseBubble();
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

void UnifiedSystemTrayController::HandleEnterpriseInfoAction() {
  UMA_HISTOGRAM_ENUMERATION("ChromeOS.SystemTray.OpenHelpPageForManaged",
                            MANAGED_TYPE_ENTERPRISE, MANAGED_TYPE_COUNT);
  Shell::Get()->system_tray_model()->client()->ShowEnterpriseInfo();
}

void UnifiedSystemTrayController::ToggleExpanded() {
  if (animation_->is_animating())
    return;

  UMA_HISTOGRAM_ENUMERATION("ChromeOS.SystemTray.ToggleExpanded",
                            TOGGLE_EXPANDED_TYPE_BY_BUTTON,
                            TOGGLE_EXPANDED_TYPE_COUNT);
  if (IsExpanded()) {
    StartAnimation(false /*expand*/);
    // Expand message center when quick settings is collapsed.
    if (bubble_)
      bubble_->ExpandMessageCenter();
  } else {
    // Collapse the message center if screen height is limited after expanding
    // the quick settings to its full height.
    if (IsMessageCenterCollapseRequired()) {
      bubble_->CollapseMessageCenter();
    }
    StartAnimation(true /*expand*/);
  }
}

void UnifiedSystemTrayController::BeginDrag(const gfx::PointF& location) {
  UpdateDragThreshold();
  // Ignore swipe collapsing when a detailed view is shown as it's confusing.
  if (detailed_view_controller_)
    return;
  drag_init_point_ = location;
  was_expanded_ = IsExpanded();
}

void UnifiedSystemTrayController::UpdateDrag(const gfx::PointF& location) {
  // Ignore swipe collapsing when a detailed view is shown as it's confusing.
  if (detailed_view_controller_)
    return;
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
  animation_metrics_reporter_->set_target_expanded_state(expand);
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
  // Ignore swipe collapsing when a detailed view is shown as it's confusing.
  if (detailed_view_controller_)
    return;
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

  if (expanded && IsMessageCenterCollapseRequired())
    bubble_->CollapseMessageCenter();
  else
    bubble_->ExpandMessageCenter();

  // If dragging is finished, animate to closer state.
  StartAnimation(expanded);
}

void UnifiedSystemTrayController::Fling(int velocity) {
  // Ignore swipe collapsing when a detailed view is shown as it's confusing.
  if (detailed_view_controller_)
    return;
  // Expand when flinging up. Collapse otherwise.
  bool expand = (velocity < 0);

  if (expand && IsMessageCenterCollapseRequired())
    bubble_->CollapseMessageCenter();
  else
    bubble_->ExpandMessageCenter();

  StartAnimation(expand);
}

void UnifiedSystemTrayController::ShowUserChooserView() {
  if (!UserChooserDetailedViewController::IsUserChooserEnabled())
    return;
  animation_->Reset(1.0);
  UpdateExpandedAmount();
  ShowDetailedView(std::make_unique<UserChooserDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowNetworkDetailedView(bool force) {
  if (!force && !IsExpanded())
    return;

  Shell::Get()->metrics()->RecordUserMetricsAction(
      UMA_STATUS_AREA_DETAILED_NETWORK_VIEW);
  ShowDetailedView(
      std::make_unique<UnifiedNetworkDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowBluetoothDetailedView() {
  if (!IsExpanded())
    return;

  Shell::Get()->metrics()->RecordUserMetricsAction(
      UMA_STATUS_AREA_DETAILED_BLUETOOTH_VIEW);
  ShowDetailedView(
      std::make_unique<UnifiedBluetoothDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowCastDetailedView() {
  Shell::Get()->metrics()->RecordUserMetricsAction(
      UMA_STATUS_AREA_DETAILED_CAST_VIEW);
  ShowDetailedView(std::make_unique<UnifiedCastDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowAccessibilityDetailedView() {
  Shell::Get()->metrics()->RecordUserMetricsAction(
      UMA_STATUS_AREA_DETAILED_ACCESSIBILITY);
  ShowDetailedView(
      std::make_unique<UnifiedAccessibilityDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowVPNDetailedView() {
  Shell::Get()->metrics()->RecordUserMetricsAction(
      UMA_STATUS_AREA_DETAILED_VPN_VIEW);
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
}

void UnifiedSystemTrayController::ShowDarkModeDetailedView() {
  ShowDetailedView(std::make_unique<DarkModeDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowNotifierSettingsView() {
  DCHECK(Shell::Get()->session_controller()->ShouldShowNotificationTray());
  DCHECK(!Shell::Get()->session_controller()->IsScreenLocked());
  ShowDetailedView(std::make_unique<UnifiedNotifierSettingsController>(this));
}

void UnifiedSystemTrayController::ShowMediaControlsDetailedView() {
  ShowDetailedView(
      std::make_unique<UnifiedMediaControlsDetailedViewController>(this));
}

void UnifiedSystemTrayController::TransitionToMainView(bool restore_focus) {
  detailed_view_controller_.reset();
  unified_view_->ResetDetailedView();
  if (restore_focus)
    unified_view_->RestoreFocus();
}

void UnifiedSystemTrayController::CloseBubble() {
  if (unified_view_->GetWidget())
    unified_view_->GetWidget()->CloseNow();
}

bool UnifiedSystemTrayController::FocusOut(bool reverse) {
  return bubble_->FocusOut(reverse);
}

void UnifiedSystemTrayController::EnsureCollapsed() {
  if (IsExpanded()) {
    animation_->Hide();
  }
}

void UnifiedSystemTrayController::EnsureExpanded() {
  if (detailed_view_controller_) {
    detailed_view_controller_.reset();
    unified_view_->ResetDetailedView();
  }
  StartAnimation(true /*expand*/);

  if (IsMessageCenterCollapseRequired())
    bubble_->CollapseMessageCenter();
}

void UnifiedSystemTrayController::AnimationEnded(
    const gfx::Animation* animation) {
  UpdateExpandedAmount();
}

void UnifiedSystemTrayController::AnimationProgressed(
    const gfx::Animation* animation) {
  UpdateExpandedAmount();
}

void UnifiedSystemTrayController::AnimationCanceled(
    const gfx::Animation* animation) {
  animation_->Reset(std::round(animation_->GetCurrentValue()));
  UpdateExpandedAmount();
}

void UnifiedSystemTrayController::OnAudioSettingsButtonClicked() {
  ShowAudioDetailedView();
}

void UnifiedSystemTrayController::ShowMediaControls() {
  unified_view_->ShowMediaControls();
}

void UnifiedSystemTrayController::HideMediaControls() {
  unified_view_->HideMediaControls();
}

void UnifiedSystemTrayController::OnMediaControlsViewClicked() {
  ShowMediaControlsDetailedView();
}

void UnifiedSystemTrayController::InitFeaturePods() {
  AddFeaturePodItem(std::make_unique<NetworkFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<BluetoothFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<AccessibilityFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<QuietModeFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<RotationLockFeaturePodController>());
  AddFeaturePodItem(std::make_unique<PrivacyScreenFeaturePodController>());
  AddFeaturePodItem(std::make_unique<NightLightFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<CastFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<VPNFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<IMEFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<LocaleFeaturePodController>(this));
  if (features::IsCaptureModeEnabled())
    AddFeaturePodItem(std::make_unique<CaptureModeFeaturePodController>());
  if (features::IsDarkLightModeEnabled())
    AddFeaturePodItem(std::make_unique<DarkModeFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<NearbyShareFeaturePodController>(this));

  // If you want to add a new feature pod item, add here.

  if (Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    UMA_HISTOGRAM_COUNTS_100("ChromeOS.SystemTray.Tablet.FeaturePodCountOnOpen",
                             unified_view_->GetVisibleFeaturePodCount());
  } else {
    UMA_HISTOGRAM_COUNTS_100("ChromeOS.SystemTray.FeaturePodCountOnOpen",
                             unified_view_->GetVisibleFeaturePodCount());
  }
}

void UnifiedSystemTrayController::AddFeaturePodItem(
    std::unique_ptr<FeaturePodControllerBase> controller) {
  DCHECK(unified_view_);
  FeaturePodButton* button = controller->CreateButton();
  button->SetExpandedAmount(IsExpanded() ? 1.0 : 0.0,
                            false /* fade_icon_button */);

  // Record DefaultView.VisibleRows UMA.
  SystemTrayItemUmaType uma_type = controller->GetUmaType();
  if (uma_type != SystemTrayItemUmaType::UMA_NOT_RECORDED &&
      button->visible_preferred()) {
    UMA_HISTOGRAM_ENUMERATION("Ash.SystemMenu.DefaultView.VisibleRows",
                              uma_type, SystemTrayItemUmaType::UMA_COUNT);
  }

  unified_view_->AddFeaturePodButton(button);
  feature_pod_controllers_.push_back(std::move(controller));
}

void UnifiedSystemTrayController::ShowDetailedView(
    std::unique_ptr<DetailedViewController> controller) {
  animation_->Reset(1.0);
  UpdateExpandedAmount();

  unified_view_->SaveFocus();
  views::FocusManager* manager = unified_view_->GetFocusManager();
  if (manager && manager->GetFocusedView())
    manager->ClearFocus();

  unified_view_->SetDetailedView(controller->CreateView());
  detailed_view_controller_ = std::move(controller);

  // |bubble_| may be null in tests.
  if (bubble_) {
    bubble_->UpdateBubble();
    // Notify accessibility features that a new view is showing.
    bubble_->NotifyAccessibilityEvent(ax::mojom::Event::kShow, true);
  }
}

void UnifiedSystemTrayController::UpdateExpandedAmount() {
  double expanded_amount = animation_->GetCurrentValue();
  unified_view_->SetExpandedAmount(expanded_amount);

  if (expanded_amount == 0.0 || expanded_amount == 1.0)
    model_->set_expanded_on_open(
        expanded_amount == 1.0
            ? UnifiedSystemTrayModel::StateOnOpen::EXPANDED
            : UnifiedSystemTrayModel::StateOnOpen::COLLAPSED);
}

void UnifiedSystemTrayController::ResetToCollapsedIfRequired() {
  if (model_->IsExplicitlyExpanded())
    return;

  if (unified_view_->feature_pods_container()->row_count() ==
      kUnifiedFeaturePodMinRows) {
    CollapseWithoutAnimating();
  }
}

void UnifiedSystemTrayController::CollapseWithoutAnimating() {
  unified_view_->SetExpandedAmount(0.0);
  animation_->Reset(0);
}

void UnifiedSystemTrayController::UpdateDragThreshold() {
  UnifiedSystemTrayView* unified_view = bubble_->unified_view();
  drag_threshold_ = unified_view->GetExpandedSystemTrayHeight() -
                    unified_view->GetCollapsedSystemTrayHeight();
}

double UnifiedSystemTrayController::GetDragExpandedAmount(
    const gfx::PointF& location) const {
  double y_diff = (location - drag_init_point_).y();
  // If already expanded, only consider swiping down. Otherwise, only consider
  // swiping up.
  if (was_expanded_) {
    return base::ClampToRange(1.0 - std::max(0.0, y_diff) / drag_threshold_,
                              0.0, 1.0);
  } else {
    return base::ClampToRange(std::max(0.0, -y_diff) / drag_threshold_, 0.0,
                              1.0);
  }
}

bool UnifiedSystemTrayController::IsExpanded() const {
  return animation_->IsShowing();
}

bool UnifiedSystemTrayController::IsMessageCenterCollapseRequired() const {
  // Note: This calculaton should be the same as
  // UnifiedMessageCenterBubble::CalculateAvailableHeight().
  return (bubble_ && bubble_->CalculateMaxHeight() -
                             unified_view_->GetExpandedSystemTrayHeight() -
                             kUnifiedMessageCenterBubbleSpacing <
                         kMessageCenterCollapseThreshold);
}

base::TimeDelta UnifiedSystemTrayController::GetAnimationDurationForReporting()
    const {
  return base::TimeDelta::FromMilliseconds(
      kSystemMenuCollapseExpandAnimationDurationMs);
}

}  // namespace ash
