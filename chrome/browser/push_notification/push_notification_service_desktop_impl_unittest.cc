// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_notification/push_notification_service_desktop_impl.h"

#include "testing/gtest/include/gtest/gtest.h"

#include <memory>

namespace push_notification {

class PushNotificationServiceDesktopImplTest : public testing::Test {
 public:
  PushNotificationServiceDesktopImplTest() = default;
  ~PushNotificationServiceDesktopImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    push_notification_service_ =
        std::make_unique<PushNotificationServiceDesktopImpl>();
  }

  std::unique_ptr<PushNotificationServiceDesktopImpl>
      push_notification_service_;
};

TEST_F(PushNotificationServiceDesktopImplTest, StartService) {
  EXPECT_TRUE(push_notification_service_);
}

}  // namespace push_notification
