// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/focus_cycler.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/channel_indicator/channel_indicator.h"
#include "ash/system/human_presence/snooping_protection_view.h"
#include "ash/system/message_center/ash_message_popup_collection.h"
#include "ash/system/message_center/message_center_ui_controller.h"
#include "ash/system/message_center/message_center_ui_delegate.h"
#include "ash/system/message_center/notification_grouping_controller.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_tray_view.h"
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
#include "ash/system/tray/tray_event_filter.h"
#include "ash/system/unified/camera_mic_tray_item_view.h"
#include "ash/system/unified/current_locale_view.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/ime_mode_view.h"
#include "ash/system/unified/managed_device_tray_item_view.h"
#include "ash/system/unified/notification_counter_view.h"
#include "ash/system/unified/notification_icons_controller.h"
#include "ash/system/unified/unified_slider_bubble_controller.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "base/debug/stack_trace.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_view_controller.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {
// The UMA histogram that records presentation time for opening QuickSettings
// and Notification Center through Status Area button.
constexpr char kStatusAreaShowBubbleHistogram[] =
    "Ash.StatusAreaShowBubble.PresentationTime";
}  // namespace

class UnifiedSystemTray::UiDelegate : public MessageCenterUiDelegate {
 public:
  explicit UiDelegate(UnifiedSystemTray* owner);

  UiDelegate(const UiDelegate&) = delete;
  UiDelegate& operator=(const UiDelegate&) = delete;

  ~UiDelegate() override;

  // MessageCenterUiDelegate:
  void OnMessageCenterContentsChanged() override;
  bool ShowPopups() override;
  void HidePopups() override;
  bool ShowMessageCenter() override;
  void HideMessageCenter() override;

  MessageCenterUiController* ui_controller() { return ui_controller_.get(); }

  void SetTrayBubbleHeight(int height) {
    message_popup_collection_->SetTrayBubbleHeight(height);
  }

  message_center::MessagePopupView* GetPopupViewForNotificationID(
      const std::string& notification_id) {
    return message_popup_collection_->GetPopupViewForNotificationID(
        notification_id);
  }

  AshMessagePopupCollection* message_popup_collection() {
    return message_popup_collection_.get();
  }

  NotificationGroupingController* grouping_controller() {
    return grouping_controller_.get();
  }

 private:
  std::unique_ptr<MessageCenterUiController> const ui_controller_;
  std::unique_ptr<AshMessagePopupCollection> message_popup_collection_;

  UnifiedSystemTray* const owner_;

  std::unique_ptr<NotificationGroupingController> grouping_controller_;
};

const base::TimeDelta UnifiedSystemTray::kNotificationCountUpdateDelay =
    base::Milliseconds(100);

UnifiedSystemTray::UiDelegate::UiDelegate(UnifiedSystemTray* owner)
    : ui_controller_(std::make_unique<MessageCenterUiController>(this)),
      message_popup_collection_(
          std::make_unique<AshMessagePopupCollection>(owner->shelf())),
      owner_(owner) {
  if (features::IsNotificationsRefreshEnabled()) {
    grouping_controller_ =
        std::make_unique<NotificationGroupingController>(owner);
  }

  ui_controller_->set_hide_on_last_notification(false);

  display::Screen* screen = display::Screen::GetScreen();
  message_popup_collection_->StartObserving(
      screen, screen->GetDisplayNearestWindow(
                  owner->shelf()->GetStatusAreaWidget()->GetNativeWindow()));
}

UnifiedSystemTray::UiDelegate::~UiDelegate() {
  // We need to destruct `message_popup_collection_` before
  // `grouping_controller_` to prevent a msan failure, so explicitly delete
  // it here.
  message_popup_collection_.reset();
}

void UnifiedSystemTray::UiDelegate::OnMessageCenterContentsChanged() {
  owner_->UpdateNotificationInternal();
}

bool UnifiedSystemTray::UiDelegate::ShowPopups() {
  if (owner_->IsBubbleShown())
    return false;
  return true;
}

void UnifiedSystemTray::UiDelegate::HidePopups() {
  message_popup_collection_->SetTrayBubbleHeight(0);
}

bool UnifiedSystemTray::UiDelegate::ShowMessageCenter() {
  if (owner_->IsBubbleShown())
    return false;

  owner_->ShowBubbleInternal();
  return true;
}

void UnifiedSystemTray::UiDelegate::HideMessageCenter() {}

UnifiedSystemTray::UnifiedSystemTray(Shelf* shelf)
    : TrayBackgroundView(
          shelf,
          (features::IsCalendarViewEnabled() ? kEndRounded : kAllRounded)),
      ui_delegate_(std::make_unique<UiDelegate>(this)),
      model_(base::MakeRefCounted<UnifiedSystemTrayModel>(shelf)),
      slider_bubble_controller_(
          std::make_unique<UnifiedSliderBubbleController>(this)),
      privacy_screen_toast_controller_(
          std::make_unique<PrivacyScreenToastController>(this)),
      notification_icons_controller_(
          std::make_unique<NotificationIconsController>(this)),
      snooping_protection_view_(features::IsSnoopingProtectionEnabled()
                                    ? new SnoopingProtectionView(shelf)
                                    : nullptr),
      current_locale_view_(new CurrentLocaleView(shelf)),
      ime_mode_view_(new ImeModeView(shelf)),
      managed_device_view_(new ManagedDeviceTrayItemView(shelf)),
      camera_view_(
          new CameraMicTrayItemView(shelf,
                                    CameraMicTrayItemView::Type::kCamera)),
      mic_view_(
          new CameraMicTrayItemView(shelf, CameraMicTrayItemView::Type::kMic)),
      time_view_(new TimeTrayItemView(shelf, TimeView::Type::kTime)) {
  tray_container()->SetMargin(
      kUnifiedTrayContentPadding -
          ShelfConfig::Get()->status_area_hit_region_padding(),
      0);
  if (features::IsCalendarViewEnabled()) {
    AddTrayItemToContainer(time_view_);
  }

  notification_icons_controller_->AddNotificationTrayItems(tray_container());
  for (TrayItemView* tray_item : notification_icons_controller_->tray_items()) {
    tray_items_.push_back(tray_item);
    AddObservedTrayItem(tray_item);
  }

  tray_items_.push_back(
      notification_icons_controller_->notification_counter_view());
  AddObservedTrayItem(
      notification_icons_controller_->notification_counter_view());

  tray_items_.push_back(notification_icons_controller_->quiet_mode_view());
  AddObservedTrayItem(notification_icons_controller_->quiet_mode_view());

  if (features::IsSnoopingProtectionEnabled())
    AddTrayItemToContainer(snooping_protection_view_);

  AddTrayItemToContainer(current_locale_view_);
  AddTrayItemToContainer(ime_mode_view_);
  AddTrayItemToContainer(managed_device_view_);
  AddTrayItemToContainer(camera_view_);
  AddTrayItemToContainer(mic_view_);

  if (features::IsSeparateNetworkIconsEnabled()) {
    network_tray_view_ =
        new NetworkTrayView(shelf, ActiveNetworkIcon::Type::kPrimary);
    AddTrayItemToContainer(
        new NetworkTrayView(shelf, ActiveNetworkIcon::Type::kCellular));
  } else {
    network_tray_view_ =
        new NetworkTrayView(shelf, ActiveNetworkIcon::Type::kSingle);
  }

  AddTrayItemToContainer(network_tray_view_);
  AddTrayItemToContainer(new PowerTrayView(shelf));

  if (features::IsReleaseTrackUiEnabled()) {
    channel_indicator_view_ = new ChannelIndicatorView(
        shelf, Shell::Get()->shell_delegate()->GetChannel());
    AddTrayItemToContainer(channel_indicator_view_);
  }

  auto vertical_clock_padding = std::make_unique<views::View>();
  vertical_clock_padding->SetPreferredSize(gfx::Size(
      0, features::IsCalendarViewEnabled() ? 0 : kTrayTimeIconTopPadding));
  vertical_clock_padding_ =
      tray_container()->AddChildView(std::move(vertical_clock_padding));

  if (!features::IsCalendarViewEnabled()) {
    AddTrayItemToContainer(time_view_);
  }

  set_separator_visibility(false);
  set_use_bounce_in_animation(false);

  ShelfConfig::Get()->AddObserver(this);
  Shell::Get()->AddShellObserver(this);
}

UnifiedSystemTray::~UnifiedSystemTray() {
  ShelfConfig::Get()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);

  DestroyBubbles();

  // We need to destruct `ui_delegate_` before `timer_` to prevent a msan
  // failure, so explicitly delete it here.
  ui_delegate_.reset();
}

void UnifiedSystemTray::AddObserver(Observer* observer) {
  if (observer)
    observers_.AddObserver(observer);
}

void UnifiedSystemTray::RemoveObserver(Observer* observer) {
  if (observer)
    observers_.RemoveObserver(observer);
}

bool UnifiedSystemTray::MoreThanOneVisibleTrayItem() const {
  bool one_visible_item = false;
  for (TrayItemView* item : tray_items_) {
    if (!item->GetVisible())
      continue;
    if (one_visible_item)
      return true;
    one_visible_item = true;
  }
  return false;
}

void UnifiedSystemTray::MaybeUpdateVerticalClockPadding() {
  const bool padding_is_visible = vertical_clock_padding_->GetVisible();

  if (shelf()->IsHorizontalAlignment()) {
    if (padding_is_visible)
      vertical_clock_padding_->SetVisible(false);
    return;
  }

  // Padding is shown when an icon besides TimeView is visible.
  const bool should_show_padding = MoreThanOneVisibleTrayItem();
  if (padding_is_visible != should_show_padding)
    vertical_clock_padding_->SetVisible(should_show_padding);
}

void UnifiedSystemTray::OnViewVisibilityChanged(views::View* observed_view,
                                                views::View* starting_view) {
  MaybeUpdateVerticalClockPadding();
}

bool UnifiedSystemTray::IsBubbleShown() const {
  return !!bubble_;
}

bool UnifiedSystemTray::IsSliderBubbleShown() const {
  return slider_bubble_controller_->IsBubbleShown();
}

bool UnifiedSystemTray::IsMessageCenterBubbleShown() const {
  if (message_center_bubble_)
    return message_center_bubble_->IsMessageCenterVisible();

  return false;
}

bool UnifiedSystemTray::IsBubbleActive() const {
  return bubble_ && bubble_->IsBubbleActive();
}

void UnifiedSystemTray::ActivateBubble() {
  if (bubble_)
    bubble_->GetBubbleWidget()->Activate();
}

void UnifiedSystemTray::CloseSecondaryBubbles() {
  slider_bubble_controller_->CloseBubble();
  privacy_screen_toast_controller_->HideToast();
}

void UnifiedSystemTray::CollapseMessageCenter() {
  if (message_center_bubble_)
    message_center_bubble_->CollapseMessageCenter();
}

void UnifiedSystemTray::ExpandMessageCenter() {
  if (message_center_bubble_)
    message_center_bubble_->ExpandMessageCenter();
}

void UnifiedSystemTray::EnsureQuickSettingsCollapsed(bool animate) {
  if (!bubble_)
    return;

  if (animate)
    bubble_->EnsureCollapsed();
  else
    bubble_->CollapseWithoutAnimating();
}

void UnifiedSystemTray::EnsureBubbleExpanded() {
  if (bubble_)
    bubble_->EnsureExpanded();
}

void UnifiedSystemTray::ShowVolumeSliderBubble() {
  slider_bubble_controller_->ShowBubble(
      UnifiedSliderBubbleController::SLIDER_TYPE_VOLUME);
}

void UnifiedSystemTray::ShowAudioDetailedViewBubble() {
  // The settings menu bubble gains focus when |show_by_click| is true.
  ShowBubble();
  bubble_->ShowAudioDetailedView();
}

void UnifiedSystemTray::ShowNetworkDetailedViewBubble() {
  ShowBubble();
  bubble_->ShowNetworkDetailedView(true /* force */);
}

void UnifiedSystemTray::SetTrayBubbleHeight(int height) {
  ui_delegate_->SetTrayBubbleHeight(height);
}

bool UnifiedSystemTray::FocusMessageCenter(bool reverse,
                                           bool collapse_quick_settings) {
  if (!IsMessageCenterBubbleShown())
    return false;

  views::Widget* message_center_widget =
      message_center_bubble_->GetBubbleWidget();
  message_center_widget->widget_delegate()->SetCanActivate(true);

  Shell::Get()->focus_cycler()->FocusWidget(message_center_widget);

  // Focus an individual element in the message center if chrome vox is
  // disabled.
  if (!ShouldEnableExtraKeyboardAccessibility())
    message_center_bubble_->FocusEntered(reverse);

  return true;
}

bool UnifiedSystemTray::FocusQuickSettings(bool reverse) {
  if (!IsBubbleShown())
    return false;

  views::Widget* quick_settings_widget = bubble_->GetBubbleWidget();
  quick_settings_widget->widget_delegate()->SetCanActivate(true);

  Shell::Get()->focus_cycler()->FocusWidget(quick_settings_widget);

  // Focus an individual element in quick settings if chrome vox is
  // disabled.
  if (!ShouldEnableExtraKeyboardAccessibility())
    bubble_->FocusEntered(reverse);

  return true;
}

void UnifiedSystemTray::NotifyLeavingCalendarView() {
  for (auto& observer : observers_)
    observer.OnLeavingCalendarView();
}

bool UnifiedSystemTray::IsQuickSettingsExplicitlyExpanded() const {
  return model_->IsExplicitlyExpanded();
}

gfx::Rect UnifiedSystemTray::GetBubbleBoundsInScreen() const {
  return bubble_ ? bubble_->GetBoundsInScreen() : gfx::Rect();
}

void UnifiedSystemTray::MaybeRecordFirstInteraction(FirstInteractionType type) {
  if (first_interaction_recorded_)
    return;
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

const char* UnifiedSystemTray::GetClassName() const {
  return "UnifiedSystemTray";
}

absl::optional<AcceleratorAction> UnifiedSystemTray::GetAcceleratorAction()
    const {
  return absl::make_optional(TOGGLE_SYSTEM_TRAY_BUBBLE);
}

void UnifiedSystemTray::OnShelfAlignmentChanged(aura::Window* root_window,
                                                ShelfAlignment old_alignment) {
  MaybeUpdateVerticalClockPadding();
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
  for (auto& observer : observers_)
    observer.OnOpeningCalendarView();
}

void UnifiedSystemTray::OnTransitioningFromCalendarToMainView() {
  SetIsActive(true);
  for (auto& observer : observers_)
    observer.OnLeavingCalendarView();
}

void UnifiedSystemTray::OnDateTrayActionPerformed(const ui::Event& event) {
  if (!bubble_)
    ShowBubble();
  bubble_->ShowCalendarView(calendar_metrics::CalendarViewShowSource::kTimeView,
                            calendar_metrics::GetEventType(event));
}

bool UnifiedSystemTray::IsShowingCalendarView() const {
  if (!bubble_)
    return false;

  return bubble_->ShowingCalendarView();
}

void UnifiedSystemTray::SetTrayEnabled(bool enabled) {
  // We should close bubble at this point. If it remains opened and interactive,
  // it can be dangerous (http://crbug.com/497080).
  if (!enabled && bubble_)
    CloseBubble();

  SetEnabled(enabled);
}

void UnifiedSystemTray::SetTargetNotification(
    const std::string& notification_id) {
  model_->SetTargetNotification(notification_id);
}

bool UnifiedSystemTray::PerformAction(const ui::Event& event) {
  if (!GetBubbleWidget()) {
    ShowBubble();
  } else if (IsShowingCalendarView()) {
    bubble_->unified_system_tray_controller()->TransitionToMainView(
        /*restore_focus=*/true);
  } else {
    CloseBubble();
  }

  return true;
}

void UnifiedSystemTray::ShowBubble() {
  // ShowBubbleInternal will be called from UiDelegate.
  if (!bubble_) {
    time_opened_ = base::TimeTicks::Now();
    ui_delegate_->ui_controller()->ShowMessageCenterBubble();
    Shell::Get()->system_tray_notifier()->NotifySystemTrayBubbleShown();
  }
}

void UnifiedSystemTray::CloseBubble() {
  base::UmaHistogramMediumTimes("Ash.QuickSettings.UserJourneyTime",
                                base::TimeTicks::Now() - time_opened_);
  // HideMessageCenterBubbleInternal will be called from UiDelegate.
  ui_delegate_->ui_controller()->HideMessageCenterBubble();

  HideBubbleInternal();
}

std::u16string UnifiedSystemTray::GetAccessibleNameForBubble() {
  if (IsBubbleShown())
    return GetAccessibleNameForQuickSettingsBubble();
  else
    return GetAccessibleNameForTray();
}

std::u16string UnifiedSystemTray::GetAccessibleNameForQuickSettingsBubble() {
  if (bubble_->unified_view()->IsDetailedViewShown())
    return bubble_->unified_view()->GetDetailedViewAccessibleName();

  return l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_ACCESSIBLE_DESCRIPTION);
}

void UnifiedSystemTray::HandleLocaleChange() {
  for (TrayItemView* item : tray_items_)
    item->HandleLocaleChange();
}

std::u16string UnifiedSystemTray::GetAccessibleNameForTray() {
  std::u16string time = base::TimeFormatTimeOfDayWithHourClockType(
      base::Time::Now(),
      Shell::Get()->system_tray_model()->clock()->hour_clock_type(),
      base::kKeepAmPm);
  std::u16string battery = PowerStatus::Get()->GetAccessibleNameString(false);
  std::vector<std::u16string> status = {time, battery};

  status.push_back(network_tray_view_->GetVisible()
                       ? network_tray_view_->GetAccessibleNameString()
                       : base::EmptyString16());
  status.push_back(mic_view_->GetVisible()
                       ? mic_view_->GetAccessibleNameString()
                       : base::EmptyString16());
  status.push_back(camera_view_->GetVisible()
                       ? camera_view_->GetAccessibleNameString()
                       : base::EmptyString16());
  status.push_back(managed_device_view_->GetVisible()
                       ? managed_device_view_->image_view()->GetTooltipText()
                       : base::EmptyString16());
  status.push_back(notification_icons_controller_->GetAccessibleNameString());
  status.push_back(ime_mode_view_->GetVisible()
                       ? ime_mode_view_->label()->GetAccessibleNameString()
                       : base::EmptyString16());
  status.push_back(
      current_locale_view_->GetVisible()
          ? current_locale_view_->label()->GetAccessibleNameString()
          : base::EmptyString16());

  return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION,
                                    status, nullptr);
}

void UnifiedSystemTray::HideBubble(const TrayBubbleView* bubble_view) {
  CloseBubble();
}

void UnifiedSystemTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {}

void UnifiedSystemTray::ClickedOutsideBubble() {
  CloseBubble();
}

void UnifiedSystemTray::UpdateLayout() {
  TrayBackgroundView::UpdateLayout();
  time_view_->UpdateAlignmentForShelf(shelf());
}

void UnifiedSystemTray::ShowBubbleInternal() {
  // Never show System Tray bubble in kiosk app mode.
  if (Shell::Get()->session_controller()->IsRunningInAppMode())
    return;

  CloseSecondaryBubbles();

  // Presentation time recorder for opening QuickSettings and Notification
  // Center through Status Area button.
  auto presentation_time_recorder = CreatePresentationTimeHistogramRecorder(
      shelf()->GetStatusAreaWidget()->GetCompositor(),
      kStatusAreaShowBubbleHistogram);
  presentation_time_recorder->RequestNext();

  bubble_ = std::make_unique<UnifiedSystemTrayBubble>(this);
  bubble_->unified_system_tray_controller()->AddObserver(this);

  message_center_bubble_ = std::make_unique<UnifiedMessageCenterBubble>(this);
  message_center_bubble_->ShowBubble();

  // crbug/1310675 Add observers in `UnifiedSystemTrayBubble` after both bubbles
  // have been completely created, without this the bubbles can be destroyed
  // before their creation is complete resulting in crashes.
  bubble_->InitializeObservers();

  if (Shell::Get()->accessibility_controller()->spoken_feedback().enabled())
    ActivateBubble();

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
  DestroyBubbles();
  SetIsActive(false);
}

void UnifiedSystemTray::UpdateNotificationInternal() {
  // Limit update frequency in order to avoid flashing when 2 updates are
  // incoming in a very short period of time. It happens when ARC++ apps
  // creating bundled notifications.
  if (!timer_.IsRunning()) {
    timer_.Start(FROM_HERE, kNotificationCountUpdateDelay, this,
                 &UnifiedSystemTray::UpdateNotificationAfterDelay);
  }
}

void UnifiedSystemTray::UpdateNotificationAfterDelay() {
  notification_icons_controller_->UpdateNotificationIndicators();
}

message_center::MessagePopupView*
UnifiedSystemTray::GetPopupViewForNotificationID(
    const std::string& notification_id) {
  return ui_delegate_->GetPopupViewForNotificationID(notification_id);
}

AshMessagePopupCollection* UnifiedSystemTray::GetMessagePopupCollection() {
  // We need a check here since this function might be called when UiDelegate's
  // dtor is triggered. In that case, the unique_ptr `ui_delegate_` is null even
  // though the UiDelegate object is still in the middle of dtor process.
  if (!ui_delegate_)
    return nullptr;
  return ui_delegate_->message_popup_collection();
}

NotificationGroupingController*
UnifiedSystemTray::GetNotificationGroupingController() {
  // We need a check here since this function might be called when UiDelegate's
  // dtor is triggered. In that case, the unique_ptr `ui_delegate_` is null even
  // though the UiDelegate object is still in the middle of dtor process.
  if (!ui_delegate_)
    return nullptr;
  return ui_delegate_->grouping_controller();
}

void UnifiedSystemTray::AddTrayItemToContainer(TrayItemView* tray_item) {
  tray_items_.push_back(tray_item);
  tray_container()->AddChildView(tray_item);
  AddObservedTrayItem(tray_item);
}

void UnifiedSystemTray::AddObservedTrayItem(TrayItemView* tray_item) {
  tray_items_observations_.AddObservation(tray_item);
}

void UnifiedSystemTray::DestroyBubbles() {
  message_center_bubble_.reset();
  if (bubble_)
    bubble_->unified_system_tray_controller()->RemoveObserver(this);
  bubble_.reset();
}

}  // namespace ash
