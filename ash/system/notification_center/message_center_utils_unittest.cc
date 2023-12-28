// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/message_center_utils.h"

#include "ash/constants/ash_constants.h"
#include "ash/public/cpp/vm_camera_mic_constants.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/message_center.h"

namespace ash::message_center_utils {

namespace {

void AddNotification(const std::string& notification_id,
                     const std::string& notifier_id) {
  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
          u"test_title", u"test message", ui::ImageModel(),
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                     notifier_id),
          message_center::RichNotificationData(),
          new message_center::NotificationDelegate()));
}

gfx::ImageSkia CreateImageForSize(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

}  // namespace

using MessageCenterUtilsTest = AshTestBase;

TEST_F(MessageCenterUtilsTest, TotalNotificationCount) {
  EXPECT_EQ(0u, GetNotificationCount());

  // VM camera/mic notifications are ignored by the counter.
  AddNotification("0", kVmCameraMicNotifierId);
  EXPECT_EQ(0u, GetNotificationCount());

  // Privacy indicator notifications are ignored by the counter.
  AddNotification("1", kPrivacyIndicatorsNotifierId);
  EXPECT_EQ(0u, GetNotificationCount());
}

// Verifies that resizing the image that exceeds the binary size limit works
// as expected.
TEST_F(MessageCenterUtilsTest, ResizeImageWhenLarge) {
  // The binary size limit is 1MB. Each pixel of the input image takes 4 bytes.
  // Therefore, the maximum pixel count is 250K.
  constexpr size_t kSizeLimit = 1000000;

  // The shrink scale = sqrt(3000 * 3000 / 250K) = 6.
  std::optional<gfx::ImageSkia> resized_image =
      ResizeImageIfExceedSizeLimit(CreateImageForSize(3000, 3000), kSizeLimit);
  EXPECT_EQ(resized_image->size(), gfx::Size(500, 500));
  EXPECT_EQ(resized_image->bitmap()->computeByteSize(), kSizeLimit);

  // The shrink scale = sqrt(1000 * 4000 / 250K) = 4.
  resized_image =
      ResizeImageIfExceedSizeLimit(CreateImageForSize(1000, 4000), kSizeLimit);
  EXPECT_EQ(resized_image->size(), gfx::Size(250, 1000));
  EXPECT_EQ(resized_image->bitmap()->computeByteSize(), kSizeLimit);

  // An image without exceeding the binary size limit does not need resizing.
  gfx::ImageSkia input_image(CreateImageForSize(200, 200));
  resized_image = ResizeImageIfExceedSizeLimit(input_image, kSizeLimit);
  EXPECT_FALSE(resized_image);
  EXPECT_LE(input_image.bitmap()->computeByteSize(), kSizeLimit);

  // The binary size limit is 400 bytes. Each pixel of the input image takes 4
  // bytes. Therefore, the maximum pixel count is 100.
  constexpr size_t kSizeLimit2 = 400;

  // An image without exceeding the binary size limit does not need resizing.
  input_image = CreateImageForSize(10, 10);
  resized_image = ResizeImageIfExceedSizeLimit(input_image, kSizeLimit2);
  EXPECT_FALSE(resized_image);
  EXPECT_LE(input_image.bitmap()->computeByteSize(), kSizeLimit2);

  // The shrink scale = sqrt(20 * 45 / 100) = 3.
  resized_image =
      ResizeImageIfExceedSizeLimit(CreateImageForSize(20, 45), kSizeLimit2);
  EXPECT_EQ(resized_image->size(), gfx::Size(6, 15));

  // Width is not a multiple of the shrink scale. Therefore, the resized image's
  // binary size is smaller than the size limit.
  EXPECT_LT(resized_image->bitmap()->computeByteSize(), kSizeLimit2);
}

}  // namespace ash::message_center_utils
