// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/notification_swipe_control_view.h"
#include <memory>

#include "ash/style/icon_button.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/message_center_style.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "ash/system/notification_center/metrics_utils.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

NotificationSwipeControlView::NotificationSwipeControlView(
    message_center::MessageView* message_view)
    : message_view_(message_view) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      kNotificationSwipeControlPadding,
      message_center_style::kSwipeControlButtonHorizontalMargin));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);

  std::unique_ptr<views::ImageButton> settings_button;
  settings_button = std::make_unique<IconButton>(
      base::BindRepeating(&NotificationSwipeControlView::ButtonPressed,
                          base::Unretained(this)),
      IconButton::Type::kMedium, &vector_icons::kSettingsOutlineIcon,
      IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME);

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
                                               bool show_settings) {
  views::BoxLayout* layout = static_cast<views::BoxLayout*>(GetLayoutManager());
  if ((button_position == ButtonPosition::RIGHT) != base::i18n::IsRTL()) {
    layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);
  } else {
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kStart);
  }
  ShowSettingsButton(show_settings);
  DeprecatedLayoutImmediately();
}

void NotificationSwipeControlView::HideButtons() {
  ShowSettingsButton(false);
  DeprecatedLayoutImmediately();
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
              has_settings_button && enough_space_to_show_button);

  message_view_->SetSlideButtonWidth(
      settings_button_ ? settings_button_width + settings_button_->width() : 0);
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

void NotificationSwipeControlView::ButtonPressed(const ui::Event& event) {
  auto weak_this = weak_factory_.GetWeakPtr();

  const std::string notification_id = message_view_->notification_id();
  message_view_->OnSettingsButtonPressed(event);
  metrics_utils::LogSettingsShown(notification_id,
                                  /*is_slide_controls=*/true,
                                  /*is_popup=*/false);

  // Button handlers of |message_view_| may have closed |this|.
  if (!weak_this) {
    return;
  }

  HideButtons();

  // Closing the swipe control is done in these button pressed handlers.
  // Otherwise, handlers might not work.
  message_view_->CloseSwipeControl();
}

BEGIN_METADATA(NotificationSwipeControlView)
END_METADATA

}  // namespace ash
