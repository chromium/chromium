// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_notification/push_notification_client_manager_desktop_impl.h"

#include "testing/gtest/include/gtest/gtest.h"

#include <memory>

namespace push_notification {

class PushNotificationClientManagerDesktopImplTest : public testing::Test {
 public:
  PushNotificationClientManagerDesktopImplTest() = default;
  ~PushNotificationClientManagerDesktopImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    push_notification_client_manager_ =
        std::make_unique<PushNotificationClientManagerDesktopImpl>();
  }

  std::unique_ptr<PushNotificationClientManagerDesktopImpl>
      push_notification_client_manager_;
};

TEST_F(PushNotificationClientManagerDesktopImplTest, AddClient) {
  EXPECT_TRUE(push_notification_client_manager_);
  // TODO(b/306398998): Test adding, removing and passing a message to a client
  // when that functionality is implemented.
}

}  // namespace push_notification
