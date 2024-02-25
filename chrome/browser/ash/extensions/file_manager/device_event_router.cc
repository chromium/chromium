// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/device_event_router.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chromeos/ash/components/disks/disk.h"
#include "content/public/browser/browser_thread.h"

namespace file_manager {

namespace file_manager_private = extensions::api::file_manager_private;
using content::BrowserThread;

DeviceEventRouter::DeviceEventRouter(
    SystemNotificationManager* notification_manager)
    : notification_manager_(notification_manager),
      resume_time_delta_(base::Seconds(10)),
      startup_time_delta_(base::Seconds(10)),
      is_starting_up_(false),
      is_resuming_(false) {}

DeviceEventRouter::DeviceEventRouter(
    SystemNotificationManager* notification_manager,
    base::TimeDelta overriding_time_delta)
    : notification_manager_(notification_manager),
      resume_time_delta_(overriding_time_delta),
      startup_time_delta_(overriding_time_delta),
      is_starting_up_(false),
      is_resuming_(false) {}

DeviceEventRouter::~DeviceEventRouter() = default;

void DeviceEventRouter::Startup() {
  is_starting_up_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DeviceEventRouter::StartupDelayed,
                     weak_factory_.GetWeakPtr()),
      startup_time_delta_);
}

void DeviceEventRouter::StartupDelayed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  is_starting_up_ = false;
}

void DeviceEventRouter::OnDeviceAdded(const std::string& device_path) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  SetDeviceState(device_path, DEVICE_STATE_USUAL);
  if (IsExternalStorageDisabled()) {
    OnDeviceEvent(file_manager_private::DeviceEventType::kDisabled, device_path,
                  "");
    return;
  }
}

void DeviceEventRouter::OnDeviceRemoved(const std::string& device_path) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SetDeviceState(device_path, DEVICE_STATE_USUAL);
  OnDeviceEvent(file_manager_private::DeviceEventType::kRemoved, device_path,
                "");
}

void DeviceEventRouter::OnDiskAdded(const ash::disks::Disk& disk,
                                    bool mounting) {
  // Do nothing.
}

void DeviceEventRouter::OnDiskRemoved(const ash::disks::Disk& disk) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (is_resuming_ || is_starting_up_) {
    return;
  }

  const std::string& device_path = disk.storage_device_path();
  if (!disk.is_read_only() && disk.is_mounted() &&
      GetDeviceState(device_path) != DEVICE_HARD_UNPLUGGED_AND_REPORTED) {
    OnDeviceEvent(file_manager_private::DeviceEventType::kHardUnplugged,
                  device_path, disk.device_label());
    SetDeviceState(device_path, DEVICE_HARD_UNPLUGGED_AND_REPORTED);
  }
}

void DeviceEventRouter::OnVolumeMounted(ash::MountError error_code,
                                        const Volume& volume) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const std::string& device_path = volume.storage_device_path().AsUTF8Unsafe();
  SetDeviceState(device_path, DEVICE_STATE_USUAL);
}

void DeviceEventRouter::OnVolumeUnmounted(ash::MountError error_code,
                                          const Volume& volume) {
  // Do nothing.
}

void DeviceEventRouter::OnFormatStarted(const std::string& device_path,
                                        const std::string& device_label,
                                        bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (success) {
    OnDeviceEvent(file_manager_private::DeviceEventType::kFormatStart,
                  device_path, device_label);
  } else {
    OnDeviceEvent(file_manager_private::DeviceEventType::kFormatFail,
                  device_path, device_label);
  }
}

void DeviceEventRouter::OnFormatCompleted(const std::string& device_path,
                                          const std::string& device_label,
                                          bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  OnDeviceEvent(success ? file_manager_private::DeviceEventType::kFormatSuccess
                        : file_manager_private::DeviceEventType::kFormatFail,
                device_path, device_label);
}

void DeviceEventRouter::OnPartitionStarted(const std::string& device_path,
                                           const std::string& device_label,
                                           bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (success) {
    OnDeviceEvent(file_manager_private::DeviceEventType::kPartitionStart,
                  device_path, device_label);
  } else {
    OnDeviceEvent(file_manager_private::DeviceEventType::kPartitionFail,
                  device_path, device_label);
  }
}

void DeviceEventRouter::OnPartitionCompleted(const std::string& device_path,
                                             const std::string& device_label,
                                             bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  OnDeviceEvent(success
                    ? file_manager_private::DeviceEventType::kPartitionSuccess
                    : file_manager_private::DeviceEventType::kPartitionFail,
                device_path, device_label);
}

void DeviceEventRouter::OnRenameStarted(const std::string& device_path,
                                        const std::string& device_label,
                                        bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  OnDeviceEvent(success ? file_manager_private::DeviceEventType::kRenameStart
                        : file_manager_private::DeviceEventType::kRenameFail,
                device_path, device_label);
}

void DeviceEventRouter::OnRenameCompleted(const std::string& device_path,
                                          const std::string& device_label,
                                          bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  OnDeviceEvent(success ? file_manager_private::DeviceEventType::kRenameSuccess
                        : file_manager_private::DeviceEventType::kRenameFail,
                device_path, device_label);
}

void DeviceEventRouter::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  is_resuming_ = true;
}

void DeviceEventRouter::SuspendDone(base::TimeDelta sleep_duration) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DeviceEventRouter::SuspendDoneDelayed,
                     weak_factory_.GetWeakPtr()),
      resume_time_delta_);
}

void DeviceEventRouter::SuspendDoneDelayed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  is_resuming_ = false;
}

DeviceState DeviceEventRouter::GetDeviceState(
    const std::string& device_path) const {
  const std::map<std::string, DeviceState>::const_iterator it =
      device_states_.find(device_path);
  return it != device_states_.end() ? it->second : DEVICE_STATE_USUAL;
}

void DeviceEventRouter::SetDeviceState(const std::string& device_path,
                                       DeviceState state) {
  if (state != DEVICE_STATE_USUAL) {
    device_states_[device_path] = state;
  } else {
    const std::map<std::string, DeviceState>::iterator it =
        device_states_.find(device_path);
    if (it != device_states_.end()) {
      device_states_.erase(it);
    }
  }
}

}  // namespace file_manager
