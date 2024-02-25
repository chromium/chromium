// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_DEVICE_EVENT_ROUTER_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_DEVICE_EVENT_ROUTER_H_

#include <map>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ash/extensions/file_manager/system_notification_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_observer.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace file_manager {

enum DeviceState {
  // Device is not being hard unplugged.
  DEVICE_STATE_USUAL,
  // Device is hard unplugged.
  DEVICE_HARD_UNPLUGGED,
  // Device is hard unplugged and reported to the JavaScript side.
  DEVICE_HARD_UNPLUGGED_AND_REPORTED
};

// Event router for device events.
class DeviceEventRouter : public VolumeManagerObserver,
                          public chromeos::PowerManagerClient::Observer {
 public:
  explicit DeviceEventRouter(SystemNotificationManager* notification_manager);

  // |overriding_time_delta| overrides time delta of delayed tasks for testing
  // |so that the tasks are executed by RunLoop::RunUntilIdle.
  DeviceEventRouter(SystemNotificationManager* notificaton_manager,
                    base::TimeDelta overriding_time_delta);

  DeviceEventRouter(const DeviceEventRouter&) = delete;
  DeviceEventRouter& operator=(const DeviceEventRouter&) = delete;

  ~DeviceEventRouter() override;

  // Turns the startup flag on, and then turns it off after few seconds.
  void Startup();

  // VolumeManagerObserver overrides.
  void OnDiskAdded(const ash::disks::Disk& disk, bool mounting) override;
  void OnDiskRemoved(const ash::disks::Disk& disk) override;
  void OnDeviceAdded(const std::string& device_path) override;
  void OnDeviceRemoved(const std::string& device_path) override;
  void OnVolumeMounted(ash::MountError error_code,
                       const Volume& volume) override;
  void OnVolumeUnmounted(ash::MountError error_code,
                         const Volume& volume) override;
  void OnFormatStarted(const std::string& device_path,
                       const std::string& device_label,
                       bool success) override;
  void OnFormatCompleted(const std::string& device_path,
                         const std::string& device_label,
                         bool success) override;
  void OnPartitionStarted(const std::string& device_path,
                          const std::string& device_label,
                          bool success) override;
  void OnPartitionCompleted(const std::string& device_path,
                            const std::string& device_label,
                            bool success) override;
  void OnRenameStarted(const std::string& device_path,
                       const std::string& device_label,
                       bool success) override;
  void OnRenameCompleted(const std::string& device_path,
                         const std::string& device_label,
                         bool success) override;

  // PowerManagerClient::Observer overrides.
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;

  bool is_resuming() const { return is_resuming_; }
  bool is_starting_up() const { return is_starting_up_; }

 protected:
  // Handles a device event containing |type| and |device_path|.
  virtual void OnDeviceEvent(
      extensions::api::file_manager_private::DeviceEventType type,
      const std::string& device_path,
      const std::string& device_label) = 0;
  // Returns external storage is disabled or not.
  virtual bool IsExternalStorageDisabled() = 0;

  SystemNotificationManager* system_notification_manager() {
    return notification_manager_;
  }

 private:
  void StartupDelayed();
  void SuspendDoneDelayed();

  // Obtains device state of the device having |device_path|.
  DeviceState GetDeviceState(const std::string& device_path) const;

  // Sets device state to the device having |device_path|.
  void SetDeviceState(const std::string& device_path, DeviceState state);

  raw_ptr<SystemNotificationManager> notification_manager_;

  // Whether to use zero time delta for testing or not.
  const base::TimeDelta resume_time_delta_;
  const base::TimeDelta startup_time_delta_;

  // Whether the profile is starting up or not.
  bool is_starting_up_;

  // Whether the system is resuming or not.
  bool is_resuming_;

  // Map of device path and device state.
  std::map<std::string, DeviceState> device_states_;

  // Thread checker.
  THREAD_CHECKER(thread_checker_);

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DeviceEventRouter> weak_factory_{this};
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_DEVICE_EVENT_ROUTER_H_
