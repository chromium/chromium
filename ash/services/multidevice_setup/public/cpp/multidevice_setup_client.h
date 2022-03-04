// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_MULTIDEVICE_SETUP_CLIENT_H_
#define ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_MULTIDEVICE_SETUP_CLIENT_H_

#include <memory>
#include <string>

#include "ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/observer_list.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

namespace multidevice_setup {

// Provides clients access to the MultiDeviceSetup API.
class MultiDeviceSetupClient {
 public:
  using HostStatusWithDevice =
      std::pair<ash::multidevice_setup::mojom::HostStatus,
                absl::optional<multidevice::RemoteDeviceRef>>;
  using FeatureStatesMap =
      base::flat_map<ash::multidevice_setup::mojom::Feature,
                     ash::multidevice_setup::mojom::FeatureState>;

  class Observer {
   public:
    // Called whenever the host status changes. If the host status is
    // HostStatus::kNoEligibleHosts or
    // HostStatus::kEligibleHostExistsButNoHostSet, the provided RemoteDeviceRef
    // will be null.
    virtual void OnHostStatusChanged(
        const HostStatusWithDevice& host_device_with_status) {}

    // Called whenever the state of any feature has changed.
    virtual void OnFeatureStatesChanged(
        const FeatureStatesMap& feature_states_map) {}

   protected:
    virtual ~Observer() = default;
  };

  using GetEligibleHostDevicesCallback =
      base::OnceCallback<void(const multidevice::RemoteDeviceRefList&)>;

  static HostStatusWithDevice GenerateDefaultHostStatusWithDevice();
  static FeatureStatesMap GenerateDefaultFeatureStatesMap(
      ash::multidevice_setup::mojom::FeatureState default_value);

  MultiDeviceSetupClient();

  MultiDeviceSetupClient(const MultiDeviceSetupClient&) = delete;
  MultiDeviceSetupClient& operator=(const MultiDeviceSetupClient&) = delete;

  virtual ~MultiDeviceSetupClient();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  virtual void GetEligibleHostDevices(
      GetEligibleHostDevicesCallback callback) = 0;
  virtual void SetHostDevice(
      const std::string& host_instance_id_or_legacy_device_id,
      const std::string& auth_token,
      ash::multidevice_setup::mojom::MultiDeviceSetup::SetHostDeviceCallback
          callback) = 0;
  virtual void RemoveHostDevice() = 0;
  virtual const HostStatusWithDevice& GetHostStatus() const = 0;
  virtual void SetFeatureEnabledState(
      ash::multidevice_setup::mojom::Feature feature,
      bool enabled,
      const absl::optional<std::string>& auth_token,
      ash::multidevice_setup::mojom::MultiDeviceSetup::
          SetFeatureEnabledStateCallback callback) = 0;
  virtual const FeatureStatesMap& GetFeatureStates() const = 0;
  ash::multidevice_setup::mojom::FeatureState GetFeatureState(
      ash::multidevice_setup::mojom::Feature feature) const;
  virtual void RetrySetHostNow(
      ash::multidevice_setup::mojom::MultiDeviceSetup::RetrySetHostNowCallback
          callback) = 0;
  virtual void TriggerEventForDebugging(
      ash::multidevice_setup::mojom::EventTypeForDebugging type,
      ash::multidevice_setup::mojom::MultiDeviceSetup::
          TriggerEventForDebuggingCallback callback) = 0;

 protected:
  void NotifyHostStatusChanged(
      const HostStatusWithDevice& host_status_with_device);
  void NotifyFeatureStateChanged(const FeatureStatesMap& feature_states_map);

 private:
  friend class MultiDeviceSetupClientImplTest;

  base::ObserverList<Observer>::Unchecked observer_list_;
};

std::string FeatureStatesMapToString(
    const MultiDeviceSetupClient::FeatureStatesMap& map);

}  // namespace multidevice_setup

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
namespace multidevice_setup {
using ::chromeos::multidevice_setup::MultiDeviceSetupClient;
}
}  // namespace ash

#endif  // ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_MULTIDEVICE_SETUP_CLIENT_H_
