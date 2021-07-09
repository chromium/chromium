// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_IMPL_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/handlers/device_name_policy_handler.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/system/statistics_provider.h"

namespace policy {

// This class observes the device setting |DeviceHostname|, and calls
// NetworkStateHandler::SetHostname() appropriately based on the value of that
// setting.
class DeviceNamePolicyHandlerImpl
    : public DeviceNamePolicyHandler,
      public chromeos::NetworkStateHandlerObserver {
 public:
  explicit DeviceNamePolicyHandlerImpl(ash::CrosSettings* cros_settings);
  ~DeviceNamePolicyHandlerImpl() override;

  // DeviceNamePolicyHandler:
  DeviceNamePolicy GetDeviceNamePolicy() const override;
  absl::optional<std::string> GetHostnameChosenByAdministrator() const override;

 private:
  friend class DeviceNamePolicyHandlerImplTest;

  DeviceNamePolicyHandlerImpl(
      ash::CrosSettings* cros_settings,
      chromeos::system::StatisticsProvider* statistics_provider);

  // NetworkStateHandlerObserver overrides
  void DefaultNetworkChanged(const chromeos::NetworkState* network) override;

  void OnDeviceHostnamePropertyChanged();

  void OnDeviceHostnamePropertyChangedAndMachineStatisticsLoaded();

  // Sets new device name policy value if different from the current policy
  // value .
  void SetDeviceNamePolicy(DeviceNamePolicy policy);

  ash::CrosSettings* cros_settings_;
  chromeos::system::StatisticsProvider* statistics_provider_;

  DeviceNamePolicy device_name_policy_ = DeviceNamePolicy::kNoPolicy;
  base::CallbackListSubscription policy_subscription_;
  std::string hostname_;
  base::WeakPtrFactory<DeviceNamePolicyHandlerImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceNamePolicyHandlerImpl);
};

std::ostream& operator<<(
    std::ostream& stream,
    const DeviceNamePolicyHandlerImpl::DeviceNamePolicy& state);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_IMPL_H_
