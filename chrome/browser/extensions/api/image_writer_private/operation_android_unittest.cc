// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/operation.h"

#include <stdint.h>

#include <memory>
#include <optional>
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
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/extensions/api/image_writer_private/destroy_partitions_operation.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider_android.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/browser/extensions/api/image_writer_private/write_from_file_operation.h"
#include "chrome/browser/extensions/api/image_writer_private/write_from_url_operation.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/url_loader_interceptor.h"
#include "crypto/obsolete/md5.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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
constexpr uint8_t kBotInterfaceNumber = 1;

struct SimulatedUsbDiskState {
  std::vector<uint8_t> storage =
      std::vector<uint8_t>(kNumBlocks * kBlockSize, kDevicePattern);
  bool unit_attention_on_open = true;
  bool persistent_unit_attention = false;
  bool fail_cbw_transfer = false;
  bool fail_read_capacity = false;
  bool fail_read_10 = false;
  bool disconnect_on_release = false;
  bool disconnect_after_sync_cache = false;
  base::OnceClosure on_write_payload_in_flight;
  bool stall_first_csw_once = false;
  bool stall_all_csw = false;
  bool corrupt_csw_tag = false;
  bool phase_error_once_on_write = false;
  bool persistent_phase_error_on_write = false;
  bool stall_write_data_with_good_csw = false;
  bool stall_write_data_once_with_good_csw = false;
  std::optional<uint32_t> override_csw_residue;
  // When set, `override_csw_residue` only applies to CSWs for this opcode.
  std::optional<uint8_t> override_csw_residue_opcode;
  int open_count = 0;
  int bot_reset_count = 0;
  std::optional<size_t> corrupt_byte_offset_after_sync;
  std::vector<uint8_t> executed_opcodes;
  std::vector<std::pair<device::mojom::UsbTransferDirection, uint8_t>>
      clear_halt_calls;
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
    if (interface_number != kBotInterfaceNumber) {
      std::move(callback).Run(device::mojom::UsbClaimInterfaceResult::kFailure);
      return;
    }
    std::move(callback).Run(device::mojom::UsbClaimInterfaceResult::kSuccess);
  }
  void ReleaseInterface(uint8_t interface_number,
                        ReleaseInterfaceCallback callback) override {
    EXPECT_EQ(interface_number, kBotInterfaceNumber);
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
    state_->clear_halt_calls.emplace_back(direction, endpoint_number);
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
    if (params &&
        params->type == device::mojom::UsbControlTransferType::CLASS &&
        params->recipient ==
            device::mojom::UsbControlTransferRecipient::INTERFACE &&
        params->request == kBotBulkOnlyMassStorageResetRequest &&
        params->value == 0 && params->index == kBotInterfaceNumber &&
        data.empty()) {
      ++state_->bot_reset_count;
      expecting_write_payload_ = false;
      next_status_ = kCswStatusCommandPassed;
      std::move(callback).Run(device::mojom::UsbTransferStatus::COMPLETED);
      return;
    }
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
      if (state_->on_write_payload_in_flight) {
        pending_write_callback_ = std::move(callback);
        std::move(state_->on_write_payload_in_flight).Run();
        return;
      }
      if (state_->stall_write_data_with_good_csw ||
          state_->stall_write_data_once_with_good_csw) {
        state_->stall_write_data_once_with_good_csw = false;
        next_status_ = kCswStatusCommandPassed;
        std::move(callback).Run(device::mojom::UsbTransferStatus::STALLED);
        return;
      }
      if (state_->persistent_phase_error_on_write ||
          state_->phase_error_once_on_write) {
        if (state_->phase_error_once_on_write) {
          state_->phase_error_once_on_write = false;
        }
        next_status_ = kCswStatusPhaseError;
        std::move(callback).Run(device::mojom::UsbTransferStatus::COMPLETED);
        return;
      }
      const size_t offset =
          static_cast<size_t>(current_logical_block_address_) * kBlockSize;
      const size_t length = static_cast<size_t>(current_blocks_) * kBlockSize;
      if (offset + length <= state_->storage.size() && data.size() >= length) {
        base::span(state_->storage)
            .subspan(offset, length)
            .copy_from(data.first(length));
        next_status_ = kCswStatusCommandPassed;
      } else {
        next_status_ = kCswStatusCommandFailed;
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
      if (state_->stall_all_csw) {
        std::move(callback).Run(device::mojom::UsbTransferStatus::STALLED, {});
        return;
      }
      if (state_->stall_first_csw_once) {
        state_->stall_first_csw_once = false;
        std::move(callback).Run(device::mojom::UsbTransferStatus::STALLED, {});
        return;
      }
      uint8_t status = next_status_;
      next_status_ = kCswStatusCommandPassed;
      if (current_opcode_ == kScsiTestUnitReadyOpcode && unit_attention_) {
        status = kCswStatusCommandFailed;
      }
      if (current_opcode_ == kScsiSynchronizeCache10Opcode &&
          state_->corrupt_byte_offset_after_sync.has_value()) {
        state_->storage[*state_->corrupt_byte_offset_after_sync] ^= 0xFF;
      }
      std::vector<uint8_t> status_bytes(kCommandStatusWrapperSize, 0);
      base::SpanWriter<uint8_t> writer(status_bytes);
      writer.WriteU32LittleEndian(kCommandStatusWrapperSignature);
      writer.WriteU32LittleEndian(state_->corrupt_csw_tag
                                      ? (current_tag_ ^ 0xDEADBEEFu)
                                      : current_tag_);
      const bool override_residue =
          !state_->override_csw_residue_opcode.has_value() ||
          *state_->override_csw_residue_opcode == current_opcode_;
      writer.WriteU32LittleEndian(
          override_residue ? state_->override_csw_residue.value_or(0u) : 0u);
      writer.WriteU8LittleEndian(status);
      std::move(callback).Run(device::mojom::UsbTransferStatus::COMPLETED,
                              std::move(status_bytes));
      if (current_opcode_ == kScsiSynchronizeCache10Opcode &&
          state_->disconnect_after_sync_cache && self_receiver_) {
        self_receiver_->Close();
      }
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
      writer.WriteU32BigEndian(
          static_cast<uint32_t>(state_->storage.size() / kBlockSize) - 1);
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
  GenericTransferOutCallback pending_write_callback_;
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
  void SetBulkPacketSize(uint32_t packet_size) {
    bulk_packet_size_ = packet_size;
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
    endpoint_in->packet_size = bulk_packet_size_;

    auto endpoint_out = device::mojom::UsbEndpointInfo::New();
    endpoint_out->endpoint_number = 2;
    endpoint_out->direction = device::mojom::UsbTransferDirection::OUTBOUND;
    endpoint_out->type = device::mojom::UsbTransferType::BULK;
    endpoint_out->packet_size = bulk_packet_size_;

    auto alternate = device::mojom::UsbAlternateInterfaceInfo::New();
    alternate->alternate_setting = 0;
    alternate->class_code = interface_class_code_;
    alternate->subclass_code = 0x06;
    alternate->protocol_code = 0x50;
    alternate->endpoints.push_back(std::move(endpoint_in));
    alternate->endpoints.push_back(std::move(endpoint_out));

    auto interface_info = device::mojom::UsbInterfaceInfo::New();
    interface_info->interface_number = kBotInterfaceNumber;
    interface_info->first_interface = kBotInterfaceNumber;
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
  uint32_t bulk_packet_size_ = 1024;
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
  EXPECT_EQ(usb_manager_.disk_state().open_count, 1);
  EXPECT_THAT(usb_manager_.disk_state().executed_opcodes,
              ElementsAre(kScsiTestUnitReadyOpcode, kScsiRequestSenseOpcode,
                          kScsiTestUnitReadyOpcode, kScsiReadCapacity10Opcode,
                          kScsiWrite10Opcode, kScsiWrite10Opcode,
                          kScsiSynchronizeCache10Opcode, kScsiRead10Opcode,
                          kScsiRead10Opcode));
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
  usb_manager_.disk_state().corrupt_byte_offset_after_sync = 0;
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

  // Verification reuses the open session and stops immediately after the first
  // mismatching READ (10) chunk without reading the second chunk.
  EXPECT_EQ(usb_manager_.disk_state().open_count, 1);
  EXPECT_THAT(usb_manager_.disk_state().executed_opcodes,
              ElementsAre(kScsiTestUnitReadyOpcode, kScsiRequestSenseOpcode,
                          kScsiTestUnitReadyOpcode, kScsiReadCapacity10Opcode,
                          kScsiWrite10Opcode, kScsiWrite10Opcode,
                          kScsiSynchronizeCache10Opcode, kScsiRead10Opcode));
  EXPECT_TRUE(usb_manager_.disk_state().interface_released);
  EXPECT_TRUE(usb_manager_.disk_state().device_closed);
}

TEST_F(OperationAndroidUsbTest,
       WriteFromFileOverUsbBotDetectsLastChunkVerifyMismatch) {
  usb_manager_.disk_state().corrupt_byte_offset_after_sync = kTestFileSize - 1;
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

  EXPECT_EQ(usb_manager_.disk_state().open_count, 1);
  EXPECT_THAT(usb_manager_.disk_state().executed_opcodes,
              ElementsAre(kScsiTestUnitReadyOpcode, kScsiRequestSenseOpcode,
                          kScsiTestUnitReadyOpcode, kScsiReadCapacity10Opcode,
                          kScsiWrite10Opcode, kScsiWrite10Opcode,
                          kScsiSynchronizeCache10Opcode, kScsiRead10Opcode,
                          kScsiRead10Opcode));
  EXPECT_TRUE(usb_manager_.disk_state().interface_released);
  EXPECT_TRUE(usb_manager_.disk_state().device_closed);
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

  EXPECT_EQ(usb_manager_.disk_state().open_count, 1);
  EXPECT_TRUE(usb_manager_.disk_state().interface_released);
  EXPECT_TRUE(usb_manager_.disk_state().device_closed);

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

TEST_F(OperationAndroidUsbTest,
       MultiChunkLargeImageStreams512KiBChunksAndVerifies) {
  // 1.25 MiB simulated USB disk; 1,179,725-byte image spans two 512 KiB chunks
  // plus one 128 KiB + 77-byte trailing chunk (3 WRITE (10) and 3 READ (10)
  // transfers).
  usb_manager_.disk_state().storage.assign(2560 * kBlockSize, kDevicePattern);
  constexpr size_t kLargeImageSize = 1152 * 1024 + 77;
  constexpr size_t kAlignedImageSize = 1152 * 1024 + kBlockSize;
  std::vector<uint8_t> large_image(kLargeImageSize);
  for (size_t i = 0; i < kLargeImageSize; ++i) {
    large_image[i] = static_cast<uint8_t>((i * 29 + 13) & 0xFF);
  }
  ASSERT_TRUE(base::WriteFile(test_utils_.GetImagePath(), large_image));

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

  EXPECT_EQ(usb_manager_.disk_state().open_count, 1);
  EXPECT_THAT(
      usb_manager_.disk_state().executed_opcodes,
      ElementsAre(kScsiTestUnitReadyOpcode, kScsiRequestSenseOpcode,
                  kScsiTestUnitReadyOpcode, kScsiReadCapacity10Opcode,
                  kScsiWrite10Opcode, kScsiWrite10Opcode, kScsiWrite10Opcode,
                  kScsiSynchronizeCache10Opcode, kScsiRead10Opcode,
                  kScsiRead10Opcode, kScsiRead10Opcode));

  base::span<const uint8_t> storage(usb_manager_.disk_state().storage);
  EXPECT_EQ(storage.first(kLargeImageSize), base::span(large_image));
  EXPECT_THAT(
      storage.subspan(kLargeImageSize, kAlignedImageSize - kLargeImageSize),
      Each(0));
  EXPECT_THAT(storage.subspan(kAlignedImageSize), Each(kDevicePattern));
}

TEST_F(OperationAndroidUsbTest, Usb2DeviceCapsTransfersAt64KiB) {
  // USB 2.0 bulk endpoints report wMaxPacketSize == 512, so a 160 KiB image
  // must split into 64 KiB + 64 KiB + 32 KiB transfers rather than two 80 KiB
  // transfers.
  usb_manager_.SetBulkPacketSize(512);
  usb_manager_.disk_state().storage.assign(512 * kBlockSize, kDevicePattern);
  constexpr size_t kUsb2ImageSize = 160 * 1024;
  std::vector<uint8_t> image(kUsb2ImageSize, 0x3C);
  ASSERT_TRUE(base::WriteFile(test_utils_.GetImagePath(), image));

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

  EXPECT_EQ(usb_manager_.disk_state().open_count, 1);
  EXPECT_THAT(
      usb_manager_.disk_state().executed_opcodes,
      ElementsAre(kScsiTestUnitReadyOpcode, kScsiRequestSenseOpcode,
                  kScsiTestUnitReadyOpcode, kScsiReadCapacity10Opcode,
                  kScsiWrite10Opcode, kScsiWrite10Opcode, kScsiWrite10Opcode,
                  kScsiSynchronizeCache10Opcode, kScsiRead10Opcode,
                  kScsiRead10Opcode, kScsiRead10Opcode));
}

TEST_F(OperationAndroidUsbTest, CancelMidWriteInFlightStopsFurtherTransfers) {
  auto operation = base::MakeRefCounted<WriteFromFileOperation>(
      manager_.AsWeakPtr(), kDummyExtensionId, test_utils_.GetImagePath(),
      "usb:guid-usb-flash:0781:5581",
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));
  operation->SetUsbDeviceManagerBinderForTesting(base::BindRepeating(
      &FakeBotUsbDeviceManager::BindReceiver, base::Unretained(&usb_manager_)));

  base::RunLoop run_loop;
  usb_manager_.disk_state().on_write_payload_in_flight =
      base::BindLambdaForTesting([&]() {
        operation->PostTask(base::BindOnce(&Operation::Cancel, operation));
        operation->PostTask(run_loop.QuitClosure());
      });

  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kWrite, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(0);
  EXPECT_CALL(manager_, OnError(kDummyExtensionId, _, _, _)).Times(0);

  operation->PostTask(base::BindOnce(&Operation::Start, operation));
  run_loop.Run();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(usb_manager_.disk_state().open_count, 1);
  EXPECT_THAT(usb_manager_.disk_state().executed_opcodes,
              ElementsAre(kScsiTestUnitReadyOpcode, kScsiRequestSenseOpcode,
                          kScsiTestUnitReadyOpcode, kScsiReadCapacity10Opcode,
                          kScsiWrite10Opcode));
  EXPECT_TRUE(usb_manager_.disk_state().interface_released);
  EXPECT_TRUE(usb_manager_.disk_state().device_closed);
  EXPECT_THAT(usb_manager_.disk_state().storage, Each(kDevicePattern));
}

TEST_F(OperationAndroidUsbTest,
       DisconnectBetweenWriteAndVerifyReportsDeviceOpenError) {
  usb_manager_.disk_state().disconnect_after_sync_cache = true;
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
      .Times(AnyNumber());
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(0);
  EXPECT_CALL(manager_,
              OnError(kDummyExtensionId, image_writer_api::Stage::kVerifyWrite,
                      _, error::kDeviceOpenError))
      .WillOnce([&]() { run_loop.Quit(); });

  operation->PostTask(base::BindOnce(&Operation::Start, operation));
  run_loop.Run();

  EXPECT_EQ(usb_manager_.disk_state().open_count, 1);
}

TEST_F(OperationAndroidUsbTest,
       WriteFromUrlOverUsbBotReusesSessionAcrossWriteAndVerify) {
  const GURL kImageUrl("http://localhost/test/image.bin");
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url != kImageUrl) {
          return false;
        }
        content::URLLoaderInterceptor::WriteResponse(test_utils_.GetImagePath(),
                                                     params->client.get());
        return true;
      }));

  std::string image_bytes;
  ASSERT_TRUE(base::ReadFileToString(test_utils_.GetImagePath(), &image_bytes));
  const std::string expected_hash = base::HexEncodeLower(
      crypto::obsolete::Md5::HashForTesting(base::as_byte_span(image_bytes)));

  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  profile_->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess()
      ->Clone(factory_remote.InitWithNewPipeAndPassReceiver());

  auto operation = base::MakeRefCounted<WriteFromUrlOperation>(
      manager_.AsWeakPtr(), kDummyExtensionId, std::move(factory_remote),
      kImageUrl, expected_hash, "usb:guid-usb-flash:0781:5581",
      base::FilePath(FILE_PATH_LITERAL("/var/tmp")));
  operation->SetUsbDeviceManagerBinderForTesting(base::BindRepeating(
      &FakeBotUsbDeviceManager::BindReceiver, base::Unretained(&usb_manager_)));

  base::RunLoop run_loop;
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kDownload, _))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId,
                                   image_writer_api::Stage::kVerifyDownload, _))
      .Times(AtLeast(1));
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

  EXPECT_EQ(usb_manager_.disk_state().open_count, 1);
  EXPECT_TRUE(usb_manager_.disk_state().interface_released);
  EXPECT_TRUE(usb_manager_.disk_state().device_closed);
}

TEST_F(OperationAndroidUsbTest,
       CswStallClearsInboundHaltAndRetriesCswOnceToSucceed) {
  usb_manager_.disk_state().stall_first_csw_once = true;
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

  EXPECT_EQ(usb_manager_.disk_state().bot_reset_count, 0);
  EXPECT_THAT(usb_manager_.disk_state().clear_halt_calls,
              ElementsAre(std::make_pair(
                  device::mojom::UsbTransferDirection::INBOUND, uint8_t{1})));
}

TEST_F(OperationAndroidUsbTest,
       DoubleCswStallRunsBotResetRecoveryAndFailsCleanly) {
  usb_manager_.disk_state().stall_all_csw = true;
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

  // First CSW stall clears INBOUND once; second CSW stall triggers BOT Reset
  // Recovery (class reset + ClearHalt INBOUND + ClearHalt OUTBOUND).
  EXPECT_EQ(usb_manager_.disk_state().bot_reset_count, 1);
  EXPECT_THAT(
      usb_manager_.disk_state().clear_halt_calls,
      ElementsAre(std::make_pair(device::mojom::UsbTransferDirection::INBOUND,
                                 uint8_t{1}),
                  std::make_pair(device::mojom::UsbTransferDirection::INBOUND,
                                 uint8_t{1}),
                  std::make_pair(device::mojom::UsbTransferDirection::OUTBOUND,
                                 uint8_t{2})));
  EXPECT_TRUE(usb_manager_.disk_state().interface_released);
  EXPECT_TRUE(usb_manager_.disk_state().device_closed);
}

TEST_F(OperationAndroidUsbTest,
       PhaseErrorTriggersBotResetRecoveryAndRetriesWriteCommand) {
  usb_manager_.disk_state().phase_error_once_on_write = true;
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

  EXPECT_EQ(usb_manager_.disk_state().bot_reset_count, 1);
  EXPECT_THAT(
      usb_manager_.disk_state().clear_halt_calls,
      ElementsAre(std::make_pair(device::mojom::UsbTransferDirection::INBOUND,
                                 uint8_t{1}),
                  std::make_pair(device::mojom::UsbTransferDirection::OUTBOUND,
                                 uint8_t{2})));
  EXPECT_THAT(usb_manager_.disk_state().executed_opcodes,
              ElementsAre(kScsiTestUnitReadyOpcode, kScsiRequestSenseOpcode,
                          kScsiTestUnitReadyOpcode, kScsiReadCapacity10Opcode,
                          kScsiWrite10Opcode, kScsiWrite10Opcode,
                          kScsiWrite10Opcode, kScsiSynchronizeCache10Opcode,
                          kScsiRead10Opcode, kScsiRead10Opcode));
  EXPECT_TRUE(usb_manager_.disk_state().interface_released);
  EXPECT_TRUE(usb_manager_.disk_state().device_closed);
}

TEST_F(OperationAndroidUsbTest,
       PersistentPhaseErrorExhaustsRetriesAndReportsWriteError) {
  usb_manager_.disk_state().persistent_phase_error_on_write = true;
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

  EXPECT_EQ(usb_manager_.disk_state().bot_reset_count, 3);
  EXPECT_TRUE(usb_manager_.disk_state().interface_released);
  EXPECT_TRUE(usb_manager_.disk_state().device_closed);
}

TEST_F(OperationAndroidUsbTest,
       InvalidCswTagRunsBotResetRecoveryAndFailsCleanly) {
  usb_manager_.disk_state().corrupt_csw_tag = true;
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

  EXPECT_EQ(usb_manager_.disk_state().bot_reset_count, 1);
  EXPECT_THAT(
      usb_manager_.disk_state().clear_halt_calls,
      ElementsAre(std::make_pair(device::mojom::UsbTransferDirection::INBOUND,
                                 uint8_t{1}),
                  std::make_pair(device::mojom::UsbTransferDirection::OUTBOUND,
                                 uint8_t{2})));
  EXPECT_TRUE(usb_manager_.disk_state().interface_released);
  EXPECT_TRUE(usb_manager_.disk_state().device_closed);
}

TEST_F(OperationAndroidUsbTest,
       NonMeaningfulCswResidueRunsBotResetRecoveryAndFailsCleanly) {
  // Reporting non-zero residue on `TEST UNIT READY` (`dCBWDataTransferLength ==
  // 0`) violates BOT 1.0 §6.3.2 (`dCSWDataResidue <= dCBWDataTransferLength`).
  usb_manager_.disk_state().unit_attention_on_open = false;
  usb_manager_.disk_state().override_csw_residue = 4u;
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

  EXPECT_EQ(usb_manager_.disk_state().bot_reset_count, 1);
  EXPECT_TRUE(usb_manager_.disk_state().interface_released);
  EXPECT_TRUE(usb_manager_.disk_state().device_closed);
}

TEST_F(OperationAndroidUsbTest,
       WriteDataStallWithCommandPassedCswRetriesWriteAndSucceeds) {
  usb_manager_.disk_state().stall_write_data_once_with_good_csw = true;
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

  // The stalled `WRITE (10)` is resent after clearing the OUT halt, without
  // BOT Reset Recovery.
  EXPECT_EQ(usb_manager_.disk_state().bot_reset_count, 0);
  EXPECT_THAT(usb_manager_.disk_state().clear_halt_calls,
              ElementsAre(std::make_pair(
                  device::mojom::UsbTransferDirection::OUTBOUND, uint8_t{2})));
  EXPECT_THAT(usb_manager_.disk_state().executed_opcodes,
              ElementsAre(kScsiTestUnitReadyOpcode, kScsiRequestSenseOpcode,
                          kScsiTestUnitReadyOpcode, kScsiReadCapacity10Opcode,
                          kScsiWrite10Opcode, kScsiWrite10Opcode,
                          kScsiWrite10Opcode, kScsiSynchronizeCache10Opcode,
                          kScsiRead10Opcode, kScsiRead10Opcode));
}

TEST_F(OperationAndroidUsbTest,
       PersistentWriteDataStallWithCommandPassedCswFailsWriteStage) {
  usb_manager_.disk_state().stall_write_data_with_good_csw = true;
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

  // The first `WRITE (10)` is attempted 3 times before the write fails.
  EXPECT_EQ(usb_manager_.disk_state().bot_reset_count, 0);
  EXPECT_THAT(
      usb_manager_.disk_state().executed_opcodes,
      ElementsAre(kScsiTestUnitReadyOpcode, kScsiRequestSenseOpcode,
                  kScsiTestUnitReadyOpcode, kScsiReadCapacity10Opcode,
                  kScsiWrite10Opcode, kScsiWrite10Opcode, kScsiWrite10Opcode));
  EXPECT_THAT(usb_manager_.disk_state().storage, Each(kDevicePattern));
  EXPECT_TRUE(usb_manager_.disk_state().interface_released);
  EXPECT_TRUE(usb_manager_.disk_state().device_closed);
}

TEST_F(OperationAndroidUsbTest,
       PersistentWriteCswResidueWithCommandPassedFailsWriteStage) {
  usb_manager_.disk_state().override_csw_residue = kBlockSize;
  usb_manager_.disk_state().override_csw_residue_opcode = kScsiWrite10Opcode;
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

  EXPECT_EQ(usb_manager_.disk_state().bot_reset_count, 0);
  EXPECT_THAT(
      usb_manager_.disk_state().executed_opcodes,
      ElementsAre(kScsiTestUnitReadyOpcode, kScsiRequestSenseOpcode,
                  kScsiTestUnitReadyOpcode, kScsiReadCapacity10Opcode,
                  kScsiWrite10Opcode, kScsiWrite10Opcode, kScsiWrite10Opcode));
  EXPECT_TRUE(usb_manager_.disk_state().interface_released);
  EXPECT_TRUE(usb_manager_.disk_state().device_closed);
}

TEST_F(OperationAndroidUsbTest, ReadCswResidueTrimsDataAndFailsVerifyStage) {
  // The simulated disk returns every requested byte, but each `READ (10)` CSW
  // reports one block of residue. Verification must drop that block and fail
  // instead of comparing bytes the drive did not claim to send.
  usb_manager_.disk_state().override_csw_residue = kBlockSize;
  usb_manager_.disk_state().override_csw_residue_opcode = kScsiRead10Opcode;
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
      .Times(AnyNumber());
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(0);
  EXPECT_CALL(manager_,
              OnError(kDummyExtensionId, image_writer_api::Stage::kVerifyWrite,
                      _, error::kHashReadError))
      .WillOnce([&]() { run_loop.Quit(); });

  operation->PostTask(base::BindOnce(&Operation::Start, operation));
  run_loop.Run();

  EXPECT_EQ(usb_manager_.disk_state().bot_reset_count, 0);
  EXPECT_THAT(usb_manager_.disk_state().executed_opcodes,
              ElementsAre(kScsiTestUnitReadyOpcode, kScsiRequestSenseOpcode,
                          kScsiTestUnitReadyOpcode, kScsiReadCapacity10Opcode,
                          kScsiWrite10Opcode, kScsiWrite10Opcode,
                          kScsiSynchronizeCache10Opcode, kScsiRead10Opcode));
  EXPECT_TRUE(usb_manager_.disk_state().interface_released);
  EXPECT_TRUE(usb_manager_.disk_state().device_closed);
}

}  // namespace
}  // namespace extensions::image_writer
