// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHROMEBOX_FOR_MEETINGS_DEVICE_INFO_DEVICE_INFO_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_CHROMEBOX_FOR_MEETINGS_DEVICE_INFO_DEVICE_INFO_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/chromeos/chromebox_for_meetings/service_adaptor.h"
#include "chromeos/dbus/chromebox_for_meetings/cfm_observer.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_info.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {
namespace cfm {

// Implementation of the DeviceInfo Service.
// Allowing query to relevant device information
class DeviceInfoService : public CfmObserver,
                          public ServiceAdaptor::Delegate,
                          public mojom::MeetDevicesInfo,
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

  // mojom::ServiceAdaptorDelegate implementation
  void OnBindService(mojo::ScopedMessagePipeHandle receiver_pipe) override;
  void OnAdaptorConnect(bool success) override;
  void OnAdaptorDisconnect() override;

  // DeviceSettingsService::Observer impl
  void DeviceSettingsUpdated() override;
  void OnDeviceSettingsServiceShutdown() override;

  // ::mojom::DeviceInfo implementation
  void AddDeviceSettingsObserver(
      ::mojo::PendingRemote<mojom::PolicyInfoObserver> observer) override;
  void GetPolicyInfo(GetPolicyInfoCallback callback) override;
  void GetSysInfo(GetSysInfoCallback callback) override;

  // Query data policy information and notify observers if there is a change.
  void UpdatePolicyInfo();

 private:
  DeviceInfoService();
  ~DeviceInfoService() override;

  // Cleanup the service on mojom connection loss.
  // Called after the DeviceInfo Service is no longer discoverable.
  void Reset();

  ServiceAdaptor service_adaptor_;
  mojo::ReceiverSet<mojom::MeetDevicesInfo> receivers_;
  mojo::RemoteSet<mojom::PolicyInfoObserver> policy_remotes_;

  mojom::PolicyInfoPtr current_policy_info_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DeviceInfoService> weak_ptr_factory_{this};
};

}  // namespace cfm
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHROMEBOX_FOR_MEETINGS_DEVICE_INFO_DEVICE_INFO_SERVICE_H_
