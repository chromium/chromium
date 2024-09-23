// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/notification_actions_view.h"

#include <algorithm>

#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/system_textfield.h"
#include "ash/style/typography.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "ash/system/notification_center/notification_style_utils.h"
#include "base/functional/bind.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {
constexpr auto kInlineReplyContainerDefaultMargins =
    gfx::Insets::TLBR(0, 6, 0, 10);
}

NotificationActionsView::NotificationActionsView() {
  SetUseDefaultFillLayout(true);
  message_center_utils::InitLayerForAnimations(this);

  buttons_container_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  buttons_container_->SetOrientation(views::LayoutOrientation::kHorizontal);
  buttons_container_->SetVisible(true);

  inline_reply_container_ =
      AddChildView(std::make_unique<views::FlexLayoutView>());
  inline_reply_container_->SetOrientation(
      views::LayoutOrientation::kHorizontal);
  inline_reply_container_->SetVisible(false);
  inline_reply_container_->SetDefault(views::kMarginsKey,
                                      kInlineReplyContainerDefaultMargins);
  inline_reply_container_->SetCrossAxisAlignment(
      views::LayoutAlignment::kCenter);

  textfield_ = inline_reply_container_->AddChildView(
      std::make_unique<SystemTextfield>(SystemTextfield::Type::kMedium));
  textfield_->SetBackgroundColorId(cros_tokens::kCrosSysSurface);
  textfield_->SetShowBackground(true);
  textfield_->SetCornerRadius(24);
  textfield_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));
  textfield_->SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
      TypographyToken::kCrosBody2));
  textfield_->SetPlaceholderTextColorId(cros_tokens::kCrosSysOnSurfaceVariant);
  textfield_->set_placeholder_font_list(
      ash::TypographyProvider::Get()->ResolveTypographyToken(
          ash::TypographyToken::kCrosBody2));
  textfield_->SetController(this);

  // base::Unretained is safe here because the `send_button_` is owned by
  // `this`.
  send_button_ =
      inline_reply_container_->AddChildView(std::make_unique<IconButton>(
          base::BindRepeating(&NotificationActionsView::SendButtonPressed,
                              base::Unretained(this)),
          IconButton::Type::kSmallFloating, &vector_icons::kSendIcon,
          /*is_togglable=*/false,
          /*has_border*/ true));
  send_button_->SetEnabled(false);
}

NotificationActionsView::~NotificationActionsView() = default;

void NotificationActionsView::UpdateWithNotification(
    const message_center::Notification& notification) {
  buttons_container_->RemoveAllChildViews();

  int button_index = 0;
  for (auto button : notification.buttons()) {
    // base::Unretained is safe here because `this` is guaranteed to outlive
    // `actions_button` which is owned by `this`. `MessageCenter` is also
    // guaranteed to outlive `this` since it owns all notifications.
    views::Button::PressedCallback callback =
        button.placeholder.has_value()
            ? base::BindRepeating(&NotificationActionsView::ReplyButtonPressed,
                                  base::Unretained(this), notification.id(),
                                  button_index, button.placeholder.value())
            : base::BindRepeating(
                  &message_center::MessageCenter::ClickOnNotificationButton,
                  base::Unretained(message_center::MessageCenter::Get()),
                  notification.id(), button_index);

    std::unique_ptr<PillButton> actions_button =
        std::make_unique<PillButton>(std::move(callback), button.title,
                                     PillButton::Type::kFloatingWithoutIcon,
                                     /*icon=*/nullptr);

    // TODO(b/330374431) Update button color after ash notification clean up.
    button_and_icon_background_color_ =
        notification_style_utils::CalculateIconBackgroundColor(&notification);

    actions_button->SetButtonTextColor(button_and_icon_background_color_);

    buttons_container_->AddChildView(std::move(actions_button));
  }

  SetEnabled(!notification.buttons().empty());
  SetVisible(!notification.buttons().empty());
}

void NotificationActionsView::ReplyButtonPressed(
    const std::string notification_id,
    const int button_index,
    const std::u16string placeholder) {
  inline_reply_container_->SetVisible(true);
  buttons_container_->SetVisible(false);

  views::AnimationBuilder()
      .OnEnded(base::BindOnce(
          [](base::WeakPtr<views::View> parent, views::View* buttons_container,
             views::View* inline_reply_container) {
            if (!parent) {
              return;
            }
            buttons_container->SetVisible(false);
            inline_reply_container->SetVisible(true);
          },
          weak_factory_.GetWeakPtr(), buttons_container_,
          inline_reply_container_))
      .Once()
      .SetDuration(base::Milliseconds(kActionButtonsFadeOutAnimationDurationMs))
      .SetOpacity(this, 0.0);

  message_center_utils::FadeInView(
      this, kActionButtonsFadeOutAnimationDurationMs,
      kInlineReplyFadeInAnimationDurationMs, gfx::Tween::LINEAR,
      "Ash.NotificationView.InlineReply.FadeIn.AnimationSmoothness");

  textfield_->SetPlaceholderText(placeholder);
  textfield_->RequestFocus();
  textfield_->SetShowFocusRing(false);

  // base::Unretained is safe here because `SendReply` can only be triggered by
  // user actions on `this`.
  send_reply_callback_ = base::BindRepeating(
      &NotificationActionsView::SendReply, base::Unretained(this),
      notification_id, button_index);
}

void NotificationActionsView::SendButtonPressed() {
  CHECK(!textfield_->GetText().empty());
  CHECK(!send_reply_callback_.is_null());

  send_reply_callback_.Run();
}

void NotificationActionsView::SetExpanded(bool expanded) {
  if (!GetEnabled()) {
    return;
  }

  if (expanded && !GetVisible()) {
    AnimateExpand();
  }

  if (!expanded && GetVisible()) {
    AnimateCollapse();
  }
}

bool NotificationActionsView::HandleKeyEvent(views::Textfield* sender,
                                             const ui::KeyEvent& event) {
  // Do not try to send a reply if no text has been input.
  if (textfield_->GetText().empty()) {
    return false;
  }

  if (event.type() == ui::EventType::kKeyPressed &&
      event.key_code() == ui::VKEY_RETURN) {
    send_reply_callback_.Run();
    return true;
  }

  return false;
}

void NotificationActionsView::OnAfterUserAction(views::Textfield* sender) {
  bool enabled = !textfield_->GetText().empty();
  send_button_->SetEnabled(enabled);
  send_button_->SetIconColor(enabled ? button_and_icon_background_color_
                                     : cros_tokens::kCrosSysDisabled);
}

void NotificationActionsView::SendReply(const std::string& notification_id,
                                        const int button_index) {
  message_center::MessageCenter::Get()->ClickOnNotificationButtonWithReply(
      notification_id, button_index, textfield_->GetText());
}

void NotificationActionsView::AnimateCollapse() {
  // TODO(b/325555641): Add animation to collapse the view.
  SetVisible(false);
}

void NotificationActionsView::AnimateExpand() {
  SetVisible(true);
  message_center_utils::FadeInView(this, 0,
                                   kInlineReplyFadeInAnimationDurationMs);
}

BEGIN_METADATA(NotificationActionsView)
END_METADATA

}  // namespace ash
