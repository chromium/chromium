// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_REMOVABLE_STORAGE_PROVIDER_ANDROID_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_REMOVABLE_STORAGE_PROVIDER_ANDROID_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/usb_device.mojom-forward.h"
#include "services/device/public/mojom/usb_manager.mojom-forward.h"

namespace extensions::image_writer {

// USB Mass Storage Bulk-Only Transport (BOT) protocol and SCSI command
// constants (USB Mass Storage Class Bulk-Only Transport 1.0 specification,
// Section 5) shared between Android USB enumeration and BOT write/verify
// sessions:
// - Every command phase sends a 31-byte Command Block Wrapper (CBW) starting
//   with the little-endian signature "USBC" (0x43425355).
// - Every status phase reads a 13-byte Command Status Wrapper (CSW) starting
//   with the little-endian signature "USBS" (0x53425355).
inline constexpr uint32_t kCommandBlockWrapperSignature = 0x43425355;  // "USBC"
inline constexpr uint32_t kCommandStatusWrapperSignature =
    0x53425355;  // "USBS"
inline constexpr size_t kCommandBlockWrapperSize = 31;
inline constexpr size_t kCommandStatusWrapperSize = 13;
inline constexpr size_t kMaxScsiCommandBlockSize = 16;
inline constexpr uint8_t kDirectionDeviceToHost = 0x80;
inline constexpr uint8_t kDirectionHostToDevice = 0x00;
// Target Logical Unit Number (LUN) on the USB storage device (always 0 for
// single-LUN USB flash drives).
inline constexpr uint8_t kLogicalUnitNumber = 0;
inline constexpr uint8_t kScsiTestUnitReadyOpcode = 0x00;
inline constexpr uint8_t kScsiRequestSenseOpcode = 0x03;
inline constexpr uint8_t kScsiReadCapacity10Opcode = 0x25;
inline constexpr uint8_t kScsiRead10Opcode = 0x28;
inline constexpr uint8_t kScsiWrite10Opcode = 0x2A;
inline constexpr uint8_t kScsiSynchronizeCache10Opcode = 0x35;
inline constexpr uint32_t kScsiRequestSenseAllocationLength = 18;
inline constexpr uint32_t kScsiReadCapacity10ResponseLength = 8;
inline constexpr uint8_t kScsiSenseKeyNotReady = 0x02;
inline constexpr uint8_t kScsiSenseKeyIllegalRequest = 0x05;
inline constexpr uint8_t kScsiSenseKeyUnitAttention = 0x06;
// SCSI Additional Sense Code (ASC) / Qualifier (ASCQ) for "LOGICAL UNIT IS IN
// PROCESS OF BECOMING READY" (0x04 / 0x01).
inline constexpr uint8_t kScsiAscLogicalUnitNotReady = 0x04;
inline constexpr uint8_t kScsiAscqBecomingReady = 0x01;

struct CommandStatusWrapper {
  uint32_t signature = 0;
  uint32_t tag = 0;
  uint32_t data_residue = 0;
  uint8_t status = 0;
};

struct UsbMassStorageEndpoints {
  uint8_t interface_number = 0;
  uint8_t endpoint_in = 0;
  uint8_t endpoint_out = 0;
};

struct ParsedDeviceId {
  std::string guid;
  uint16_t vendor_id = 0;
  uint16_t product_id = 0;
};

// Serializes a 31-byte USB Mass Storage Bulk-Only Transport Command Block
// Wrapper (CBW) containing `scsi_command` (a 6-to-16-byte SCSI command
// descriptor block).
std::vector<uint8_t> BuildCommandBlockWrapper(
    uint32_t tag,
    uint32_t data_transfer_length,
    uint8_t direction_flags,
    base::span<const uint8_t> scsi_command);

// Parses a 13-byte USB Mass Storage Bulk-Only Transport Command Status Wrapper
// (CSW).
std::optional<CommandStatusWrapper> ParseCommandStatusWrapper(
    base::span<const uint8_t> bytes);

// Formats or parses the `storage_unit_id` string (`"usb:<guid>:<vid>:<pid>"`)
// used to identify Android USB Mass Storage devices across enumeration and
// write/verify operations.
std::string FormatUsbDeviceId(std::string_view guid,
                              uint16_t vendor_id,
                              uint16_t product_id);
std::optional<ParsedDeviceId> ParseUsbDeviceId(
    std::string_view storage_unit_id);

// Searches `info` for a USB Mass Storage Bulk-Only Transport (class 0x08,
// subclass 0x06, protocol 0x50) interface with both bulk IN and bulk OUT
// endpoints. Returns the interface and endpoint numbers if found, or
// `std::nullopt` otherwise.
std::optional<UsbMassStorageEndpoints> ExtractMassStorageEndpoints(
    const device::mojom::UsbDeviceInfo& info);

// Initiates asynchronous USB and `/sys/block` removable storage enumeration on
// Android using the DeviceService `UsbDeviceManager` and default `/sys/block`,
// `/dev/block`, and `/dev` directories. Must be called on
// `content::BrowserThread::UI`; posts blocking `/sys/block` file I/O to a
// background `base::ThreadPool` task.
void GetAllDevicesOnAndroid(
    RemovableStorageProvider::DeviceListReadyCallback callback);

// Test entry point accepting a caller-provided `usb_manager` remote and root
// directory paths (`sys_block_dir`, `dev_block_dir`, `dev_dir`).
void GetAllDevicesOnAndroidForTesting(
    mojo::PendingRemote<device::mojom::UsbDeviceManager> usb_manager,
    base::FilePath sys_block_dir,
    base::FilePath dev_block_dir,
    base::FilePath dev_dir,
    RemovableStorageProvider::DeviceListReadyCallback callback);

}  // namespace extensions::image_writer

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_REMOVABLE_STORAGE_PROVIDER_ANDROID_H_
