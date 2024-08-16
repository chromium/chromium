// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/focus_cycler.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/camera/autozoom_toast_controller.h"
#include "ash/system/channel_indicator/channel_indicator.h"
#include "ash/system/channel_indicator/channel_indicator_utils.h"
#include "ash/system/hotspot/hotspot_tray_view.h"
#include "ash/system/human_presence/snooping_protection_view.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_tray_view.h"
#include "ash/system/notification_center/ash_message_popup_collection.h"
#include "ash/system/power/tray_power.h"
#include "ash/system/privacy_screen/privacy_screen_toast_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/time/time_tray_item_view.h"
#include "ash/system/time/time_view.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/unified/current_locale_view.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/ime_mode_view.h"
#include "ash/system/unified/managed_device_tray_item_view.h"
#include "ash/system/unified/screen_capture_tray_item_view.h"
#include "ash/system/unified/unified_slider_bubble_controller.h"
#include "ash/system/unified/unified_slider_view.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/user_education/user_education_class_properties.h"
#include "ash/user_education/welcome_tour/welcome_tour_metrics.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/capture/video/chromeos/video_capture_features_chromeos.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {
// The UMA histogram that records presentation time for opening QuickSettings
// through `UnifiedSystemTray` button.
constexpr char kStatusAreaShowBubbleHistogram[] =
    "Ash.StatusAreaShowBubble.PresentationTime";
}  // namespace

UnifiedSystemTray::UnifiedSystemTray(Shelf* shelf)
    : TrayBackgroundView(shelf,
                         TrayBackgroundViewCatalogName::kUnifiedSystem,
                         kEndRounded),
      model_(base::MakeRefCounted<UnifiedSystemTrayModel>(shelf)),
      slider_bubble_controller_(
          std::make_unique<UnifiedSliderBubbleController>(this)),
      privacy_screen_toast_controller_(
          std::make_unique<PrivacyScreenToastController>(this)) {
  SetCallback(base::BindRepeating(&UnifiedSystemTray::OnButtonPressed,
                                  base::Unretained(this)));

  if (features::IsUserEducationEnabled()) {
    // NOTE: Set `kHelpBubbleContextKey` before `views::kElementIdentifierKey`
    // in case registration causes a help bubble to be created synchronously.
    SetProperty(kHelpBubbleContextKey, HelpBubbleContext::kAsh);
  }
  SetProperty(views::kElementIdentifierKey, kUnifiedSystemTrayElementId);

  if (media::ShouldEnableAutoFraming()) {
    autozoom_toast_controller_ = std::make_unique<AutozoomToastController>(
        this, std::make_unique<AutozoomToastController::Delegate>());
  }

  tray_container()->SetMargin(
      kUnifiedTrayContentPadding -
          ShelfConfig::Get()->status_area_hit_region_padding(),
      0);

  time_view_ = AddTrayItemToContainer(
      std::make_unique<TimeTrayItemView>(shelf, TimeView::Type::kTime));


  AddTrayItemToContainer(std::make_unique<ScreenCaptureTrayItemView>(shelf));

  if (features::IsSnoopingProtectionEnabled()) {
    AddTrayItemToContainer(std::make_unique<SnoopingProtectionView>(shelf));
  }

  current_locale_view_ =
      AddTrayItemToContainer(std::make_unique<CurrentLocaleView>(shelf));
  ime_mode_view_ = AddTrayItemToContainer(std::make_unique<ImeModeView>(shelf));
  managed_device_view_ = AddTrayItemToContainer(
      std::make_unique<ManagedDeviceTrayItemView>(shelf));
  hotspot_tray_view_ =
      AddTrayItemToContainer(std::make_unique<HotspotTrayView>(shelf));

  if (features::IsSeparateNetworkIconsEnabled()) {
    AddTrayItemToContainer(std::make_unique<NetworkTrayView>(
        shelf, ActiveNetworkIcon::Type::kCellular));
    network_tray_view_ =
        AddTrayItemToContainer(std::make_unique<NetworkTrayView>(
            shelf, ActiveNetworkIcon::Type::kPrimary));
  } else {
    network_tray_view_ =
        AddTrayItemToContainer(std::make_unique<NetworkTrayView>(
            shelf, ActiveNetworkIcon::Type::kSingle));
  }

  power_tray_view_ =
      AddTrayItemToContainer(std::make_unique<PowerTrayView>(shelf));

  if (ShouldChannelIndicatorBeShown()) {
    base::RecordAction(base::UserMetricsAction("Tray_ShowChannelInfo"));
    channel_indicator_view_ =
        AddTrayItemToContainer(std::make_unique<ChannelIndicatorView>(
            shelf, Shell::Get()->shell_delegate()->GetChannel()));
  }

  set_separator_visibility(false);
  set_use_bounce_in_animation(false);

  ShelfConfig::Get()->AddObserver(this);
}

UnifiedSystemTray::~UnifiedSystemTray() {
  ShelfConfig::Get()->RemoveObserver(this);

  DestroyBubble();
}

void UnifiedSystemTray::AddObserver(Observer* observer) {
  if (observer) {
    observers_.AddObserver(observer);
  }
}

void UnifiedSystemTray::RemoveObserver(Observer* observer) {
  if (observer) {
    observers_.RemoveObserver(observer);
  }
}

void UnifiedSystemTray::OnButtonPressed(const ui::Event& event) {
  if (!bubble_) {
    ShowBubble();
  } else if (IsShowingCalendarView()) {
    bubble_->unified_system_tray_controller()->TransitionToMainView(
        /*restore_focus=*/true);
  } else {
    CloseBubble();
    return;
  }

  if (features::IsWelcomeTourEnabled()) {
    welcome_tour_metrics::RecordInteraction(
        Shell::Get()->session_controller()->GetLastActiveUserPrefService(),
        welcome_tour_metrics::Interaction::kQuickSettings);
  }
}

bool UnifiedSystemTray::IsBubbleShown() const {
  return !!bubble_;
}

bool UnifiedSystemTray::IsSliderBubbleShown() const {
  return slider_bubble_controller_->IsBubbleShown();
}

UnifiedSliderView* UnifiedSystemTray::GetSliderView() const {
  return slider_bubble_controller_->slider_view();
}

bool UnifiedSystemTray::IsBubbleActive() const {
  return bubble_ && bubble_->IsBubbleActive();
}

void UnifiedSystemTray::ActivateBubble() {
  if (bubble_) {
    bubble_->GetBubbleWidget()->Activate();
  }
}

void UnifiedSystemTray::CloseSecondaryBubbles() {
  slider_bubble_controller_->CloseBubble();
  privacy_screen_toast_controller_->HideToast();
  if (autozoom_toast_controller_) {
    autozoom_toast_controller_->HideToast();
  }
}

void UnifiedSystemTray::ShowVolumeSliderBubble() {
  slider_bubble_controller_->ShowBubble(
      UnifiedSliderBubbleController::SLIDER_TYPE_VOLUME);
}

void UnifiedSystemTray::ShowAudioDetailedViewBubble() {
  ShowBubble();

  // There is a case that `bubble_` is still a nullptr after `ShowBubble()` is
  // called (e.g. in kiosk mode, `ShowBubbleInternal()` will early return, and
  // `bubble_` is still uninitialized). Only show detailed view if `bubble_` is
  // not null.
  if (bubble_) {
    bubble_->ShowAudioDetailedView();
  }
}

void UnifiedSystemTray::ShowDisplayDetailedViewBubble() {
  ShowBubble();

  // There is a case that `bubble_` is still a nullptr after `ShowBubble()` is
  // called (e.g. in kiosk mode, `ShowBubbleInternal()` will early return, and
  // `bubble_` is still uninitialized). Only show detailed view if `bubble_` is
  // not null.
  if (bubble_) {
    bubble_->ShowDisplayDetailedView();
  }
}

void UnifiedSystemTray::ShowNetworkDetailedViewBubble() {
  ShowBubble();

  // There is a case that `bubble_` is still a nullptr after `ShowBubble()` is
  // called (e.g. in kiosk mode, `ShowBubbleInternal()` will early return, and
  // `bubble_` is still uninitialized). Only show detailed view if `bubble_` is
  // not null.
  if (bubble_) {
    bubble_->ShowNetworkDetailedView();
  }
}

void UnifiedSystemTray::NotifySecondaryBubbleHeight(int height) {
  for (auto& observer : observers_) {
    observer.OnSliderBubbleHeightChanged();
  }
}

void UnifiedSystemTray::NotifyLeavingCalendarView() {
  for (auto& observer : observers_) {
    observer.OnLeavingCalendarView();
  }
}

gfx::Rect UnifiedSystemTray::GetBubbleBoundsInScreen() const {
  return bubble_ ? bubble_->GetBoundsInScreen() : gfx::Rect();
}

// TODO(b/310298302) Remove or rename this method. Also remove the
// `FirstInteractionType` enum.
void UnifiedSystemTray::MaybeRecordFirstInteraction(FirstInteractionType type) {
  if (first_interaction_recorded_) {
    return;
  }
  first_interaction_recorded_ = true;

  UMA_HISTOGRAM_ENUMERATION("ChromeOS.SystemTray.FirstInteraction", type,
                            FirstInteractionType::kMaxValue);
}

void UnifiedSystemTray::UpdateAfterLoginStatusChange() {
  SetVisiblePreferred(true);
  PreferredSizeChanged();
}

bool UnifiedSystemTray::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
}

views::Widget* UnifiedSystemTray::GetBubbleWidget() const {
  return bubble_ ? bubble_->GetBubbleWidget() : nullptr;
}

TrayBubbleView* UnifiedSystemTray::GetBubbleView() {
  return bubble_ ? bubble_->GetBubbleView() : nullptr;
}

std::optional<AcceleratorAction> UnifiedSystemTray::GetAcceleratorAction()
    const {
  return std::make_optional(AcceleratorAction::kToggleSystemTrayBubble);
}

void UnifiedSystemTray::OnShelfConfigUpdated() {
  // Ensure the margin is updated correctly depending on whether dense shelf
  // is currently shown or not.
  tray_container()->SetMargin(
      kUnifiedTrayContentPadding -
          ShelfConfig::Get()->status_area_hit_region_padding(),
      0);
}

void UnifiedSystemTray::OnOpeningCalendarView() {
  SetIsActive(false);
  for (auto& observer : observers_) {
    observer.OnOpeningCalendarView();
  }
}

void UnifiedSystemTray::OnTransitioningFromCalendarToMainView() {
  SetIsActive(true);
  for (auto& observer : observers_) {
    observer.OnLeavingCalendarView();
  }
}

void UnifiedSystemTray::OnDisplayTabletStateChanged(
    display::TabletState state) {
  if (display::IsTabletStateChanging(state)) {
    // Do nothing when the tablet state is still in the process of transition.
    return;
  }

  UpdateLayout();
}

void UnifiedSystemTray::OnDateTrayActionPerformed(const ui::Event& event) {
  if (!bubble_) {
    ShowBubble();
  }

  // System Tray bubble will never be shown in kiosk app mode. So after
  // `ShowBubble()` is called, there's still no bubble, the calendar view should
  // not show up.
  if (!bubble_) {
    return;
  }

  bubble_->ShowCalendarView(calendar_metrics::CalendarViewShowSource::kTimeView,
                            calendar_metrics::GetEventType(event));
}

bool UnifiedSystemTray::IsShowingCalendarView() const {
  if (!bubble_) {
    return false;
  }

  return bubble_->ShowingCalendarView();
}

bool UnifiedSystemTray::ShouldChannelIndicatorBeShown() const {
  return channel_indicator_utils::IsDisplayableChannel(
      Shell::Get()->shell_delegate()->GetChannel());
}

void UnifiedSystemTray::SetTrayEnabled(bool enabled) {
  // We should close bubble at this point. If it remains opened and interactive,
  // it can be dangerous (http://crbug.com/497080).
  if (!enabled && bubble_) {
    CloseBubble();
  }

  SetEnabled(enabled);
}

void UnifiedSystemTray::ShowBubble() {
  // ShowBubbleInternal will be called from UiDelegate.
  if (!bubble_) {
    time_opened_ = base::TimeTicks::Now();
    ShowBubbleInternal();
    Shell::Get()->system_tray_notifier()->NotifySystemTrayBubbleShown();
  }
}

void UnifiedSystemTray::CloseBubbleInternal() {
  base::UmaHistogramMediumTimes("Ash.QuickSettings.UserJourneyTime",
                                base::TimeTicks::Now() - time_opened_);
  HideBubbleInternal();
}

std::u16string UnifiedSystemTray::GetAccessibleNameForBubble() {
  if (IsBubbleShown()) {
    return GetAccessibleNameForQuickSettingsBubble();
  } else {
    return GetAccessibleNameForTray();
  }
}

std::u16string UnifiedSystemTray::GetAccessibleNameForQuickSettingsBubble() {
    if (bubble_->quick_settings_view()->IsDetailedViewShown()) {
      return bubble_->quick_settings_view()->GetDetailedViewAccessibleName();
    }

    return l10n_util::GetStringUTF16(
        IDS_ASH_QUICK_SETTINGS_BUBBLE_ACCESSIBLE_DESCRIPTION);
}

void UnifiedSystemTray::HandleLocaleChange() {
  // Re-adds the child views to force the layer's bounds to be updated
  // (`SetLayerBounds`) for text direction (if needed).
  tray_container()->RemoveAllChildViewsWithoutDeleting();
  for (TrayItemView* item : tray_items_) {
    item->HandleLocaleChange();
    tray_container()->AddChildView(item);
  }
}

std::u16string UnifiedSystemTray::GetAccessibleNameForTray() {
  std::u16string time = base::TimeFormatTimeOfDayWithHourClockType(
      base::Time::Now(),
      Shell::Get()->system_tray_model()->clock()->hour_clock_type(),
      base::kKeepAmPm);
  std::u16string battery = PowerStatus::Get()->GetAccessibleNameString(false);
  std::vector<std::u16string> status = {time, battery};

  status.push_back(channel_indicator_view_ &&
                           channel_indicator_view_->GetVisible()
                       ? channel_indicator_view_->GetAccessibleNameString()
                       : std::u16string());

  std::u16string network_string, hotspot_string;
  if (network_tray_view_->GetVisible()) {
    network_string = network_tray_view_->GetAccessibleNameString();
  }
  if (hotspot_tray_view_ && hotspot_tray_view_->GetVisible()) {
    hotspot_string = hotspot_tray_view_->GetAccessibleNameString();
  }
  if (!network_string.empty() && !hotspot_string.empty()) {
    status.push_back(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_ACCESSIBLE_DESCRIPTION,
        {hotspot_string, network_string}, /*offsets=*/nullptr));
  } else if (!hotspot_string.empty()) {
    status.push_back(hotspot_string);
  } else {
    status.push_back(network_string);
  }

  status.push_back(managed_device_view_->GetVisible()
                       ? managed_device_view_->image_view()->GetTooltipText()
                       : std::u16string());

  status.push_back(ime_mode_view_->GetVisible()
                       ? ime_mode_view_->label()->GetAccessibleNameString()
                       : std::u16string());
  status.push_back(
      current_locale_view_->GetVisible()
          ? current_locale_view_->label()->GetAccessibleNameString()
          : std::u16string());

  return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION,
                                    status, nullptr);
}

void UnifiedSystemTray::HideBubble(const TrayBubbleView* bubble_view) {
  CloseBubble();
}

void UnifiedSystemTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {}

void UnifiedSystemTray::ClickedOutsideBubble(const ui::LocatedEvent& event) {
  const gfx::Point event_location =
      event.target() ? event.target()->GetScreenLocation(event)
                     : event.root_location();

  // When Quick Settings bubble is opened and the date tray is clicked, the
  // bubble should not be closed since it will transition to show calendar.
  if (shelf()->GetStatusAreaWidget()->date_tray()->GetBoundsInScreen().Contains(
          event_location)) {
    return;
  }

  CloseBubble();
}

void UnifiedSystemTray::UpdateLayout() {
  TrayBackgroundView::UpdateLayout();
  time_view_->UpdateAlignmentForShelf(shelf());
}

void UnifiedSystemTray::ShowBubbleInternal() {
  // Never show System Tray bubble in kiosk app mode.
  if (Shell::Get()->session_controller()->IsRunningInAppMode()) {
    return;
  }

  CloseSecondaryBubbles();

  // Presentation time recorder for opening QuickSettings through
  // UnifiedSystemTray button.
  auto presentation_time_recorder = CreatePresentationTimeHistogramRecorder(
      shelf()->GetStatusAreaWidget()->GetCompositor(),
      kStatusAreaShowBubbleHistogram);
  presentation_time_recorder->RequestNext();

  bubble_ = std::make_unique<UnifiedSystemTrayBubble>(this);
  bubble_->unified_system_tray_controller()->AddObserver(this);

  // crbug/1310675 Add observers in `UnifiedSystemTrayBubble` after both bubbles
  // have been completely created, without this the bubbles can be destroyed
  // before their creation is complete resulting in crashes.
  bubble_->InitializeObservers();

  if (Shell::Get()->accessibility_controller()->spoken_feedback().enabled()) {
    ActivateBubble();
  }

  first_interaction_recorded_ = false;

  // Do not set the tray as active if the date tray is already active, this
  // happens if `ShowBubble()` is called through `OnDateTrayActionPerformed()`.
  if (shelf()->status_area_widget()->date_tray() &&
      shelf()->status_area_widget()->date_tray()->is_active()) {
    return;
  }
  SetIsActive(true);
}

void UnifiedSystemTray::HideBubbleInternal() {
  DestroyBubble();
  SetIsActive(false);
}

template <typename T>
T* UnifiedSystemTray::AddTrayItemToContainer(
    std::unique_ptr<T> tray_item_view) {
  T* unowned_tray_item_view =
      tray_container()->AddChildView(std::move(tray_item_view));
  tray_items_.push_back(unowned_tray_item_view);
  return unowned_tray_item_view;
}

void UnifiedSystemTray::DestroyBubble() {
  if (bubble_) {
    bubble_->unified_system_tray_controller()->RemoveObserver(this);
  }
  bubble_.reset();
}

void UnifiedSystemTray::UpdateTrayItemColor(bool is_active) {
  for (TrayItemView* tray_item : tray_items_) {
    tray_item->UpdateLabelOrImageViewColor(is_active);
  }
}

BEGIN_METADATA(UnifiedSystemTray)
END_METADATA

}  // namespace ash
