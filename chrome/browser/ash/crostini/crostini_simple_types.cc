// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_simple_types.h"

namespace crostini {

const char* CrostiniResultString(const CrostiniResult res) {
#define ENTRY(name)          \
  case CrostiniResult::name: \
    return #name
  switch (res) {
    ENTRY(SUCCESS);
    ENTRY(CREATE_DISK_IMAGE_FAILED);
    ENTRY(VM_START_FAILED);
    ENTRY(VM_STOP_FAILED);
    ENTRY(DESTROY_DISK_IMAGE_FAILED);
    ENTRY(LIST_VM_DISKS_FAILED);
    ENTRY(CLIENT_ERROR);
    ENTRY(CONTAINER_DOWNLOAD_TIMED_OUT);
    ENTRY(CONTAINER_CREATE_CANCELLED);
    ENTRY(CONTAINER_CREATE_FAILED);
    ENTRY(CONTAINER_START_CANCELLED);
    ENTRY(CONTAINER_START_FAILED);
    ENTRY(INSTALL_LINUX_PACKAGE_FAILED);
    ENTRY(BLOCKING_OPERATION_ALREADY_ACTIVE);
    ENTRY(UNINSTALL_PACKAGE_FAILED);
    ENTRY(SSHFS_MOUNT_ERROR);
    ENTRY(OFFLINE_WHEN_UPGRADE_REQUIRED);
    ENTRY(LOAD_COMPONENT_FAILED);
    ENTRY(CROSTINI_UNINSTALLER_RUNNING);
    ENTRY(UNKNOWN_ERROR);
    ENTRY(CONTAINER_EXPORT_IMPORT_FAILED);
    ENTRY(CONTAINER_EXPORT_IMPORT_FAILED_VM_STOPPED);
    ENTRY(CONTAINER_EXPORT_IMPORT_FAILED_VM_STARTED);
    ENTRY(CONTAINER_EXPORT_IMPORT_FAILED_ARCHITECTURE);
    ENTRY(DISK_IMAGE_NO_RESPONSE);
    ENTRY(DISK_IMAGE_IN_PROGRESS);
    ENTRY(DISK_IMAGE_FAILED_NO_SPACE);
    ENTRY(DISK_IMAGE_FAILED);
    ENTRY(DISK_IMAGE_CANCELLED);
    ENTRY(NOT_ALLOWED);
    ENTRY(CONTAINER_EXPORT_IMPORT_FAILED_SPACE);
    ENTRY(GET_CONTAINER_SSH_KEYS_FAILED);
    ENTRY(CONTAINER_EXPORT_IMPORT_CANCELLED);
    ENTRY(RESTART_ABORTED);
    ENTRY(RESTART_FAILED_VM_STOPPED);
    ENTRY(UPGRADE_CONTAINER_STARTED);
    ENTRY(UPGRADE_CONTAINER_ALREADY_RUNNING);
    ENTRY(UPGRADE_CONTAINER_NOT_SUPPORTED);
    ENTRY(UPGRADE_CONTAINER_ALREADY_UPGRADED);
    ENTRY(UPGRADE_CONTAINER_FAILED);
    ENTRY(CANCEL_UPGRADE_CONTAINER_FAILED);
    ENTRY(CONCIERGE_START_FAILED);
    ENTRY(CONTAINER_CONFIGURATION_FAILED);
    ENTRY(LOAD_COMPONENT_UPDATE_IN_PROGRESS);
    ENTRY(NEVER_FINISHED);
    ENTRY(CONTAINER_SETUP_FAILED);
    ENTRY(START_LXD_FAILED);
    ENTRY(INSTALL_IMAGE_LOADER_TIMED_OUT);
    ENTRY(CREATE_DISK_IMAGE_TIMED_OUT);
    ENTRY(START_TERMINA_VM_TIMED_OUT);
    ENTRY(START_LXD_TIMED_OUT);
    ENTRY(CREATE_CONTAINER_TIMED_OUT);
    ENTRY(SETUP_CONTAINER_TIMED_OUT);
    ENTRY(START_CONTAINER_TIMED_OUT);
    ENTRY(FETCH_SSH_KEYS_TIMED_OUT);
    ENTRY(MOUNT_CONTAINER_TIMED_OUT);
    ENTRY(UNKNOWN_STATE_TIMED_OUT);
    ENTRY(NEED_UPDATE);
    ENTRY(SHARE_PATHS_FAILED);
    ENTRY(UNREGISTERED_APPLICATION);
    ENTRY(VSH_CONNECT_FAILED);
    ENTRY(CONTAINER_STOP_FAILED);
    ENTRY(CONTAINER_STOP_CANCELLED);
    ENTRY(WAYLAND_SERVER_CREATION_FAILED);
    ENTRY(CONFIGURE_CONTAINER_TIMED_OUT);
    ENTRY(RESTART_REQUEST_CANCELLED);
    ENTRY(CREATE_DISK_IMAGE_NO_RESPONSE);
    ENTRY(CREATE_DISK_IMAGE_ALREADY_EXISTS);
    ENTRY(UNINSTALL_TERMINA_FAILED);
    ENTRY(START_LXD_FAILED_SIGNAL);
    ENTRY(CONTAINER_CREATE_FAILED_SIGNAL);
    ENTRY(STOP_VM_NO_RESPONSE);
    ENTRY(SIGNAL_NOT_CONNECTED);
    ENTRY(INSTALL_TERMINA_CANCELLED);
    ENTRY(START_TIMED_OUT);
  }
#undef ENTRY
  return "unknown code";
}

LinuxPackageInfo::LinuxPackageInfo() = default;
LinuxPackageInfo::LinuxPackageInfo(LinuxPackageInfo&&) = default;
LinuxPackageInfo::LinuxPackageInfo(const LinuxPackageInfo&) = default;
LinuxPackageInfo& LinuxPackageInfo::operator=(LinuxPackageInfo&&) = default;
LinuxPackageInfo& LinuxPackageInfo::operator=(const LinuxPackageInfo&) =
    default;
LinuxPackageInfo::~LinuxPackageInfo() = default;

ContainerInfo::ContainerInfo(std::string container_name,
                             std::string container_username,
                             std::string container_homedir,
                             std::string ipv4_address,
                             uint32_t sftp_vsock_port)
    : name(std::move(container_name)),
      username(std::move(container_username)),
      homedir(std::move(container_homedir)),
      ipv4_address(std::move(ipv4_address)),
      sftp_vsock_port(sftp_vsock_port) {}
ContainerInfo::~ContainerInfo() = default;
ContainerInfo::ContainerInfo(ContainerInfo&&) = default;
ContainerInfo::ContainerInfo(const ContainerInfo&) = default;
ContainerInfo& ContainerInfo::operator=(ContainerInfo&&) = default;
ContainerInfo& ContainerInfo::operator=(const ContainerInfo&) = default;
}  // namespace crostini
