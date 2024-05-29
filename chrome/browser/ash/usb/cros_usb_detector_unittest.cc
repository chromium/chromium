// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/usb/cros_usb_detector.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_move_support.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/plugin_vm/fake_plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_test_helper.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/ash/components/dbus/vm_plugin_dispatcher/fake_vm_plugin_dispatcher_client.h"
#include "chromeos/ash/components/disks/disk.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/mock_disk_mount_manager.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"

namespace ash {

namespace {

using testing::_;
using MountCallback = ::base::OnceCallback<void(MountError)>;

const char* kCrostiniTestContainerName = "test-container";

// USB device product name.
const char* kProductName_1 = "Google Product A";
const char* kProductName_2 = "Google Product B";
const char* kProductName_3 = "Google Product C";
const char* kUnknownProductName = "USB device";
const char* kManufacturerName = "Google";

const int kUsbConfigWithInterfaces = 1;

struct InterfaceCodes {
  InterfaceCodes(uint8_t device_class,
                 uint8_t subclass_code,
                 uint8_t protocol_code)
      : device_class(device_class),
        subclass_code(subclass_code),
        protocol_code(protocol_code) {}
  uint8_t device_class;
  uint8_t subclass_code;
  uint8_t protocol_code;
};

scoped_refptr<device::FakeUsbDeviceInfo> CreateTestDeviceFromCodes(
    uint8_t device_class,
    const std::vector<InterfaceCodes>& interface_codes) {
  auto config = device::mojom::UsbConfigurationInfo::New();
  config->configuration_value = kUsbConfigWithInterfaces;
  // The usb_utils do not filter by device class, only by configurations, and
  // the FakeUsbDeviceInfo does not set up configurations for a fake device's
  // class code. This helper sets up a configuration to match a devices class
  // code so that USB devices can be filtered out.
  for (size_t i = 0; i < interface_codes.size(); ++i) {
    auto alternate = device::mojom::UsbAlternateInterfaceInfo::New();
    alternate->alternate_setting = 0;
    alternate->class_code = interface_codes[i].device_class;
    alternate->subclass_code = interface_codes[i].subclass_code;
    alternate->protocol_code = interface_codes[i].protocol_code;

    auto interface = device::mojom::UsbInterfaceInfo::New();
    interface->interface_number = i;
    interface->alternates.push_back(std::move(alternate));

    config->interfaces.push_back(std::move(interface));
  }

  std::vector<device::mojom::UsbConfigurationInfoPtr> configs;
  configs.push_back(std::move(config));

  scoped_refptr<device::FakeUsbDeviceInfo> device =
      new device::FakeUsbDeviceInfo(/*vendor_id*/ 0, /*product_id*/ 1,
                                    device_class, std::move(configs));
  device->SetActiveConfig(kUsbConfigWithInterfaces);
  return device;
}

scoped_refptr<device::FakeUsbDeviceInfo> CreateTestDeviceOfClass(
    uint8_t device_class) {
  return CreateTestDeviceFromCodes(device_class,
                                   {InterfaceCodes(device_class, 0xff, 0xff)});
}

class TestCrosUsbDeviceObserver : public CrosUsbDeviceObserver {
 public:
  void OnUsbDevicesChanged() override { ++notify_count_; }

  int notify_count() const { return notify_count_; }

 private:
  int notify_count_ = 0;
};

}  // namespace

class CrosUsbDetectorTest : public BrowserWithTestWindowTest {
 public:
  CrosUsbDetectorTest() {
    // Needed for GuestOsStabilityMonitor.
    ChunneldClient::InitializeFake();
    ash::CiceroneClient::InitializeFake();
    ConciergeClient::InitializeFake();
    SeneschalClient::InitializeFake();
    VmPluginDispatcherClient::InitializeFake();
    fake_cicerone_client_ = ash::FakeCiceroneClient::Get();
    fake_concierge_client_ = FakeConciergeClient::Get();
    fake_vm_plugin_dispatcher_client_ =
        static_cast<FakeVmPluginDispatcherClient*>(
            VmPluginDispatcherClient::Get());

    mock_disk_mount_manager_ =
        new testing::NiceMock<disks::MockDiskMountManager>;
    disks::DiskMountManager::InitializeForTesting(mock_disk_mount_manager_);
  }

  CrosUsbDetectorTest(const CrosUsbDetectorTest&) = delete;
  CrosUsbDetectorTest& operator=(const CrosUsbDetectorTest&) = delete;

  ~CrosUsbDetectorTest() override {
    disks::DiskMountManager::Shutdown();
    VmPluginDispatcherClient::Shutdown();
    SeneschalClient::Shutdown();
    ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    ChunneldClient::Shutdown();
  }

  void SetUp() override {
    cros_usb_detector_ = std::make_unique<CrosUsbDetector>();
    BrowserWithTestWindowTest::SetUp();
    crostini_test_helper_ =
        std::make_unique<crostini::CrostiniTestHelper>(profile());

    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    display_service_ = std::make_unique<NotificationDisplayServiceTester>(
        nullptr /* profile */);

    // Set a fake USB device manager before ConnectToDeviceManager().
    mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager;
    device_manager_.AddReceiver(
        device_manager.InitWithNewPipeAndPassReceiver());
    CrosUsbDetector::Get()->SetDeviceManagerForTesting(
        std::move(device_manager));
    // Create a default VM instance which is running.
    crostini::CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
        crostini::kCrostiniDefaultVmName);
  }

  void TearDown() override {
    crostini_test_helper_.reset();
    BrowserWithTestWindowTest::TearDown();
    cros_usb_detector_.reset();
  }

  void ConnectToDeviceManager() {
    CrosUsbDetector::Get()->ConnectToDeviceManager();
  }

  MOCK_METHOD1(OnAttach, void(bool success));

  void AttachDeviceToGuest(const guest_os::GuestId& guest_id,
                           const std::string& guid,
                           bool vm_success = true,
                           bool container_success = true) {
    std::optional<vm_tools::concierge::AttachUsbDeviceResponse>
        attach_device_response;
    attach_device_response.emplace();
    attach_device_response->set_success(vm_success);
    attach_device_response->set_guest_port(0);
    fake_concierge_client_->set_attach_usb_device_response(
        std::move(attach_device_response));

    if (vm_success && !guest_id.container_name.empty()) {
      vm_tools::cicerone::AttachUsbToContainerResponse
          attach_container_response;
      attach_container_response.set_status(
          container_success
              ? vm_tools::cicerone::AttachUsbToContainerResponse_Status_OK
              : vm_tools::cicerone::AttachUsbToContainerResponse_Status_FAILED);
      if (!container_success) {
        attach_container_response.set_failure_reason("Expected failure");
      }
      fake_cicerone_client_->set_attach_usb_to_container_response(
          std::move(attach_container_response));

      EXPECT_CALL(*this, OnAttach(container_success));
    } else {
      EXPECT_CALL(*this, OnAttach(vm_success));
    }

    cros_usb_detector_->AttachUsbDeviceToGuest(
        guest_id, guid,
        base::BindOnce(&CrosUsbDetectorTest::OnAttach, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void DetachDeviceFromVm(const std::string& vm_name,
                          const std::string& guid,
                          bool expected_success) {
    cros_usb_detector_->DetachUsbDeviceFromVm(
        vm_name, guid,
        base::BindOnce(
            [](bool expected, bool actual) { EXPECT_EQ(expected, actual); },
            expected_success));
    base::RunLoop().RunUntilIdle();
  }

  // The GetSingle..() functions expect only one device is present and may crash
  // if there are no devices (we can't use ASSERT_EQ as they return values).

  CrosUsbDeviceInfo GetSingleDeviceInfo() const {
    auto devices = cros_usb_detector_->GetShareableDevices();
    EXPECT_EQ(1U, devices.size());
    return devices.front();
  }

  std::optional<uint8_t> GetSingleGuestPort() const {
    EXPECT_EQ(1U, cros_usb_detector_->usb_devices_.size());
    return cros_usb_detector_->usb_devices_.begin()->second.guest_port;
  }

  void AddDisk(const std::string& name,
               int bus_number,
               int device_number,
               bool mounted) {
    mock_disk_mount_manager_->CreateDiskEntryForMountDevice(
        disks::Disk::Builder()
            .SetBusNumber(bus_number)
            .SetDeviceNumber(device_number)
            .SetDevicePath("/dev/" + name)
            .SetMountPath("/mount/" + name)
            .SetIsMounted(mounted)
            .Build());
    if (mounted) {
      NotifyMountEvent(name, disks::DiskMountManager::MOUNTING);
    }
  }

  void NotifyMountEvent(const std::string& name,
                        disks::DiskMountManager::MountEvent event,
                        MountError mount_error = MountError::kSuccess) {
    // In theory we should also clear the mounted flag from the disk, but we
    // don't rely on that.
    disks::DiskMountManager::MountPoint info{"/dev/" + name, "/mount/" + name,
                                             MountType::kDevice};
    mock_disk_mount_manager_->NotifyMountEvent(event, mount_error, info);
  }

 protected:
  std::u16string connection_message(const char* product_name) {
    return base::ASCIIToUTF16(base::StringPrintf(
        "Open Settings to connect %s to Linux", product_name));
  }

  std::u16string expected_title() { return u"USB device detected"; }

  device::FakeUsbDeviceManager device_manager_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  raw_ptr<disks::MockDiskMountManager, DanglingUntriaged>
      mock_disk_mount_manager_;
  disks::DiskMountManager::Disks disks_;

  raw_ptr<ash::FakeCiceroneClient, DanglingUntriaged> fake_cicerone_client_;
  raw_ptr<FakeConciergeClient, DanglingUntriaged> fake_concierge_client_;
  raw_ptr<FakeVmPluginDispatcherClient, DanglingUntriaged>
      fake_vm_plugin_dispatcher_client_;

  TestCrosUsbDeviceObserver usb_device_observer_;
  std::unique_ptr<CrosUsbDetector> cros_usb_detector_;

  std::unique_ptr<crostini::CrostiniTestHelper> crostini_test_helper_;
};

TEST_F(CrosUsbDetectorTest, UsbDeviceAddedAndRemoved) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  auto device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, kProductName_1, "002");
  device_manager_.AddDevice(device);
  base::RunLoop().RunUntilIdle();

  std::string notification_id =
      CrosUsbDetector::MakeNotificationId(device->guid());

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(notification_id);
  ASSERT_TRUE(notification);

  EXPECT_EQ(expected_title(), notification->title());
  EXPECT_EQ(connection_message(kProductName_1), notification->message());
  EXPECT_TRUE(notification->delegate());

  device_manager_.RemoveDevice(device);
  base::RunLoop().RunUntilIdle();
  // Device is removed, so notification should be removed too.
  EXPECT_FALSE(display_service_->GetNotification(notification_id));
}

TEST_F(CrosUsbDetectorTest, NotificationShown) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  auto device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, kProductName_1, "002");
  std::string notification_id =
      CrosUsbDetector::MakeNotificationId(device->guid());

  // Notifications should not be shown if no VMs enabled.
  crostini::FakeCrostiniFeatures crostini_features;
  crostini_features.set_enabled(false);
  device_manager_.AddDevice(device);
  base::RunLoop().RunUntilIdle();

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(notification_id);
  EXPECT_FALSE(notification);
  device_manager_.RemoveDevice(device);
  base::RunLoop().RunUntilIdle();

  // Notification should have 1 button when only crostini is enabled.
  crostini_features.set_enabled(true);
  device_manager_.AddDevice(device);
  base::RunLoop().RunUntilIdle();
  notification = display_service_->GetNotification(notification_id);
  ASSERT_TRUE(notification);
  EXPECT_EQ(notification->buttons().size(), 1u);
  device_manager_.RemoveDevice(device);
  base::RunLoop().RunUntilIdle();

  // Should have 2 buttons when Plugin VM is enabled.
  plugin_vm::FakePluginVmFeatures plugin_vm_features;
  plugin_vm_features.set_enabled(true);
  device_manager_.AddDevice(device);
  base::RunLoop().RunUntilIdle();
  notification = display_service_->GetNotification(notification_id);
  ASSERT_TRUE(notification);
  EXPECT_EQ(notification->buttons().size(), 2u);
  device_manager_.RemoveDevice(device);
  base::RunLoop().RunUntilIdle();

  // Should have 2 buttions when ARCVM is enabled but user disables ARC.
  // ARC is disabled by default in test.
  arc::ResetArcAllowedCheckForTesting(profile());
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv(
      {"", "--enable-arcvm", "--arc-availability=officially-supported"});
  EXPECT_TRUE(arc::IsArcVmEnabled());
  device_manager_.AddDevice(device);
  base::RunLoop().RunUntilIdle();
  notification = display_service_->GetNotification(notification_id);
  ASSERT_TRUE(notification);
  EXPECT_EQ(notification->buttons().size(), 2u);
  device_manager_.RemoveDevice(device);
  base::RunLoop().RunUntilIdle();

  // Should have 3 buttions when ARCVM is enabled and user enables ARC but the
  // feature is disabled.
  ASSERT_TRUE(arc::SetArcPlayStoreEnabledForProfile(profile(), true));
  device_manager_.AddDevice(device);
  base::RunLoop().RunUntilIdle();
  notification = display_service_->GetNotification(notification_id);
  ASSERT_TRUE(notification);
  EXPECT_EQ(notification->buttons().size(), 3u);
  device_manager_.RemoveDevice(device);
  base::RunLoop().RunUntilIdle();

  // Now should have 4 buttons when Bruschetta is enabled.
  AddContainerToPrefs(profile(), bruschetta::GetBruschettaAlphaId(), {});
  device_manager_.AddDevice(device);
  base::RunLoop().RunUntilIdle();
  notification = display_service_->GetNotification(notification_id);
  ASSERT_TRUE(notification);
  EXPECT_EQ(notification->buttons().size(), 4u);
  device_manager_.RemoveDevice(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrosUsbDetectorTest, NotificationNotShownForEthernetInterface) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  auto device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0x200, USB_CLASS_COMM, USB_COMM_SUBCLASS_ETHERNET, 0xff, 0x0100, 0, 0, 0,
      0, "my cool manufacturer", "my cool ethernet adapter", "SN1337");
  std::string notification_id =
      CrosUsbDetector::MakeNotificationId(device->guid());

  // Notifications should not be shown if no VMs enabled.
  crostini::FakeCrostiniFeatures crostini_features;
  crostini_features.set_enabled(false);
  device_manager_.AddDevice(device);
  base::RunLoop().RunUntilIdle();

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(notification_id);
  EXPECT_FALSE(notification);
  device_manager_.RemoveDevice(device);
  base::RunLoop().RunUntilIdle();

  // Notification should never be shown
  crostini_features.set_enabled(true);
  device_manager_.AddDevice(device);
  base::RunLoop().RunUntilIdle();
  notification = display_service_->GetNotification(notification_id);
  ASSERT_FALSE(notification);
}

TEST_F(CrosUsbDetectorTest, NotificationControlledByPolicy) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  auto device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, kProductName_1, "002");
  std::string notification_id =
      CrosUsbDetector::MakeNotificationId(device->guid());

  // Notifications should be shown if UsbDetectorNotificationEnabled policy is
  // unset.
  crostini::FakeCrostiniFeatures crostini_features;
  crostini_features.set_enabled(true);
  device_manager_.AddDevice(device);
  base::RunLoop().RunUntilIdle();
  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(notification_id);
  EXPECT_TRUE(notification);
  device_manager_.RemoveDevice(device);
  base::RunLoop().RunUntilIdle();

  // Notifications should not be shown if UsbDetectorNotificationEnabled policy
  // is false.
  profile()->GetPrefs()->SetBoolean(prefs::kUsbDetectorNotificationEnabled,
                                    false);
  device_manager_.AddDevice(device);
  base::RunLoop().RunUntilIdle();
  notification = display_service_->GetNotification(notification_id);
  EXPECT_FALSE(notification);
  device_manager_.RemoveDevice(device);
  base::RunLoop().RunUntilIdle();

  // Notifications should be shown if UsbDetectorNotificationEnabled policy is
  // true.
  profile()->GetPrefs()->SetBoolean(prefs::kUsbDetectorNotificationEnabled,
                                    true);
  device_manager_.AddDevice(device);
  base::RunLoop().RunUntilIdle();
  notification = display_service_->GetNotification(notification_id);
  EXPECT_TRUE(notification);
  device_manager_.RemoveDevice(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(CrosUsbDetectorTest, UsbNotificationClicked) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  auto device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, kProductName_1, "002");
  device_manager_.AddDevice(device);
  base::RunLoop().RunUntilIdle();

  std::string notification_id =
      CrosUsbDetector::MakeNotificationId(device->guid());

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(notification_id);
  ASSERT_TRUE(notification);

  notification->delegate()->Click(0, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_GE(fake_concierge_client_->attach_usb_device_call_count(), 1);
  // Notification should close.
  EXPECT_FALSE(display_service_->GetNotification(notification_id));
}

TEST_F(CrosUsbDetectorTest, UsbDeviceClassAdbAdded) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  const int kAdbClass = 0xff;
  const int kAdbSubclass = 0x42;
  const int kAdbProtocol = 0x1;
  // Adb interface as well as a forbidden interface
  scoped_refptr<device::FakeUsbDeviceInfo> device = CreateTestDeviceFromCodes(
      /* USB_CLASS_HID */ 0x03,
      {InterfaceCodes(kAdbClass, kAdbSubclass, kAdbProtocol),
       InterfaceCodes(0x03, 0xff, 0xff)});

  device_manager_.AddDevice(device);
  base::RunLoop().RunUntilIdle();

  std::string notification_id =
      CrosUsbDetector::MakeNotificationId(device->guid());
  ASSERT_TRUE(display_service_->GetNotification(notification_id));
  // ADB interface wins.
  EXPECT_EQ(1U, cros_usb_detector_->GetShareableDevices().size());
}

TEST_F(CrosUsbDetectorTest, UsbDeviceClassWithoutNotificationAdded) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  scoped_refptr<device::FakeUsbDeviceInfo> device =
      CreateTestDeviceOfClass(/* USB_CLASS_AUDIO */ 0x01);

  device_manager_.AddDevice(device);
  base::RunLoop().RunUntilIdle();

  std::string notification_id =
      CrosUsbDetector::MakeNotificationId(device->guid());
  ASSERT_FALSE(display_service_->GetNotification(notification_id));
  EXPECT_EQ(1U, cros_usb_detector_->GetShareableDevices().size());
}

TEST_F(CrosUsbDetectorTest, UsbDeviceWithoutProductNameAddedAndRemoved) {
  std::string product_name;
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  auto device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, product_name, "002");
  device_manager_.AddDevice(device);
  base::RunLoop().RunUntilIdle();

  std::string notification_id =
      CrosUsbDetector::MakeNotificationId(device->guid());

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(notification_id);
  ASSERT_TRUE(notification);

  EXPECT_EQ(expected_title(), notification->title());
  EXPECT_EQ(connection_message("USB device from Google"),
            notification->message());
  EXPECT_TRUE(notification->delegate());

  device_manager_.RemoveDevice(device);
  base::RunLoop().RunUntilIdle();
  // Device is removed, so notification should be removed too.
  EXPECT_FALSE(display_service_->GetNotification(notification_id));
}

TEST_F(CrosUsbDetectorTest,
       UsbDeviceWithoutProductNameOrManufacturerNameAddedAndRemoved) {
  std::string product_name;
  std::string manufacturer_name;
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  auto device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, manufacturer_name, product_name, "002");
  device_manager_.AddDevice(device);
  base::RunLoop().RunUntilIdle();

  std::string notification_id =
      CrosUsbDetector::MakeNotificationId(device->guid());

  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(notification_id);
  ASSERT_TRUE(notification);
  EXPECT_EQ(expected_title(), notification->title());
  EXPECT_EQ(connection_message(kUnknownProductName), notification->message());
  EXPECT_TRUE(notification->delegate());

  device_manager_.RemoveDevice(device);
  base::RunLoop().RunUntilIdle();
  // Device is removed, so notification should be removed too.
  EXPECT_FALSE(display_service_->GetNotification(notification_id));
}

TEST_F(CrosUsbDetectorTest, UsbDeviceWasThereBeforeAndThenRemoved) {
  // USB device was added before cros_usb_detector was created.
  auto device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, kProductName_1, "002");
  device_manager_.AddDevice(device);
  base::RunLoop().RunUntilIdle();

  std::string notification_id =
      CrosUsbDetector::MakeNotificationId(device->guid());

  EXPECT_FALSE(display_service_->GetNotification(notification_id));

  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  device_manager_.RemoveDevice(device);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id));
}

TEST_F(
    CrosUsbDetectorTest,
    ThreeUsbDevicesWereThereBeforeAndThenRemovedBeforeUsbDetectorWasCreated) {
  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, kProductName_1, "002");
  std::string notification_id_1 =
      CrosUsbDetector::MakeNotificationId(device_1->guid());

  auto device_2 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      3, 4, kManufacturerName, kProductName_2, "005");
  std::string notification_id_2 =
      CrosUsbDetector::MakeNotificationId(device_2->guid());

  auto device_3 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      6, 7, kManufacturerName, kProductName_3, "008");
  std::string notification_id_3 =
      CrosUsbDetector::MakeNotificationId(device_3->guid());

  // Three usb devices were added and removed before cros_usb_detector was
  // created.
  device_manager_.AddDevice(device_1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id_1));

  device_manager_.AddDevice(device_2);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id_2));

  device_manager_.AddDevice(device_3);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id_3));

  device_manager_.RemoveDevice(device_1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id_1));

  device_manager_.RemoveDevice(device_2);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id_2));

  device_manager_.RemoveDevice(device_3);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id_3));

  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(display_service_->GetNotification(notification_id_1));
  EXPECT_FALSE(display_service_->GetNotification(notification_id_2));
  EXPECT_FALSE(display_service_->GetNotification(notification_id_3));
}

TEST_F(CrosUsbDetectorTest,
       ThreeUsbDevicesWereThereBeforeAndThenRemovedAfterUsbDetectorWasCreated) {
  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, kProductName_1, "002");
  std::string notification_id_1 =
      CrosUsbDetector::MakeNotificationId(device_1->guid());

  auto device_2 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      3, 4, kManufacturerName, kProductName_2, "005");
  std::string notification_id_2 =
      CrosUsbDetector::MakeNotificationId(device_2->guid());

  auto device_3 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      6, 7, kManufacturerName, kProductName_3, "008");
  std::string notification_id_3 =
      CrosUsbDetector::MakeNotificationId(device_3->guid());

  // Three usb devices were added before cros_usb_detector was created.
  device_manager_.AddDevice(device_1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id_1));

  device_manager_.AddDevice(device_2);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id_2));

  device_manager_.AddDevice(device_3);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id_3));

  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(display_service_->GetNotification(notification_id_1));
  EXPECT_FALSE(display_service_->GetNotification(notification_id_2));
  EXPECT_FALSE(display_service_->GetNotification(notification_id_3));

  device_manager_.RemoveDevice(device_1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id_1));

  device_manager_.RemoveDevice(device_2);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id_2));

  device_manager_.RemoveDevice(device_3);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id_3));
}

TEST_F(CrosUsbDetectorTest,
       TwoUsbDevicesWereThereBeforeAndThenRemovedAndNewUsbDeviceAdded) {
  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, kProductName_1, "002");
  std::string notification_id_1 =
      CrosUsbDetector::MakeNotificationId(device_1->guid());

  auto device_2 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      3, 4, kManufacturerName, kProductName_2, "005");
  std::string notification_id_2 =
      CrosUsbDetector::MakeNotificationId(device_2->guid());

  // Two usb devices were added before cros_usb_detector was created.
  device_manager_.AddDevice(device_1);
  device_manager_.AddDevice(device_2);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id_1));
  EXPECT_FALSE(display_service_->GetNotification(notification_id_2));

  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id_1));
  EXPECT_FALSE(display_service_->GetNotification(notification_id_2));

  device_manager_.RemoveDevice(device_1);
  device_manager_.RemoveDevice(device_2);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(display_service_->GetNotification(notification_id_1));
  EXPECT_FALSE(display_service_->GetNotification(notification_id_2));

  device_manager_.AddDevice(device_2);
  base::RunLoop().RunUntilIdle();
  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(notification_id_2);
  ASSERT_TRUE(notification);

  EXPECT_EQ(expected_title(), notification->title());
  EXPECT_EQ(connection_message(kProductName_2), notification->message());
  EXPECT_TRUE(notification->delegate());

  device_manager_.RemoveDevice(device_2);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id_2));
}

TEST_F(CrosUsbDetectorTest, ThreeUsbDevicesAddedAndRemoved) {
  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, kProductName_1, "002");
  std::string notification_id_1 =
      CrosUsbDetector::MakeNotificationId(device_1->guid());

  auto device_2 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      3, 4, kManufacturerName, kProductName_2, "005");
  std::string notification_id_2 =
      CrosUsbDetector::MakeNotificationId(device_2->guid());

  auto device_3 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      6, 7, kManufacturerName, kProductName_3, "008");
  std::string notification_id_3 =
      CrosUsbDetector::MakeNotificationId(device_3->guid());

  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  device_manager_.AddDevice(device_1);
  base::RunLoop().RunUntilIdle();
  std::optional<message_center::Notification> notification_1 =
      display_service_->GetNotification(notification_id_1);
  ASSERT_TRUE(notification_1);

  EXPECT_EQ(expected_title(), notification_1->title());
  EXPECT_EQ(connection_message(kProductName_1), notification_1->message());
  EXPECT_TRUE(notification_1->delegate());

  device_manager_.RemoveDevice(device_1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id_1));

  device_manager_.AddDevice(device_2);
  base::RunLoop().RunUntilIdle();
  std::optional<message_center::Notification> notification_2 =
      display_service_->GetNotification(notification_id_2);
  ASSERT_TRUE(notification_2);

  EXPECT_EQ(expected_title(), notification_2->title());
  EXPECT_EQ(connection_message(kProductName_2), notification_2->message());
  EXPECT_TRUE(notification_2->delegate());

  device_manager_.RemoveDevice(device_2);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id_2));

  device_manager_.AddDevice(device_3);
  base::RunLoop().RunUntilIdle();
  std::optional<message_center::Notification> notification_3 =
      display_service_->GetNotification(notification_id_3);
  ASSERT_TRUE(notification_3);

  EXPECT_EQ(expected_title(), notification_3->title());
  EXPECT_EQ(connection_message(kProductName_3), notification_3->message());
  EXPECT_TRUE(notification_3->delegate());

  device_manager_.RemoveDevice(device_3);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id_3));
}

TEST_F(CrosUsbDetectorTest, ThreeUsbDeviceAddedAndRemovedDifferentOrder) {
  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, kProductName_1, "002");
  std::string notification_id_1 =
      CrosUsbDetector::MakeNotificationId(device_1->guid());

  auto device_2 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      3, 4, kManufacturerName, kProductName_2, "005");
  std::string notification_id_2 =
      CrosUsbDetector::MakeNotificationId(device_2->guid());

  auto device_3 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      6, 7, kManufacturerName, kProductName_3, "008");
  std::string notification_id_3 =
      CrosUsbDetector::MakeNotificationId(device_3->guid());

  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  device_manager_.AddDevice(device_1);
  base::RunLoop().RunUntilIdle();
  std::optional<message_center::Notification> notification_1 =
      display_service_->GetNotification(notification_id_1);
  ASSERT_TRUE(notification_1);

  EXPECT_EQ(expected_title(), notification_1->title());
  EXPECT_EQ(connection_message(kProductName_1), notification_1->message());
  EXPECT_TRUE(notification_1->delegate());

  device_manager_.AddDevice(device_2);
  base::RunLoop().RunUntilIdle();
  std::optional<message_center::Notification> notification_2 =
      display_service_->GetNotification(notification_id_2);
  ASSERT_TRUE(notification_2);

  EXPECT_EQ(expected_title(), notification_2->title());
  EXPECT_EQ(connection_message(kProductName_2), notification_2->message());
  EXPECT_TRUE(notification_2->delegate());

  device_manager_.RemoveDevice(device_2);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id_2));

  device_manager_.AddDevice(device_3);
  base::RunLoop().RunUntilIdle();
  std::optional<message_center::Notification> notification_3 =
      display_service_->GetNotification(notification_id_3);
  ASSERT_TRUE(notification_3);

  EXPECT_EQ(expected_title(), notification_3->title());
  EXPECT_EQ(connection_message(kProductName_3), notification_3->message());
  EXPECT_TRUE(notification_3->delegate());

  device_manager_.RemoveDevice(device_1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id_1));

  device_manager_.RemoveDevice(device_3);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_service_->GetNotification(notification_id_3));
}

TEST_F(CrosUsbDetectorTest, AttachDeviceToVmSetsGuestPort) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, kProductName_1, "002");
  device_manager_.AddDevice(device_1);
  base::RunLoop().RunUntilIdle();

  auto device_info = GetSingleDeviceInfo();
  EXPECT_FALSE(GetSingleGuestPort().has_value());
  AttachDeviceToGuest(guest_os::GuestId(crostini::kCrostiniDefaultVmName, ""),
                      device_info.guid);

  device_info = GetSingleDeviceInfo();
  EXPECT_TRUE(device_info.shared_guest_id.has_value());
  EXPECT_EQ(crostini::kCrostiniDefaultVmName,
            device_info.shared_guest_id->vm_name);
  EXPECT_TRUE(GetSingleGuestPort().has_value());
  EXPECT_EQ(0U, *GetSingleGuestPort());
}

TEST_F(CrosUsbDetectorTest, AttachingAlreadyAttachedDeviceIsANoOp) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, kProductName_1, "002");
  device_manager_.AddDevice(device_1);
  base::RunLoop().RunUntilIdle();

  auto device_info = GetSingleDeviceInfo();
  EXPECT_FALSE(device_info.shared_guest_id.has_value());

  AttachDeviceToGuest(guest_os::GuestId(crostini::kCrostiniDefaultVmName, ""),
                      device_info.guid);
  cros_usb_detector_->AddUsbDeviceObserver(&usb_device_observer_);
  AttachDeviceToGuest(guest_os::GuestId(crostini::kCrostiniDefaultVmName, ""),
                      device_info.guid);
  EXPECT_EQ(0, usb_device_observer_.notify_count());
  device_info = GetSingleDeviceInfo();
  EXPECT_TRUE(device_info.shared_guest_id.has_value());
  EXPECT_EQ(crostini::kCrostiniDefaultVmName,
            device_info.shared_guest_id->vm_name);
}

TEST_F(CrosUsbDetectorTest, DeviceCanBeAttachedToArcVmWhenCrostiniIsDisabled) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, kProductName_1, "002");
  device_manager_.AddDevice(device_1);
  base::RunLoop().RunUntilIdle();

  auto device_info = GetSingleDeviceInfo();
  AttachDeviceToGuest(guest_os::GuestId(arc::kArcVmName, ""), device_info.guid);
  base::RunLoop().RunUntilIdle();
  device_info = GetSingleDeviceInfo();
  EXPECT_TRUE(device_info.shared_guest_id.has_value());
  EXPECT_EQ(arc::kArcVmName, device_info.shared_guest_id->vm_name);
}

TEST_F(CrosUsbDetectorTest, SharedDevicesGetAttachedOnStartup) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, kProductName_1, "002");
  device_manager_.AddDevice(device_1);
  base::RunLoop().RunUntilIdle();

  cros_usb_detector_->ConnectSharedDevicesOnVmStartup(
      crostini::kCrostiniDefaultVmName);
  base::RunLoop().RunUntilIdle();
  // No device is shared with Crostini, yet.
  EXPECT_EQ(0, usb_device_observer_.notify_count());
  auto device_info = GetSingleDeviceInfo();
  EXPECT_FALSE(device_info.shared_guest_id.has_value());

  AttachDeviceToGuest(guest_os::GuestId(crostini::kCrostiniDefaultVmName, ""),
                      device_info.guid);
  base::RunLoop().RunUntilIdle();
  device_info = GetSingleDeviceInfo();
  EXPECT_TRUE(device_info.shared_guest_id.has_value());
  EXPECT_EQ(crostini::kCrostiniDefaultVmName,
            device_info.shared_guest_id->vm_name);

  // Concierge::VmStarted signal should trigger connections.
  cros_usb_detector_->AddUsbDeviceObserver(&usb_device_observer_);
  vm_tools::concierge::VmStartedSignal vm_started_signal;
  vm_started_signal.set_name(crostini::kCrostiniDefaultVmName);
  fake_concierge_client_->NotifyVmStarted(vm_started_signal);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, usb_device_observer_.notify_count());
  device_info = GetSingleDeviceInfo();
  EXPECT_TRUE(device_info.shared_guest_id.has_value());
  EXPECT_EQ(crostini::kCrostiniDefaultVmName,
            device_info.shared_guest_id->vm_name);

  // VmPluginDispatcherClient::OnVmStateChanged RUNNING should also trigger.
  vm_tools::plugin_dispatcher::VmStateChangedSignal vm_state_changed_signal;
  vm_state_changed_signal.set_vm_name(crostini::kCrostiniDefaultVmName);
  vm_state_changed_signal.set_vm_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING);
  fake_vm_plugin_dispatcher_client_->NotifyVmStateChanged(
      vm_state_changed_signal);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, usb_device_observer_.notify_count());
  device_info = GetSingleDeviceInfo();
  EXPECT_TRUE(device_info.shared_guest_id.has_value());
  EXPECT_EQ(crostini::kCrostiniDefaultVmName,
            device_info.shared_guest_id->vm_name);
}

TEST_F(CrosUsbDetectorTest, SwitchDeviceWithAttachSuccess) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, kProductName_1, "002");
  device_manager_.AddDevice(device_1);
  base::RunLoop().RunUntilIdle();

  auto device_info = GetSingleDeviceInfo();
  EXPECT_FALSE(device_info.shared_guest_id.has_value());

  AttachDeviceToGuest(guest_os::GuestId("VM1", ""), device_info.guid,
                      /*vm_success=*/false);
  device_info = GetSingleDeviceInfo();
  ASSERT_TRUE(device_info.shared_guest_id.has_value());
  EXPECT_EQ("VM1", device_info.shared_guest_id->vm_name);

  // Shared but not attached to VM1 -> attached to VM2
  AttachDeviceToGuest(guest_os::GuestId("VM2", ""), device_info.guid);
  device_info = GetSingleDeviceInfo();
  ASSERT_TRUE(device_info.shared_guest_id.has_value());
  EXPECT_EQ("VM2", device_info.shared_guest_id->vm_name);
  EXPECT_EQ(fake_concierge_client_->detach_usb_device_call_count(), 0);

  // Attached to VM2 -> attached to VM3
  AttachDeviceToGuest(guest_os::GuestId("VM3", ""), device_info.guid);
  device_info = GetSingleDeviceInfo();
  ASSERT_TRUE(device_info.shared_guest_id.has_value());
  EXPECT_EQ("VM3", device_info.shared_guest_id->vm_name);
  EXPECT_GE(fake_concierge_client_->detach_usb_device_call_count(), 1);
}

TEST_F(CrosUsbDetectorTest, SwitchDeviceWithAttachFailure) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, kProductName_1, "002");
  device_manager_.AddDevice(device_1);
  base::RunLoop().RunUntilIdle();

  auto device_info = GetSingleDeviceInfo();
  EXPECT_FALSE(device_info.shared_guest_id.has_value());

  AttachDeviceToGuest(guest_os::GuestId("VM1", ""), device_info.guid);
  device_info = GetSingleDeviceInfo();
  EXPECT_TRUE(device_info.shared_guest_id.has_value());
  EXPECT_EQ("VM1", device_info.shared_guest_id->vm_name);

  // Attached to VM1 -> shared but not attached to VM2
  AttachDeviceToGuest(guest_os::GuestId("VM2", ""), device_info.guid,
                      /*vm_success=*/false);
  device_info = GetSingleDeviceInfo();
  EXPECT_TRUE(device_info.shared_guest_id.has_value());
  EXPECT_EQ("VM2", device_info.shared_guest_id->vm_name);
  EXPECT_GE(fake_concierge_client_->detach_usb_device_call_count(), 1);

  // Shared but not attached to VM2 -> shared but not attached to VM3
  AttachDeviceToGuest(guest_os::GuestId("VM3", ""), device_info.guid,
                      /*vm_success=*/false);
  device_info = GetSingleDeviceInfo();
  EXPECT_TRUE(device_info.shared_guest_id.has_value());
  EXPECT_EQ("VM3", device_info.shared_guest_id->vm_name);
}

TEST_F(CrosUsbDetectorTest, DetachFromDifferentVM) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, kProductName_1, "002");
  device_manager_.AddDevice(device_1);
  base::RunLoop().RunUntilIdle();

  auto device_info = GetSingleDeviceInfo();
  EXPECT_FALSE(device_info.shared_guest_id.has_value());

  AttachDeviceToGuest(guest_os::GuestId("VM1", ""), device_info.guid);
  device_info = GetSingleDeviceInfo();
  EXPECT_TRUE(device_info.shared_guest_id.has_value());
  EXPECT_EQ("VM1", device_info.shared_guest_id->vm_name);

  // Device is not attached to VM2, so this will no-op.
  DetachDeviceFromVm("VM2", device_info.guid, /*expected_success=*/false);
  EXPECT_EQ(fake_concierge_client_->detach_usb_device_call_count(), 0);
  EXPECT_EQ("VM1", device_info.shared_guest_id->vm_name);
}

TEST_F(CrosUsbDetectorTest, AttachUnmountFilesystemSuccess) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  device_manager_.CreateAndAddDevice(
      0x0200, 0xff, 0xff, 0xff, 0x0100, 1, 2, /*bus_number=*/3,
      /*port_number=*/4, kManufacturerName, kProductName_1, "5");
  base::RunLoop().RunUntilIdle();

  AddDisk("disk1", 3, 4, true);
  AddDisk("disk2", 3, 4, /*mounted=*/false);
  NotifyMountEvent("disk2", disks::DiskMountManager::MOUNTING,
                   MountError::kInternalError);
  AddDisk("disk3", 3, 5, true);
  AddDisk("disk4", 3, 4, true);
  AddDisk("disk5", 2, 4, true);
  MountCallback callback1;
  MountCallback callback4;
  EXPECT_CALL(*mock_disk_mount_manager_, UnmountPath("/mount/disk1", _))
      .WillOnce(MoveArg<1>(&callback1));
  EXPECT_CALL(*mock_disk_mount_manager_, UnmountPath("/mount/disk4", _))
      .WillOnce(MoveArg<1>(&callback4));

  auto device_info = GetSingleDeviceInfo();
  AttachDeviceToGuest(guest_os::GuestId("VM1", ""), device_info.guid);
  EXPECT_EQ(fake_concierge_client_->attach_usb_device_call_count(), 0);

  // Unmount events would normally be fired by the DiskMountManager.
  NotifyMountEvent("disk1", disks::DiskMountManager::UNMOUNTING);
  std::move(callback1).Run(MountError::kSuccess);
  base::RunLoop().RunUntilIdle();

  device_info = GetSingleDeviceInfo();
  EXPECT_FALSE(device_info.shared_guest_id.has_value());
  EXPECT_EQ(fake_concierge_client_->attach_usb_device_call_count(), 0);

  // All unmounts must complete before sharing succeeds.
  NotifyMountEvent("disk4", disks::DiskMountManager::UNMOUNTING);
  std::move(callback4).Run(MountError::kSuccess);
  base::RunLoop().RunUntilIdle();

  device_info = GetSingleDeviceInfo();
  EXPECT_GE(fake_concierge_client_->attach_usb_device_call_count(), 1);
  EXPECT_TRUE(device_info.shared_guest_id.has_value());
  EXPECT_EQ("VM1", device_info.shared_guest_id->vm_name);
}

TEST_F(CrosUsbDetectorTest, AttachUnmountFilesystemFailure) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  device_manager_.CreateAndAddDevice(
      0x0200, 0xff, 0xff, 0xff, 0x0100, 1, 2, /*bus_number=*/1,
      /*port_number=*/5, kManufacturerName, kProductName_1, "5");
  base::RunLoop().RunUntilIdle();

  AddDisk("disk1", 1, 5, true);
  AddDisk("disk2", 1, 5, true);
  AddDisk("disk3", 1, 5, true);
  MountCallback callback1;
  MountCallback callback2;
  MountCallback callback3;
  EXPECT_CALL(*mock_disk_mount_manager_, UnmountPath("/mount/disk1", _))
      .WillOnce(MoveArg<1>(&callback1));
  EXPECT_CALL(*mock_disk_mount_manager_, UnmountPath("/mount/disk2", _))
      .WillOnce(MoveArg<1>(&callback2));
  EXPECT_CALL(*mock_disk_mount_manager_, UnmountPath("/mount/disk3", _))
      .WillOnce(MoveArg<1>(&callback3));

  // Unmount events would normally be fired by the DiskMountManager.
  AttachDeviceToGuest(guest_os::GuestId("VM1", ""), GetSingleDeviceInfo().guid,
                      /*vm_success=*/false);
  NotifyMountEvent("disk1", disks::DiskMountManager::UNMOUNTING);
  std::move(callback1).Run(MountError::kSuccess);
  std::move(callback2).Run(MountError::kUnknownError);
  NotifyMountEvent("disk3", disks::DiskMountManager::UNMOUNTING);
  std::move(callback3).Run(MountError::kSuccess);
  base::RunLoop().RunUntilIdle();

  // AttachDeviceToGuest() verifies CrosUsbDetector correctly calls the
  // completion callback, so there's not much to check here.
  EXPECT_EQ(fake_concierge_client_->attach_usb_device_call_count(), 0);
}

TEST_F(CrosUsbDetectorTest, ReassignPromptForSharedDevice) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  device_manager_.CreateAndAddDevice(0x1234, 0x5678);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(GetSingleDeviceInfo().prompt_before_sharing);
  auto guid = GetSingleDeviceInfo().guid;

  AttachDeviceToGuest(guest_os::GuestId("VM1", ""), guid);
  EXPECT_TRUE(GetSingleDeviceInfo().prompt_before_sharing);

  DetachDeviceFromVm("VM1", guid, /*expected_success=*/true);
  EXPECT_FALSE(GetSingleDeviceInfo().prompt_before_sharing);
}

TEST_F(CrosUsbDetectorTest, ReassignPromptForStorageDevice) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  // Disks mounted before the usb device is detected by the CrosUsbDetector
  // require a prompt.
  AddDisk("disk_early", 1, 5, true);

  device_manager_.CreateAndAddDevice(
      0x0200, 0xff, 0xff, 0xff, 0x0100, 1, 2, /*bus_number=*/1,
      /*port_number=*/5, kManufacturerName, kProductName_1, "5");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(GetSingleDeviceInfo().prompt_before_sharing);

  NotifyMountEvent("disk_early", disks::DiskMountManager::UNMOUNTING);
  EXPECT_FALSE(GetSingleDeviceInfo().prompt_before_sharing);

  // A disk which fails to mount shouldn't cause the prompt to be shown.
  AddDisk("disk_error", 1, 5, /*mounted=*/false);
  NotifyMountEvent("disk_error", disks::DiskMountManager::MOUNTING,
                   MountError::kInternalError);
  EXPECT_FALSE(GetSingleDeviceInfo().prompt_before_sharing);

  AddDisk("disk_success", 1, 5, true);
  EXPECT_TRUE(GetSingleDeviceInfo().prompt_before_sharing);
}

TEST_F(CrosUsbDetectorTest, AttachDeviceToVmWithContainer) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, kProductName_1, "002");
  device_manager_.AddDevice(device_1);
  base::RunLoop().RunUntilIdle();

  auto device_info = GetSingleDeviceInfo();
  EXPECT_FALSE(GetSingleGuestPort().has_value());
  AttachDeviceToGuest(
      guest_os::GuestId(crostini::kCrostiniDefaultVmName,
                        crostini::kCrostiniDefaultContainerName),
      device_info.guid);

  device_info = GetSingleDeviceInfo();
  EXPECT_TRUE(device_info.shared_guest_id.has_value());
  EXPECT_EQ(crostini::kCrostiniDefaultVmName,
            device_info.shared_guest_id->vm_name);
  EXPECT_EQ(crostini::kCrostiniDefaultContainerName,
            device_info.shared_guest_id->container_name);
  EXPECT_EQ(0U, GetSingleGuestPort());
}

TEST_F(CrosUsbDetectorTest, ReattachDeviceToAnotherContainer) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, kProductName_1, "002");
  device_manager_.AddDevice(device_1);
  base::RunLoop().RunUntilIdle();

  auto device_info = GetSingleDeviceInfo();
  EXPECT_FALSE(device_info.shared_guest_id.has_value());

  AttachDeviceToGuest(
      guest_os::GuestId(crostini::kCrostiniDefaultVmName,
                        crostini::kCrostiniDefaultContainerName),
      device_info.guid);
  AttachDeviceToGuest(guest_os::GuestId(crostini::kCrostiniDefaultVmName,
                                        kCrostiniTestContainerName),
                      device_info.guid);
  device_info = GetSingleDeviceInfo();
  EXPECT_TRUE(device_info.shared_guest_id.has_value());
  EXPECT_EQ(crostini::kCrostiniDefaultVmName,
            device_info.shared_guest_id->vm_name);
  EXPECT_EQ(kCrostiniTestContainerName,
            device_info.shared_guest_id->container_name);
}

TEST_F(CrosUsbDetectorTest, AttachDeviceOnContainerStartup) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, kProductName_1, "002");
  device_manager_.AddDevice(device_1);
  base::RunLoop().RunUntilIdle();

  auto device_info = GetSingleDeviceInfo();
  // Attaching to container fails, as if the container is not started yet.
  // But the VM attach should succeed and result in a valid guest_port.
  AttachDeviceToGuest(
      guest_os::GuestId(crostini::kCrostiniDefaultVmName,
                        crostini::kCrostiniDefaultContainerName),
      device_info.guid, /*vm_success=*/true,
      /*container_success=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetSingleGuestPort().has_value());

  vm_tools::cicerone::AttachUsbToContainerResponse response;
  response.set_status(
      vm_tools::cicerone::AttachUsbToContainerResponse_Status_OK);
  fake_cicerone_client_->set_attach_usb_to_container_response(
      std::move(response));

  // cicerone::ContainerStartedSignal triggers container attachment.
  cros_usb_detector_->AddUsbDeviceObserver(&usb_device_observer_);
  vm_tools::cicerone::ContainerStartedSignal container_started_signal;
  container_started_signal.set_vm_name(crostini::kCrostiniDefaultVmName);
  container_started_signal.set_container_name(
      crostini::kCrostiniDefaultContainerName);
  fake_cicerone_client_->NotifyContainerStarted(
      std::move(container_started_signal));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, usb_device_observer_.notify_count());
  device_info = GetSingleDeviceInfo();
  EXPECT_EQ(crostini::kCrostiniDefaultVmName,
            device_info.shared_guest_id->vm_name);
  EXPECT_EQ(crostini::kCrostiniDefaultContainerName,
            device_info.shared_guest_id->container_name);
}

TEST_F(CrosUsbDetectorTest, DetachDeviceOnContainerDeleted) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, kManufacturerName, kProductName_1, "002");
  device_manager_.AddDevice(device_1);
  base::RunLoop().RunUntilIdle();

  auto device_info = GetSingleDeviceInfo();
  // Attaching to container fails, as if the container is not started yet.
  // But the VM attach should succeed and result in a valid guest_port.
  AttachDeviceToGuest(
      guest_os::GuestId(crostini::kCrostiniDefaultVmName,
                        crostini::kCrostiniDefaultContainerName),
      device_info.guid);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetSingleGuestPort().has_value());

  // cicerone::LxdContainerDeletedSignal triggers detachment.
  cros_usb_detector_->AddUsbDeviceObserver(&usb_device_observer_);
  vm_tools::cicerone::LxdContainerDeletedSignal container_deleted_signal;
  container_deleted_signal.set_status(
      vm_tools::cicerone::LxdContainerDeletedSignal_Status_DELETED);
  container_deleted_signal.set_vm_name(crostini::kCrostiniDefaultVmName);
  container_deleted_signal.set_container_name(
      crostini::kCrostiniDefaultContainerName);
  fake_cicerone_client_->NotifyLxdContainerDeleted(
      std::move(container_deleted_signal));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, usb_device_observer_.notify_count());
  EXPECT_FALSE(GetSingleGuestPort().has_value());
  device_info = GetSingleDeviceInfo();
  EXPECT_FALSE(device_info.shared_guest_id.has_value());
}

}  // namespace ash
