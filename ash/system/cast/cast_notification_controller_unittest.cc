// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/cast_notification_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_cast_config_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"

namespace ash {
namespace {

SinkAndRoute CreateDeviceNoRoute() {
  SinkAndRoute device;
  device.sink.id = "fake_sink_id_noroute";
  device.sink.name = "Sink Name NoRoute";
  device.sink.sink_icon_type = SinkIconType::kCast;

  return device;
}

SinkAndRoute CreateDeviceNonlocalRoute() {
  SinkAndRoute device;
  device.sink.id = "fake_sink_id_nonlocalroute";
  device.sink.name = "Sink Name NonlocalRoute";
  device.sink.sink_icon_type = SinkIconType::kCast;
  device.route.id = "fake_route_id_nonlocalroute";
  device.route.title = "Casting tab";
  device.route.is_local_source = false;
  device.route.content_source = ContentSource::kTab;

  return device;
}

SinkAndRoute CreateDeviceLocalRoute() {
  SinkAndRoute device;
  device.sink.id = "fake_sink_id_localroute";
  device.sink.name = "Sink Name localRoute";
  device.sink.sink_icon_type = SinkIconType::kCast;
  device.route.id = "fake_route_id_localroute";
  device.route.title = "Casting tab";
  device.route.is_local_source = true;
  device.route.content_source = ContentSource::kTab;

  return device;
}

SinkAndRoute CreateDeviceLocalRouteDesktop() {
  SinkAndRoute device;
  device.sink.id = "fake_sink_id_localroutedesktop";
  device.sink.name = "Sink Name localRouteDesktop";
  device.sink.sink_icon_type = SinkIconType::kCast;
  device.route.id = "fake_route_id_localroutedesktop";
  device.route.title = "Casting screen";
  device.route.is_local_source = true;
  device.route.content_source = ContentSource::kDesktop;

  return device;
}

}  // namespace

class CastNotificationControllerTest : public AshTestBase {
 public:
  CastNotificationControllerTest() = default;

  CastNotificationControllerTest(const CastNotificationControllerTest&) =
      delete;
  CastNotificationControllerTest& operator=(
      const CastNotificationControllerTest&) = delete;

  ~CastNotificationControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    notification_controller_ = std::make_unique<CastNotificationController>();
    cast_config_.set_has_sinks_and_routes(true);
    cast_config_.set_has_active_route(true);
  }

  message_center::Notification* GetNotification() {
    return message_center::MessageCenter::Get()->FindNotificationById(
        "chrome://cast");
  }

  void ClickOnNotificationButton(int index = 0) {
    message_center::MessageCenter::Get()->ClickOnNotificationButton(
        "chrome://cast", index);
  }

  void ClickOnNotificationBody() {
    message_center::MessageCenter::Get()->ClickOnNotification("chrome://cast");
  }

  std::unique_ptr<CastNotificationController> notification_controller_;
  TestCastConfigController cast_config_;
};

TEST_F(CastNotificationControllerTest, Notification) {
  cast_config_.set_has_sinks_and_routes(false);
  cast_config_.set_has_active_route(false);

  // There should be no cast notification to start with.
  EXPECT_FALSE(GetNotification());

  // A device with no route should not create a notification.
  cast_config_.set_has_sinks_and_routes(true);
  notification_controller_->OnDevicesUpdated({CreateDeviceNoRoute()});
  EXPECT_FALSE(GetNotification());

  // A device with a non-local route should not create a notification.
  cast_config_.set_has_active_route(true);
  notification_controller_->OnDevicesUpdated({CreateDeviceNonlocalRoute()});
  EXPECT_FALSE(GetNotification());

  // A device with a local route should create a pinned notification.
  notification_controller_->OnDevicesUpdated({CreateDeviceLocalRoute()});
  EXPECT_TRUE(GetNotification()->pinned());

  cast_config_.set_has_sinks_and_routes(false);
  cast_config_.set_has_active_route(false);
  notification_controller_->OnDevicesUpdated({});
  EXPECT_FALSE(GetNotification());
}

TEST_F(CastNotificationControllerTest, StopCasting) {
  // Create notification.
  SinkAndRoute device = CreateDeviceLocalRoute();
  notification_controller_->OnDevicesUpdated({device});
  EXPECT_TRUE(GetNotification()->pinned());

  ClickOnNotificationBody();
  EXPECT_EQ(cast_config_.stop_casting_count(), 1u);
  EXPECT_EQ(cast_config_.stop_casting_route_id(), device.route.id);

  cast_config_.ResetRouteIds();

  ClickOnNotificationButton(0);
  EXPECT_EQ(cast_config_.stop_casting_count(), 2u);
  EXPECT_EQ(cast_config_.stop_casting_route_id(), device.route.id);
}

TEST_F(CastNotificationControllerTest, FreezeUi) {
  // Create notification.
  SinkAndRoute device = CreateDeviceLocalRoute();
  // Make the device "freezable" so the freeze (pause) button appears.
  device.route.freeze_info.can_freeze = true;
  notification_controller_->OnDevicesUpdated({device});
  EXPECT_TRUE(GetNotification()->pinned());

  // There should be two notification buttons.
  EXPECT_EQ(GetNotification()->buttons().size(), 2u);

  // The first button should be the freeze button.
  ClickOnNotificationButton(0);
  EXPECT_EQ(cast_config_.freeze_route_count(), 1u);
  EXPECT_EQ(cast_config_.stop_casting_count(), 0u);
  EXPECT_EQ(cast_config_.freeze_route_route_id(), device.route.id);

  // The second button should be the stop button.
  ClickOnNotificationButton(1);
  EXPECT_EQ(cast_config_.freeze_route_count(), 1u);
  EXPECT_EQ(cast_config_.stop_casting_count(), 1u);
  EXPECT_EQ(cast_config_.stop_casting_route_id(), device.route.id);

  cast_config_.ResetRouteIds();

  // Clicking on the notification body should still stop casting.
  ClickOnNotificationBody();
  EXPECT_EQ(cast_config_.freeze_route_count(), 1u);
  EXPECT_EQ(cast_config_.stop_casting_count(), 2u);
  EXPECT_EQ(cast_config_.stop_casting_route_id(), device.route.id);

  // Set the device to a frozen state, then regenerate the notification.
  device.route.freeze_info.is_frozen = true;
  notification_controller_->OnDevicesUpdated({device});

  // The first button should now call unfreeze.
  ClickOnNotificationButton(0);
  EXPECT_EQ(cast_config_.unfreeze_route_count(), 1u);
  EXPECT_EQ(cast_config_.stop_casting_count(), 2u);
  EXPECT_EQ(cast_config_.unfreeze_route_route_id(), device.route.id);
}

TEST_F(CastNotificationControllerTest, FreezeWithTrayOpen) {
  // Create notification.
  SinkAndRoute device = CreateDeviceLocalRoute();
  // Make the device "freezable" so the freeze (pause) button appears.
  device.route.freeze_info.can_freeze = true;
  notification_controller_->OnDevicesUpdated({device});
  EXPECT_TRUE(GetNotification()->pinned());

  // Open the unified system tray.
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());

  // Pressing freeze (first button) should close the tray.
  ClickOnNotificationButton(0);
  EXPECT_FALSE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());

  // Allow the Widget to close and notify the CastNotificationController.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(cast_config_.freeze_route_count(), 1u);
}

TEST_F(CastNotificationControllerTest,
       NotificationMessage_CastingTab_CannotPause) {
  // Create a tab casting route that cannot be paused.
  SinkAndRoute device = CreateDeviceLocalRoute();
  notification_controller_->OnDevicesUpdated({device});
  EXPECT_EQ(GetNotification()->message(),
            base::UTF8ToUTF16(device.route.title));
}
TEST_F(CastNotificationControllerTest,
       NotificationMessage_CastingScreen_CannotPause) {
  // Create a screen casting route that cannot be paused.
  SinkAndRoute device = CreateDeviceLocalRouteDesktop();
  notification_controller_->OnDevicesUpdated({device});
  std::u16string desktop_casting_message = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_CAST_CAST_DESKTOP_NOTIFICATION_MESSAGE);
  EXPECT_EQ(GetNotification()->message(), desktop_casting_message);
}
TEST_F(CastNotificationControllerTest,
       NotificationMessage_CastingTab_CanPause) {
  // Create a tab casting route that can be paused.
  SinkAndRoute device = CreateDeviceLocalRoute();
  device.route.freeze_info.can_freeze = true;
  notification_controller_->OnDevicesUpdated({device});
  std::u16string tab_casting_can_pause_message = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_CAST_NOTIFICATION_MESSAGE_TAB_CAN_PAUSE);
  EXPECT_EQ(GetNotification()->message(), tab_casting_can_pause_message);
}
TEST_F(CastNotificationControllerTest,
       NotificationMessage_CastingScreen_CanPause) {
  // Create a screen casting route that can be paused.
  SinkAndRoute device = CreateDeviceLocalRouteDesktop();
  device.route.freeze_info.can_freeze = true;
  notification_controller_->OnDevicesUpdated({device});
  std::u16string screen_casting_can_pause_message = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_CAST_NOTIFICATION_MESSAGE_SCREEN_CAN_PAUSE);
  EXPECT_EQ(GetNotification()->message(), screen_casting_can_pause_message);
}
TEST_F(CastNotificationControllerTest, NotificationMessage_CastingTab_Paused) {
  // Create a tab casting route that is paused,
  SinkAndRoute device = CreateDeviceLocalRoute();
  device.route.freeze_info.can_freeze = true;
  device.route.freeze_info.is_frozen = true;
  notification_controller_->OnDevicesUpdated({device});
  std::u16string tab_casting_paused_message = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_CAST_NOTIFICATION_MESSAGE_PAUSED);
  EXPECT_EQ(GetNotification()->message(), tab_casting_paused_message);
}

TEST_F(CastNotificationControllerTest,
       NotificationMessage_CastingScreen_Paused) {
  // Create a screen casting route that is paused,
  SinkAndRoute device = CreateDeviceLocalRouteDesktop();
  device.route.freeze_info.can_freeze = true;
  device.route.freeze_info.is_frozen = true;
  notification_controller_->OnDevicesUpdated({device});
  std::u16string screen_casting_paused_message = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_CAST_NOTIFICATION_MESSAGE_SCREEN_PAUSED);
  EXPECT_EQ(GetNotification()->message(), screen_casting_paused_message);
}

// Regression test for b/280864232
TEST_F(CastNotificationControllerTest, NewRouteStop) {
  // Create notification.
  SinkAndRoute device = CreateDeviceLocalRoute();
  // Make the device "freezable" so the freeze (pause) button appears.
  device.route.freeze_info.can_freeze = true;
  notification_controller_->OnDevicesUpdated({device});

  // There should be a notfiication with 2 buttons.
  EXPECT_TRUE(GetNotification()->pinned());
  EXPECT_EQ(GetNotification()->buttons().size(), 2u);

  // Update the list of devices so that now there is a non-freezable route.
  device.route.freeze_info.can_freeze = false;
  notification_controller_->OnDevicesUpdated({device});

  // There should be a notification with 1 button.
  EXPECT_TRUE(GetNotification()->pinned());
  EXPECT_EQ(GetNotification()->buttons().size(), 1u);

  // Clicking the button should stop the route, and not call freeze / unfreeze.
  ClickOnNotificationButton(0);
  EXPECT_EQ(cast_config_.freeze_route_count(), 0u);
  EXPECT_EQ(cast_config_.unfreeze_route_count(), 0u);
  EXPECT_EQ(cast_config_.stop_casting_count(), 1u);
  EXPECT_EQ(cast_config_.stop_casting_route_id(), device.route.id);
}

}  // namespace ash
