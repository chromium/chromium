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
#include "ash/wm/window_properties.h"
#include "base/i18n/rtl.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
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
  auto init_params = CreateInitParamsForTrayBubble(tray);
  init_params.has_shadow = false;
  init_params.close_on_deactivate = false;
  init_params.reroute_event_handler = false;

  // Anchor rect and insets for this bubble is set in `UpdatePosition()`.
  init_params.anchor_rect = gfx::Rect();
  init_params.insets = gfx::Insets();

  bubble_view_ = new TrayBubbleView(init_params);

  notification_center_view_ =
      bubble_view_->AddChildView(std::make_unique<NotificationCenterView>());

  time_to_click_recorder_ =
      std::make_unique<TimeToClickRecorder>(this, notification_center_view_);

  notification_center_view_->AddObserver(this);
}

void UnifiedMessageCenterBubble::ShowBubble() {
  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  bubble_widget_->AddObserver(this);
  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);

  aura::Window* bubble_window = bubble_widget_->GetNativeWindow();
  bubble_window->SetProperty(kStayInOverviewOnActivationKey, true);

  // Stack system tray bubble's window above message center's window, such that
  // message center's shadow will not cover on system tray.
  tray_->GetBubbleWindowContainer()->StackChildAbove(
      tray_->bubble()->GetBubbleWidget()->GetNativeWindow(), bubble_window);

  if (features::IsSystemTrayShadowEnabled()) {
    // Create a shadow for bubble widget.
    shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForWindow(
        bubble_widget_->GetNativeWindow(), SystemShadow::Type::kElevation12);
    shadow_->SetRoundedCornerRadius(kBubbleCornerRadius);
  }

  bubble_view_->InitializeAndShowBubble();
  notification_center_view_->Init();
  UpdateBubbleState();

  tray_event_filter_ = std::make_unique<TrayEventFilter>(
      bubble_widget_, bubble_view_, /*tray_button=*/tray_);
  tray_->bubble()->unified_view()->AddObserver(this);
}

UnifiedMessageCenterBubble::~UnifiedMessageCenterBubble() {
  if (bubble_widget_) {
    if (tray_->bubble()->unified_view()) {
      tray_->bubble()->unified_view()->RemoveObserver(this);
    }
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

void UnifiedMessageCenterBubble::CollapseMessageCenter() {}

void UnifiedMessageCenterBubble::ExpandMessageCenter() {}

void UnifiedMessageCenterBubble::UpdatePosition() {}

void UnifiedMessageCenterBubble::FocusEntered(bool reverse) {}

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
  return false;
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

void UnifiedMessageCenterBubble::HideBubble(const TrayBubbleView* bubble_view) {
  tray_->CloseBubble();
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

void UnifiedMessageCenterBubble::OnViewIsDeleting(views::View* observed_view) {
  CHECK(!features::IsQsRevampEnabled());
  auto* system_tray_bubble = tray_->bubble();
  // The `UnifiedSystemTray` drops its `UnifiedSystemTrayBubble` pointer during
  // shutdown.
  if (!system_tray_bubble) {
    return;
  }
  auto* unified_view = system_tray_bubble->unified_view();
  if (observed_view != unified_view) {
    return;
  }

  unified_view->RemoveObserver(this);
}

void UnifiedMessageCenterBubble::OnWidgetDestroying(views::Widget* widget) {
  CHECK_EQ(bubble_widget_, widget);
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

void UnifiedMessageCenterBubble::UpdateBubbleState() {}

int UnifiedMessageCenterBubble::CalculateAvailableHeight() {
  // TODO(crbug/1311738): Temporary fix to prevent crashes in case the quick
  // settings bubble is destroyed before the message center bubble. In the long
  // term we should remove this code altogether and calculate the max height for
  // the message center bubble separately.
  if (!tray_->bubble())
    return 0;

  auto* window_container = tray_->GetBubbleWindowContainer();
  return CalculateMaxTrayBubbleHeight(window_container) -
         tray_->bubble()->GetCurrentTrayHeight() -
         GetBubbleInsetHotseatCompensation(window_container) -
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
