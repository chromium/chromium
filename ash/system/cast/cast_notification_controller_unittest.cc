// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/cast_notification_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/cast_config_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"

namespace ash {
namespace {

class TestCastConfigController : public CastConfigController {
 public:
  TestCastConfigController() = default;
  TestCastConfigController(const TestCastConfigController&) = delete;
  TestCastConfigController& operator=(const TestCastConfigController&) = delete;
  ~TestCastConfigController() override = default;

  // CastConfigController:
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  bool HasMediaRouterForPrimaryProfile() const override { return true; }
  bool HasSinksAndRoutes() const override { return has_sinks_and_routes_; }
  bool HasActiveRoute() const override { return has_active_routes_; }
  bool AccessCodeCastingEnabled() const override {
    return access_code_casting_enabled_;
  }
  void RequestDeviceRefresh() override {}
  const std::vector<SinkAndRoute>& GetSinksAndRoutes() override {
    return sinks_and_routes_;
  }
  void CastToSink(const std::string& sink_id) override {}
  void StopCasting(const std::string& route_id) override {
    ++stop_casting_count_;
    stop_casting_route_id_ = route_id;
  }
  void FreezeRoute(const std::string& route_id) override {
    ++freeze_route_count_;
    freeze_route_route_id_ = route_id;
  }
  void UnfreezeRoute(const std::string& route_id) override {
    ++unfreeze_route_count_;
    unfreeze_route_route_id_ = route_id;
  }

  bool has_sinks_and_routes_ = false;
  bool has_active_routes_ = false;
  bool access_code_casting_enabled_ = false;
  std::vector<SinkAndRoute> sinks_and_routes_;
  size_t stop_casting_count_ = 0;
  std::string stop_casting_route_id_;
  size_t freeze_route_count_ = 0;
  std::string freeze_route_route_id_;
  size_t unfreeze_route_count_ = 0;
  std::string unfreeze_route_route_id_;
};

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
  // There should be no cast notification to start with.
  EXPECT_FALSE(GetNotification());

  // A device with no route should not create a notification.
  cast_config_.has_sinks_and_routes_ = true;
  notification_controller_->OnDevicesUpdated({CreateDeviceNoRoute()});
  EXPECT_FALSE(GetNotification());

  // A device with a non-local route should not create a notification.
  cast_config_.has_active_routes_ = true;
  notification_controller_->OnDevicesUpdated({CreateDeviceNonlocalRoute()});
  EXPECT_FALSE(GetNotification());

  // A device with a local route should create a pinned notification.
  notification_controller_->OnDevicesUpdated({CreateDeviceLocalRoute()});
  EXPECT_TRUE(GetNotification()->pinned());

  cast_config_.has_sinks_and_routes_ = false;
  cast_config_.has_active_routes_ = false;
  notification_controller_->OnDevicesUpdated({});
  EXPECT_FALSE(GetNotification());
}

TEST_F(CastNotificationControllerTest, StopCasting) {
  // Create notification.
  cast_config_.has_sinks_and_routes_ = true;
  cast_config_.has_active_routes_ = true;
  SinkAndRoute device = CreateDeviceLocalRoute();
  notification_controller_->OnDevicesUpdated({device});
  EXPECT_TRUE(GetNotification()->pinned());

  ClickOnNotificationBody();
  EXPECT_EQ(cast_config_.stop_casting_count_, 1u);
  EXPECT_EQ(cast_config_.stop_casting_route_id_, device.route.id);

  cast_config_.stop_casting_route_id_ = "";

  ClickOnNotificationButton(0);
  EXPECT_EQ(cast_config_.stop_casting_count_, 2u);
  EXPECT_EQ(cast_config_.stop_casting_route_id_, device.route.id);
}

TEST_F(CastNotificationControllerTest, FreezeUi) {
  // Create notification.
  cast_config_.has_sinks_and_routes_ = true;
  cast_config_.has_active_routes_ = true;
  SinkAndRoute device = CreateDeviceLocalRoute();
  // Make the device "freezable" so the freeze (pause) button appears.
  device.route.freeze_info.can_freeze = true;
  notification_controller_->OnDevicesUpdated({device});
  EXPECT_TRUE(GetNotification()->pinned());

  // There should be two notification buttons.
  EXPECT_EQ(GetNotification()->buttons().size(), 2u);

  // The first button should be the freeze button.
  ClickOnNotificationButton(0);
  EXPECT_EQ(cast_config_.freeze_route_count_, 1u);
  EXPECT_EQ(cast_config_.stop_casting_count_, 0u);
  EXPECT_EQ(cast_config_.freeze_route_route_id_, device.route.id);

  // The second button should be the stop button.
  ClickOnNotificationButton(1);
  EXPECT_EQ(cast_config_.freeze_route_count_, 1u);
  EXPECT_EQ(cast_config_.stop_casting_count_, 1u);
  EXPECT_EQ(cast_config_.stop_casting_route_id_, device.route.id);

  cast_config_.stop_casting_route_id_ = "";

  // Clicking on the notification body should still stop casting.
  ClickOnNotificationBody();
  EXPECT_EQ(cast_config_.freeze_route_count_, 1u);
  EXPECT_EQ(cast_config_.stop_casting_count_, 2u);
  EXPECT_EQ(cast_config_.stop_casting_route_id_, device.route.id);

  // Set the device to a frozen state, then regenerate the notification.
  device.route.freeze_info.is_frozen = true;
  notification_controller_->OnDevicesUpdated({device});

  // The first button should now call unfreeze.
  ClickOnNotificationButton(0);
  EXPECT_EQ(cast_config_.unfreeze_route_count_, 1u);
  EXPECT_EQ(cast_config_.stop_casting_count_, 2u);
  EXPECT_EQ(cast_config_.unfreeze_route_route_id_, device.route.id);
}

TEST_F(CastNotificationControllerTest, FreezeWithTrayOpen) {
  // Create notification.
  cast_config_.has_sinks_and_routes_ = true;
  cast_config_.has_active_routes_ = true;
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

  EXPECT_EQ(cast_config_.freeze_route_count_, 1u);
}

TEST_F(CastNotificationControllerTest, NotificationMessage) {
  cast_config_.has_sinks_and_routes_ = true;
  cast_config_.has_active_routes_ = true;

  // Create notification for a tab casting route.
  SinkAndRoute device1 = CreateDeviceLocalRoute();
  notification_controller_->OnDevicesUpdated({device1});
  EXPECT_EQ(GetNotification()->message(),
            base::UTF8ToUTF16(device1.route.title));

  // Create notification for a desktop route.
  SinkAndRoute device2 = CreateDeviceLocalRouteDesktop();
  notification_controller_->OnDevicesUpdated({device2});
  std::u16string desktop_casting_message = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_CAST_CAST_DESKTOP_NOTIFICATION_MESSAGE);
  EXPECT_EQ(GetNotification()->message(), desktop_casting_message);

  // Create notification for a paused route.
  SinkAndRoute device3 = CreateDeviceLocalRoute();
  device3.route.freeze_info.is_frozen = true;
  notification_controller_->OnDevicesUpdated({device3});
  std::u16string casting_paused_message =
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_CAST_PAUSED);
  EXPECT_EQ(GetNotification()->message(), casting_paused_message);

  SinkAndRoute device4 = CreateDeviceLocalRouteDesktop();
  device4.route.freeze_info.is_frozen = true;
  notification_controller_->OnDevicesUpdated({device4});
  EXPECT_EQ(GetNotification()->message(), casting_paused_message);
}

}  // namespace ash
