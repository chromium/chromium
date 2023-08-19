// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/message_center_test_util.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

namespace {

const gfx::Image CreateTestImage(int width,
                                 int height,
                                 SkColor color = SK_ColorGREEN) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return gfx::Image::CreateFrom1xBitmap(bitmap);
}

}  // namespace

std::unique_ptr<message_center::Notification> CreateSimpleNotification(
    const std::string& id,
    bool has_image,
    const GURL& origin_url) {
  message_center::NotifierId notifier_id;
  notifier_id.profile_id = "abc@gmail.com";
  notifier_id.type = message_center::NotifierType::WEB_PAGE;
  notifier_id.url = origin_url;

  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, id, u"test_title",
      u"test message", ui::ImageModel(), std::u16string() /* display_source */,
      origin_url, notifier_id, message_center::RichNotificationData(),
      new message_center::NotificationDelegate());

  if (has_image) {
    notification->set_image(CreateTestImage(320, 300));
  }
  return notification;
}

}  // namespace ash
