// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/focus_cycler.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/ash_message_popup_collection.h"
#include "ash/system/message_center/message_center_ui_controller.h"
#include "ash/system/message_center/message_center_ui_delegate.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_tray_view.h"
#include "ash/system/power/tray_power.h"
#include "ash/system/privacy_screen/privacy_screen_toast_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/time/time_tray_item_view.h"
#include "ash/system/time/time_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/unified/current_locale_view.h"
#include "ash/system/unified/ime_mode_view.h"
#include "ash/system/unified/managed_device_tray_item_view.h"
#include "ash/system/unified/notification_counter_view.h"
#include "ash/system/unified/unified_slider_bubble_controller.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/message_center/message_center.h"

namespace ash {

class UnifiedSystemTray::UiDelegate : public MessageCenterUiDelegate {
 public:
  explicit UiDelegate(UnifiedSystemTray* owner);
  ~UiDelegate() override;

  // MessageCenterUiDelegate:
  void OnMessageCenterContentsChanged() override;
  bool ShowPopups() override;
  void HidePopups() override;
  bool ShowMessageCenter(bool show_by_click) override;
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

 private:
  std::unique_ptr<MessageCenterUiController> ui_controller_;
  std::unique_ptr<AshMessagePopupCollection> message_popup_collection_;

  UnifiedSystemTray* const owner_;

  DISALLOW_COPY_AND_ASSIGN(UiDelegate);
};

const base::TimeDelta UnifiedSystemTray::kNotificationCountUpdateDelay =
    base::TimeDelta::FromMilliseconds(100);

UnifiedSystemTray::UiDelegate::UiDelegate(UnifiedSystemTray* owner)
    : owner_(owner) {
  ui_controller_ = std::make_unique<MessageCenterUiController>(this);
  ui_controller_->set_hide_on_last_notification(false);
  message_popup_collection_ =
      std::make_unique<AshMessagePopupCollection>(owner->shelf());
  display::Screen* screen = display::Screen::GetScreen();
  message_popup_collection_->StartObserving(
      screen, screen->GetDisplayNearestWindow(
                  owner->shelf()->GetStatusAreaWidget()->GetNativeWindow()));
}

UnifiedSystemTray::UiDelegate::~UiDelegate() = default;

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

bool UnifiedSystemTray::UiDelegate::ShowMessageCenter(bool show_by_click) {
  if (owner_->IsBubbleShown())
    return false;

  owner_->ShowBubbleInternal(show_by_click);
  return true;
}

void UnifiedSystemTray::UiDelegate::HideMessageCenter() {}

UnifiedSystemTray::UnifiedSystemTray(Shelf* shelf)
    : TrayBackgroundView(shelf),
      ui_delegate_(std::make_unique<UiDelegate>(this)),
      model_(std::make_unique<UnifiedSystemTrayModel>(
          shelf->GetStatusAreaWidget()->GetRootView())),
      slider_bubble_controller_(
          std::make_unique<UnifiedSliderBubbleController>(this)),
      privacy_screen_toast_controller_(
          std::make_unique<PrivacyScreenToastController>(this)),
      current_locale_view_(new CurrentLocaleView(shelf)),
      ime_mode_view_(new ImeModeView(shelf)),
      managed_device_view_(new ManagedDeviceTrayItemView(shelf)),
      notification_counter_item_(new NotificationCounterView(shelf)),
      quiet_mode_view_(new QuietModeView(shelf)),
      time_view_(new tray::TimeTrayItemView(shelf)) {
  tray_container()->SetMargin(
      kUnifiedTrayContentPadding -
          ShelfConfig::Get()->status_area_hit_region_padding(),
      0);
  AddTrayItemToContainer(current_locale_view_);
  AddTrayItemToContainer(ime_mode_view_);
  AddTrayItemToContainer(managed_device_view_);
  AddTrayItemToContainer(notification_counter_item_);
  AddTrayItemToContainer(quiet_mode_view_);

  if (features::IsSeparateNetworkIconsEnabled()) {
    network_tray_view_ =
        new tray::NetworkTrayView(shelf, ActiveNetworkIcon::Type::kPrimary);
    AddTrayItemToContainer(
        new tray::NetworkTrayView(shelf, ActiveNetworkIcon::Type::kCellular));
  } else {
    network_tray_view_ =
        new tray::NetworkTrayView(shelf, ActiveNetworkIcon::Type::kSingle);
  }
  AddTrayItemToContainer(network_tray_view_);
  AddTrayItemToContainer(new tray::PowerTrayView(shelf));
  AddTrayItemToContainer(time_view_);

  set_separator_visibility(false);

  ShelfConfig::Get()->AddObserver(this);
}

UnifiedSystemTray::~UnifiedSystemTray() {
  ShelfConfig::Get()->RemoveObserver(this);

  message_center_bubble_.reset();
  // Close bubble immediately when the bubble is closed on dtor.
  if (bubble_)
    bubble_->CloseNow();
  bubble_.reset();
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
    bubble_->ActivateBubble();
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
  ShowBubble(true /* show_by_click */);
  bubble_->ShowAudioDetailedView();
}

void UnifiedSystemTray::ShowNetworkDetailedViewBubble(bool show_by_click) {
  ShowBubble(show_by_click);
  bubble_->ShowNetworkDetailedView(true /* force */);
}

void UnifiedSystemTray::SetTrayBubbleHeight(int height) {
  ui_delegate_->SetTrayBubbleHeight(height);
}

void UnifiedSystemTray::FocusFirstNotification() {
  FocusMessageCenter(false /*reverse*/);

  // Do not focus an individual element in quick settings if chrome vox is
  // enabled
  if (!ShouldEnableExtraKeyboardAccessibility())
    message_center_bubble()->FocusFirstNotification();
}

bool UnifiedSystemTray::FocusMessageCenter(bool reverse) {
  if (!IsMessageCenterBubbleShown())
    return false;

  views::Widget* message_center_widget =
      message_center_bubble_->GetBubbleWidget();
  message_center_widget->widget_delegate()->SetCanActivate(true);

  Shell::Get()->focus_cycler()->FocusWidget(message_center_widget);

  // Focus an individual element in the message center if chrome vox is
  // disabled. Otherwise, ensure the message center is expanded.
  if (!ShouldEnableExtraKeyboardAccessibility()) {
    message_center_bubble_->FocusEntered(reverse);
  } else if (message_center_bubble_->IsMessageCenterCollapsed()) {
    ExpandMessageCenter();
    EnsureQuickSettingsCollapsed(true /*animate*/);
  }
  return true;
}

bool UnifiedSystemTray::FocusQuickSettings(bool reverse) {
  if (!IsBubbleShown())
    return false;

  views::Widget* quick_settings_widget = bubble_->GetBubbleWidget();
  quick_settings_widget->widget_delegate()->SetCanActivate(true);

  Shell::Get()->focus_cycler()->FocusWidget(quick_settings_widget);

  // Focus an individual element in quick settings if chrome vox is
  // disabled. Otherwise, ensure quick settings is expanded.
  if (!ShouldEnableExtraKeyboardAccessibility())
    bubble_->FocusEntered(reverse);
  else
    EnsureBubbleExpanded();

  return true;
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

const char* UnifiedSystemTray::GetClassName() const {
  return "UnifiedSystemTray";
}

void UnifiedSystemTray::OnShelfConfigUpdated() {
  // Ensure the margin is updated correctly depending on whether dense shelf
  // is currently shown or not.
  tray_container()->SetMargin(
      kUnifiedTrayContentPadding -
          ShelfConfig::Get()->status_area_hit_region_padding(),
      0);
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
  if (bubble_) {
    CloseBubble();
  } else {
    ShowBubble(event.IsMouseEvent() || event.IsGestureEvent());
    if (event.IsKeyEvent() || (event.flags() & ui::EF_TOUCH_ACCESSIBILITY))
      ActivateBubble();
  }
  return true;
}

void UnifiedSystemTray::ShowBubble(bool show_by_click) {
  // ShowBubbleInternal will be called from UiDelegate.
  if (!bubble_)
    ui_delegate_->ui_controller()->ShowMessageCenterBubble(show_by_click);
}

void UnifiedSystemTray::CloseBubble() {
  // HideMessageCenterBubbleInternal will be called from UiDelegate.
  ui_delegate_->ui_controller()->HideMessageCenterBubble();

  HideBubbleInternal();
}

base::string16 UnifiedSystemTray::GetAccessibleNameForBubble() {
  if (IsBubbleShown())
    return GetAccessibleNameForQuickSettingsBubble();
  else
    return GetAccessibleNameForTray();
}

base::string16 UnifiedSystemTray::GetAccessibleNameForQuickSettingsBubble() {
  if (bubble_->unified_view()->IsDetailedViewShown())
    return bubble_->unified_view()->GetDetailedViewAccessibleName();

  return l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_ACCESSIBLE_DESCRIPTION);
}

void UnifiedSystemTray::HandleLocaleChange() {
  for (TrayItemView* item : tray_items_)
    item->HandleLocaleChange();
}

base::string16 UnifiedSystemTray::GetAccessibleNameForTray() {
  base::string16 time = base::TimeFormatTimeOfDayWithHourClockType(
      base::Time::Now(),
      Shell::Get()->system_tray_model()->clock()->hour_clock_type(),
      base::kKeepAmPm);
  base::string16 battery = PowerStatus::Get()->GetAccessibleNameString(false);
  std::vector<base::string16> status = {time, battery};

  status.push_back(network_tray_view_->GetVisible()
                       ? network_tray_view_->GetAccessibleNameString()
                       : base::EmptyString16());
  status.push_back(notification_counter_item_->GetVisible()
                       ? notification_counter_item_->GetAccessibleNameString()
                       : base::EmptyString16());
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

void UnifiedSystemTray::ShowBubbleInternal(bool show_by_click) {
  // Never show System Tray bubble in kiosk app mode.
  if (Shell::Get()->session_controller()->IsRunningInAppMode())
    return;

  CloseSecondaryBubbles();

  bubble_ = std::make_unique<UnifiedSystemTrayBubble>(this, show_by_click);

  message_center_bubble_ = std::make_unique<UnifiedMessageCenterBubble>(this);
  message_center_bubble_->ShowBubble();

  first_interaction_recorded_ = false;

  SetIsActive(true);
}

void UnifiedSystemTray::HideBubbleInternal() {
  message_center_bubble_.reset();
  bubble_.reset();
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
  notification_counter_item_->Update();
  quiet_mode_view_->Update();
}

message_center::MessagePopupView*
UnifiedSystemTray::GetPopupViewForNotificationID(
    const std::string& notification_id) {
  return ui_delegate_->GetPopupViewForNotificationID(notification_id);
}

void UnifiedSystemTray::AddTrayItemToContainer(TrayItemView* tray_item) {
  tray_items_.push_back(tray_item);
  tray_container()->AddChildView(tray_item);
}
}  // namespace ash
