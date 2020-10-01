// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_VOLUME_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_VOLUME_MANAGER_OBSERVER_H_

#include <string>

#include "chromeos/dbus/cros_disks_client.h"

namespace chromeos {
namespace disks {
class Disk;
}  // namespace disks
}  // namespace chromeos

namespace file_manager {

class Volume;

// Observer interface of volume related events.
class VolumeManagerObserver {
 public:
  virtual ~VolumeManagerObserver() = default;

  // Fired when a new disk is added.
  virtual void OnDiskAdded(const chromeos::disks::Disk& disk, bool mounting) {}

  // Fired when a disk is removed.
  virtual void OnDiskRemoved(const chromeos::disks::Disk& disk) {}

  // Fired when a new device is added.
  virtual void OnDeviceAdded(const std::string& device_path) {}

  // Fired when a device is removed.
  virtual void OnDeviceRemoved(const std::string& device_path) {}

  // Fired when a volume is mounted.
  virtual void OnVolumeMounted(chromeos::MountError error_code,
                               const Volume& volume) {}

  // Fired when a volume is unmounted.
  virtual void OnVolumeUnmounted(chromeos::MountError error_code,
                                 const Volume& volume) {}

  // Fired when formatting a device is started (or failed to start).
  virtual void OnFormatStarted(const std::string& device_path,
                               const std::string& device_label,
                               bool success) {}

  // Fired when formatting a device is completed (or terminated on error).
  virtual void OnFormatCompleted(const std::string& device_path,
                                 const std::string& device_label,
                                 bool success) {}

  // Fired when partitioning a device is started.
  virtual void OnPartitionStarted(const std::string& device_path,
                                  const std::string& device_label,
                                  bool success) {}

  // Fired when partitioning a device is completed (or terminated on error).
  virtual void OnPartitionCompleted(const std::string& device_path,
                                    const std::string& device_label,
                                    bool success) {}

  // Fired when renaming a device is started (or failed to start).
  virtual void OnRenameStarted(const std::string& device_path,
                               const std::string& device_label,
                               bool success) {}

  // Fired when renaming a device is completed (or terminated on error).
  virtual void OnRenameCompleted(const std::string& device_path,
                                 const std::string& device_label,
                                 bool success) {}
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_VOLUME_MANAGER_OBSERVER_H_
