// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_REMOVABLE_STORAGE_PROVIDER_ANDROID_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_REMOVABLE_STORAGE_PROVIDER_ANDROID_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/usb_device.mojom-forward.h"
#include "services/device/public/mojom/usb_manager.mojom-forward.h"

namespace extensions::image_writer {

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
