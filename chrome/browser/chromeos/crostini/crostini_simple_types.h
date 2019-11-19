// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_SIMPLE_TYPES_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_SIMPLE_TYPES_H_

#include <string>

#include "base/files/file_path.h"
#include "chromeos/dbus/concierge/service.pb.h"

// This file contains simple C++ types (enums and Plain-Old-Data structs).
// Importantly, #include'ing this file will not depend on eventually executing
// "#include <dbus/dbus.h>",

namespace crostini {

// Result types for various callbacks etc.

// WARNING: Do not remove or re-order these values, as they are used in user
// visible error messages and logs. New entries should only be added to the end.
// This message was added during development of M74, error codes from prior
// versions may differ from the numbering here.
enum class CrostiniResult {
  SUCCESS = 0,
  // DBUS_ERROR = 1,
  // UNPARSEABLE_RESPONSE = 2,
  // INSUFFICIENT_DISK = 3,
  CREATE_DISK_IMAGE_FAILED = 4,
  VM_START_FAILED = 5,
  VM_STOP_FAILED = 6,
  DESTROY_DISK_IMAGE_FAILED = 7,
  LIST_VM_DISKS_FAILED = 8,
  CLIENT_ERROR = 9,
  // DISK_TYPE_ERROR = 10,
  CONTAINER_DOWNLOAD_TIMED_OUT = 11,
  CONTAINER_CREATE_CANCELLED = 12,
  CONTAINER_CREATE_FAILED = 13,
  CONTAINER_START_CANCELLED = 14,
  CONTAINER_START_FAILED = 15,
  // LAUNCH_CONTAINER_APPLICATION_FAILED = 16,
  INSTALL_LINUX_PACKAGE_FAILED = 17,
  BLOCKING_OPERATION_ALREADY_ACTIVE = 18,
  UNINSTALL_PACKAGE_FAILED = 19,
  SSHFS_MOUNT_ERROR = 20,
  OFFLINE_WHEN_UPGRADE_REQUIRED = 21,
  LOAD_COMPONENT_FAILED = 22,
  // PERMISSION_BROKER_ERROR = 23,
  // ATTACH_USB_FAILED = 24,
  // DETACH_USB_FAILED = 25,
  // LIST_USB_FAILED = 26,
  CROSTINI_UNINSTALLER_RUNNING = 27,
  // UNKNOWN_USB_DEVICE = 28,
  UNKNOWN_ERROR = 29,
  CONTAINER_EXPORT_IMPORT_FAILED = 30,
  CONTAINER_EXPORT_IMPORT_FAILED_VM_STOPPED = 31,
  CONTAINER_EXPORT_IMPORT_FAILED_VM_STARTED = 32,
  CONTAINER_EXPORT_IMPORT_FAILED_ARCHITECTURE = 33,
  NOT_ALLOWED = 34,
  CONTAINER_EXPORT_IMPORT_FAILED_SPACE = 35,
  GET_CONTAINER_SSH_KEYS_FAILED = 36,
  CONTAINER_EXPORT_IMPORT_CANCELLED = 37,
  RESTART_ABORTED = 38,
  RESTART_FAILED_VM_STOPPED = 39,
  UPGRADE_CONTAINER_STARTED = 40,
  UPGRADE_CONTAINER_ALREADY_RUNNING = 41,
  UPGRADE_CONTAINER_NOT_SUPPORTED = 42,
  UPGRADE_CONTAINER_ALREADY_UPGRADED = 43,
  UPGRADE_CONTAINER_FAILED = 44,
  CANCEL_UPGRADE_CONTAINER_FAILED = 45,
  CONCIERGE_START_FAILED = 46,
  kMaxValue = CONCIERGE_START_FAILED,
};

enum class InstallLinuxPackageProgressStatus {
  SUCCEEDED,
  FAILED,
  DOWNLOADING,
  INSTALLING,
};

enum class VmState {
  STARTING,
  STARTED,
  STOPPING,
};

enum class UninstallPackageProgressStatus {
  SUCCEEDED,
  FAILED,
  UNINSTALLING,  // In progress
};

// TODO(juwa): delete this once the new version of tremplin has shipped.
enum class ExportContainerProgressStatus {
  // Deprecated. Has been replaced by STREAMING.
  PACK,
  // Deprecated. Has been replaced by STREAMING.
  DOWNLOAD,
  STREAMING,
};

enum class ImportContainerProgressStatus {
  UPLOAD,
  UNPACK,
  FAILURE_ARCHITECTURE,
  FAILURE_SPACE,
};

enum class UpgradeContainerProgressStatus {
  SUCCEEDED,
  FAILED,
  UPGRADING,
};

enum class ContainerVersion {
  UNKNOWN,
  STRETCH,
  BUSTER,
};

struct VmInfo {
  VmState state;
  vm_tools::concierge::VmInfo info;
  bool usb_devices_shared = false;
};

struct StreamingExportStatus {
  uint32_t total_files;
  uint64_t total_bytes;
  uint32_t exported_files;
  uint64_t exported_bytes;
};

struct ContainerInfo {
  ContainerInfo(std::string name, std::string username, std::string homedir);
  ~ContainerInfo();
  ContainerInfo(const ContainerInfo&);

  std::string name;
  std::string username;
  base::FilePath homedir;
  bool sshfs_mounted = false;
};

// Return type when getting app icons from within a container.
struct Icon {
  std::string desktop_file_id;

  // Icon file content in PNG format.
  std::string content;
};

struct LinuxPackageInfo {
  LinuxPackageInfo();
  LinuxPackageInfo(const LinuxPackageInfo&);
  ~LinuxPackageInfo();

  bool success;

  // A textual reason for the failure, only set when success is false.
  std::string failure_reason;

  // The remaining fields are only set when success is true.
  // package_id is given as "name;version;arch;data".
  std::string package_id;
  std::string name;
  std::string version;
  std::string summary;
  std::string description;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_SIMPLE_TYPES_H_
