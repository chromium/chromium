// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_app_manager.h"

#include <memory>

#include "ash/public/cpp/notification_utils.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "ash/webui/eche_app_ui/accessibility_provider.h"
#include "ash/webui/eche_app_ui/eche_alert_generator.h"
#include "ash/webui/eche_app_ui/eche_connection_status_handler.h"
#include "ash/webui/eche_app_ui/eche_keyboard_layout_handler.h"
#include "ash/webui/eche_app_ui/eche_stream_orientation_observer.h"
#include "ash/webui/eche_app_ui/eche_stream_status_change_handler.h"
#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "ash/webui/eche_app_ui/system_info.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/presence_monitor_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/presence_monitor_client_impl.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_debug_manager_client.h"
#include "google_apis/gaia/gaia_id.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"

namespace ash::eche_app {

namespace {

constexpr auto kAccountId =
    AccountId::Literal::FromUserEmailGaiaId("test@test",
                                            GaiaId::Literal("123456789"));

void LaunchEcheAppFunction(
    const std::optional<int64_t>& notification_id,
    const std::string& package_name,
    const std::u16string& visible_name,
    const std::optional<int64_t>& user_id,
    const gfx::Image& icon,
    const std::u16string& phone_name,
    AppsLaunchInfoProvider* apps_launcher_info_provider) {}

class FakePresenceMonitorClient : public secure_channel::PresenceMonitorClient {
 public:
  FakePresenceMonitorClient() = default;
  ~FakePresenceMonitorClient() override = default;

 private:
  // secure_channel::PresenceMonitorClient:
  void SetPresenceMonitorCallbacks(
      secure_channel::PresenceMonitor::ReadyCallback ready_callback,
      secure_channel::PresenceMonitor::DeviceSeenCallback device_seen_callback)
      override {}
  void StartMonitoring(
      const multidevice::RemoteDeviceRef& remote_device_ref,
      const multidevice::RemoteDeviceRef& local_device_ref) override {}
  void StopMonitoring() override {}
};

class FakeAccessibilityProviderProxy : public AccessibilityProviderProxy {
 public:
  FakeAccessibilityProviderProxy() = default;
  ~FakeAccessibilityProviderProxy() override = default;

  bool UseFullFocusMode() override { return false; }
  ax::android::mojom::AccessibilityFilterType GetFilterType() override {
    return ax::android::mojom::AccessibilityFilterType::ALL;
  }
  void OnViewTracked() override {}

  bool IsAccessibilityEnabled() override { return true; }

  void SetAccessibilityEnabledStateChangedCallback(
      base::RepeatingCallback<void(bool)> callback) override {}
  void SetExploreByTouchEnabledStateChangedCallback(
      base::RepeatingCallback<void(bool)> callback) override {}
};

}  // namespace

const size_t kNumTestDevices = 3;

class EcheAppManagerTest : public AshTestBase {
 public:
  EcheAppManagerTest(const EcheAppManagerTest&) = delete;
  EcheAppManagerTest& operator=(const EcheAppManagerTest&) = delete;

 protected:
  EcheAppManagerTest()
      : test_devices_(
            multidevice::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}
  ~EcheAppManagerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    std::unique_ptr<bluez::BluezDBusManagerSetter> dbus_setter =
        bluez::BluezDBusManager::GetSetterForTesting();
    auto fake_bluetooth_debug_manager_client =
        std::make_unique<bluez::FakeBluetoothDebugManagerClient>();

    // We need to initialize BluezDBusManager early to prevent
    // Bluetooth*::Create() methods from picking the real instead of fake
    // implementations.
    dbus_setter->SetBluetoothDebugManagerClient(
        std::unique_ptr<bluez::BluetoothDebugManagerClient>(
            std::move(fake_bluetooth_debug_manager_client)));

    DCHECK(test_web_view_factory_.get());
    AshTestBase::SetUp();

    ASSERT_TRUE(
        ash_test_helper()->user_session_test_environment().AddRegularUser(
            kAccountId));
    ash_test_helper()->user_session_test_environment().LogIn(kAccountId);

    fake_phone_hub_manager_ = std::make_unique<phonehub::FakePhoneHubManager>();
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->set_local_device_metadata(test_devices_[0]);
    fake_device_sync_client_->NotifyReady();
    fake_multidevice_setup_client_ =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();
    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();

    std::unique_ptr<FakePresenceMonitorClient> fake_presence_monitor_client =
        std::make_unique<FakePresenceMonitorClient>();

    manager_ = std::make_unique<EcheAppManager>(
        &test_pref_service_,
        user_manager::UserManager::Get()->FindUser(kAccountId),
        fake_phone_hub_manager_.get(), fake_device_sync_client_.get(),
        fake_multidevice_setup_client_.get(), fake_secure_channel_client_.get(),
        std::move(fake_presence_monitor_client),
        std::make_unique<FakeAccessibilityProviderProxy>(),
        base::BindRepeating(&LaunchEcheAppFunction), base::DoNothing());
  }

  void TearDown() override {
    manager_->Shutdown();
    manager_.reset();
    fake_secure_channel_client_.reset();
    fake_multidevice_setup_client_.reset();
    fake_device_sync_client_.reset();
    fake_phone_hub_manager_.reset();
    AshTestBase::TearDown();
  }

  const message_center::Notification* GetNotification(
      const std::string& notification_id) {
    const user_manager::User& user =
        CHECK_DEREF(user_manager::UserManager::Get()->FindUser(kAccountId));
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
        CreateUserScopedNotificationId(notification_id, user.username_hash()));
  }

  EcheAppManager* manager() { return manager_.get(); }

  mojo::Remote<mojom::SignalingMessageExchanger>&
  signaling_message_exchanger_remote() {
    return signaling_message_exchanger_remote_;
  }

  mojo::Remote<mojom::SystemInfoProvider>& system_info_provider_remote() {
    return system_info_provider_remote_;
  }

  mojo::Remote<mojom::UidGenerator>& uid_generator_remote() {
    return uid_generator_remote_;
  }

  mojo::Remote<mojom::NotificationGenerator>& notification_generator_remote() {
    return notification_generator_remote_;
  }

  mojo::Remote<mojom::DisplayStreamHandler>& display_stream_handler_remote() {
    return display_stream_handler_remote_;
  }

  mojo::Remote<mojom::StreamOrientationObserver>&
  stream_orientation_observer_remote() {
    return stream_orientation_observer_remote_;
  }

  mojo::Remote<mojom::ConnectionStatusObserver>&
  connection_status_observer_remote() {
    return connection_status_observer_remote_;
  }

  mojo::Remote<mojom::KeyboardLayoutHandler>& keyboard_layout_handler_remote() {
    return keyboard_layout_handler_remote_;
  }

  void Bind() {
    manager_->BindSignalingMessageExchangerInterface(
        signaling_message_exchanger_remote_.BindNewPipeAndPassReceiver());
    manager_->BindSystemInfoProviderInterface(
        system_info_provider_remote_.BindNewPipeAndPassReceiver());
    manager_->BindUidGeneratorInterface(
        uid_generator_remote_.BindNewPipeAndPassReceiver());
    manager_->BindNotificationGeneratorInterface(
        notification_generator_remote_.BindNewPipeAndPassReceiver());
    manager_->BindDisplayStreamHandlerInterface(
        display_stream_handler_remote_.BindNewPipeAndPassReceiver());
    manager_->BindStreamOrientationObserverInterface(
        stream_orientation_observer_remote_.BindNewPipeAndPassReceiver());
    manager_->BindConnectionStatusObserverInterface(
        connection_status_observer_remote_.BindNewPipeAndPassReceiver());
    manager_->BindKeyboardLayoutHandlerInterface(
        keyboard_layout_handler_remote_.BindNewPipeAndPassReceiver());
  }

 private:
  const multidevice::RemoteDeviceRefList test_devices_;

  TestingPrefServiceSimple test_pref_service_;
  std::unique_ptr<phonehub::FakePhoneHubManager> fake_phone_hub_manager_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  std::unique_ptr<secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<EcheAppManager> manager_;
  std::unique_ptr<TestAshWebViewFactory> test_web_view_factory_ =
      std::make_unique<TestAshWebViewFactory>();

  mojo::Remote<mojom::SignalingMessageExchanger>
      signaling_message_exchanger_remote_;
  mojo::Remote<mojom::SystemInfoProvider> system_info_provider_remote_;
  mojo::Remote<mojom::UidGenerator> uid_generator_remote_;
  mojo::Remote<mojom::NotificationGenerator> notification_generator_remote_;
  mojo::Remote<mojom::DisplayStreamHandler> display_stream_handler_remote_;
  mojo::Remote<mojom::StreamOrientationObserver>
      stream_orientation_observer_remote_;
  mojo::Remote<mojom::ConnectionStatusObserver>
      connection_status_observer_remote_;
  mojo::Remote<mojom::KeyboardLayoutHandler> keyboard_layout_handler_remote_;
};

TEST_F(EcheAppManagerTest, BindCheck) {
  EXPECT_FALSE(signaling_message_exchanger_remote());
  EXPECT_FALSE(system_info_provider_remote());
  EXPECT_FALSE(uid_generator_remote());
  EXPECT_FALSE(notification_generator_remote());
  EXPECT_FALSE(display_stream_handler_remote());
  EXPECT_FALSE(stream_orientation_observer_remote());
  EXPECT_FALSE(connection_status_observer_remote());
  EXPECT_FALSE(keyboard_layout_handler_remote());

  Bind();

  EXPECT_TRUE(signaling_message_exchanger_remote());
  EXPECT_TRUE(system_info_provider_remote());
  EXPECT_TRUE(uid_generator_remote());
  EXPECT_TRUE(notification_generator_remote());
  EXPECT_TRUE(display_stream_handler_remote());
  EXPECT_TRUE(stream_orientation_observer_remote());
  EXPECT_TRUE(connection_status_observer_remote());
  EXPECT_TRUE(keyboard_layout_handler_remote());
}

TEST_F(EcheAppManagerTest, GetSystemInfo) {
  const char kLsbRelease[] =
      "CHROMEOS_RELEASE_NAME=Non Chrome OS\n"
      "CHROMEOS_RELEASE_VERSION=1.2.3.4\n";
  const base::Time lsb_release_time(
      base::Time::FromSecondsSinceUnixEpoch(12345.6));
  base::test::ScopedChromeOSVersionInfo version(kLsbRelease, lsb_release_time);
  std::unique_ptr<SystemInfo> system_info =
      EcheAppManager::GetSystemInfo(/*user=*/nullptr);

  EXPECT_EQ("1.2.3", system_info->GetOsVersion());
  EXPECT_EQ("Chrome device", system_info->GetDeviceType());
}

TEST_F(EcheAppManagerTest, CloseConnectionOrLaunchErrorNotifications) {
  std::u16string title = u"title";
  std::u16string message = u"message";
  manager()->ShowNotification(
      title, message,
      std::make_unique<LaunchAppHelper::NotificationInfo>(
          LaunchAppHelper::NotificationInfo::Category::kNative,
          LaunchAppHelper::NotificationInfo::NotificationType::kScreenLock));
  manager()->ShowNotification(
      title, message,
      std::make_unique<LaunchAppHelper::NotificationInfo>(
          LaunchAppHelper::NotificationInfo::Category::kWebUI,
          mojom::WebNotificationType::CONNECTION_FAILED));
  manager()->ShowNotification(
      title, message,
      std::make_unique<LaunchAppHelper::NotificationInfo>(
          LaunchAppHelper::NotificationInfo::Category::kWebUI,
          mojom::WebNotificationType::DEVICE_IDLE));
  manager()->ShowNotification(
      title, message,
      std::make_unique<LaunchAppHelper::NotificationInfo>(
          LaunchAppHelper::NotificationInfo::Category::kWebUI,
          mojom::WebNotificationType::INVALID_NOTIFICATION));
  manager()->CloseConnectionOrLaunchErrorNotifications();

  const message_center::Notification* notification =
      GetNotification(kEcheAppScreenLockNotifierId);
  ASSERT_TRUE(notification);
  notification = GetNotification(kEcheAppRetryConnectionNotifierId);
  ASSERT_FALSE(notification);
  notification = GetNotification(kEcheAppInactivityNotifierId);
  ASSERT_FALSE(notification);
  notification = GetNotification(kEcheAppFromWebWithoutButtonNotifierId);
  ASSERT_FALSE(notification);
}

}  // namespace ash::eche_app
