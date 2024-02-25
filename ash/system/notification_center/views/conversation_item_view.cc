// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/conversation_item_view.h"

#include <memory>

#include "ash/public/cpp/rounded_image_view.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/notification_style_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/text_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/vector_icons.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr auto kConversationItemViewDefaultMargins =
    gfx::Insets::TLBR(0, 0, 12, 14);
constexpr int kIconSize = 24;

}  // namespace

namespace ash {

ConversationItemView::ConversationItemView(
    const message_center::NotificationItem& notification_item) {
  SetOrientation(views::LayoutOrientation::kHorizontal);
  SetDefault(views::kMarginsKey, kConversationItemViewDefaultMargins);
  auto icon_view = std::make_unique<RoundedImageView>();
  icon_view->SetCornerRadius(kIconSize / 2);
  icon_view->SetImage(
      notification_style_utils::CreateNotificationItemIcon(&notification_item));
  AddChildView(std::move(icon_view));

  auto title = std::make_unique<views::Label>();
  title->SetText(notification_item.title());
  title->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  notification_style_utils::ConfigureLabelStyle(
      title.get(), kNotificationTitleLabelSize,
      /*is_color_primary=*/true, gfx::Font::Weight::MEDIUM);

  auto message = std::make_unique<views::Label>();
  message->SetText(notification_item.message());
  message->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  message->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  message->SetTextStyle(views::style::STYLE_SECONDARY);
  message->SetFontList(gfx::FontList({kGoogleSansFont}, gfx::Font::NORMAL,
                                     kNotificationMessageLabelSize,
                                     gfx::Font::Weight::MEDIUM));
  notification_style_utils::ConfigureLabelStyle(message.get(),
                                                kNotificationMessageLabelSize,
                                                /*is_color_primary=*/false);

  auto label_container = std::make_unique<views::FlexLayoutView>();
  label_container->SetOrientation(views::LayoutOrientation::kVertical);
  label_container->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  label_container->AddChildView(std::move(title));
  label_container->AddChildView(std::move(message));
  AddChildView(std::move(label_container));
}

ConversationItemView::~ConversationItemView() = default;

BEGIN_METADATA(ConversationItemView)
END_METADATA

}  // namespace ash
