// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/message_center_test_util.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

std::unique_ptr<message_center::Notification> CreateSimpleNotification(
    const std::string& id,
    bool has_image,
    const GURL& origin_url,
    bool has_inline_reply) {
  message_center::NotifierId notifier_id;
  notifier_id.profile_id = "abc@gmail.com";
  notifier_id.type = message_center::NotifierType::WEB_PAGE;
  notifier_id.url = origin_url;

  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, id, u"test_title",
      u"test message", ui::ImageModel(), std::u16string() /* display_source */,
      origin_url, notifier_id, message_center::RichNotificationData(),
      new message_center::NotificationDelegate());

  if (has_inline_reply) {
    message_center::ButtonInfo button_info(u"test button");
    button_info.placeholder = std::u16string();
    notification->set_buttons({button_info});
  }

  if (has_image) {
    notification->SetImage(gfx::test::CreateImage(320, 300));
  }
  return notification;
}

}  // namespace ash
