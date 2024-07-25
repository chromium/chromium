// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_DEVICE_INFO_DEVICE_INFO_SERVICE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_DEVICE_INFO_DEVICE_INFO_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_observer.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_adaptor.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_info.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::cfm {

// Implementation of the DeviceInfo Service.
// Allowing query to relevant device information
class DeviceInfoService : public CfmObserver,
                          public chromeos::cfm::ServiceAdaptor::Delegate,
                          public chromeos::cfm::mojom::MeetDevicesInfo,
                          public DeviceSettingsService::Observer {
 public:
  DeviceInfoService(const DeviceInfoService&) = delete;
  DeviceInfoService& operator=(const DeviceInfoService&) = delete;

  // Manage singleton instance.
  static void Initialize();
  static void Shutdown();
  static DeviceInfoService* Get();
  static bool IsInitialized();

 protected:
  // mojom::CfmObserver implementation
  bool ServiceRequestReceived(const std::string& interface_name) override;

  // chromeos::cfm::ServiceAdaptor::Delegate implementation
  void OnBindService(mojo::ScopedMessagePipeHandle receiver_pipe) override;
  void OnAdaptorConnect(bool success) override;
  void OnAdaptorDisconnect() override;

  // DeviceSettingsService::Observer impl
  void DeviceSettingsUpdated() override;
  void OnDeviceSettingsServiceShutdown() override;

  // mojom::MeetDevicesInfo implementation
  void AddDeviceSettingsObserver(
      ::mojo::PendingRemote<chromeos::cfm::mojom::PolicyInfoObserver> observer)
      override;
  void GetPolicyInfo(GetPolicyInfoCallback callback) override;
  void GetSysInfo(GetSysInfoCallback callback) override;
  void GetMachineStatisticsInfo(
      GetMachineStatisticsInfoCallback callback) override;

  // Query data policy information and notify observers if there is a change.
  void UpdatePolicyInfo();

 private:
  DeviceInfoService();
  ~DeviceInfoService() override;

  // Populate mojom with information from PolicyInfo
  void PopulatePolicyInfoFromProto(
      chromeos::cfm::mojom::PolicyInfoPtr& policy_info);

  // Populate mojom with information from ChromeDeviceSettingsProto
  void PopulateChromeDeviceSettingsFromProto(
      chromeos::cfm::mojom::PolicyInfoPtr& policy_info);

  // Update boolean indicating if Machine Statistics have loaded.
  void ScheduleOnMachineStatisticsLoaded();

  void SetOnMachineStatisticsLoaded(bool loaded);

  // Cleanup the service on mojom connection loss.
  // Called after the DeviceInfo Service is no longer discoverable.
  void Reset();

  chromeos::cfm::ServiceAdaptor service_adaptor_;
  mojo::ReceiverSet<chromeos::cfm::mojom::MeetDevicesInfo> receivers_;
  mojo::RemoteSet<chromeos::cfm::mojom::PolicyInfoObserver> policy_remotes_;

  chromeos::cfm::mojom::PolicyInfoPtr current_policy_info_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  bool on_machine_statistics_loaded_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DeviceInfoService> weak_ptr_factory_{this};
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_DEVICE_INFO_DEVICE_INFO_SERVICE_H_
