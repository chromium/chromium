// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_test_base.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "components/sharing_message/mock_sharing_service.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sync_device_info/device_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/test/clipboard_test_util.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/message_center/public/cpp/notification.h"

SharedClipboardTestBase::SharedClipboardTestBase()
    : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

SharedClipboardTestBase::~SharedClipboardTestBase() = default;

void SharedClipboardTestBase::SetUp() {
  notification_tester_ =
      std::make_unique<NotificationDisplayServiceTester>(&profile_);
  sharing_service_ = std::make_unique<MockSharingService>();
  ui::TestClipboard::CreateForCurrentThread();
}

void SharedClipboardTestBase::TearDown() {
  ui::Clipboard::DestroyClipboardForCurrentThread();
}

components_sharing_message::SharingMessage
SharedClipboardTestBase::CreateMessage(const std::string& guid,
                                       const std::string& device_name) {
  components_sharing_message::SharingMessage message;
  message.set_sender_guid(guid);
  message.set_sender_device_name(device_name);
  return message;
}

std::string SharedClipboardTestBase::GetClipboardText() {
  std::u16string text;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr, &text);
  return base::UTF16ToUTF8(text);
}

SkBitmap SharedClipboardTestBase::GetClipboardImage() {
  SkBitmap bitmap;
  std::vector<uint8_t> png_data =
      ui::clipboard_test_util::ReadPng(ui::Clipboard::GetForCurrentThread());
  gfx::PNGCodec::Decode(png_data.data(), png_data.size(), &bitmap);
  return bitmap;
}

message_center::Notification SharedClipboardTestBase::GetNotification() {
  auto notifications = notification_tester_->GetDisplayedNotificationsForType(
      NotificationHandler::Type::SHARING);
  EXPECT_EQ(notifications.size(), 1u);

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());

  return notification;
}
