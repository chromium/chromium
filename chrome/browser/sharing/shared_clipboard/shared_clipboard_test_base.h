// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_TEST_BASE_H_
#define CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_TEST_BASE_H_

#include <memory>
#include <string>

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

class MockSharingService;
class NotificationDisplayServiceTester;

namespace components_sharing_message {
class SharingMessage;
}  // namespace components_sharing_message

namespace message_center {
class Notification;
}  // namespace message_center

class SharedClipboardTestBase : public testing::Test {
 public:
  SharedClipboardTestBase();

  SharedClipboardTestBase(const SharedClipboardTestBase&) = delete;
  SharedClipboardTestBase& operator=(const SharedClipboardTestBase&) = delete;

  ~SharedClipboardTestBase() override;

  void SetUp() override;

  void TearDown() override;

  components_sharing_message::SharingMessage CreateMessage(
      const std::string& guid,
      const std::string& device_name);

  std::string GetClipboardText();
  SkBitmap GetClipboardImage();

  message_center::Notification GetNotification();

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;
  std::unique_ptr<MockSharingService> sharing_service_;
};

#endif  // CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_TEST_BASE_H_
