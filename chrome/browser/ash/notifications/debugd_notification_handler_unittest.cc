// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/debugd_notification_handler.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"

namespace ash {

constexpr char kPacketCaptureNotificationId[] = "debugd-packetcapture";

class DebugdNotificationHandlerTest : public AshTestBase {
 public:
  DebugdNotificationHandlerTest() = default;
  DebugdNotificationHandlerTest(const DebugdNotificationHandlerTest&) = delete;
  DebugdNotificationHandlerTest& operator=(
      const DebugdNotificationHandlerTest&) = delete;

  void SetUp() override {
    AshTestBase::SetUp();
    debug_daemon_client_ = std::make_unique<FakeDebugDaemonClient>();
    handler_ =
        std::make_unique<DebugdNotificationHandler>(debug_daemon_client_.get());
    message_center_ = message_center::MessageCenter::Get();
  }

 protected:
  std::unique_ptr<DebugDaemonClient> debug_daemon_client_;
  std::unique_ptr<DebugdNotificationHandler> handler_;
  raw_ptr<message_center::MessageCenter, DanglingUntriaged> message_center_;
};

TEST_F(DebugdNotificationHandlerTest,
       NotificationAppearsAndDisappearsOnSignals) {
  // Simulate like the start signal is received.
  debug_daemon_client_->PacketCaptureStartSignalReceived(nullptr);
  // Verify the notification appears.
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      kPacketCaptureNotificationId));
  // Simulate like the start signal is received.
  debug_daemon_client_->PacketCaptureStopSignalReceived(nullptr);
  // Verify the notification disappears.
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      kPacketCaptureNotificationId));
}

TEST_F(DebugdNotificationHandlerTest, NotitificationDisappearsOnButtonClicked) {
  // Simulate like the start signal is received and notification is shown.
  debug_daemon_client_->PacketCaptureStartSignalReceived(nullptr);
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      kPacketCaptureNotificationId));
  // Click on notification button and verify the notification gets removed.
  message_center_->ClickOnNotificationButton(kPacketCaptureNotificationId,
                                             /*button_index*/ 0);
  EXPECT_FALSE(message_center_->FindVisibleNotificationById(
      kPacketCaptureNotificationId));
}

TEST_F(DebugdNotificationHandlerTest,
       NotificationStillShownAfterNotificationBodyIsClicked) {
  // Simulate like the start signal is received and notification is shown.
  debug_daemon_client_->PacketCaptureStartSignalReceived(nullptr);
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      kPacketCaptureNotificationId));
  // Click on notification body and verify the notification is still visible.
  message_center_->ClickOnNotification(kPacketCaptureNotificationId);
  EXPECT_TRUE(message_center_->FindVisibleNotificationById(
      kPacketCaptureNotificationId));
}

}  // namespace ash
