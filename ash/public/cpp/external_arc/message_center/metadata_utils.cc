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
    const std::string notification_id,
    ArcNotificationData* data,
    const message_center::NotifierId notifier_id,
    message_center::RichNotificationData rich_data,
    scoped_refptr<message_center::NotificationDelegate> delegate) {
  const bool render_on_chrome =
      features::IsRenderArcNotificationsByChromeEnabled() &&
      data->render_on_chrome;

  if (data->texts && render_on_chrome) {
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

  auto notification = std::make_unique<message_center::Notification>(
      notification_type, notification_id, base::UTF8ToUTF16(data->title),
      base::UTF8ToUTF16(data->message), ui::ImageModel(), u"arc", GURL(),
      notifier_id, rich_data, delegate);

  return notification;
}

}  // namespace ash
