// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/operation.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/browser/extensions/api/image_writer_private/write_from_file_operation.h"
#include "chrome/test/base/testing_profile.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions::image_writer {
namespace {

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;

class FakeBotUsbDeviceManager : public device::mojom::UsbDeviceManager {
 public:
  FakeBotUsbDeviceManager() = default;
  ~FakeBotUsbDeviceManager() override = default;

  void BindReceiver(
      mojo::PendingReceiver<device::mojom::UsbDeviceManager> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  void SetInterfaceClassCode(uint8_t class_code) {
    interface_class_code_ = class_code;
  }

  device::mojom::UsbDeviceInfoPtr MakeDeviceInfo() const {
    auto info = device::mojom::UsbDeviceInfo::New();
    info->guid = "guid-usb-flash";
    info->vendor_id = 0x0781;
    info->product_id = 0x5581;
    info->manufacturer_name = u"SanDisk";
    info->product_name = u"Ultra";
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
    alternate->class_code = interface_class_code_;
    alternate->subclass_code = 0x06;
    alternate->protocol_code = 0x50;
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

  void EnumerateDevicesAndSetClient(
      mojo::PendingAssociatedRemote<device::mojom::UsbDeviceManagerClient>
          client,
      EnumerateDevicesAndSetClientCallback callback) override {
    GetDevices(nullptr, std::move(callback));
  }

  void GetDevices(device::mojom::UsbEnumerationOptionsPtr options,
                  GetDevicesCallback callback) override {
    std::vector<device::mojom::UsbDeviceInfoPtr> list;
    list.push_back(MakeDeviceInfo());
    std::move(callback).Run(std::move(list));
  }

  void GetDevice(
      const std::string& guid,
      const std::vector<uint8_t>& blocked_interface_classes,
      mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
      mojo::PendingRemote<device::mojom::UsbDeviceClient> device_client)
      override {
    GetUnrestrictedDevice(guid, blocked_interface_classes,
                          std::move(device_receiver), std::move(device_client));
  }

  void GetUnrestrictedDevice(
      const std::string& guid,
      const std::vector<uint8_t>& blocked_interface_classes,
      mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
      mojo::PendingRemote<device::mojom::UsbDeviceClient> device_client)
      override {
    last_unrestricted_device_guid_ = guid;
  }

  void GetSecurityKeyDevice(
      const std::string& guid,
      mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
      mojo::PendingRemote<device::mojom::UsbDeviceClient> device_client)
      override {}

  void RefreshDeviceInfo(const std::string& guid,
                         RefreshDeviceInfoCallback callback) override {
    if (guid != "guid-usb-flash") {
      std::move(callback).Run(nullptr);
      return;
    }
    std::move(callback).Run(MakeDeviceInfo());
  }

  void SetClient(
      mojo::PendingAssociatedRemote<device::mojom::UsbDeviceManagerClient>
          client) override {}

  const std::string& last_unrestricted_device_guid() const {
    return last_unrestricted_device_guid_;
  }

 private:
  mojo::ReceiverSet<device::mojom::UsbDeviceManager> receivers_;
  uint8_t interface_class_code_ = 0x08;
  std::string last_unrestricted_device_guid_;
};

class OperationAndroidUsbTest : public ImageWriterUnitTestBase {
 protected:
  OperationAndroidUsbTest()
      : profile_(std::make_unique<TestingProfile>()),
        manager_(profile_.get()) {}

  std::unique_ptr<TestingProfile> profile_;
  MockOperationManager manager_;
  FakeBotUsbDeviceManager usb_manager_;
};

TEST_F(OperationAndroidUsbTest, ConnectToUsbDeviceBindsUnrestrictedDevice) {
  auto operation = base::MakeRefCounted<WriteFromFileOperation>(
      manager_.AsWeakPtr(), kDummyExtensionId, test_utils_.GetImagePath(),
      "usb:guid-usb-flash:0781:5581",
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));
  operation->SetUsbDeviceManagerBinderForTesting(
      base::BindRepeating(&FakeBotUsbDeviceManager::BindReceiver,
                          base::Unretained(&usb_manager_)));

  base::RunLoop run_loop;
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kWrite, _))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(0);
  // Fails closed with `kDeviceWriteError` after binding the unrestricted USB
  // device until `MojoUsbBotSession` is wired up in (4b/6).
  EXPECT_CALL(manager_,
              OnError(kDummyExtensionId, image_writer_api::Stage::kWrite, _,
                      error::kDeviceWriteError))
      .WillOnce([&]() { run_loop.Quit(); });

  operation->PostTask(base::BindOnce(&Operation::Start, operation));
  run_loop.Run();

  EXPECT_EQ(usb_manager_.last_unrestricted_device_guid(), "guid-usb-flash");
}

TEST_F(OperationAndroidUsbTest, MissingUsbDeviceReportsDeviceOpenError) {
  auto operation = base::MakeRefCounted<WriteFromFileOperation>(
      manager_.AsWeakPtr(), kDummyExtensionId, test_utils_.GetImagePath(),
      "usb:missing-guid:0781:5581",
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));
  operation->SetUsbDeviceManagerBinderForTesting(
      base::BindRepeating(&FakeBotUsbDeviceManager::BindReceiver,
                          base::Unretained(&usb_manager_)));

  base::RunLoop run_loop;
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kWrite, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(0);
  EXPECT_CALL(manager_,
              OnError(kDummyExtensionId, image_writer_api::Stage::kWrite, _,
                      error::kDeviceOpenError))
      .WillOnce([&]() { run_loop.Quit(); });

  operation->PostTask(base::BindOnce(&Operation::Start, operation));
  run_loop.Run();

  EXPECT_TRUE(usb_manager_.last_unrestricted_device_guid().empty());
}

TEST_F(OperationAndroidUsbTest,
       MismatchedVendorOrProductIdReportsDeviceOpenError) {
  auto operation = base::MakeRefCounted<WriteFromFileOperation>(
      manager_.AsWeakPtr(), kDummyExtensionId, test_utils_.GetImagePath(),
      "usb:guid-usb-flash:1234:5678",
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));
  operation->SetUsbDeviceManagerBinderForTesting(
      base::BindRepeating(&FakeBotUsbDeviceManager::BindReceiver,
                          base::Unretained(&usb_manager_)));

  base::RunLoop run_loop;
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kWrite, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(0);
  EXPECT_CALL(manager_,
              OnError(kDummyExtensionId, image_writer_api::Stage::kWrite, _,
                      error::kDeviceOpenError))
      .WillOnce([&]() { run_loop.Quit(); });

  operation->PostTask(base::BindOnce(&Operation::Start, operation));
  run_loop.Run();

  EXPECT_TRUE(usb_manager_.last_unrestricted_device_guid().empty());
}

TEST_F(OperationAndroidUsbTest,
       DeviceWithoutMassStorageInterfaceReportsDeviceOpenError) {
  usb_manager_.SetInterfaceClassCode(0x03);  // HID class, not Mass Storage.
  auto operation = base::MakeRefCounted<WriteFromFileOperation>(
      manager_.AsWeakPtr(), kDummyExtensionId, test_utils_.GetImagePath(),
      "usb:guid-usb-flash:0781:5581",
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));
  operation->SetUsbDeviceManagerBinderForTesting(
      base::BindRepeating(&FakeBotUsbDeviceManager::BindReceiver,
                          base::Unretained(&usb_manager_)));

  base::RunLoop run_loop;
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kWrite, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(0);
  EXPECT_CALL(manager_,
              OnError(kDummyExtensionId, image_writer_api::Stage::kWrite, _,
                      error::kDeviceOpenError))
      .WillOnce([&]() { run_loop.Quit(); });

  operation->PostTask(base::BindOnce(&Operation::Start, operation));
  run_loop.Run();

  EXPECT_TRUE(usb_manager_.last_unrestricted_device_guid().empty());
}

}  // namespace
}  // namespace extensions::image_writer
