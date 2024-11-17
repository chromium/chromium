// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/message_center/metadata_utils.h"

#include "ash/constants/ash_features.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/message_center/public/cpp/message_center_constants.h"

using arc::mojom::ArcNotificationData;

namespace ash {

std::unique_ptr<message_center::Notification>
CreateNotificationFromArcNotificationData(
    const message_center::NotificationType notification_type,
    const std::string& notification_id,
    ArcNotificationData* data,
    const message_center::NotifierId notifier_id,
    message_center::RichNotificationData rich_data,
    scoped_refptr<message_center::NotificationDelegate> delegate) {
  const bool is_parent = data->children_data.has_value();

  const bool render_on_chrome =
      features::IsRenderArcNotificationsByChromeEnabled() &&
      data->render_on_chrome;

  if (!is_parent) {
    rich_data.progress = std::clamp(
        static_cast<int>(std::round(static_cast<float>(data->progress_current) /
                                    data->progress_max * 100)),
        -1, 100);
  }

  // Add buttons to Chrome rendered ARC notifications only, as ARC rendered
  // notifications already have buttons.
  if (render_on_chrome && data->buttons && !is_parent) {
    const auto& buttons = *data->buttons;
    for (size_t i = 0; i < buttons.size(); ++i) {
      const auto& button = buttons[i];
      const auto button_label = button->label;
      message_center::ButtonInfo rich_data_button;
      rich_data_button.title = base::UTF8ToUTF16(button_label);

      if (i == static_cast<size_t>(data->reply_button_index)) {
        rich_data_button.placeholder =
            button->buttonPlaceholder.has_value()
                ? base::UTF8ToUTF16(button->buttonPlaceholder.value())
                : std::u16string();
      }
      rich_data.buttons.emplace_back(rich_data_button);
    }
  }

  if (render_on_chrome && data->texts && !is_parent) {
    const auto& texts = *data->texts;
    const size_t num_texts = texts.size();
    const size_t num_items =
        std::min(num_texts, message_center::kNotificationMaximumItems - 1);

    for (size_t i = 0; i < num_items; i++) {
      rich_data.items.emplace_back(std::u16string(),
                                   base::UTF8ToUTF16(texts[i]));
    }

    if (num_texts > message_center::kNotificationMaximumItems) {
      // Show an ellipsis as the last item if there are more than
      // kNotificationMaximumItems items.
      rich_data.items.emplace_back(std::u16string(), u"\u2026");
    } else if (num_texts == message_center::kNotificationMaximumItems) {
      // Show the kNotificationMaximumItems-th item if there are exactly
      // kNotificationMaximumItems items.
      rich_data.items.emplace_back(std::u16string(),
                                   base::UTF8ToUTF16(texts[num_texts - 1]));
    }
  }

  if (render_on_chrome && data->messages) {
    for (const auto& message : *data->messages) {
      rich_data.items.emplace_back(message_center::NotificationItem(
          base::UTF8ToUTF16(message->sender_name.value_or("")),
          base::UTF8ToUTF16(message->message.value_or("")),
          ui::ImageModel::FromImage(gfx::Image::CreateFrom1xBitmap(
              message->sender_icon.value_or(*data->small_icon)))));
    }
    data->message = "";
  }

  auto notification = std::make_unique<message_center::Notification>(
      notification_type, notification_id, base::UTF8ToUTF16(data->title),
      base::UTF8ToUTF16(data->message), ui::ImageModel(),
      /*display_source=*/
      base::UTF8ToUTF16(data->app_display_name.value_or(std::string())),
      /*origin_url=*/GURL(), notifier_id, rich_data, delegate);

  if (is_parent) {
    notification->SetGroupParent();
  }

  return notification;
}

}  // namespace ash
