// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/unified_message_center_bubble.h"

#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/bubble/bubble_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/system_shadow.h"
#include "ash/system/message_center/message_center_style.h"
#include "ash/system/notification_center/notification_center_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_event_filter.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "base/i18n/rtl.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/widget/widget.h"

namespace ash {

// We need to draw a custom inner border for the message center in a separate
// layer so we can properly clip ARC notifications. Each ARC notification is
// contained in its own Window with its own layer, and the border needs to be
// drawn on top of them all.
class UnifiedMessageCenterBubble::Border : public ui::LayerDelegate {
 public:
  Border() : layer_(ui::LAYER_TEXTURED) {
    layer_.set_delegate(this);
    layer_.SetFillsBoundsOpaquely(false);
  }

  Border(const Border&) = delete;
  Border& operator=(const Border&) = delete;

  ~Border() override = default;

  ui::Layer* layer() { return &layer_; }

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    gfx::Rect bounds = layer()->bounds();
    ui::PaintRecorder recorder(context, bounds.size());
    gfx::Canvas* canvas = recorder.canvas();

    // Draw a solid rounded rect as the inner border.
    cc::PaintFlags flags;
    flags.setColor(message_center_style::kSeparatorColor);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(canvas->image_scale());
    flags.setAntiAlias(true);
    canvas->DrawRoundRect(bounds, kBubbleCornerRadius, flags);
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  ui::Layer layer_;
};

UnifiedMessageCenterBubble::UnifiedMessageCenterBubble(UnifiedSystemTray* tray)
    : tray_(tray), border_(std::make_unique<Border>()) {
  TrayBubbleView::InitParams init_params;
  init_params.delegate = GetWeakPtr();
  // Anchor within the overlay container.
  init_params.parent_window = tray->GetBubbleWindowContainer();
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.preferred_width = kTrayMenuWidth;
  init_params.has_shadow = false;
  init_params.close_on_deactivate = false;
  init_params.translucent = true;

  bubble_view_ = new TrayBubbleView(init_params);

  notification_center_view_ =
      bubble_view_->AddChildView(std::make_unique<NotificationCenterView>(
          nullptr /* parent */, tray->model(), this));

  time_to_click_recorder_ =
      std::make_unique<TimeToClickRecorder>(this, notification_center_view_);

  notification_center_view_->AddObserver(this);
}

void UnifiedMessageCenterBubble::ShowBubble() {
  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  bubble_widget_->AddObserver(this);
  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);

  // Stack system tray bubble's window above message center's window, such that
  // message center's shadow will not cover on system tray.
  tray_->GetBubbleWindowContainer()->StackChildAbove(
      tray_->bubble()->GetBubbleWidget()->GetNativeWindow(),
      bubble_widget_->GetNativeWindow());

  if (features::IsSystemTrayShadowEnabled()) {
    // Create a shadow for bubble widget.
    shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForWindow(
        bubble_widget_->GetNativeWindow(), SystemShadow::Type::kElevation12);
    shadow_->SetRoundedCornerRadius(kBubbleCornerRadius);
  }

  bubble_view_->InitializeAndShowBubble();
  notification_center_view_->Init();
  UpdateBubbleState();

  tray_->tray_event_filter()->AddBubble(this);
  tray_->bubble()->unified_view()->AddObserver(this);
}

UnifiedMessageCenterBubble::~UnifiedMessageCenterBubble() {
  if (bubble_widget_) {
    tray_->tray_event_filter()->RemoveBubble(this);
    tray_->bubble()->unified_view()->RemoveObserver(this);
    CHECK(notification_center_view_);
    notification_center_view_->RemoveObserver(this);

    bubble_view_->ResetDelegate();
    bubble_widget_->RemoveObserver(this);
    bubble_widget_->Close();
  }
  CHECK(!views::WidgetObserver::IsInObserverList());
}

gfx::Rect UnifiedMessageCenterBubble::GetBoundsInScreen() const {
  DCHECK(bubble_view_);
  return bubble_view_->GetBoundsInScreen();
}

void UnifiedMessageCenterBubble::CollapseMessageCenter() {
  if (notification_center_view_->collapsed())
    return;

  notification_center_view_->SetCollapsed(true /*animate*/);
}

void UnifiedMessageCenterBubble::ExpandMessageCenter() {
  if (!notification_center_view_->collapsed())
    return;

  if (tray_->IsShowingCalendarView())
    tray_->bubble()->unified_system_tray_controller()->TransitionToMainView(
        /*restore_focus=*/true);
  notification_center_view_->SetExpanded();
  UpdatePosition();
  tray_->EnsureQuickSettingsCollapsed(true /*animate*/);
}

void UnifiedMessageCenterBubble::UpdatePosition() {
  int available_height = CalculateAvailableHeight();

  notification_center_view_->SetMaxHeight(available_height);
  notification_center_view_->SetAvailableHeight(available_height);

  if (!tray_->bubble())
    return;

  // Shelf bubbles need to be offset from the shelf, otherwise they will be
  // flush with the shelf. The bounds can't be shifted via insets because this
  // enlarges the layer bounds and this can break ARC notification rounded
  // corners. Apply the offset to the anchor rect.
  gfx::Rect anchor_rect = tray_->shelf()->GetSystemTrayAnchorRect();
  gfx::Insets tray_bubble_insets = GetTrayBubbleInsets();

  int offset;
  switch (tray_->shelf()->alignment()) {
    case ShelfAlignment::kLeft:
      offset = tray_bubble_insets.left();
      break;
    case ShelfAlignment::kRight:
      offset = -tray_bubble_insets.right();
      break;
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      if (base::i18n::IsRTL()) {
        offset = tray_bubble_insets.left();
        break;
      }
      offset = -tray_bubble_insets.right();
      break;
  }

  anchor_rect.set_x(anchor_rect.x() + offset);
  anchor_rect.set_y(anchor_rect.y() - tray_->bubble()->GetCurrentTrayHeight() -
                    tray_bubble_insets.bottom() -
                    kUnifiedMessageCenterBubbleSpacing);
  bubble_view_->ChangeAnchorRect(anchor_rect);

  notification_center_view_->UpdateNotificationBar();

  if (shadow_) {
    // When the last notification is removed, the content bounds of message
    // center may become too small such which makes the shadow's bounds smaller
    // than its blur region. To avoid this, we hide the shadow when the message
    // center has no notifications.
    shadow_->GetLayer()->SetVisible(
        notification_center_view_->notification_list_view()
            ->GetTotalNotificationCount());
  }
}

void UnifiedMessageCenterBubble::FocusEntered(bool reverse) {
  notification_center_view_->FocusEntered(reverse);
}

bool UnifiedMessageCenterBubble::FocusOut(bool reverse) {
  return tray_->FocusQuickSettings(reverse);
}

void UnifiedMessageCenterBubble::ActivateQuickSettingsBubble() {
  tray_->ActivateBubble();
}

bool UnifiedMessageCenterBubble::IsMessageCenterVisible() {
  return !!bubble_widget_ && notification_center_view_ &&
         notification_center_view_->GetVisible();
}

bool UnifiedMessageCenterBubble::IsMessageCenterCollapsed() {
  return notification_center_view_->collapsed();
}

TrayBackgroundView* UnifiedMessageCenterBubble::GetTray() const {
  return tray_;
}

TrayBubbleView* UnifiedMessageCenterBubble::GetBubbleView() const {
  return bubble_view_;
}

views::Widget* UnifiedMessageCenterBubble::GetBubbleWidget() const {
  return bubble_widget_;
}

std::u16string UnifiedMessageCenterBubble::GetAccessibleNameForBubble() {
  return l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_ACCESSIBLE_NAME);
}

bool UnifiedMessageCenterBubble::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
}

void UnifiedMessageCenterBubble::OnViewPreferredSizeChanged(
    views::View* observed_view) {
  UpdatePosition();
  bubble_view_->Layout();
}

void UnifiedMessageCenterBubble::OnViewVisibilityChanged(
    views::View* observed_view,
    views::View* starting_view) {
  // Hide the message center widget if the message center is not
  // visible. This is to ensure we do not see an empty bubble.
  if (observed_view != notification_center_view_)
    return;

  bubble_view_->UpdateBubble();
}

void UnifiedMessageCenterBubble::OnWidgetDestroying(views::Widget* widget) {
  CHECK_EQ(bubble_widget_, widget);
  tray_->tray_event_filter()->RemoveBubble(this);
  tray_->bubble()->unified_view()->RemoveObserver(this);
  notification_center_view_->RemoveObserver(this);
  bubble_widget_->RemoveObserver(this);
  bubble_widget_ = nullptr;
  shadow_.reset();
  bubble_view_->ResetDelegate();

  // Close the quick settings bubble as well, which may not automatically happen
  // when dismissing the message center bubble by pressing ESC.
  tray_->CloseBubble();
}

void UnifiedMessageCenterBubble::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  if (active)
    tray_->bubble()->OnMessageCenterActivated();
}

void UnifiedMessageCenterBubble::OnDisplayConfigurationChanged() {
  UpdateBubbleState();
}

void UnifiedMessageCenterBubble::UpdateBubbleState() {
  if (CalculateAvailableHeight() < kMessageCenterCollapseThreshold &&
      notification_center_view_->notification_list_view()
          ->GetTotalNotificationCount()) {
    if (tray_->IsQuickSettingsExplicitlyExpanded()) {
      notification_center_view_->SetCollapsed(false /*animate*/);
    } else {
      notification_center_view_->SetExpanded();
      tray_->EnsureQuickSettingsCollapsed(false /*animate*/);
    }
  } else if (notification_center_view_->collapsed()) {
    notification_center_view_->SetExpanded();
  }

  UpdatePosition();
}

int UnifiedMessageCenterBubble::CalculateAvailableHeight() {
  // TODO(crbug/1311738): Temporary fix to prevent crashes in case the quick
  // settings bubble is destroyed before the message center bubble. In the long
  // term we should remove this code altogether and calculate the max height for
  // the message center bubble separately.
  if (!tray_->bubble())
    return 0;

  return CalculateMaxTrayBubbleHeight() -
         tray_->bubble()->GetCurrentTrayHeight() -
         GetBubbleInsetHotseatCompensation() -
         kUnifiedMessageCenterBubbleSpacing;
}

void UnifiedMessageCenterBubble::RecordTimeToClick() {
  // TODO(tengs): We are currently only using this handler to record the first
  // interaction (i.e. whether the message center or quick settings was clicked
  // first). Maybe log the time to click if it is useful in the future.

  tray_->MaybeRecordFirstInteraction(
      UnifiedSystemTray::FirstInteractionType::kMessageCenter);
}

}  // namespace ash
