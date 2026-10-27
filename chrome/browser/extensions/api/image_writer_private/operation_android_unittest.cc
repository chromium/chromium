// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/operation.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/containers/span_writer.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/api/image_writer_private/destroy_partitions_operation.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider_android.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/browser/extensions/api/image_writer_private/write_from_file_operation.h"
#include "chrome/test/base/testing_profile.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
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
using testing::Contains;
using testing::Each;
using testing::ElementsAre;
using testing::Not;

constexpr uint32_t kBlockSize = 512;
constexpr uint32_t kNumBlocks = 128;

struct SimulatedUsbDiskState {
  std::vector<uint8_t> storage =
      std::vector<uint8_t>(kNumBlocks * kBlockSize, kDevicePattern);
  bool unit_attention_on_open = true;
  bool persistent_unit_attention = false;
  bool fail_cbw_transfer = false;
  bool fail_read_capacity = false;
  bool fail_read_10 = false;
  bool disconnect_on_release = false;
  int open_count = 0;
  std::optional<size_t> corrupt_byte_offset_on_second_open;
  std::vector<uint8_t> executed_opcodes;
  bool interface_released = false;
  bool device_closed = false;
};

// Simulated USB Mass Storage Bulk-Only Transport disk backed by an in-memory
// byte vector.
class InMemoryBotUsbDisk : public device::mojom::UsbDevice {
 public:
  explicit InMemoryBotUsbDisk(SimulatedUsbDiskState* state)
      : state_(state), unit_attention_(state->unit_attention_on_open) {}

  ~InMemoryBotUsbDisk() override = default;

  void set_self_receiver(
      mojo::SelfOwnedReceiverRef<device::mojom::UsbDevice> self_receiver) {
    self_receiver_ = std::move(self_receiver);
  }

  void Open(OpenCallback callback) override {
    std::move(callback).Run(device::mojom::UsbOpenDeviceResult::NewSuccess(
        device::mojom::UsbOpenDeviceSuccess::OK));
  }
  void Close(CloseCallback callback) override {
    state_->device_closed = true;
    std::move(callback).Run();
  }
  void SetConfiguration(uint8_t value,
                        SetConfigurationCallback callback) override {
    std::move(callback).Run(true);
  }
  void ClaimInterface(uint8_t interface_number,
                      ClaimInterfaceCallback callback) override {
    std::move(callback).Run(device::mojom::UsbClaimInterfaceResult::kSuccess);
  }
  void ReleaseInterface(uint8_t interface_number,
                        ReleaseInterfaceCallback callback) override {
    state_->interface_released = true;
    if (state_->disconnect_on_release && self_receiver_) {
      self_receiver_->Close();
      return;
    }
    std::move(callback).Run(true);
  }
  void SetInterfaceAlternateSetting(
      uint8_t interface_number,
      uint8_t alternate_setting,
      SetInterfaceAlternateSettingCallback callback) override {
    std::move(callback).Run(true);
  }
  void Reset(ResetCallback callback) override { std::move(callback).Run(true); }
  void ClearHalt(device::mojom::UsbTransferDirection direction,
                 uint8_t endpoint_number,
                 ClearHaltCallback callback) override {
    std::move(callback).Run(true);
  }
  void ControlTransferIn(device::mojom::UsbControlTransferParamsPtr params,
                         uint32_t length,
                         uint32_t timeout,
                         ControlTransferInCallback callback) override {
    std::move(callback).Run(device::mojom::UsbTransferStatus::TRANSFER_ERROR,
                            {});
  }
  void ControlTransferOut(device::mojom::UsbControlTransferParamsPtr params,
                          base::span<const uint8_t> data,
                          uint32_t timeout,
                          ControlTransferOutCallback callback) override {
    std::move(callback).Run(device::mojom::UsbTransferStatus::TRANSFER_ERROR);
  }

  void GenericTransferOut(uint8_t endpoint_number,
                          base::span<const uint8_t> data,
                          uint32_t timeout,
                          GenericTransferOutCallback callback) override {
    if (endpoint_number != 2 || state_->fail_cbw_transfer) {
      std::move(callback).Run(device::mojom::UsbTransferStatus::TRANSFER_ERROR);
      return;
    }
    if (!expecting_write_payload_ && data.size() == kCommandBlockWrapperSize) {
      base::SpanReader reader(data);
      uint32_t signature = 0;
      uint32_t tag = 0;
      uint32_t data_transfer_length = 0;
      uint8_t flags = 0;
      uint8_t lun = 0;
      uint8_t command_length = 0;
      uint8_t opcode = 0;
      if (reader.ReadU32LittleEndian(signature) &&
          signature == kCommandBlockWrapperSignature &&
          reader.ReadU32LittleEndian(tag) &&
          reader.ReadU32LittleEndian(data_transfer_length) &&
          reader.ReadU8LittleEndian(flags) && reader.ReadU8LittleEndian(lun) &&
          lun == kLogicalUnitNumber &&
          reader.ReadU8LittleEndian(command_length) && command_length >= 6 &&
          command_length <= kMaxScsiCommandBlockSize &&
          reader.ReadU8LittleEndian(opcode)) {
        current_tag_ = tag;
        current_opcode_ = opcode;
        state_->executed_opcodes.push_back(opcode);
        if (current_opcode_ == kScsiWrite10Opcode ||
            current_opcode_ == kScsiRead10Opcode) {
          uint32_t logical_block_address = 0;
          uint16_t num_blocks = 0;
          if (reader.Skip(1u) &&
              reader.ReadU32BigEndian(logical_block_address) &&
              reader.Skip(1u) && reader.ReadU16BigEndian(num_blocks)) {
            current_logical_block_address_ = logical_block_address;
            current_blocks_ = num_blocks;
          }
          if (current_opcode_ == kScsiWrite10Opcode &&
              data_transfer_length > 0) {
            expecting_write_payload_ = true;
          }
        }
        std::move(callback).Run(device::mojom::UsbTransferStatus::COMPLETED);
        return;
      }
    }

    if (expecting_write_payload_) {
      expecting_write_payload_ = false;
      const size_t offset =
          static_cast<size_t>(current_logical_block_address_) * kBlockSize;
      const size_t length = static_cast<size_t>(current_blocks_) * kBlockSize;
      if (offset + length <= state_->storage.size() && data.size() >= length) {
        base::span(state_->storage)
            .subspan(offset, length)
            .copy_from(data.first(length));
        next_status_ = 0;
      } else {
        next_status_ = 1;
      }
      std::move(callback).Run(device::mojom::UsbTransferStatus::COMPLETED);
      return;
    }

    std::move(callback).Run(device::mojom::UsbTransferStatus::TRANSFER_ERROR);
  }

  void GenericTransferIn(uint8_t endpoint_number,
                         uint32_t length,
                         uint32_t timeout,
                         GenericTransferInCallback callback) override {
    if (endpoint_number != 1) {
      std::move(callback).Run(device::mojom::UsbTransferStatus::TRANSFER_ERROR,
                              {});
      return;
    }
    if (length == kCommandStatusWrapperSize) {
      uint8_t status = next_status_;
      next_status_ = 0;
      if (current_opcode_ == kScsiTestUnitReadyOpcode && unit_attention_) {
        status = 1;
      }
      std::vector<uint8_t> status_bytes(kCommandStatusWrapperSize, 0);
      base::SpanWriter<uint8_t> writer(status_bytes);
      writer.WriteU32LittleEndian(kCommandStatusWrapperSignature);
      writer.WriteU32LittleEndian(current_tag_);
      writer.WriteU32LittleEndian(0u);
      writer.WriteU8LittleEndian(status);
      std::move(callback).Run(device::mojom::UsbTransferStatus::COMPLETED,
                              std::move(status_bytes));
      return;
    }

    if (current_opcode_ == kScsiRequestSenseOpcode &&
        length == kScsiRequestSenseAllocationLength) {
      if (!state_->persistent_unit_attention) {
        unit_attention_ = false;
      }
      std::vector<uint8_t> sense(kScsiRequestSenseAllocationLength, 0);
      sense[0] = 0x70;
      sense[2] = kScsiSenseKeyUnitAttention;
      std::move(callback).Run(device::mojom::UsbTransferStatus::COMPLETED,
                              std::move(sense));
      return;
    }

    if (current_opcode_ == kScsiReadCapacity10Opcode &&
        length == kScsiReadCapacity10ResponseLength) {
      if (state_->fail_read_capacity) {
        std::move(callback).Run(
            device::mojom::UsbTransferStatus::TRANSFER_ERROR, {});
        return;
      }
      std::vector<uint8_t> capacity_response(kScsiReadCapacity10ResponseLength,
                                             0);
      base::SpanWriter<uint8_t> writer(capacity_response);
      writer.WriteU32BigEndian(kNumBlocks - 1);
      writer.WriteU32BigEndian(kBlockSize);
      std::move(callback).Run(device::mojom::UsbTransferStatus::COMPLETED,
                              std::move(capacity_response));
      return;
    }

    if (current_opcode_ == kScsiRead10Opcode) {
      if (state_->fail_read_10) {
        std::move(callback).Run(
            device::mojom::UsbTransferStatus::TRANSFER_ERROR, {});
        return;
      }
      const size_t offset =
          static_cast<size_t>(current_logical_block_address_) * kBlockSize;
      const size_t read_length =
          static_cast<size_t>(current_blocks_) * kBlockSize;
      if (offset + read_length <= state_->storage.size()) {
        auto slice = base::span(state_->storage).subspan(offset, read_length);
        std::vector<uint8_t> out(slice.begin(), slice.end());
        std::move(callback).Run(device::mojom::UsbTransferStatus::COMPLETED,
                                std::move(out));
        return;
      }
    }

    std::move(callback).Run(device::mojom::UsbTransferStatus::TRANSFER_ERROR,
                            {});
  }

  void IsochronousTransferIn(uint8_t endpoint_number,
                             const std::vector<uint32_t>& packet_lengths,
                             uint32_t timeout,
                             IsochronousTransferInCallback callback) override {
    std::move(callback).Run({}, {});
  }
  void IsochronousTransferOut(
      uint8_t endpoint_number,
      base::span<const uint8_t> data,
      const std::vector<uint32_t>& packet_lengths,
      uint32_t timeout,
      IsochronousTransferOutCallback callback) override {
    std::move(callback).Run({});
  }

 private:
  raw_ptr<SimulatedUsbDiskState> state_;
  mojo::SelfOwnedReceiverRef<device::mojom::UsbDevice> self_receiver_;
  bool unit_attention_;
  bool expecting_write_payload_ = false;
  uint32_t current_tag_ = 0;
  uint8_t current_opcode_ = 0;
  uint32_t current_logical_block_address_ = 0;
  uint16_t current_blocks_ = 0;
  uint8_t next_status_ = 0;
};

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
    if (guid != "guid-usb-flash") {
      return;
    }
    last_unrestricted_device_guid_ = guid;
    ++disk_state_.open_count;
    if (disk_state_.open_count == 2 &&
        disk_state_.corrupt_byte_offset_on_second_open.has_value()) {
      disk_state_.storage[*disk_state_.corrupt_byte_offset_on_second_open] ^=
          0xFF;
    }
    auto disk = std::make_unique<InMemoryBotUsbDisk>(&disk_state_);
    InMemoryBotUsbDisk* raw_disk = disk.get();
    auto ref = mojo::MakeSelfOwnedReceiver(std::move(disk),
                                           std::move(device_receiver));
    raw_disk->set_self_receiver(std::move(ref));
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
  SimulatedUsbDiskState& disk_state() { return disk_state_; }

 private:
  mojo::ReceiverSet<device::mojom::UsbDeviceManager> receivers_;
  uint8_t interface_class_code_ = 0x08;
  std::string last_unrestricted_device_guid_;
  SimulatedUsbDiskState disk_state_;
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

TEST_F(OperationAndroidUsbTest, WriteFromFileOverUsbBotSucceeds) {
  auto operation = base::MakeRefCounted<WriteFromFileOperation>(
      manager_.AsWeakPtr(), kDummyExtensionId, test_utils_.GetImagePath(),
      "usb:guid-usb-flash:0781:5581",
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));
  operation->SetUsbDeviceManagerBinderForTesting(base::BindRepeating(
      &FakeBotUsbDeviceManager::BindReceiver, base::Unretained(&usb_manager_)));

  base::RunLoop run_loop;
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kWrite, _))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kVerifyWrite, _))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).WillOnce([&]() {
    run_loop.Quit();
  });
  EXPECT_CALL(manager_, OnError(kDummyExtensionId, _, _, _)).Times(0);

  operation->PostTask(base::BindOnce(&Operation::Start, operation));
  run_loop.Run();

  EXPECT_EQ(usb_manager_.last_unrestricted_device_guid(), "guid-usb-flash");
  EXPECT_THAT(usb_manager_.disk_state().executed_opcodes,
              ElementsAre(kScsiTestUnitReadyOpcode, kScsiRequestSenseOpcode,
                          kScsiTestUnitReadyOpcode, kScsiReadCapacity10Opcode,
                          kScsiWrite10Opcode, kScsiWrite10Opcode,
                          kScsiSynchronizeCache10Opcode,
                          kScsiTestUnitReadyOpcode, kScsiRequestSenseOpcode,
                          kScsiTestUnitReadyOpcode, kScsiReadCapacity10Opcode,
                          kScsiRead10Opcode, kScsiRead10Opcode));
  EXPECT_TRUE(usb_manager_.disk_state().interface_released);
  EXPECT_TRUE(usb_manager_.disk_state().device_closed);

  std::string expected_image;
  ASSERT_TRUE(
      base::ReadFileToString(test_utils_.GetImagePath(), &expected_image));
  ASSERT_LE(expected_image.size(), usb_manager_.disk_state().storage.size());
  EXPECT_EQ(base::span(usb_manager_.disk_state().storage)
                .first(expected_image.size()),
            base::as_byte_span(expected_image));
  EXPECT_THAT(base::span(usb_manager_.disk_state().storage)
                  .subspan(expected_image.size()),
              Each(kDevicePattern));
}

TEST_F(OperationAndroidUsbTest, WriteFromFileOverUsbBotDetectsVerifyMismatch) {
  usb_manager_.disk_state().corrupt_byte_offset_on_second_open = 0;
  auto operation = base::MakeRefCounted<WriteFromFileOperation>(
      manager_.AsWeakPtr(), kDummyExtensionId, test_utils_.GetImagePath(),
      "usb:guid-usb-flash:0781:5581",
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));
  operation->SetUsbDeviceManagerBinderForTesting(base::BindRepeating(
      &FakeBotUsbDeviceManager::BindReceiver, base::Unretained(&usb_manager_)));

  base::RunLoop run_loop;
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kWrite, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kVerifyWrite, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(0);
  EXPECT_CALL(manager_,
              OnError(kDummyExtensionId, image_writer_api::Stage::kVerifyWrite,
                      _, error::kVerificationFailed))
      .WillOnce([&]() { run_loop.Quit(); });

  operation->PostTask(base::BindOnce(&Operation::Start, operation));
  run_loop.Run();

  // Verification should stop immediately after the first mismatching READ (10)
  // chunk without reading the second chunk.
  EXPECT_THAT(
      usb_manager_.disk_state().executed_opcodes,
      ElementsAre(kScsiTestUnitReadyOpcode, kScsiRequestSenseOpcode,
                  kScsiTestUnitReadyOpcode, kScsiReadCapacity10Opcode,
                  kScsiWrite10Opcode, kScsiWrite10Opcode,
                  kScsiSynchronizeCache10Opcode, kScsiTestUnitReadyOpcode,
                  kScsiRequestSenseOpcode, kScsiTestUnitReadyOpcode,
                  kScsiReadCapacity10Opcode, kScsiRead10Opcode));
}

TEST_F(OperationAndroidUsbTest,
       WriteFromFileOverUsbBotDetectsLastChunkVerifyMismatch) {
  usb_manager_.disk_state().corrupt_byte_offset_on_second_open =
      kTestFileSize - 1;
  auto operation = base::MakeRefCounted<WriteFromFileOperation>(
      manager_.AsWeakPtr(), kDummyExtensionId, test_utils_.GetImagePath(),
      "usb:guid-usb-flash:0781:5581",
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));
  operation->SetUsbDeviceManagerBinderForTesting(base::BindRepeating(
      &FakeBotUsbDeviceManager::BindReceiver, base::Unretained(&usb_manager_)));

  base::RunLoop run_loop;
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kWrite, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kVerifyWrite, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(0);
  EXPECT_CALL(manager_,
              OnError(kDummyExtensionId, image_writer_api::Stage::kVerifyWrite,
                      _, error::kVerificationFailed))
      .WillOnce([&]() { run_loop.Quit(); });

  operation->PostTask(base::BindOnce(&Operation::Start, operation));
  run_loop.Run();

  EXPECT_THAT(usb_manager_.disk_state().executed_opcodes,
              ElementsAre(kScsiTestUnitReadyOpcode, kScsiRequestSenseOpcode,
                          kScsiTestUnitReadyOpcode, kScsiReadCapacity10Opcode,
                          kScsiWrite10Opcode, kScsiWrite10Opcode,
                          kScsiSynchronizeCache10Opcode,
                          kScsiTestUnitReadyOpcode, kScsiRequestSenseOpcode,
                          kScsiTestUnitReadyOpcode, kScsiReadCapacity10Opcode,
                          kScsiRead10Opcode, kScsiRead10Opcode));
}

TEST_F(OperationAndroidUsbTest,
       Read10TransferErrorDuringVerifyReportsHashReadError) {
  usb_manager_.disk_state().fail_read_10 = true;
  auto operation = base::MakeRefCounted<WriteFromFileOperation>(
      manager_.AsWeakPtr(), kDummyExtensionId, test_utils_.GetImagePath(),
      "usb:guid-usb-flash:0781:5581",
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));
  operation->SetUsbDeviceManagerBinderForTesting(base::BindRepeating(
      &FakeBotUsbDeviceManager::BindReceiver, base::Unretained(&usb_manager_)));

  base::RunLoop run_loop;
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kWrite, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kVerifyWrite, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(0);
  EXPECT_CALL(manager_,
              OnError(kDummyExtensionId, image_writer_api::Stage::kVerifyWrite,
                      _, error::kHashReadError))
      .WillOnce([&]() { run_loop.Quit(); });

  operation->PostTask(base::BindOnce(&Operation::Start, operation));
  run_loop.Run();
}

TEST_F(OperationAndroidUsbTest,
       PersistentUnitAttentionExhaustsRetriesAndReportsDeviceOpenError) {
  usb_manager_.disk_state().persistent_unit_attention = true;
  auto operation = base::MakeRefCounted<WriteFromFileOperation>(
      manager_.AsWeakPtr(), kDummyExtensionId, test_utils_.GetImagePath(),
      "usb:guid-usb-flash:0781:5581",
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));
  operation->SetUsbDeviceManagerBinderForTesting(base::BindRepeating(
      &FakeBotUsbDeviceManager::BindReceiver, base::Unretained(&usb_manager_)));

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

  EXPECT_THAT(usb_manager_.disk_state().executed_opcodes,
              ElementsAre(kScsiTestUnitReadyOpcode, kScsiRequestSenseOpcode,
                          kScsiTestUnitReadyOpcode, kScsiRequestSenseOpcode,
                          kScsiTestUnitReadyOpcode, kScsiRequestSenseOpcode));
  EXPECT_TRUE(usb_manager_.disk_state().interface_released);
  EXPECT_TRUE(usb_manager_.disk_state().device_closed);
}

TEST_F(OperationAndroidUsbTest,
       TransferErrorOnCommandBlockReportsDeviceOpenError) {
  usb_manager_.disk_state().fail_cbw_transfer = true;
  auto operation = base::MakeRefCounted<WriteFromFileOperation>(
      manager_.AsWeakPtr(), kDummyExtensionId, test_utils_.GetImagePath(),
      "usb:guid-usb-flash:0781:5581",
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));
  operation->SetUsbDeviceManagerBinderForTesting(base::BindRepeating(
      &FakeBotUsbDeviceManager::BindReceiver, base::Unretained(&usb_manager_)));

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

  EXPECT_TRUE(usb_manager_.disk_state().interface_released);
  EXPECT_TRUE(usb_manager_.disk_state().device_closed);
}

TEST_F(OperationAndroidUsbTest,
       DisconnectDuringReleaseAfterSuccessStillCompletes) {
  usb_manager_.disk_state().disconnect_on_release = true;
  auto operation = base::MakeRefCounted<WriteFromFileOperation>(
      manager_.AsWeakPtr(), kDummyExtensionId, test_utils_.GetImagePath(),
      "usb:guid-usb-flash:0781:5581",
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));
  operation->SetUsbDeviceManagerBinderForTesting(base::BindRepeating(
      &FakeBotUsbDeviceManager::BindReceiver, base::Unretained(&usb_manager_)));

  base::RunLoop run_loop;
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kWrite, _))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kVerifyWrite, _))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).WillOnce([&]() {
    run_loop.Quit();
  });
  EXPECT_CALL(manager_, OnError(kDummyExtensionId, _, _, _)).Times(0);

  operation->PostTask(base::BindOnce(&Operation::Start, operation));
  run_loop.Run();

  EXPECT_TRUE(usb_manager_.disk_state().interface_released);
}

TEST_F(OperationAndroidUsbTest, MissingUsbDeviceReportsDeviceOpenError) {
  auto operation = base::MakeRefCounted<WriteFromFileOperation>(
      manager_.AsWeakPtr(), kDummyExtensionId, test_utils_.GetImagePath(),
      "usb:missing-guid:0781:5581",
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));
  operation->SetUsbDeviceManagerBinderForTesting(base::BindRepeating(
      &FakeBotUsbDeviceManager::BindReceiver, base::Unretained(&usb_manager_)));

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
  operation->SetUsbDeviceManagerBinderForTesting(base::BindRepeating(
      &FakeBotUsbDeviceManager::BindReceiver, base::Unretained(&usb_manager_)));

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
  operation->SetUsbDeviceManagerBinderForTesting(base::BindRepeating(
      &FakeBotUsbDeviceManager::BindReceiver, base::Unretained(&usb_manager_)));

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
       NonBlockAlignedImageZeroPadsTrailingBlockAndLeavesRestUntouched) {
  constexpr size_t kUnalignedImageSize = 64 * kBlockSize + 100;
  constexpr size_t kAlignedImageSize = 65 * kBlockSize;
  ASSERT_TRUE(test_utils_.FillFile(test_utils_.GetImagePath(), kImagePattern,
                                   static_cast<int>(kUnalignedImageSize)));

  auto operation = base::MakeRefCounted<WriteFromFileOperation>(
      manager_.AsWeakPtr(), kDummyExtensionId, test_utils_.GetImagePath(),
      "usb:guid-usb-flash:0781:5581",
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));
  operation->SetUsbDeviceManagerBinderForTesting(base::BindRepeating(
      &FakeBotUsbDeviceManager::BindReceiver, base::Unretained(&usb_manager_)));

  base::RunLoop run_loop;
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kWrite, _))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kVerifyWrite, _))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).WillOnce([&]() {
    run_loop.Quit();
  });
  EXPECT_CALL(manager_, OnError(kDummyExtensionId, _, _, _)).Times(0);

  operation->PostTask(base::BindOnce(&Operation::Start, operation));
  run_loop.Run();

  base::span<const uint8_t> storage(usb_manager_.disk_state().storage);
  EXPECT_THAT(storage.first(kUnalignedImageSize), Each(kImagePattern));
  EXPECT_THAT(storage.subspan(kUnalignedImageSize,
                              kAlignedImageSize - kUnalignedImageSize),
              Each(0));
  EXPECT_THAT(storage.subspan(kAlignedImageSize), Each(kDevicePattern));
}

TEST_F(OperationAndroidUsbTest, DestroyPartitionsWipesPrimaryAndBackupGptOnly) {
  auto operation = base::MakeRefCounted<DestroyPartitionsOperation>(
      manager_.AsWeakPtr(), kDummyExtensionId, "usb:guid-usb-flash:0781:5581",
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));
  operation->SetUsbDeviceManagerBinderForTesting(base::BindRepeating(
      &FakeBotUsbDeviceManager::BindReceiver, base::Unretained(&usb_manager_)));

  base::RunLoop run_loop;
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId, _, _)).Times(AtLeast(1));
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).WillOnce([&]() {
    run_loop.Quit();
  });
  EXPECT_CALL(manager_, OnError(kDummyExtensionId, _, _, _)).Times(0);

  operation->PostTask(base::BindOnce(&Operation::Start, operation));
  run_loop.Run();

  // 33 trailing blocks = kBackupGptBlocks (1 GPT header block + 32 partition
  // entry blocks).
  constexpr size_t kBackupGptBytes = 33 * kBlockSize;
  base::span<const uint8_t> storage(usb_manager_.disk_state().storage);
  ASSERT_GT(storage.size(), kPartitionTableSize + kBackupGptBytes);
  EXPECT_THAT(storage.first(kPartitionTableSize), Each(0));
  EXPECT_THAT(
      storage.subspan(kPartitionTableSize,
                      storage.size() - kPartitionTableSize - kBackupGptBytes),
      Each(kDevicePattern));
  EXPECT_THAT(storage.last(kBackupGptBytes), Each(0));
}

TEST_F(OperationAndroidUsbTest,
       ImageLargerThanUsbDeviceCapacityFailsBeforeWriting) {
  constexpr size_t kOversizedImageSize = (kNumBlocks + 2) * kBlockSize;
  ASSERT_TRUE(test_utils_.FillFile(test_utils_.GetImagePath(), kImagePattern,
                                   static_cast<int>(kOversizedImageSize)));

  auto operation = base::MakeRefCounted<WriteFromFileOperation>(
      manager_.AsWeakPtr(), kDummyExtensionId, test_utils_.GetImagePath(),
      "usb:guid-usb-flash:0781:5581",
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));
  operation->SetUsbDeviceManagerBinderForTesting(base::BindRepeating(
      &FakeBotUsbDeviceManager::BindReceiver, base::Unretained(&usb_manager_)));

  base::RunLoop run_loop;
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kWrite, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(0);
  EXPECT_CALL(manager_,
              OnError(kDummyExtensionId, image_writer_api::Stage::kWrite, _,
                      error::kDeviceWriteError))
      .WillOnce([&]() { run_loop.Quit(); });

  operation->PostTask(base::BindOnce(&Operation::Start, operation));
  run_loop.Run();

  EXPECT_THAT(usb_manager_.disk_state().executed_opcodes,
              Not(Contains(kScsiWrite10Opcode)));
  EXPECT_THAT(usb_manager_.disk_state().storage, Each(kDevicePattern));
}

TEST_F(OperationAndroidUsbTest,
       ReadCapacityFailureReportsDeviceOpenErrorWithoutWriting) {
  usb_manager_.disk_state().fail_read_capacity = true;
  auto operation = base::MakeRefCounted<WriteFromFileOperation>(
      manager_.AsWeakPtr(), kDummyExtensionId, test_utils_.GetImagePath(),
      "usb:guid-usb-flash:0781:5581",
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));
  operation->SetUsbDeviceManagerBinderForTesting(base::BindRepeating(
      &FakeBotUsbDeviceManager::BindReceiver, base::Unretained(&usb_manager_)));

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

  EXPECT_THAT(usb_manager_.disk_state().executed_opcodes,
              Not(Contains(kScsiWrite10Opcode)));
  EXPECT_THAT(usb_manager_.disk_state().storage, Each(kDevicePattern));
}

}  // namespace
}  // namespace extensions::image_writer
