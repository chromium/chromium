// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/date/date_view.h"
#include "ash/system/date/tray_system_info.h"
#include "ash/system/message_center/ash_popup_alignment_delegate.h"
#include "ash/system/message_center/message_center_ui_controller.h"
#include "ash/system/message_center/message_center_ui_delegate.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_tray_view.h"
#include "ash/system/network/tray_network_state_observer.h"
#include "ash/system/power/tray_power.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/unified/ime_mode_view.h"
#include "ash/system/unified/managed_device_view.h"
#include "ash/system/unified/notification_counter_view.h"
#include "ash/system/unified/unified_slider_bubble_controller.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/network/network_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_popup_collection.h"

namespace ash {

class UnifiedSystemTray::UiDelegate : public MessageCenterUiDelegate {
 public:
  UiDelegate(UnifiedSystemTray* owner);
  ~UiDelegate() override;

  // MessageCenterUiDelegate:
  void OnMessageCenterContentsChanged() override;
  bool ShowPopups() override;
  void HidePopups() override;
  bool ShowMessageCenter(bool show_by_click) override;
  void HideMessageCenter() override;

  MessageCenterUiController* ui_controller() { return ui_controller_.get(); }

  void SetTrayBubbleHeight(int height) {
    popup_alignment_delegate_->SetTrayBubbleHeight(height);
  }

 private:
  std::unique_ptr<MessageCenterUiController> ui_controller_;
  std::unique_ptr<AshPopupAlignmentDelegate> popup_alignment_delegate_;
  std::unique_ptr<message_center::MessagePopupCollection>
      message_popup_collection_;

  UnifiedSystemTray* const owner_;

  DISALLOW_COPY_AND_ASSIGN(UiDelegate);
};

const base::TimeDelta UnifiedSystemTray::kNotificationCountUpdateDelay =
    base::TimeDelta::FromMilliseconds(100);

UnifiedSystemTray::UiDelegate::UiDelegate(UnifiedSystemTray* owner)
    : owner_(owner) {
  ui_controller_ = std::make_unique<MessageCenterUiController>(this);
  ui_controller_->set_hide_on_last_notification(false);
  popup_alignment_delegate_ =
      std::make_unique<AshPopupAlignmentDelegate>(owner->shelf());
  message_popup_collection_ =
      std::make_unique<message_center::MessagePopupCollection>(
          popup_alignment_delegate_.get());
  message_popup_collection_->set_inverse();
  display::Screen* screen = display::Screen::GetScreen();
  popup_alignment_delegate_->StartObserving(
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
  popup_alignment_delegate_->SetTrayBubbleHeight(0);
}

bool UnifiedSystemTray::UiDelegate::ShowMessageCenter(bool show_by_click) {
  if (owner_->IsBubbleShown())
    return false;

  owner_->ShowBubbleInternal(show_by_click);
  return true;
}

void UnifiedSystemTray::UiDelegate::HideMessageCenter() {
  owner_->HideBubbleInternal();
}

class UnifiedSystemTray::NetworkStateDelegate
    : public TrayNetworkStateObserver::Delegate {
 public:
  explicit NetworkStateDelegate(tray::NetworkTrayView* tray_view);
  ~NetworkStateDelegate() override;

  // TrayNetworkStateObserver::Delegate
  void NetworkStateChanged(bool notify_a11y) override;

 private:
  tray::NetworkTrayView* const tray_view_;
  const std::unique_ptr<TrayNetworkStateObserver> network_state_observer_;

  DISALLOW_COPY_AND_ASSIGN(NetworkStateDelegate);
};

UnifiedSystemTray::NetworkStateDelegate::NetworkStateDelegate(
    tray::NetworkTrayView* tray_view)
    : tray_view_(tray_view),
      network_state_observer_(
          std::make_unique<TrayNetworkStateObserver>(this)) {}

UnifiedSystemTray::NetworkStateDelegate::~NetworkStateDelegate() = default;

void UnifiedSystemTray::NetworkStateDelegate::NetworkStateChanged(
    bool notify_a11y) {
  tray_view_->UpdateNetworkStateHandlerIcon();
  tray_view_->UpdateConnectionStatus(tray::GetConnectedNetwork(), notify_a11y);
}

UnifiedSystemTray::UnifiedSystemTray(Shelf* shelf)
    : TrayBackgroundView(shelf),
      ui_delegate_(std::make_unique<UiDelegate>(this)),
      model_(std::make_unique<UnifiedSystemTrayModel>()),
      slider_bubble_controller_(
          std::make_unique<UnifiedSliderBubbleController>(this)),
      ime_mode_view_(new ImeModeView()),
      managed_device_view_(new ManagedDeviceView()),
      notification_counter_item_(new NotificationCounterView()),
      quiet_mode_view_(new QuietModeView()),
      time_view_(new tray::TimeTrayItemView(nullptr, shelf)) {
  tray_container()->SetMargin(kUnifiedTrayContentPadding, 0);
  tray_container()->AddChildView(ime_mode_view_);
  tray_container()->AddChildView(managed_device_view_);
  tray_container()->AddChildView(notification_counter_item_);
  tray_container()->AddChildView(quiet_mode_view_);

  // It is possible in unit tests that it's missing.
  if (chromeos::NetworkHandler::IsInitialized()) {
    tray::NetworkTrayView* network_item = new tray::NetworkTrayView(nullptr);
    network_state_delegate_ =
        std::make_unique<NetworkStateDelegate>(network_item);
    tray_container()->AddChildView(network_item);
  }

  tray_container()->AddChildView(new tray::PowerTrayView(nullptr));
  tray_container()->AddChildView(time_view_);

  SetInkDropMode(InkDropMode::ON);
  set_separator_visibility(false);
}

UnifiedSystemTray::~UnifiedSystemTray() {
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

bool UnifiedSystemTray::IsBubbleActive() const {
  return bubble_ && bubble_->IsBubbleActive();
}

void UnifiedSystemTray::ActivateBubble() {
  if (bubble_)
    bubble_->ActivateBubble();
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
  ShowBubble(false /* show_by_click */);
  bubble_->ShowAudioDetailedView();
}

void UnifiedSystemTray::SetTrayBubbleHeight(int height) {
  ui_delegate_->SetTrayBubbleHeight(height);
}

gfx::Rect UnifiedSystemTray::GetBubbleBoundsInScreen() const {
  return bubble_ ? bubble_->GetBoundsInScreen() : gfx::Rect();
}

void UnifiedSystemTray::UpdateAfterLoginStatusChange() {
  SetVisible(true);
  PreferredSizeChanged();
}

bool UnifiedSystemTray::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_controller()->IsSpokenFeedbackEnabled();
}

void UnifiedSystemTray::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  TrayBackgroundView::AddInkDropLayer(ink_drop_layer);
  ink_drop_layer_ = ink_drop_layer;
}

void UnifiedSystemTray::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  DCHECK_EQ(ink_drop_layer, ink_drop_layer_);
  TrayBackgroundView::RemoveInkDropLayer(ink_drop_layer);
  ink_drop_layer_ = nullptr;
}

void UnifiedSystemTray::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  TrayBackgroundView::OnBoundsChanged(previous_bounds);
  // Workarounding an ui::Layer bug that layer mask is not properly updated.
  // https://crbug.com/860367
  // TODO(tetsui): Remove after the bug is fixed on ui::Layer side.
  if (ink_drop_layer_) {
    ResetInkDropMask();
    InstallInkDropMask(ink_drop_layer_);
  }
}

void UnifiedSystemTray::SetTrayEnabled(bool enabled) {
  // We should close bubble at this point. If it remains opened and interactive,
  // it can be dangerous (http://crbug.com/497080).
  if (!enabled && bubble_)
    CloseBubble();

  SetEnabled(enabled);
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
  // HideBubbleInternal will be called from UiDelegate.
  ui_delegate_->ui_controller()->HideMessageCenterBubble();
}

base::string16 UnifiedSystemTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

base::string16 UnifiedSystemTray::GetAccessibleNameForTray() {
  base::string16 time = base::TimeFormatTimeOfDayWithHourClockType(
      base::Time::Now(),
      Shell::Get()->system_tray_model()->clock()->hour_clock_type(),
      base::kKeepAmPm);
  base::string16 battery = PowerStatus::Get()->GetAccessibleNameString(false);
  return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION,
                                    time, battery);
}

void UnifiedSystemTray::HideBubble(const TrayBubbleView* bubble_view) {
  CloseBubble();
}

void UnifiedSystemTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {}

void UnifiedSystemTray::ClickedOutsideBubble() {
  CloseBubble();
}

void UnifiedSystemTray::UpdateAfterShelfAlignmentChange() {
  TrayBackgroundView::UpdateAfterShelfAlignmentChange();
  time_view_->UpdateAlignmentForShelf(shelf());
}

void UnifiedSystemTray::ShowBubbleInternal(bool show_by_click) {
  // Never show System Tray bubble in kiosk app mode.
  if (Shell::Get()->session_controller()->IsRunningInAppMode())
    return;

  // Hide volume/brightness slider popup.
  slider_bubble_controller_->CloseBubble();

  bubble_ = std::make_unique<UnifiedSystemTrayBubble>(this, show_by_click);
  SetIsActive(true);
}

void UnifiedSystemTray::HideBubbleInternal() {
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

}  // namespace ash
