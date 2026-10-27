// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider_android.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <compare>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/containers/span_writer.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/common/extensions/api/image_writer_private.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"

namespace extensions::image_writer {

namespace {

constexpr uint8_t kUsbClassMassStorage = 0x08;
constexpr uint8_t kUsbSubClassScsi = 0x06;
constexpr uint8_t kUsbProtocolBulkOnly = 0x50;
constexpr unsigned int kUsbTimeoutMs = 10000;

// In Linux sysfs, a block device's `device` symlink resolves into a path such
// as `/sys/devices/.../usbX/.../hostY/targetY:0:0/Y:0:0:0/block/sdZ`, where the
// enclosing USB device directory (`idVendor`/`idProduct`) is typically 4-6
// levels above the block device. Cap upward traversal at 12 levels to guard
// against unexpected hierarchy depth or symlink cycles.
constexpr int kMaxSysfsUsbAncestorDepth = 12;

// Parses the 8-byte big-endian parameter data returned by the SCSI
// `READ CAPACITY (10)` command:
// - Bytes 0..3: 0-based Logical Block Address (LBA) of the last logical block
//   on the medium (or 0xFFFFFFFF if the capacity exceeds 2 TiB / is invalid).
// - Bytes 4..7: Block length in bytes (typically 512 or 4096).
double ParseReadCapacityBytes(base::span<const uint8_t> data) {
  if (data.size() < kScsiReadCapacity10ResponseLength) {
    return 0.0;
  }
  const uint32_t last_logical_block_address =
      base::U32FromBigEndian(data.first<4>());
  const uint32_t block_length_bytes =
      base::U32FromBigEndian(data.subspan<4, 4>());
  return (last_logical_block_address == 0xFFFFFFFF || block_length_bytes == 0)
             ? 0.0
             : (static_cast<double>(last_logical_block_address) + 1.0) *
                   static_cast<double>(block_length_bytes);
}

struct UsbMassStorageCandidate {
  std::string guid;
  uint16_t vendor_id = 0;
  uint16_t product_id = 0;
  // UTF-8 encoded USB descriptor strings.
  std::string vendor_name;
  std::string product_name;
  std::string serial_number;
  UsbMassStorageEndpoints endpoints;
};

struct UsbDeviceKey {
  uint16_t vendor_id = 0;
  uint16_t product_id = 0;
  std::string serial_number;

  bool operator==(const UsbDeviceKey&) const = default;
  auto operator<=>(const UsbDeviceKey&) const = default;
};

std::optional<UsbMassStorageCandidate> ExtractMassStorageCandidate(
    const device::mojom::UsbDeviceInfo& info) {
  std::optional<UsbMassStorageEndpoints> endpoints =
      ExtractMassStorageEndpoints(info);
  if (!endpoints.has_value()) {
    return std::nullopt;
  }
  return UsbMassStorageCandidate{
      .guid = info.guid,
      .vendor_id = info.vendor_id,
      .product_id = info.product_id,
      .vendor_name = info.manufacturer_name.has_value()
                         ? base::UTF16ToUTF8(*info.manufacturer_name)
                         : std::string(),
      .product_name = info.product_name.has_value()
                          ? base::UTF16ToUTF8(*info.product_name)
                          : std::string(),
      .serial_number = info.serial_number.has_value()
                           ? base::UTF16ToUTF8(*info.serial_number)
                           : std::string(),
      .endpoints = *endpoints,
  };
}

std::string ReadTrimmedSysfsFile(const base::FilePath& dir,
                                 std::string_view rel_path) {
  std::string content;
  return base::ReadFileToString(dir.Append(rel_path), &content)
             ? std::string(base::TrimWhitespaceASCII(content, base::TRIM_ALL))
             : std::string();
}

std::optional<uint64_t> ReadUint64SysfsFile(const base::FilePath& dir,
                                            std::string_view rel_path) {
  uint64_t value = 0;
  if (!base::StringToUint64(ReadTrimmedSysfsFile(dir, rel_path), &value)) {
    return std::nullopt;
  }
  return value;
}

std::optional<uint32_t> ReadHexSysfsFile(const base::FilePath& dir,
                                         std::string_view rel_path) {
  uint32_t value = 0;
  if (!base::HexStringToUInt(ReadTrimmedSysfsFile(dir, rel_path), &value)) {
    return std::nullopt;
  }
  return value;
}

void AppendStorageDevice(StorageDeviceList& list,
                         std::string id,
                         double capacity,
                         std::string_view vendor,
                         std::string_view model) {
  api::image_writer_private::RemovableStorageDevice item;
  item.storage_unit_id = std::move(id);
  item.capacity = capacity;
  item.vendor = vendor.empty() ? "USB" : std::string(vendor);
  item.model = std::string(model);
  item.removable = true;
  list.data.push_back(std::move(item));
}

bool IsIgnoredBlockDeviceName(std::string_view name) {
  static constexpr std::array<std::string_view, 7> kIgnoredPrefixes = {
      "loop", "ram", "zram", "dm-", "md", "mtd", "nbd",
  };
  return std::ranges::any_of(kIgnoredPrefixes, [name](std::string_view prefix) {
    return base::StartsWith(name, prefix);
  });
}

std::optional<base::FilePath> FindUsbAncestor(const base::FilePath& start_dir) {
  base::FilePath current = base::MakeAbsoluteFilePath(start_dir);
  if (current.empty()) {
    return std::nullopt;
  }
  for (int depth = 0; depth < kMaxSysfsUsbAncestorDepth; ++depth) {
    if (base::PathExists(current.Append("idVendor")) &&
        base::PathExists(current.Append("idProduct"))) {
      return current;
    }
    base::FilePath parent = current.DirName();
    // Stop at the filesystem root, where `FilePath("/").DirName()` returns
    // `"/"` rather than an empty path.
    if (parent == current) {
      break;
    }
    current = parent;
  }
  return std::nullopt;
}

// Scans `/sys/block`, `/dev/block`, and `/dev` on a background thread and
// reconciles kernel block devices against `UsbDeviceManager` candidates.
class SysfsBlockDeviceScanner {
 public:
  SysfsBlockDeviceScanner(base::FilePath sys_block_dir,
                          base::FilePath dev_block_dir,
                          base::FilePath dev_dir)
      : sys_block_dir_(std::move(sys_block_dir)),
        dev_block_dir_(std::move(dev_block_dir)),
        dev_dir_(std::move(dev_dir)) {}

  scoped_refptr<StorageDeviceList> GetDeviceList(
      const std::vector<UsbMassStorageCandidate>& usb_candidates) const {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    auto device_list = base::MakeRefCounted<StorageDeviceList>();
    std::set<UsbDeviceKey> accessible_block_usb_keys;
    std::map<UsbDeviceKey, double> sysfs_usb_capacities;

    // 1. Enumerate kernel block devices in /sys/block.
    base::FileEnumerator block_enum(sys_block_dir_, /*recursive=*/false,
                                    base::FileEnumerator::DIRECTORIES |
                                        base::FileEnumerator::SHOW_SYM_LINKS);
    for (base::FilePath sys_block = block_enum.Next(); !sys_block.empty();
         sys_block = block_enum.Next()) {
      InspectSysfsBlockDevice(sys_block, *device_list,
                              accessible_block_usb_keys, sysfs_usb_capacities);
    }

    // 2. Add USB Mass Storage BOT devices discovered via UsbDeviceManager.
    for (const auto& candidate : usb_candidates) {
      const UsbDeviceKey usb_key{
          .vendor_id = candidate.vendor_id,
          .product_id = candidate.product_id,
          .serial_number = candidate.serial_number,
      };
      if (accessible_block_usb_keys.contains(usb_key)) {
        continue;
      }
      double capacity = 0;
      if (auto it = sysfs_usb_capacities.find(usb_key);
          it != sysfs_usb_capacities.end()) {
        capacity = it->second;
      }
      AppendStorageDevice(*device_list,
                          FormatUsbDeviceId(candidate.guid, candidate.vendor_id,
                                            candidate.product_id),
                          capacity, candidate.vendor_name,
                          candidate.product_name.empty()
                              ? "Flash Drive"
                              : candidate.product_name);
    }

    return device_list;
  }

 private:
  // Inspects a single `/sys/block/<dev>` directory. Records the USB device's
  // capacity in `sysfs_usb_capacities` (even if the block node is not directly
  // writable by Chrome) and, if the corresponding `/dev/block/<dev>` or
  // `/dev/<dev>` node is read/write accessible, appends it to `device_list`
  // and records its identity in `accessible_block_usb_keys`.
  void InspectSysfsBlockDevice(
      const base::FilePath& sys_block,
      StorageDeviceList& device_list,
      std::set<UsbDeviceKey>& accessible_block_usb_keys,
      std::map<UsbDeviceKey, double>& sysfs_usb_capacities) const {
    const std::string dev_name = sys_block.BaseName().value();
    if (IsIgnoredBlockDeviceName(dev_name)) {
      return;
    }
    const bool is_removable =
        ReadUint64SysfsFile(sys_block, "removable").value_or(0) != 0;
    const std::optional<base::FilePath> usb_dir =
        FindUsbAncestor(sys_block.Append("device"));
    const uint64_t sectors = ReadUint64SysfsFile(sys_block, "size").value_or(0);
    if ((!is_removable && !usb_dir.has_value()) ||
        ReadUint64SysfsFile(sys_block, "ro").value_or(0) != 0 || sectors == 0) {
      return;
    }

    const double capacity =
        static_cast<double>(sectors) *
        static_cast<double>(
            ReadUint64SysfsFile(sys_block, "queue/logical_block_size")
                .value_or(512));
    std::optional<uint32_t> vid;
    std::optional<uint32_t> pid;
    if (usb_dir.has_value()) {
      vid = ReadHexSysfsFile(*usb_dir, "idVendor");
      pid = ReadHexSysfsFile(*usb_dir, "idProduct");
    }
    const bool has_ids = vid.has_value() && pid.has_value();
    UsbDeviceKey usb_key;
    if (has_ids) {
      usb_key = UsbDeviceKey{
          .vendor_id = static_cast<uint16_t>(*vid),
          .product_id = static_cast<uint16_t>(*pid),
          .serial_number = ReadTrimmedSysfsFile(*usb_dir, "serial"),
      };
      sysfs_usb_capacities[usb_key] = capacity;
    }

    base::FilePath dev_node = dev_block_dir_.Append(dev_name);
    if (!base::PathExists(dev_node)) {
      dev_node = dev_dir_.Append(dev_name);
    }
    if (!base::PathIsReadable(dev_node) || !base::PathIsWritable(dev_node)) {
      return;
    }

    std::string vendor = ReadTrimmedSysfsFile(sys_block, "device/vendor");
    std::string model = ReadTrimmedSysfsFile(sys_block, "device/model");
    if (model.empty()) {
      model = ReadTrimmedSysfsFile(sys_block, "device/name");
    }
    if (usb_dir.has_value()) {
      if (has_ids) {
        accessible_block_usb_keys.insert(usb_key);
      }
      if (vendor.empty()) {
        vendor = ReadTrimmedSysfsFile(*usb_dir, "manufacturer");
      }
      if (model.empty()) {
        model = ReadTrimmedSysfsFile(*usb_dir, "product");
      }
    }
    AppendStorageDevice(device_list, dev_node.value(), capacity, vendor,
                        model.empty() ? "Storage Device" : model);
  }

  const base::FilePath sys_block_dir_;
  const base::FilePath dev_block_dir_;
  const base::FilePath dev_dir_;
};

// Queries the storage capacity of a USB Mass Storage Bulk-Only Transport device
// over `device::mojom::UsbDevice` on the UI thread using SCSI
// `TEST UNIT READY`, `REQUEST SENSE`, and `READ CAPACITY (10)`.
class MojoUsbCapacityReader {
 public:
  using ResultCallback = base::OnceCallback<void(double)>;

  MojoUsbCapacityReader(mojo::PendingRemote<device::mojom::UsbDevice> device,
                        const UsbMassStorageCandidate& candidate,
                        ResultCallback callback)
      : device_(std::move(device)),
        candidate_(candidate),
        callback_(std::move(callback)) {}

  MojoUsbCapacityReader(const MojoUsbCapacityReader&) = delete;
  MojoUsbCapacityReader& operator=(const MojoUsbCapacityReader&) = delete;

  ~MojoUsbCapacityReader() = default;

  void Start() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    device_.set_disconnect_handler(base::BindOnce(
        &MojoUsbCapacityReader::OnDisconnected, weak_factory_.GetWeakPtr()));
    device_->Open(base::BindOnce(&MojoUsbCapacityReader::OnOpened,
                                 weak_factory_.GetWeakPtr()));
  }

 private:
  using BotCommandCallback =
      base::OnceCallback<void(bool, std::vector<uint8_t>)>;

  void OnOpened(device::mojom::UsbOpenDeviceResultPtr result) {
    if (!result || !result->is_success()) {
      return Finish(0.0);
    }
    opened_ = true;
    device_->ClaimInterface(candidate_.endpoints.interface_number,
                            base::BindOnce(&MojoUsbCapacityReader::OnClaimed,
                                           weak_factory_.GetWeakPtr()));
  }

  void OnClaimed(device::mojom::UsbClaimInterfaceResult result) {
    if (result != device::mojom::UsbClaimInterfaceResult::kSuccess) {
      return Finish(0.0);
    }
    claimed_ = true;
    // Send a 6-byte SCSI `TEST UNIT READY` command (opcode 0x00) to verify the
    // logical unit is ready and clear any initial `UNIT ATTENTION` condition
    // before reading capacity.
    SendCommandWithRetry(
        {kScsiTestUnitReadyOpcode, 0, 0, 0, 0, 0}, kDirectionHostToDevice, 0,
        /*attempts_left=*/3,
        base::BindOnce(&MojoUsbCapacityReader::OnTestUnitReadyDone,
                       weak_factory_.GetWeakPtr()));
  }

  void OnTestUnitReadyDone(bool ok, std::vector<uint8_t>) {
    if (!ok) {
      return Finish(0.0);
    }
    // Send a 10-byte SCSI `READ CAPACITY (10)` command (opcode 0x25) to read
    // the 8-byte capacity descriptor (last logical block address + block size).
    SendCommandWithRetry(
        {kScsiReadCapacity10Opcode, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        kDirectionDeviceToHost, kScsiReadCapacity10ResponseLength,
        /*attempts_left=*/3,
        base::BindOnce(&MojoUsbCapacityReader::OnReadCapacityDone,
                       weak_factory_.GetWeakPtr()));
  }

  void OnReadCapacityDone(bool ok, std::vector<uint8_t> data) {
    Finish(ok ? ParseReadCapacityBytes(data) : 0.0);
  }

  // Sends a 6-to-16-byte SCSI command descriptor block (`scsi_command`) wrapped
  // in a USB Mass Storage Bulk-Only Transport Command Block Wrapper, retrying
  // with `REQUEST SENSE` if the device reports a `CHECK CONDITION` status.
  void SendCommandWithRetry(std::vector<uint8_t> scsi_command,
                            uint8_t direction_flags,
                            uint32_t data_length,
                            int attempts_left,
                            BotCommandCallback callback) {
    active_scsi_command_ = std::move(scsi_command);
    active_direction_flags_ = direction_flags;
    active_data_length_ = data_length;
    attempts_left_ = attempts_left;
    active_callback_ = std::move(callback);
    SendActiveCommandBlock();
  }

  void SendActiveCommandBlock() {
    active_tag_ = next_tag_++;
    active_data_.clear();
    std::vector<uint8_t> command_block_packet =
        BuildCommandBlockWrapper(active_tag_, active_data_length_,
                                 active_direction_flags_, active_scsi_command_);
    device_->GenericTransferOut(
        candidate_.endpoints.endpoint_out, command_block_packet, kUsbTimeoutMs,
        base::BindOnce(&MojoUsbCapacityReader::OnCommandBlockSent,
                       weak_factory_.GetWeakPtr()));
  }

  void OnCommandBlockSent(device::mojom::UsbTransferStatus status) {
    if (status != device::mojom::UsbTransferStatus::COMPLETED) {
      return std::move(active_callback_).Run(false, {});
    }
    if (active_data_length_ > 0 &&
        (active_direction_flags_ & kDirectionDeviceToHost)) {
      device_->GenericTransferIn(
          candidate_.endpoints.endpoint_in, active_data_length_, kUsbTimeoutMs,
          base::BindOnce(&MojoUsbCapacityReader::OnDataInReceived,
                         weak_factory_.GetWeakPtr()));
      return;
    }
    ReadCommandStatus();
  }

  void OnDataInReceived(device::mojom::UsbTransferStatus status,
                        base::span<const uint8_t> data) {
    if (status != device::mojom::UsbTransferStatus::COMPLETED &&
        status != device::mojom::UsbTransferStatus::SHORT_PACKET) {
      return std::move(active_callback_).Run(false, {});
    }
    active_data_.assign(data.begin(), data.end());
    ReadCommandStatus();
  }

  void ReadCommandStatus() {
    device_->GenericTransferIn(
        candidate_.endpoints.endpoint_in, kCommandStatusWrapperSize,
        kUsbTimeoutMs,
        base::BindOnce(&MojoUsbCapacityReader::OnCommandStatusReceived,
                       weak_factory_.GetWeakPtr()));
  }

  void OnCommandStatusReceived(device::mojom::UsbTransferStatus status,
                               base::span<const uint8_t> status_bytes) {
    if (status != device::mojom::UsbTransferStatus::COMPLETED &&
        status != device::mojom::UsbTransferStatus::SHORT_PACKET) {
      return std::move(active_callback_).Run(false, {});
    }
    std::optional<CommandStatusWrapper> status_wrapper =
        ParseCommandStatusWrapper(status_bytes);
    if (!status_wrapper ||
        status_wrapper->signature != kCommandStatusWrapperSignature ||
        status_wrapper->tag != active_tag_) {
      return std::move(active_callback_).Run(false, {});
    }
    // Status 0x00 indicates Command Passed (good status) per the USB Mass
    // Storage Bulk-Only Transport 1.0 specification (Table 5.3).
    if (status_wrapper->status == 0) {
      return std::move(active_callback_).Run(true, std::move(active_data_));
    }
    // Status 0x01 indicates Command Failed (for example, `UNIT ATTENTION` on
    // newly connected media); issue `REQUEST SENSE` and retry if attempts
    // remain.
    if (status_wrapper->status != 1 || attempts_left_ <= 1) {
      return std::move(active_callback_).Run(false, {});
    }
    --attempts_left_;
    request_sense_tag_ = next_tag_++;
    const std::array<uint8_t, 6> request_sense_command = {
        kScsiRequestSenseOpcode,
        0,
        0,
        0,
        static_cast<uint8_t>(kScsiRequestSenseAllocationLength),
        0};
    std::vector<uint8_t> sense_command_packet = BuildCommandBlockWrapper(
        request_sense_tag_, kScsiRequestSenseAllocationLength,
        kDirectionDeviceToHost, request_sense_command);
    device_->GenericTransferOut(
        candidate_.endpoints.endpoint_out, sense_command_packet, kUsbTimeoutMs,
        base::BindOnce(&MojoUsbCapacityReader::OnRequestSenseCommandSent,
                       weak_factory_.GetWeakPtr()));
  }

  void OnRequestSenseCommandSent(device::mojom::UsbTransferStatus status) {
    if (status != device::mojom::UsbTransferStatus::COMPLETED) {
      return std::move(active_callback_).Run(false, {});
    }
    device_->GenericTransferIn(
        candidate_.endpoints.endpoint_in, kScsiRequestSenseAllocationLength,
        kUsbTimeoutMs,
        base::BindOnce(&MojoUsbCapacityReader::OnRequestSenseDataReceived,
                       weak_factory_.GetWeakPtr()));
  }

  void OnRequestSenseDataReceived(device::mojom::UsbTransferStatus status,
                                  base::span<const uint8_t>) {
    if (status != device::mojom::UsbTransferStatus::COMPLETED &&
        status != device::mojom::UsbTransferStatus::SHORT_PACKET) {
      return std::move(active_callback_).Run(false, {});
    }
    device_->GenericTransferIn(
        candidate_.endpoints.endpoint_in, kCommandStatusWrapperSize,
        kUsbTimeoutMs,
        base::BindOnce(&MojoUsbCapacityReader::OnRequestSenseStatusReceived,
                       weak_factory_.GetWeakPtr()));
  }

  void OnRequestSenseStatusReceived(device::mojom::UsbTransferStatus status,
                                    base::span<const uint8_t> status_bytes) {
    if (status != device::mojom::UsbTransferStatus::COMPLETED &&
        status != device::mojom::UsbTransferStatus::SHORT_PACKET) {
      return std::move(active_callback_).Run(false, {});
    }
    std::optional<CommandStatusWrapper> status_wrapper =
        ParseCommandStatusWrapper(status_bytes);
    if (!status_wrapper ||
        status_wrapper->signature != kCommandStatusWrapperSignature ||
        status_wrapper->tag != request_sense_tag_ ||
        status_wrapper->status != 0) {
      return std::move(active_callback_).Run(false, {});
    }
    SendActiveCommandBlock();
  }

  void OnDisconnected() {
    claimed_ = false;
    opened_ = false;
    if (callback_) {
      std::move(callback_).Run(result_capacity_.value_or(0.0));
    }
  }

  void Finish(double capacity) {
    CHECK(callback_);
    result_capacity_ = capacity;
    if (!device_.is_connected()) {
      return OnDisconnected();
    }
    if (claimed_) {
      claimed_ = false;
      device_->ReleaseInterface(
          candidate_.endpoints.interface_number,
          base::BindOnce(&MojoUsbCapacityReader::OnReleased,
                         weak_factory_.GetWeakPtr()));
      return;
    }
    OnReleased(true);
  }

  void OnReleased(bool /*success*/) {
    if (opened_ && device_.is_connected()) {
      opened_ = false;
      device_->Close(base::BindOnce(&MojoUsbCapacityReader::OnClosed,
                                    weak_factory_.GetWeakPtr()));
      return;
    }
    opened_ = false;
    if (callback_) {
      std::move(callback_).Run(result_capacity_.value_or(0.0));
    }
  }

  void OnClosed() {
    if (callback_) {
      std::move(callback_).Run(result_capacity_.value_or(0.0));
    }
  }

  mojo::Remote<device::mojom::UsbDevice> device_;
  UsbMassStorageCandidate candidate_;
  ResultCallback callback_;
  std::optional<double> result_capacity_;
  BotCommandCallback active_callback_;
  std::vector<uint8_t> active_scsi_command_;
  std::vector<uint8_t> active_data_;
  uint32_t active_tag_ = 0;
  uint32_t request_sense_tag_ = 0;
  uint32_t active_data_length_ = 0;
  uint8_t active_direction_flags_ = 0;
  int attempts_left_ = 0;
  bool opened_ = false;
  bool claimed_ = false;
  uint32_t next_tag_ = 1;
  base::WeakPtrFactory<MojoUsbCapacityReader> weak_factory_{this};
};

// Coordinates asynchronous Android removable storage enumeration. All methods
// run on `content::BrowserThread::UI`. Uses `base::RefCountedThreadSafe`
// because the `PostTaskAndReplyWithResult` reply callback holding `this` may be
// destroyed on a background thread if shutdown occurs while the
// `SKIP_ON_SHUTDOWN` task is in flight.
class AndroidUsbDeviceEnumerator
    : public base::RefCountedThreadSafe<AndroidUsbDeviceEnumerator> {
 public:
  AndroidUsbDeviceEnumerator(
      mojo::PendingRemote<device::mojom::UsbDeviceManager> usb_manager,
      base::FilePath sys_block_dir,
      base::FilePath dev_block_dir,
      base::FilePath dev_dir,
      RemovableStorageProvider::DeviceListReadyCallback callback)
      : callback_(std::move(callback)),
        usb_manager_(std::move(usb_manager)),
        sys_block_dir_(std::move(sys_block_dir)),
        dev_block_dir_(std::move(dev_block_dir)),
        dev_dir_(std::move(dev_dir)) {}

  void Start() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!usb_manager_.is_bound()) {
      FinalizeDeviceList(nullptr);
      return;
    }
    usb_manager_.set_disconnect_handler(base::BindOnce(
        &AndroidUsbDeviceEnumerator::OnManagerDisconnected, this));
    usb_manager_->GetDevices(
        /*options=*/nullptr,
        base::BindOnce(&AndroidUsbDeviceEnumerator::OnGetDevices, this));
  }

 private:
  friend class base::RefCountedThreadSafe<AndroidUsbDeviceEnumerator>;
  ~AndroidUsbDeviceEnumerator() = default;

  void OnManagerDisconnected() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    FinalizeDeviceList(nullptr);
  }

  void OnGetDevices(std::vector<device::mojom::UsbDeviceInfoPtr> devices) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    std::vector<std::string> candidate_guids;
    for (const auto& info : devices) {
      if (info && ExtractMassStorageEndpoints(*info).has_value()) {
        candidate_guids.push_back(info->guid);
      }
    }
    if (candidate_guids.empty()) {
      RunBackgroundScan();
      return;
    }
    base::RepeatingClosure barrier = base::BarrierClosure(
        candidate_guids.size(),
        base::BindOnce(&AndroidUsbDeviceEnumerator::RunBackgroundScan, this));
    for (const std::string& guid : candidate_guids) {
      usb_manager_->RefreshDeviceInfo(
          guid, base::BindOnce(&AndroidUsbDeviceEnumerator::OnRefreshDeviceInfo,
                               this, barrier));
    }
  }

  void OnRefreshDeviceInfo(base::OnceClosure done_closure,
                           device::mojom::UsbDeviceInfoPtr refreshed_info) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (refreshed_info) {
      if (std::optional<UsbMassStorageCandidate> updated =
              ExtractMassStorageCandidate(*refreshed_info)) {
        refreshed_candidates_.push_back(std::move(*updated));
      }
    }
    std::move(done_closure).Run();
  }

  void RunBackgroundScan() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    CHECK(!started_background_scan_);
    started_background_scan_ = true;
    // Pass a copy of `refreshed_candidates_` to the background scan so
    // `refreshed_candidates_` remains populated on the UI thread for
    // `OnBackgroundScanComplete()` to look up candidates by GUID.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(
            [](base::FilePath sys_block_dir, base::FilePath dev_block_dir,
               base::FilePath dev_dir,
               std::vector<UsbMassStorageCandidate> usb_candidates) {
              return SysfsBlockDeviceScanner(std::move(sys_block_dir),
                                             std::move(dev_block_dir),
                                             std::move(dev_dir))
                  .GetDeviceList(usb_candidates);
            },
            sys_block_dir_, dev_block_dir_, dev_dir_, refreshed_candidates_),
        base::BindOnce(&AndroidUsbDeviceEnumerator::OnBackgroundScanComplete,
                       this));
  }

  void OnBackgroundScanComplete(scoped_refptr<StorageDeviceList> device_list) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!device_list || !usb_manager_.is_bound()) {
      FinalizeDeviceList(std::move(device_list));
      return;
    }

    // For each USB Mass Storage candidate whose capacity could not be read from
    // `/sys/block` (for example, when SELinux blocks untrusted_app access to
    // the USB sysfs hierarchy on Android), open the USB device over Mojo and
    // query its capacity directly via SCSI `READ CAPACITY (10)`.
    struct PendingCapacityQuery {
      size_t item_index;
      size_t candidate_index;
    };
    std::vector<PendingCapacityQuery> queries;
    for (size_t i = 0; i < device_list->data.size(); ++i) {
      const auto& item = device_list->data[i];
      if (item.capacity > 0) {
        continue;
      }
      std::optional<ParsedDeviceId> parsed_id =
          ParseUsbDeviceId(item.storage_unit_id);
      if (!parsed_id.has_value()) {
        continue;
      }
      auto it = std::ranges::find(refreshed_candidates_, parsed_id->guid,
                                  &UsbMassStorageCandidate::guid);
      if (it == refreshed_candidates_.end()) {
        continue;
      }
      queries.push_back({
          .item_index = i,
          .candidate_index =
              static_cast<size_t>(it - refreshed_candidates_.begin()),
      });
    }

    if (queries.empty()) {
      FinalizeDeviceList(std::move(device_list));
      return;
    }

    base::RepeatingClosure barrier = base::BarrierClosure(
        queries.size(),
        base::BindOnce(&AndroidUsbDeviceEnumerator::FinalizeDeviceList, this,
                       device_list));
    for (const auto& query : queries) {
      const UsbMassStorageCandidate& candidate =
          refreshed_candidates_[query.candidate_index];
      mojo::PendingRemote<device::mojom::UsbDevice> device_remote;
      usb_manager_->GetUnrestrictedDevice(
          candidate.guid, {}, device_remote.InitWithNewPipeAndPassReceiver(),
          mojo::NullRemote());
      auto reader = std::make_unique<MojoUsbCapacityReader>(
          std::move(device_remote), candidate,
          base::BindOnce(&AndroidUsbDeviceEnumerator::OnMojoCapacityResult,
                         this, device_list, query.item_index, barrier));
      capacity_readers_.push_back(std::move(reader));
      capacity_readers_.back()->Start();
    }
  }

  void OnMojoCapacityResult(scoped_refptr<StorageDeviceList> device_list,
                            size_t item_index,
                            base::OnceClosure done_closure,
                            double capacity) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (capacity > 0) {
      device_list->data[item_index].capacity = capacity;
    }
    std::move(done_closure).Run();
  }

  void FinalizeDeviceList(scoped_refptr<StorageDeviceList> device_list) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!callback_) {
      return;
    }
    if (device_list) {
      std::erase_if(device_list->data,
                    [](const auto& item) { return item.capacity <= 0; });
    }
    std::move(callback_).Run(std::move(device_list));
  }

  RemovableStorageProvider::DeviceListReadyCallback callback_;
  mojo::Remote<device::mojom::UsbDeviceManager> usb_manager_;
  const base::FilePath sys_block_dir_;
  const base::FilePath dev_block_dir_;
  const base::FilePath dev_dir_;
  std::vector<UsbMassStorageCandidate> refreshed_candidates_;
  std::vector<std::unique_ptr<MojoUsbCapacityReader>> capacity_readers_;
  bool started_background_scan_ = false;
};

}  // namespace

std::string FormatUsbDeviceId(std::string_view guid,
                              uint16_t vendor_id,
                              uint16_t product_id) {
  return base::StringPrintf("usb:%.*s:%04x:%04x", static_cast<int>(guid.size()),
                            guid.data(), vendor_id, product_id);
}

std::optional<ParsedDeviceId> ParseUsbDeviceId(
    std::string_view storage_unit_id) {
  if (!base::StartsWith(storage_unit_id, "usb:")) {
    return std::nullopt;
  }
  std::vector<std::string_view> parts = base::SplitStringPiece(
      storage_unit_id, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  uint32_t parsed_vendor_id = 0;
  uint32_t parsed_product_id = 0;
  if (parts.size() != 4 || parts[1].empty() ||
      !base::HexStringToUInt(parts[2], &parsed_vendor_id) ||
      !base::HexStringToUInt(parts[3], &parsed_product_id) ||
      parsed_vendor_id > 0xFFFF || parsed_product_id > 0xFFFF) {
    return std::nullopt;
  }
  return ParsedDeviceId{
      .guid = std::string(parts[1]),
      .vendor_id = static_cast<uint16_t>(parsed_vendor_id),
      .product_id = static_cast<uint16_t>(parsed_product_id),
  };
}

std::vector<uint8_t> BuildCommandBlockWrapper(
    uint32_t tag,
    uint32_t data_transfer_length,
    uint8_t direction_flags,
    base::span<const uint8_t> scsi_command) {
  std::vector<uint8_t> packet(kCommandBlockWrapperSize, 0);
  base::SpanWriter<uint8_t> writer(packet);
  writer.WriteU32LittleEndian(kCommandBlockWrapperSignature);
  writer.WriteU32LittleEndian(tag);
  writer.WriteU32LittleEndian(data_transfer_length);
  writer.WriteU8LittleEndian(direction_flags);
  writer.WriteU8LittleEndian(kLogicalUnitNumber);
  const size_t command_length =
      std::min(scsi_command.size(), kMaxScsiCommandBlockSize);
  writer.WriteU8LittleEndian(static_cast<uint8_t>(command_length));
  writer.Write(scsi_command.first(command_length));
  return packet;
}

std::optional<CommandStatusWrapper> ParseCommandStatusWrapper(
    base::span<const uint8_t> bytes) {
  base::SpanReader reader(bytes);
  CommandStatusWrapper status_wrapper;
  if (!reader.ReadU32LittleEndian(status_wrapper.signature) ||
      !reader.ReadU32LittleEndian(status_wrapper.tag) ||
      !reader.ReadU32LittleEndian(status_wrapper.data_residue) ||
      !reader.ReadU8LittleEndian(status_wrapper.status)) {
    return std::nullopt;
  }
  return status_wrapper;
}

std::optional<UsbMassStorageEndpoints> ExtractMassStorageEndpoints(
    const device::mojom::UsbDeviceInfo& info) {
  for (const auto& config : info.configurations) {
    if (!config) {
      continue;
    }
    for (const auto& iface : config->interfaces) {
      if (!iface) {
        continue;
      }
      for (const auto& alt : iface->alternates) {
        if (!alt || alt->class_code != kUsbClassMassStorage ||
            alt->subclass_code != kUsbSubClassScsi ||
            alt->protocol_code != kUsbProtocolBulkOnly) {
          continue;
        }
        uint8_t found_endpoint_in = 0;
        uint8_t found_endpoint_out = 0;
        for (const auto& endpoint : alt->endpoints) {
          if (!endpoint ||
              endpoint->type != device::mojom::UsbTransferType::BULK) {
            continue;
          }
          if (endpoint->direction ==
              device::mojom::UsbTransferDirection::INBOUND) {
            found_endpoint_in = endpoint->endpoint_number;
          } else if (endpoint->direction ==
                     device::mojom::UsbTransferDirection::OUTBOUND) {
            found_endpoint_out = endpoint->endpoint_number;
          }
        }
        if (found_endpoint_in != 0 && found_endpoint_out != 0) {
          return UsbMassStorageEndpoints{
              .interface_number = iface->interface_number,
              .endpoint_in = found_endpoint_in,
              .endpoint_out = found_endpoint_out,
          };
        }
      }
    }
  }
  return std::nullopt;
}

void GetAllDevicesOnAndroid(
    RemovableStorageProvider::DeviceListReadyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojo::PendingRemote<device::mojom::UsbDeviceManager> usb_manager;
  content::GetDeviceService().BindUsbDeviceManager(
      usb_manager.InitWithNewPipeAndPassReceiver());
  base::MakeRefCounted<AndroidUsbDeviceEnumerator>(
      std::move(usb_manager), base::FilePath(FILE_PATH_LITERAL("/sys/block")),
      base::FilePath(FILE_PATH_LITERAL("/dev/block")),
      base::FilePath(FILE_PATH_LITERAL("/dev")), std::move(callback))
      ->Start();
}

void GetAllDevicesOnAndroidForTesting(
    mojo::PendingRemote<device::mojom::UsbDeviceManager> usb_manager,
    base::FilePath sys_block_dir,
    base::FilePath dev_block_dir,
    base::FilePath dev_dir,
    RemovableStorageProvider::DeviceListReadyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::MakeRefCounted<AndroidUsbDeviceEnumerator>(
      std::move(usb_manager), std::move(sys_block_dir),
      std::move(dev_block_dir), std::move(dev_dir), std::move(callback))
      ->Start();
}

}  // namespace extensions::image_writer
