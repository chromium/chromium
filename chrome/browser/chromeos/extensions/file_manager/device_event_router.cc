// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/device_event_router.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chromeos/disks/disk.h"
#include "content/public/browser/browser_thread.h"

namespace file_manager {
namespace {
namespace file_manager_private = extensions::api::file_manager_private;
using content::BrowserThread;
}  // namespace

DeviceEventRouter::DeviceEventRouter()
    : resume_time_delta_(base::TimeDelta::FromSeconds(10)),
      startup_time_delta_(base::TimeDelta::FromSeconds(10)),
      is_starting_up_(false),
      is_resuming_(false) {}

DeviceEventRouter::DeviceEventRouter(base::TimeDelta overriding_time_delta)
    : resume_time_delta_(overriding_time_delta),
      startup_time_delta_(overriding_time_delta),
      is_starting_up_(false),
      is_resuming_(false) {}

DeviceEventRouter::~DeviceEventRouter() = default;

void DeviceEventRouter::Startup() {
  is_starting_up_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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
    OnDeviceEvent(file_manager_private::DEVICE_EVENT_TYPE_DISABLED,
                  device_path);
    return;
  }
}

void DeviceEventRouter::OnDeviceRemoved(const std::string& device_path) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SetDeviceState(device_path, DEVICE_STATE_USUAL);
  OnDeviceEvent(file_manager_private::DEVICE_EVENT_TYPE_REMOVED, device_path);
}

void DeviceEventRouter::OnDiskAdded(const chromeos::disks::Disk& disk,
                                    bool mounting) {
  // Do nothing.
}

void DeviceEventRouter::OnDiskRemoved(const chromeos::disks::Disk& disk) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (is_resuming_ || is_starting_up_)
    return;

  const std::string& device_path = disk.storage_device_path();
  if (!disk.is_read_only() && disk.is_mounted() &&
      GetDeviceState(device_path) != DEVICE_HARD_UNPLUGGED_AND_REPORTED) {
    OnDeviceEvent(file_manager_private::DEVICE_EVENT_TYPE_HARD_UNPLUGGED,
                  device_path);
    SetDeviceState(device_path, DEVICE_HARD_UNPLUGGED_AND_REPORTED);
  }
}

void DeviceEventRouter::OnVolumeMounted(chromeos::MountError error_code,
                                        const Volume& volume) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const std::string& device_path = volume.storage_device_path().AsUTF8Unsafe();
  SetDeviceState(device_path, DEVICE_STATE_USUAL);
}

void DeviceEventRouter::OnVolumeUnmounted(chromeos::MountError error_code,
                                          const Volume& volume) {
  // Do nothing.
}

void DeviceEventRouter::OnFormatStarted(const std::string& device_path,
                                        bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (success) {
    OnDeviceEvent(file_manager_private::DEVICE_EVENT_TYPE_FORMAT_START,
                  device_path);
  } else {
    OnDeviceEvent(file_manager_private::DEVICE_EVENT_TYPE_FORMAT_FAIL,
                  device_path);
  }
}

void DeviceEventRouter::OnFormatCompleted(const std::string& device_path,
                                          bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  OnDeviceEvent(success ? file_manager_private::DEVICE_EVENT_TYPE_FORMAT_SUCCESS
                        : file_manager_private::DEVICE_EVENT_TYPE_FORMAT_FAIL,
                device_path);
}

void DeviceEventRouter::OnRenameStarted(const std::string& device_path,
                                        bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  OnDeviceEvent(success ? file_manager_private::DEVICE_EVENT_TYPE_RENAME_START
                        : file_manager_private::DEVICE_EVENT_TYPE_RENAME_FAIL,
                device_path);
}

void DeviceEventRouter::OnRenameCompleted(const std::string& device_path,
                                          bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  OnDeviceEvent(success ? file_manager_private::DEVICE_EVENT_TYPE_RENAME_SUCCESS
                        : file_manager_private::DEVICE_EVENT_TYPE_RENAME_FAIL,
                device_path);
}

void DeviceEventRouter::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  is_resuming_ = true;
}

void DeviceEventRouter::SuspendDone(const base::TimeDelta& sleep_duration) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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
    if (it != device_states_.end())
      device_states_.erase(it);
  }
}

}  // namespace file_manager
