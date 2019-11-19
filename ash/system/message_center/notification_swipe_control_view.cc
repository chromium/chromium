// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/notification_swipe_control_view.h"

#include "ash/system/message_center/message_center_style.h"
#include "ui/base/l10n/l10n_util.h"
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
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(message_center_style::kSwipeControlButtonVerticalMargin,
                  message_center_style::kSwipeControlButtonHorizontalMargin),
      message_center_style::kSwipeControlButtonHorizontalMargin));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);

  // Draw on its own layer to round corners
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
      gesture_amount < 0 ? NotificationSwipeControlView::ButtonPosition::RIGHT
                         : NotificationSwipeControlView::ButtonPosition::LEFT;
  message_center::NotificationControlButtonsView* buttons =
      message_view_->GetControlButtonsView();
  // Ignore when GetControlButtonsView() returns null.
  if (!buttons)
    return;
  bool has_settings_button = buttons->settings_button();
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
  SetBackground(views::CreateBackgroundFromPainter(
      std::make_unique<message_center::NotificationBackgroundPainter>(
          top_radius, bottom_radius,
          message_center_style::kSwipeControlBackgroundColor)));
  SchedulePaint();
}

void NotificationSwipeControlView::ShowSettingsButton(bool show) {
  if (show && !settings_button_) {
    settings_button_ = new views::ImageButton(this);
    settings_button_->SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(
            message_center::kNotificationSettingsButtonIcon,
            message_center_style::kSwipeControlButtonImageSize,
            gfx::kChromeIconGrey));
    settings_button_->SetImageHorizontalAlignment(
        views::ImageButton::ALIGN_CENTER);
    settings_button_->SetImageVerticalAlignment(
        views::ImageButton::ALIGN_MIDDLE);
    settings_button_->SetPreferredSize(
        gfx::Size(message_center_style::kSwipeControlButtonSize,
                  message_center_style::kSwipeControlButtonSize));

    settings_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
    settings_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
    settings_button_->SetBackground(
        views::CreateSolidBackground(SK_ColorTRANSPARENT));

    AddChildView(settings_button_);
    Layout();
  } else if (!show && settings_button_) {
    DCHECK(Contains(settings_button_));
    delete settings_button_;
    settings_button_ = nullptr;
  }
}

void NotificationSwipeControlView::ShowSnoozeButton(bool show) {
  if (show && !snooze_button_) {
    snooze_button_ = new views::ImageButton(this);
    snooze_button_->SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(
            message_center::kNotificationSnoozeButtonIcon,
            message_center_style::kSwipeControlButtonImageSize,
            gfx::kChromeIconGrey));
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

void NotificationSwipeControlView::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  DCHECK(sender);
  if (sender == settings_button_)
    message_view_->OnSettingsButtonPressed(event);
  else if (sender == snooze_button_)
    message_view_->OnSnoozeButtonPressed(event);
  HideButtons();

  // Closing the swipe control is done in these button pressed handlers.
  // Otherwise, handlers might not work.
  message_view_->CloseSwipeControl();
}

}  // namespace ash
