// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_SIMPLE_TYPES_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_SIMPLE_TYPES_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"

// This file contains simple C++ types. Simple isn't a precise term, but as a
// guideline enums and PoD structs are simple while structs/classes with methods
// other than trivial or defaulted constructors or destructors are not.
// Importantly, #include'ing this file will not depend on eventually executing
// "#include <dbus/dbus.h>",

namespace crostini {

// Result types for various callbacks etc.
//
// WARNING: Do not remove or re-order these values, as they are used in user
// visible error messages and logs. New entries should only be added to the end.
// This message was added during development of M74, error codes from prior
// versions may differ from the numbering here.
// If you add anything here make sure to also update enums.xml and the plx
// scripts in
// https://plx.corp.google.com/home2/home/collections/c16e3c1474497b821
// and CrostiniResultString in crostini_simple_types.cc.
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
  CONTAINER_CONFIGURATION_FAILED = 47,
  LOAD_COMPONENT_UPDATE_IN_PROGRESS = 48,
  NEVER_FINISHED = 49,
  CONTAINER_SETUP_FAILED = 50,
  START_LXD_FAILED = 51,
  INSTALL_IMAGE_LOADER_TIMED_OUT = 52,
  CREATE_DISK_IMAGE_TIMED_OUT = 53,
  START_TERMINA_VM_TIMED_OUT = 54,
  START_LXD_TIMED_OUT = 55,
  CREATE_CONTAINER_TIMED_OUT = 56,
  SETUP_CONTAINER_TIMED_OUT = 57,
  START_CONTAINER_TIMED_OUT = 58,
  FETCH_SSH_KEYS_TIMED_OUT = 59,
  MOUNT_CONTAINER_TIMED_OUT = 60,
  UNKNOWN_STATE_TIMED_OUT = 61,
  NEED_UPDATE = 62,
  SHARE_PATHS_FAILED = 63,
  UNREGISTERED_APPLICATION = 64,
  VSH_CONNECT_FAILED = 65,
  CONTAINER_STOP_FAILED = 66,
  CONTAINER_STOP_CANCELLED = 67,
  WAYLAND_SERVER_CREATION_FAILED = 68,
  CONFIGURE_CONTAINER_TIMED_OUT = 69,
  // Prior to M104, RESTART_ABORTED was used for this.
  RESTART_REQUEST_CANCELLED = 70,
  CREATE_DISK_IMAGE_NO_RESPONSE = 71,
  CREATE_DISK_IMAGE_ALREADY_EXISTS = 72,
  UNINSTALL_TERMINA_FAILED = 73,
  START_LXD_FAILED_SIGNAL = 74,
  CONTAINER_CREATE_FAILED_SIGNAL = 75,
  STOP_VM_NO_RESPONSE = 76,
  SIGNAL_NOT_CONNECTED = 77,
  INSTALL_TERMINA_CANCELLED = 78,
  START_TIMED_OUT = 79,
  DISK_IMAGE_NO_RESPONSE = 80,
  DISK_IMAGE_IN_PROGRESS = 81,
  DISK_IMAGE_FAILED = 82,
  DISK_IMAGE_FAILED_NO_SPACE = 83,
  DISK_IMAGE_CANCELLED = 84,
  kMaxValue = DISK_IMAGE_CANCELLED,
  // When adding a new value, check you've followed the steps in the comment at
  // the top of this enum.
};

// Returns the string name of the CrostiniResult.
const char* CrostiniResultString(const CrostiniResult res);

using CrostiniSuccessCallback =
    base::OnceCallback<void(bool success, const std::string& failure_reason)>;

enum class RestartSource {
  kOther,
  kInstaller,
  kMultiContainerCreation,
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

enum class DiskImageProgressStatus {
  IN_PROGRESS,
  FAILURE_SPACE,
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
  BULLSEYE,
  BOOKWORM,
};

struct VmInfo {
  VmState state;
  vm_tools::concierge::VmInfo info;
};

struct StreamingExportStatus {
  uint32_t total_files;
  uint64_t total_bytes;
  uint32_t exported_files;
  uint64_t exported_bytes;
};

struct ContainerInfo {
  ContainerInfo(std::string name,
                std::string username,
                std::string homedir,
                std::string ipv4_address,
                uint32_t sftp_vsock_port = 0);
  ~ContainerInfo();
  ContainerInfo(ContainerInfo&&);
  ContainerInfo(const ContainerInfo&);
  ContainerInfo& operator=(ContainerInfo&&);
  ContainerInfo& operator=(const ContainerInfo&);

  std::string name;
  std::string username;
  base::FilePath homedir;
  std::string ipv4_address;
  uint32_t sftp_vsock_port;
};

// Return type when getting app icons from within a container.
struct Icon {
  std::string desktop_file_id;

  // Icon file content in specified format.
  std::string content;

  // Icon format (e.g. PNG, SVG)
  vm_tools::cicerone::DesktopIcon::Format format;
};

struct LinuxPackageInfo {
  LinuxPackageInfo();
  LinuxPackageInfo(LinuxPackageInfo&&);
  LinuxPackageInfo(const LinuxPackageInfo&);
  LinuxPackageInfo& operator=(LinuxPackageInfo&&);
  LinuxPackageInfo& operator=(const LinuxPackageInfo&);
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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CorruptionStates {
  MOUNT_FAILED = 0,
  MOUNT_ROLLED_BACK = 1,
  OTHER_CORRUPTION = 2,
  kMaxValue = OTHER_CORRUPTION,
};

// Dialog types used by CrostiniDialogStatusObserver.
enum class DialogType {
  INSTALLER,
  UPGRADER,
  REMOVER,
};

enum class UpgradeDialogEvent {
  kDialogShown = 0,
  kUpgradeSuccess = 1,
  kUpgradeCanceled = 2,
  kUpgradeFailed = 3,
  kNotStarted = 4,
  kDidBackup = 5,
  kBackupSucceeded = 6,
  kBackupFailed = 7,
  kDidRestore = 8,
  kRestoreSucceeded = 9,
  kRestoreFailed = 10,
  kMaxValue = kRestoreFailed,
};

// Keep this in sync with CrostiniDiskImageType in enums.xml
enum class CrostiniDiskImageType {
  kUnknown = 0,
  kQCow2Sparse = 1,
  kRawSparse = 2,
  kRawPreallocated = 3,
  kMultiDisk = 4,
  kMaxValue = kMultiDisk,
};

}  // namespace crostini

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ContainerOsVersion {
  kUnknown = 0,
  kDebianStretch = 1,
  kDebianBuster = 2,
  kDebianOther = 3,
  kOtherOs = 4,
  kDebianBullseye = 5,
  kDebianBookworm = 6,
  kMaxValue = kDebianBookworm,
};

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_SIMPLE_TYPES_H_
