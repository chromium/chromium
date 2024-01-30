// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/conversation_notification_view.h"

#include <memory>

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/notification_center/ash_notification_control_button_factory.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/notification_style_utils.h"
#include "ash/system/notification_center/views/ash_notification_expand_button.h"
#include "ash/system/notification_center/views/conversation_item_view.h"
#include "base/functional/bind.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr auto kAppIconContainerInteriorMargin =
    gfx::Insets::TLBR(16, 12, 0, 12);
constexpr auto kCollapsedPreviewContainerDefaultMargin =
    gfx::Insets::TLBR(0, 0, 0, 8);
constexpr auto kCollapsedPreviewContainerInteriorMargin =
    gfx::Insets::TLBR(0, 0, 19, 0);
constexpr auto kConversationsContainerInteriorMargin =
    gfx::Insets::TLBR(8, 12, 0, 0);
constexpr auto kExpandButtonContainerInteriorMargin =
    gfx::Insets::TLBR(0, 12, 0, 12);
constexpr auto kMainTextContainerInteriorMargin =
    gfx::Insets::TLBR(12, 0, 0, 0);

}  // namespace

namespace ash {

ConversationNotificationView::ConversationNotificationView(
    const message_center::Notification& notification)
    : MessageView(notification) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kNotificationViewPadding));

  auto conversations_container = std::make_unique<views::FlexLayoutView>();
  conversations_container->SetID(ViewId::kConversationContainer);
  conversations_container->SetOrientation(views::LayoutOrientation::kVertical);
  conversations_container->SetInteriorMargin(
      kConversationsContainerInteriorMargin);

  AddChildView(CreateMainContainer(notification));
  conversations_container_ = AddChildView(std::move(conversations_container));

  UpdateWithNotification(notification);

  views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);
}

ConversationNotificationView::~ConversationNotificationView() = default;

void ConversationNotificationView::ToggleExpand() {
  expanded_ = !expanded_;
  conversations_container_->SetVisible(expanded_);
  collapsed_preview_container_->SetVisible(!expanded_);
  PreferredSizeChanged();
}

bool ConversationNotificationView::IsExpanded() const {
  return expanded_;
}

void ConversationNotificationView::OnThemeChanged() {
  views::View::OnThemeChanged();
  if (control_buttons_view_) {
    control_buttons_view_->SetButtonIconColors(
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorPrimary));
  }
}

void ConversationNotificationView::UpdateWithNotification(
    const Notification& notification) {
  UpdateControlButtonsVisibilityWithNotification(notification);

  if (notification.items().empty()) {
    return;
  }

  auto* conversation_container = GetViewByID(ViewId::kConversationContainer);
  // TODO(b/284512022): We should update the container with new
  // conversations instead of repopulating for every update.
  conversation_container->RemoveAllChildViews();
  for (auto& item : notification.items()) {
    conversation_container->AddChildView(
        std::make_unique<ConversationItemView>(item));
  }
}

message_center::NotificationControlButtonsView*
ConversationNotificationView::GetControlButtonsView() const {
  return control_buttons_view_;
}

std::unique_ptr<views::FlexLayoutView>
ConversationNotificationView::CreateMainContainer(
    const Notification& notification) {
  auto container = std::make_unique<views::FlexLayoutView>();
  container->SetID(ViewId::kCollapsedModeContainer);
  container->SetOrientation(views::LayoutOrientation::kHorizontal);

  auto app_icon_container = std::make_unique<views::FlexLayoutView>();
  app_icon_container->SetInteriorMargin(kAppIconContainerInteriorMargin);
  app_icon_container->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  app_icon_container->SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  auto app_icon_view = std::make_unique<RoundedImageView>();
  app_icon_view->SetID(ViewId::kMainIcon);
  app_icon_view->SetImage(
      notification_style_utils::CreateNotificationAppIcon(&notification));
  app_icon_view->SetCornerRadius(kNotificationAppIconImageSize / 2);

  app_icon_container->AddChildView(std::move(app_icon_view));

  auto main_text_container = std::make_unique<views::FlexLayoutView>();
  main_text_container->SetOrientation(views::LayoutOrientation::kVertical);
  main_text_container->SetInteriorMargin(kMainTextContainerInteriorMargin);
  main_text_container->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  main_text_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kUnbounded)));

  auto title = std::make_unique<views::Label>();
  title->SetID(ViewId::kTitleLabel);
  title->SetText(notification.title());

  notification_style_utils::ConfigureLabelStyle(
      title.get(), kNotificationTitleLabelSize, true,
      gfx::Font::Weight::MEDIUM);

  auto collapsed_preview_container = std::make_unique<views::FlexLayoutView>();
  collapsed_preview_container->SetID(ViewId::kCollapsedPreviewContainer);
  collapsed_preview_container->SetInteriorMargin(
      kCollapsedPreviewContainerInteriorMargin);
  collapsed_preview_container->SetOrientation(
      views::LayoutOrientation::kHorizontal);
  collapsed_preview_container->SetMainAxisAlignment(
      views::LayoutAlignment::kEnd);
  collapsed_preview_container->SetDefault(
      views::kMarginsKey, kCollapsedPreviewContainerDefaultMargin);
  collapsed_preview_container->SetVisible(!expanded_);

  auto collapsed_preview_title = std::make_unique<views::Label>();
  collapsed_preview_title->SetText(notification.items().rbegin()->title());
  notification_style_utils::ConfigureLabelStyle(
      collapsed_preview_title.get(), kNotificationTitleLabelSize, true,
      gfx::Font::Weight::MEDIUM);

  auto collapsed_preview_message = std::make_unique<views::Label>();
  collapsed_preview_message->SetText(notification.items().rbegin()->message());

  collapsed_preview_container->AddChildView(std::move(collapsed_preview_title));
  collapsed_preview_container->AddChildView(
      std::move(collapsed_preview_message));

  main_text_container->AddChildView(std::move(title));
  collapsed_preview_container_ =
      main_text_container->AddChildView(std::move(collapsed_preview_container));

  container->AddChildView(std::move(app_icon_container));
  container->AddChildView(std::move(main_text_container));
  container->AddChildView(CreateNotificationControlButtonsView());

  return container;
}

std::unique_ptr<views::FlexLayoutView>
ConversationNotificationView::CreateNotificationControlButtonsView() {
  auto control_buttons_container = std::make_unique<views::FlexLayoutView>();
  control_buttons_container->SetOrientation(
      views::LayoutOrientation::kVertical);
  control_buttons_container->SetCrossAxisAlignment(
      views::LayoutAlignment::kEnd);

  auto expand_button_container = std::make_unique<views::FlexLayoutView>();
  expand_button_container->SetInteriorMargin(
      kExpandButtonContainerInteriorMargin);
  auto expand_button = std::make_unique<AshNotificationExpandButton>();
  expand_button->SetID(ViewId::kExpandButton);
  // base::Unretained is safe here because the `expand_button` is owned by
  // `this` and will be destroyed before `this`.
  expand_button->SetCallback(base::BindRepeating(
      &ConversationNotificationView::ToggleExpand, base::Unretained(this)));

  expand_button_container->AddChildView(std::move(expand_button));

  auto view =
      std::make_unique<message_center::NotificationControlButtonsView>(this);
  view->SetID(ViewId::kControlButtonsView);
  view->SetBetweenButtonSpacing(kNotificationControlButtonsHorizontalSpacing);
  view->SetCloseButtonIcon(vector_icons::kCloseChromeRefreshIcon);
  view->SetSettingsButtonIcon(vector_icons::kSettingsOutlineIcon);
  view->SetButtonIconColors(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary));
  view->SetNotificationControlButtonFactory(
      std::make_unique<AshNotificationControlButtonFactory>());

  control_buttons_view_ =
      control_buttons_container->AddChildView(std::move(view));
  control_buttons_container->AddChildView(std::move(expand_button_container));

  return control_buttons_container;
}

BEGIN_METADATA(ConversationNotificationView, MessageView)
END_METADATA

}  // namespace ash
