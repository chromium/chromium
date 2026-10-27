// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider_android.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <compare>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
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

// In Linux sysfs, a block device's `device` symlink resolves into a path such
// as `/sys/devices/.../usbX/.../hostY/targetY:0:0/Y:0:0:0/block/sdZ`, where the
// enclosing USB device directory (`idVendor`/`idProduct`) is typically 4-6
// levels above the block device. Cap upward traversal at 12 levels to guard
// against unexpected hierarchy depth or symlink cycles.
constexpr int kMaxSysfsUsbAncestorDepth = 12;

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
      AppendStorageDevice(
          *device_list,
          base::StringPrintf("usb:%s:%04x:%04x", candidate.guid.c_str(),
                             candidate.vendor_id, candidate.product_id),
          capacity, candidate.vendor_name,
          candidate.product_name.empty() ? "Flash Drive"
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
            sys_block_dir_, dev_block_dir_, dev_dir_,
            std::move(refreshed_candidates_)),
        base::BindOnce(&AndroidUsbDeviceEnumerator::OnBackgroundScanComplete,
                       this));
  }

  void OnBackgroundScanComplete(scoped_refptr<StorageDeviceList> device_list) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    FinalizeDeviceList(std::move(device_list));
  }

  void FinalizeDeviceList(scoped_refptr<StorageDeviceList> device_list) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!callback_) {
      return;
    }
    std::move(callback_).Run(std::move(device_list));
  }

  RemovableStorageProvider::DeviceListReadyCallback callback_;
  mojo::Remote<device::mojom::UsbDeviceManager> usb_manager_;
  const base::FilePath sys_block_dir_;
  const base::FilePath dev_block_dir_;
  const base::FilePath dev_dir_;
  std::vector<UsbMassStorageCandidate> refreshed_candidates_;
  bool started_background_scan_ = false;
};

}  // namespace

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
