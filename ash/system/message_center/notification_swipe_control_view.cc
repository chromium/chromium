// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/notification_swipe_control_view.h"
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/style/icon_button.h"
#include "ash/system/message_center/message_center_constants.h"
#include "ash/system/message_center/message_center_style.h"
#include "ash/system/message_center/message_center_utils.h"
#include "ash/system/message_center/metrics_utils.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/vector_icons.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_background_painter.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

const char NotificationSwipeControlView::kViewClassName[] =
    "NotificationSwipeControlView";

NotificationSwipeControlView::NotificationSwipeControlView(
    message_center::MessageView* message_view)
    : message_view_(message_view) {
  gfx::Insets padding =
      features::IsNotificationsRefreshEnabled()
          ? kNotificationSwipeControlPadding
          : gfx::Insets::VH(
                message_center_style::kSwipeControlButtonVerticalMargin,
                message_center_style::kSwipeControlButtonHorizontalMargin);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, padding,
      message_center_style::kSwipeControlButtonHorizontalMargin));
  layout->set_cross_axis_alignment(
      features::IsNotificationsRefreshEnabled()
          ? views::BoxLayout::CrossAxisAlignment::kCenter
          : views::BoxLayout::CrossAxisAlignment::kStart);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);

  std::unique_ptr<views::ImageButton> settings_button;
  if (features::IsNotificationsRefreshEnabled()) {
    settings_button = std::make_unique<IconButton>(
        base::BindRepeating(&NotificationSwipeControlView::ButtonPressed,
                            base::Unretained(this), ButtonId::kSettings),
        IconButton::Type::kMedium, &vector_icons::kSettingsOutlineIcon,
        IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME);
  } else {
    settings_button = std::make_unique<views::ImageButton>(
        base::BindRepeating(&NotificationSwipeControlView::ButtonPressed,
                            base::Unretained(this), ButtonId::kSettings));
    settings_button->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            message_center::kNotificationSettingsButtonIcon, ui::kColorIcon,
            message_center_style::kSwipeControlButtonImageSize));
    settings_button->SetPreferredSize(
        gfx::Size(message_center_style::kSwipeControlButtonSize,
                  message_center_style::kSwipeControlButtonSize));

    settings_button->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
    settings_button->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
    settings_button->SetBackground(
        views::CreateSolidBackground(SK_ColorTRANSPARENT));
    settings_button->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  }

  settings_button->SetImageHorizontalAlignment(
      views::ImageButton::ALIGN_CENTER);
  settings_button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);

  settings_button_ = AddChildView(std::move(settings_button));
  settings_button_->SetVisible(false);

  // Draw on their own layers to round corners and perform animation.
  settings_button_->SetPaintToLayer();
  settings_button_->layer()->SetFillsBoundsOpaquely(false);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

NotificationSwipeControlView::~NotificationSwipeControlView() = default;

void NotificationSwipeControlView::ShowButtons(ButtonPosition button_position,
                                               bool show_settings,
                                               bool show_snooze) {
  views::BoxLayout* layout = static_cast<views::BoxLayout*>(GetLayoutManager());
  if ((button_position == ButtonPosition::RIGHT) != base::i18n::IsRTL()) {
    layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);
  } else {
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kStart);
  }
  ShowSettingsButton(show_settings);
  ShowSnoozeButton(show_snooze);
  Layout();
}

void NotificationSwipeControlView::HideButtons() {
  ShowSettingsButton(false);
  ShowSnoozeButton(false);
  Layout();
}

void NotificationSwipeControlView::UpdateButtonsVisibility() {
  float gesture_amount = message_view_->GetSlideAmount();
  if (gesture_amount == 0) {
    HideButtons();
    return;
  }

  NotificationSwipeControlView::ButtonPosition button_position =
      gesture_amount < 0 ? ButtonPosition::RIGHT : ButtonPosition::LEFT;
  message_center::NotificationControlButtonsView* buttons =
      message_view_->GetControlButtonsView();
  // Ignore when GetControlButtonsView() returns null.
  if (!buttons) {
    return;
  }
  bool has_settings_button = buttons->settings_button();

  if (features::IsNotificationsRefreshEnabled()) {
    if (!has_settings_button) {
      return;
    }

    int extra_padding = button_position == ButtonPosition::RIGHT
                            ? kNotificationSwipeControlPadding.left()
                            : kNotificationSwipeControlPadding.right();
    int settings_button_width = settings_button_->GetPreferredSize().width();

    // We only show the settings button if we have enough space for display.
    bool enough_space_to_show_button =
        abs(gesture_amount) >= settings_button_width + extra_padding;
    ShowButtons(button_position,
                has_settings_button && enough_space_to_show_button,
                /*show_snooze=*/false);

    message_view_->SetSlideButtonWidth(
        settings_button_ ? settings_button_width + settings_button_->width()
                         : 0);
    return;
  }

  bool has_snooze_button = buttons->snooze_button();
  ShowButtons(button_position, has_settings_button, has_snooze_button);

  int control_button_count =
      (has_settings_button ? 1 : 0) + (has_snooze_button ? 1 : 0);
  int control_button_width =
      message_center_style::kSwipeControlButtonSize * control_button_count +
      message_center_style::kSwipeControlButtonHorizontalMargin *
          (control_button_count ? control_button_count + 1 : 0);
  message_view_->SetSlideButtonWidth(control_button_width);

  // Update opacity based on the swipe progress. The swipe controls should
  // gradually disappear as the user swipes the notification away.
  float full_opacity_width =
      message_center_style::kSwipeControlFullOpacityRatio *
      control_button_width;
  float fade_out_width = message_view_->width() - full_opacity_width;
  DCHECK(fade_out_width > 0);
  float swipe_progress = std::max(
      0.0f, (fabs(gesture_amount) - full_opacity_width) / fade_out_width);
  float opacity = std::max(0.0f, 1.0f - swipe_progress);

  layer()->SetOpacity(opacity);
}

void NotificationSwipeControlView::UpdateCornerRadius(int top_radius,
                                                      int bottom_radius) {
  // In the new notification UI, there will be no swipe control background.
  if (features::IsNotificationsRefreshEnabled()) {
    return;
  }
  SetBackground(views::CreateBackgroundFromPainter(
      std::make_unique<message_center::NotificationBackgroundPainter>(
          top_radius, bottom_radius,
          message_center_style::kSwipeControlBackgroundColor)));
  SchedulePaint();
}

void NotificationSwipeControlView::ShowSettingsButton(bool show) {
  bool was_visible = settings_button_->GetVisible();
  settings_button_->SetVisible(show);

  // Fade in animation if the visibility changes from false to true.
  if (!was_visible && show) {
    message_center_utils::FadeInView(
        settings_button_, /*delay_in_ms=*/0,
        kNotificationSwipeControlFadeInDurationMs, gfx::Tween::Type::LINEAR,
        "Ash.Notification.SwipeControl.FadeIn.AnimationSmoothness");
  }
}

void NotificationSwipeControlView::ShowSnoozeButton(bool show) {
  // We don't show the snooze button in the new feature.
  if (features::IsNotificationsRefreshEnabled()) {
    return;
  }

  if (show && !snooze_button_) {
    snooze_button_ = new views::ImageButton(
        base::BindRepeating(&NotificationSwipeControlView::ButtonPressed,
                            base::Unretained(this), ButtonId::kSnooze));
    snooze_button_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            message_center::kNotificationSnoozeButtonIcon, ui::kColorIcon,
            message_center_style::kSwipeControlButtonImageSize));
    snooze_button_->SetImageHorizontalAlignment(
        views::ImageButton::ALIGN_CENTER);
    snooze_button_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    snooze_button_->SetPreferredSize(
        gfx::Size(message_center_style::kSwipeControlButtonSize,
                  message_center_style::kSwipeControlButtonSize));

    snooze_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
    snooze_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
    snooze_button_->SetBackground(
        views::CreateSolidBackground(SK_ColorTRANSPARENT));
    snooze_button_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

    AddChildViewAt(snooze_button_, 0);
    Layout();
  } else if (!show && snooze_button_) {
    DCHECK(Contains(snooze_button_));
    delete snooze_button_;
    snooze_button_ = nullptr;
  }
}

const char* NotificationSwipeControlView::GetClassName() const {
  return kViewClassName;
}

void NotificationSwipeControlView::ButtonPressed(ButtonId button,
                                                 const ui::Event& event) {
  auto weak_this = weak_factory_.GetWeakPtr();

  const std::string notification_id = message_view_->notification_id();
  if (button == ButtonId::kSettings) {
    message_view_->OnSettingsButtonPressed(event);
    metrics_utils::LogSettingsShown(notification_id,
                                    /*is_slide_controls=*/true,
                                    /*is_popup=*/false);
  } else {
    message_view_->OnSnoozeButtonPressed(event);
    metrics_utils::LogSnoozed(notification_id,
                              /*is_slide_controls=*/true,
                              /*is_popup=*/false);
  }

  // Button handlers of |message_view_| may have closed |this|.
  if (!weak_this) {
    return;
  }

  HideButtons();

  // Closing the swipe control is done in these button pressed handlers.
  // Otherwise, handlers might not work.
  message_view_->CloseSwipeControl();
}

}  // namespace ash
