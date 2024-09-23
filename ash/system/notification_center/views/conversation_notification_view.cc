// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/conversation_notification_view.h"

#include <memory>

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/typography.h"
#include "ash/system/notification_center/ash_notification_control_button_factory.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/notification_style_utils.h"
#include "ash/system/notification_center/views/ash_notification_expand_button.h"
#include "ash/system/notification_center/views/conversation_item_view.h"
#include "ash/system/notification_center/views/notification_actions_view.h"
#include "ash/system/notification_center/views/timestamp_view.h"
#include "base/functional/bind.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/text_elider.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int kTitleRowMinimumWidthWithIcon = 186;
constexpr int kTitleCharacterLimit =
    (kTitleRowMinimumWidthWithIcon /
         message_center::kMinPixelsPerTitleCharacter -
     12) /
    2;
constexpr int kAppNameCharacterLimit = kTitleCharacterLimit;
constexpr auto kAppIconContainerInteriorMargin =
    gfx::Insets::TLBR(16, 12, 0, 0);
constexpr auto kCollapsedPreviewContainerDefaultMargin =
    gfx::Insets::TLBR(0, 0, 0, 8);
constexpr auto kCollapsedPreviewContainerInteriorMargin =
    gfx::Insets::TLBR(0, 0, 19, 0);
constexpr auto kConversationsContainerInteriorMargin =
    gfx::Insets::TLBR(8, 12, 0, 0);
constexpr auto kExpandButtonContainerInteriorMargin =
    gfx::Insets::TLBR(0, 12, 0, 12);
constexpr auto kTextContainerInteriorMargin = gfx::Insets::TLBR(12, 12, 0, 0);
constexpr auto kTitleRowDefaultMargin = gfx::Insets::TLBR(0, 0, 0, 6);
constexpr auto kActionsViewMargin = gfx::Insets::TLBR(0, 10, 12, 0);
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
  actions_view_ = AddChildView(std::make_unique<NotificationActionsView>());
  actions_view_->SetProperty(views::kMarginsKey, kActionsViewMargin);
  UpdateWithNotification(notification);

  views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);
}

ConversationNotificationView::~ConversationNotificationView() = default;

void ConversationNotificationView::ToggleExpand() {
  expanded_ = !expanded_;
  conversations_container_->SetVisible(expanded_);
  collapsed_preview_container_->SetVisible(!expanded_);
  expand_button_->SetExpanded(expanded_);
  actions_view_->SetExpanded(expanded_);
  // Updates expand state to message center, and let notification delegate
  // handle the state update.
  SetExpanded(expanded_);

  app_name_view_->SetVisible(expanded_);
  app_name_divider_->SetVisible(expanded_);

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

  actions_view_->UpdateWithNotification(notification);
  actions_view_->SetExpanded(expanded_);

  // TODO(b/333740702): Clean up string truncation.
  title_->SetText(gfx::TruncateString(notification.title(),
                                      kTitleCharacterLimit, gfx::WORD_BREAK));

  app_name_view_->SetText(gfx::TruncateString(
      notification.display_source(), kAppNameCharacterLimit, gfx::WORD_BREAK));

  if (notification.items().empty()) {
    return;
  }

  timestamp_->SetTimestamp(notification.timestamp());

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

void ConversationNotificationView::ToggleInlineSettings(
    const ui::Event& event) {
  bool should_show_inline_settings = !inline_settings_view_->GetVisible();
  inline_settings_view_->SetVisible(should_show_inline_settings);
  collapsed_preview_container_->SetVisible(!should_show_inline_settings &&
                                           !expanded_);
  conversations_container_->SetVisible(!should_show_inline_settings &&
                                       expanded_);
  right_controls_container_->SetVisible(!should_show_inline_settings);

  PreferredSizeChanged();
}

std::unique_ptr<views::FlexLayoutView>
ConversationNotificationView::CreateMainContainer(
    const Notification& notification) {
  auto main_container = std::make_unique<views::FlexLayoutView>();
  main_container->SetID(ViewId::kCollapsedModeContainer);
  main_container->SetOrientation(views::LayoutOrientation::kHorizontal);

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

  auto center_content_container = std::make_unique<views::FlexLayoutView>();
  center_content_container->SetOrientation(views::LayoutOrientation::kVertical);
  center_content_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded)));

  center_content_container->AddChildView(CreateTextContainer(notification));
  set_inline_settings_enabled(
      notification.rich_notification_data().settings_button_handler ==
      message_center::SettingsButtonHandler::INLINE);
  if (inline_settings_enabled()) {
    inline_settings_view_ = center_content_container->AddChildView(
        notification_style_utils::CreateInlineSettingsViewForMessageView(this));
    inline_settings_view_->SetVisible(false);
  }

  main_container->AddChildView(std::move(app_icon_container));
  main_container->AddChildView(std::move(center_content_container));
  right_controls_container_ =
      main_container->AddChildView(CreateRightControlsContainer());

  return main_container;
}

std::unique_ptr<views::FlexLayoutView>
ConversationNotificationView::CreateRightControlsContainer() {
  auto right_controls_container = std::make_unique<views::FlexLayoutView>();
  right_controls_container->SetOrientation(views::LayoutOrientation::kVertical);
  right_controls_container->SetCrossAxisAlignment(views::LayoutAlignment::kEnd);

  auto expand_button_container = std::make_unique<views::FlexLayoutView>();
  expand_button_container->SetInteriorMargin(
      kExpandButtonContainerInteriorMargin);
  auto expand_button = std::make_unique<AshNotificationExpandButton>();
  expand_button->SetID(ViewId::kExpandButton);
  // base::Unretained is safe here because the `expand_button` is owned by
  // `this` and will be destroyed before `this`.
  expand_button->SetCallback(base::BindRepeating(
      &ConversationNotificationView::ToggleExpand, base::Unretained(this)));

  expand_button_ =
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
      right_controls_container->AddChildView(std::move(view));
  right_controls_container->AddChildView(std::move(expand_button_container));
  return right_controls_container;
}

std::unique_ptr<views::FlexLayoutView>
ConversationNotificationView::CreateTextContainer(
    const message_center::Notification& notification) {
  auto text_container = std::make_unique<views::FlexLayoutView>();
  text_container->SetOrientation(views::LayoutOrientation::kVertical);
  text_container->SetInteriorMargin(kTextContainerInteriorMargin);
  text_container->SetCrossAxisAlignment(views::LayoutAlignment::kStart);

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
  if (!notification.items().empty()) {
    collapsed_preview_title->SetText(notification.items().rbegin()->title());
  }
  notification_style_utils::ConfigureLabelStyle(
      collapsed_preview_title.get(), kNotificationTitleLabelSize, true,
      gfx::Font::Weight::MEDIUM);

  auto collapsed_preview_message = std::make_unique<views::Label>();
  if (!notification.items().empty()) {
    collapsed_preview_message->SetText(
        notification.items().rbegin()->message());
  }

  collapsed_preview_container->AddChildView(std::move(collapsed_preview_title));
  collapsed_preview_container->AddChildView(
      std::move(collapsed_preview_message));

  text_container->AddChildView(CreateTitleRow(notification));
  collapsed_preview_container_ =
      text_container->AddChildView(std::move(collapsed_preview_container));
  return text_container;
}

std::unique_ptr<views::FlexLayoutView>
ConversationNotificationView::CreateTitleRow(const Notification& notification) {
  auto title_row = std::make_unique<views::FlexLayoutView>();
  title_row->SetOrientation(views::LayoutOrientation::kHorizontal);
  title_row->SetDefault(views::kMarginsKey, kTitleRowDefaultMargin);

  // Create Title view and add it to title row.
  auto title = std::make_unique<views::Label>();
  title->SetID(ViewId::kTitleLabel);
  title_ = title_row->AddChildView(std::move(title));
  // TODO(b/333740702): Clean up string truncation.
  title_->SetText(gfx::TruncateString(notification.title(),
                                      kTitleCharacterLimit, gfx::WORD_BREAK));
  ash::TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                             *title_);

  // Title divider view
  auto divider = std::make_unique<views::Label>();
  divider->SetText(kNotificationTitleRowDivider);
  notification_style_utils::ConfigureLabelStyle(divider.get(),
                                                kNotificationSecondaryLabelSize,
                                                /*is_color_primary=*/false);
  title_row->AddChildView(std::move(divider));

  // App name view
  auto app_name_view = std::make_unique<views::Label>();
  app_name_view->SetID(ViewId::kTitleLabel);
  app_name_view_ = title_row->AddChildView(std::move(app_name_view));
  app_name_view_->SetText(gfx::TruncateString(
      notification.display_source(), kAppNameCharacterLimit, gfx::WORD_BREAK));
  ash::TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosAnnotation1,
                                             *app_name_view_);

  // App name divider view
  auto app_name_divider = std::make_unique<views::Label>();
  app_name_divider->SetText(kNotificationTitleRowDivider);
  notification_style_utils::ConfigureLabelStyle(app_name_divider.get(),
                                                kNotificationSecondaryLabelSize,
                                                /*is_color_primary=*/false);
  app_name_divider_ = title_row->AddChildView(std::move(app_name_divider));

  // Timestamp view
  auto timestamp = std::make_unique<TimestampView>();
  notification_style_utils::ConfigureLabelStyle(timestamp.get(),
                                                kNotificationSecondaryLabelSize,
                                                /*is_color_primary=*/false);
  timestamp_ = title_row->AddChildView(std::move(timestamp));

  return title_row;
}

BEGIN_METADATA(ConversationNotificationView)
END_METADATA

}  // namespace ash
