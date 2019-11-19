// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_TEST_BASE_H_
#define CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_TEST_BASE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockSharingService;
class NotificationDisplayServiceTester;

namespace chrome_browser_sharing {
class SharingMessage;
}  // namespace chrome_browser_sharing

namespace message_center {
class Notification;
}  // namespace message_center

class SharedClipboardTestBase : public testing::Test {
 public:
  SharedClipboardTestBase();
  ~SharedClipboardTestBase() override;

  void SetUp() override;

  void TearDown() override;

  chrome_browser_sharing::SharingMessage CreateMessage(std::string guid,
                                                       std::string device_name);

  std::string GetClipboardText();

  message_center::Notification GetNotification();

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;
  std::unique_ptr<MockSharingService> sharing_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedClipboardTestBase);
};

#endif  // CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_TEST_BASE_H_
