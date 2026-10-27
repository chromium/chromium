// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider_android.h"

#include <stdint.h>

#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions::image_writer {
namespace {

device::mojom::UsbDeviceInfoPtr CreateUsbDeviceInfo(
    const std::string& guid,
    uint16_t vendor_id,
    uint16_t product_id,
    const std::string& manufacturer,
    const std::string& product,
    uint8_t class_code,
    uint8_t subclass_code,
    uint8_t protocol_code,
    const std::string& serial_number = "SN0001") {
  auto info = device::mojom::UsbDeviceInfo::New();
  info->guid = guid;
  info->vendor_id = vendor_id;
  info->product_id = product_id;
  info->manufacturer_name = base::UTF8ToUTF16(manufacturer);
  info->product_name = base::UTF8ToUTF16(product);
  info->serial_number = base::UTF8ToUTF16(serial_number);
  info->active_configuration = 1;

  auto endpoint_in = device::mojom::UsbEndpointInfo::New();
  endpoint_in->endpoint_number = 1;
  endpoint_in->direction = device::mojom::UsbTransferDirection::INBOUND;
  endpoint_in->type = device::mojom::UsbTransferType::BULK;
  endpoint_in->packet_size = 512;

  auto endpoint_out = device::mojom::UsbEndpointInfo::New();
  endpoint_out->endpoint_number = 2;
  endpoint_out->direction = device::mojom::UsbTransferDirection::OUTBOUND;
  endpoint_out->type = device::mojom::UsbTransferType::BULK;
  endpoint_out->packet_size = 512;

  auto alternate = device::mojom::UsbAlternateInterfaceInfo::New();
  alternate->alternate_setting = 0;
  alternate->class_code = class_code;
  alternate->subclass_code = subclass_code;
  alternate->protocol_code = protocol_code;
  alternate->endpoints.push_back(std::move(endpoint_in));
  alternate->endpoints.push_back(std::move(endpoint_out));

  auto interface_info = device::mojom::UsbInterfaceInfo::New();
  interface_info->interface_number = 0;
  interface_info->first_interface = 0;
  interface_info->alternates.push_back(std::move(alternate));

  auto config = device::mojom::UsbConfigurationInfo::New();
  config->configuration_value = 1;
  config->interfaces.push_back(std::move(interface_info));

  info->configurations.push_back(std::move(config));
  return info;
}

class TestUsbDeviceManager : public device::mojom::UsbDeviceManager {
 public:
  TestUsbDeviceManager() = default;
  ~TestUsbDeviceManager() override = default;

  mojo::PendingRemote<device::mojom::UsbDeviceManager>
  BindNewPipeAndPassRemote() {
    mojo::PendingRemote<device::mojom::UsbDeviceManager> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void AddDevice(device::mojom::UsbDeviceInfoPtr info, bool grant_permission) {
    std::string guid = info->guid;
    devices_[guid] = std::move(info);
    permissions_[guid] = grant_permission;
  }

  void EnumerateDevicesAndSetClient(
      mojo::PendingAssociatedRemote<device::mojom::UsbDeviceManagerClient>
          client,
      EnumerateDevicesAndSetClientCallback callback) override {
    GetDevices(/*options=*/nullptr, std::move(callback));
  }

  void GetDevices(device::mojom::UsbEnumerationOptionsPtr options,
                  GetDevicesCallback callback) override {
    std::vector<device::mojom::UsbDeviceInfoPtr> list;
    for (const auto& [guid, info] : devices_) {
      list.push_back(info.Clone());
    }
    std::move(callback).Run(std::move(list));
  }

  void GetDevice(
      const std::string& guid,
      const std::vector<uint8_t>& blocked_interface_classes,
      mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
      mojo::PendingRemote<device::mojom::UsbDeviceClient> device_client)
      override {}

  void GetUnrestrictedDevice(
      const std::string& guid,
      const std::vector<uint8_t>& blocked_interface_classes,
      mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
      mojo::PendingRemote<device::mojom::UsbDeviceClient> device_client)
      override {}

  void GetSecurityKeyDevice(
      const std::string& guid,
      mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
      mojo::PendingRemote<device::mojom::UsbDeviceClient> device_client)
      override {}

  void RefreshDeviceInfo(const std::string& guid,
                         RefreshDeviceInfoCallback callback) override {
    if (!devices_.contains(guid) || !permissions_[guid]) {
      std::move(callback).Run(nullptr);
      return;
    }
    std::move(callback).Run(devices_[guid].Clone());
  }

  void SetClient(
      mojo::PendingAssociatedRemote<device::mojom::UsbDeviceManagerClient>
          client) override {}

 private:
  mojo::ReceiverSet<device::mojom::UsbDeviceManager> receivers_;
  std::map<std::string, device::mojom::UsbDeviceInfoPtr> devices_;
  std::map<std::string, bool> permissions_;
};

class RemovableStorageProviderAndroidTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    sys_block_dir_ = temp_dir_.GetPath().Append("sys/block");
    dev_block_dir_ = temp_dir_.GetPath().Append("dev/block");
    dev_dir_ = temp_dir_.GetPath().Append("dev");
    ASSERT_TRUE(base::CreateDirectory(sys_block_dir_));
    ASSERT_TRUE(base::CreateDirectory(dev_block_dir_));
    ASSERT_TRUE(base::CreateDirectory(dev_dir_));
  }

  scoped_refptr<StorageDeviceList> EnumerateDevices() {
    base::test::TestFuture<scoped_refptr<StorageDeviceList>> future;
    GetAllDevicesOnAndroidForTesting(usb_manager_.BindNewPipeAndPassRemote(),
                                     sys_block_dir_, dev_block_dir_, dev_dir_,
                                     future.GetCallback());
    return future.Get();
  }

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath sys_block_dir_;
  base::FilePath dev_block_dir_;
  base::FilePath dev_dir_;
  TestUsbDeviceManager usb_manager_;
};

TEST_F(RemovableStorageProviderAndroidTest,
       ExtractMassStorageEndpointsMatchesBotInterface) {
  auto valid_bot =
      CreateUsbDeviceInfo("guid-bot", 0x0781, 0x5581, "SanDisk", "Ultra",
                          /*class_code=*/0x08, /*subclass_code=*/0x06,
                          /*protocol_code=*/0x50);
  std::optional<UsbMassStorageEndpoints> endpoints =
      ExtractMassStorageEndpoints(*valid_bot);
  ASSERT_TRUE(endpoints.has_value());
  EXPECT_EQ(endpoints->interface_number, 0u);
  EXPECT_EQ(endpoints->endpoint_in, 1u);
  EXPECT_EQ(endpoints->endpoint_out, 2u);

  auto non_bot =
      CreateUsbDeviceInfo("guid-hid", 0x046d, 0xc077, "Logitech", "Mouse",
                          /*class_code=*/0x03, /*subclass_code=*/0x01,
                          /*protocol_code=*/0x02);
  EXPECT_FALSE(ExtractMassStorageEndpoints(*non_bot).has_value());
}

TEST_F(RemovableStorageProviderAndroidTest,
       EnumeratesPermittedUsbMassStorageDevices) {
  // 1. Permitted USB Mass Storage Bulk-Only Transport flash drive.
  usb_manager_.AddDevice(
      CreateUsbDeviceInfo("guid-sandisk", 0x0781, 0x5581, "SanDisk", "Ultra",
                          /*class_code=*/0x08, /*subclass_code=*/0x06,
                          /*protocol_code=*/0x50),
      /*grant_permission=*/true);

  // 2. USB Mass Storage device where Android USB permission was denied.
  usb_manager_.AddDevice(
      CreateUsbDeviceInfo("guid-denied", 0x0951, 0x1666, "Kingston",
                          "DataTraveler", 0x08, 0x06, 0x50),
      /*grant_permission=*/false);

  // 3. Non-storage USB device (HID Mouse).
  usb_manager_.AddDevice(
      CreateUsbDeviceInfo("guid-mouse", 0x046d, 0xc077, "Logitech", "Mouse",
                          /*class_code=*/0x03, /*subclass_code=*/0x01,
                          /*protocol_code=*/0x02),
      /*grant_permission=*/true);

  scoped_refptr<StorageDeviceList> devices = EnumerateDevices();
  ASSERT_TRUE(devices);
  ASSERT_EQ(devices->data.size(), 1u);

  EXPECT_EQ(devices->data[0].storage_unit_id, "usb:guid-sandisk:0781:5581");
  EXPECT_EQ(devices->data[0].vendor, "SanDisk");
  EXPECT_EQ(devices->data[0].model, "Ultra");
  EXPECT_TRUE(devices->data[0].removable);
}

// Verifies sysfs block device enumeration, USB ancestor traversal, ignored
// virtual block prefixes, and deduplication against UsbDeviceManager entries.
TEST_F(RemovableStorageProviderAndroidTest,
       EnumeratesSysfsBlockDevicesAndDeduplicatesUsb) {
  // Create a fake USB device hierarchy under `<temp>/sys/devices/usb1/1-1`.
  base::FilePath usb_ancestor =
      temp_dir_.GetPath().Append("sys/devices/usb1/1-1");
  base::FilePath scsi_dev = usb_ancestor.Append("host0/target0/block_dev");
  ASSERT_TRUE(base::CreateDirectory(scsi_dev));
  ASSERT_TRUE(base::WriteFile(usb_ancestor.Append("idVendor"), "0781\n"));
  ASSERT_TRUE(base::WriteFile(usb_ancestor.Append("idProduct"), "5581\n"));
  ASSERT_TRUE(base::WriteFile(usb_ancestor.Append("serial"), "SN1111\n"));
  ASSERT_TRUE(
      base::WriteFile(usb_ancestor.Append("manufacturer"), "SanDisk\n"));
  ASSERT_TRUE(base::WriteFile(usb_ancestor.Append("product"), "Extreme\n"));

  // Create `/sys/block/sda` linked to `scsi_dev` with 16000 sectors of 512B.
  base::FilePath sda_sys = sys_block_dir_.Append("sda");
  ASSERT_TRUE(base::CreateDirectory(sda_sys.Append("queue")));
  ASSERT_TRUE(base::CreateSymbolicLink(scsi_dev, sda_sys.Append("device")));
  ASSERT_TRUE(base::WriteFile(sda_sys.Append("removable"), "1\n"));
  ASSERT_TRUE(base::WriteFile(sda_sys.Append("ro"), "0\n"));
  ASSERT_TRUE(base::WriteFile(sda_sys.Append("size"), "16000\n"));
  ASSERT_TRUE(
      base::WriteFile(sda_sys.Append("queue/logical_block_size"), "512\n"));

  // Create a writable `/dev/block/sda` node.
  base::FilePath sda_dev = dev_block_dir_.Append("sda");
  ASSERT_TRUE(base::WriteFile(sda_dev, ""));

  // Create an ignored virtual block device (`/sys/block/loop0`).
  base::FilePath loop_sys = sys_block_dir_.Append("loop0");
  ASSERT_TRUE(base::CreateDirectory(loop_sys));
  ASSERT_TRUE(base::WriteFile(loop_sys.Append("removable"), "1\n"));
  ASSERT_TRUE(base::WriteFile(loop_sys.Append("ro"), "0\n"));
  ASSERT_TRUE(base::WriteFile(loop_sys.Append("size"), "8000\n"));
  ASSERT_TRUE(base::WriteFile(dev_block_dir_.Append("loop0"), ""));

  // Register two identical VID/PID USB devices with different serial numbers:
  // - SN1111 matches `/dev/block/sda` and should be deduplicated.
  // - SN2222 is a second drive of the same model and should still be listed.
  usb_manager_.AddDevice(
      CreateUsbDeviceInfo("guid-sda-dup", 0x0781, 0x5581, "SanDisk", "Extreme",
                          /*class_code=*/0x08, /*subclass_code=*/0x06,
                          /*protocol_code=*/0x50, "SN1111"),
      /*grant_permission=*/true);
  usb_manager_.AddDevice(
      CreateUsbDeviceInfo("guid-second-drive", 0x0781, 0x5581, "SanDisk",
                          "Extreme", /*class_code=*/0x08,
                          /*subclass_code=*/0x06, /*protocol_code=*/0x50,
                          "SN2222"),
      /*grant_permission=*/true);

  scoped_refptr<StorageDeviceList> devices = EnumerateDevices();
  ASSERT_TRUE(devices);
  ASSERT_EQ(devices->data.size(), 2u);

  EXPECT_EQ(devices->data[0].storage_unit_id, sda_dev.value());
  EXPECT_DOUBLE_EQ(devices->data[0].capacity, 16000.0 * 512.0);
  EXPECT_EQ(devices->data[0].vendor, "SanDisk");
  EXPECT_EQ(devices->data[0].model, "Extreme");

  EXPECT_EQ(devices->data[1].storage_unit_id,
            "usb:guid-second-drive:0781:5581");
}

TEST_F(RemovableStorageProviderAndroidTest,
       ReportsNullDeviceListWhenUsbManagerDisconnected) {
  mojo::PendingRemote<device::mojom::UsbDeviceManager> disconnected_manager;
  std::ignore = disconnected_manager.InitWithNewPipeAndPassReceiver();

  base::test::TestFuture<scoped_refptr<StorageDeviceList>> future;
  GetAllDevicesOnAndroidForTesting(std::move(disconnected_manager),
                                   sys_block_dir_, dev_block_dir_, dev_dir_,
                                   future.GetCallback());
  EXPECT_FALSE(future.Get());
}

}  // namespace
}  // namespace extensions::image_writer
