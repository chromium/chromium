// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_HOSTNAME_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_HOSTNAME_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace policy {

// This class observes the device setting |DeviceHostname|, and calls
// NetworkStateHandler::SetHostname() appropriately based on the value of that
// setting.
class HostnameHandler : public chromeos::NetworkStateHandlerObserver {
 public:
  explicit HostnameHandler(chromeos::CrosSettings* cros_settings);
  ~HostnameHandler() override;

  // NetworkStateHandlerObserver overrides
  void DefaultNetworkChanged(const chromeos::NetworkState* network) override;

  void Shutdown();

 private:
  friend class HostnameHandlerTest;

  // Uses template to build a hostname. Returns valid hostname (after parameter
  // substitution) or empty string, if substitution result is not a valid
  // hostname.
  static std::string FormatHostname(const std::string& name_template,
                                    const std::string& asset_id,
                                    const std::string& serial,
                                    const std::string& mac,
                                    const std::string& machine_name,
                                    const std::string& location);

  void OnDeviceHostnamePropertyChanged();

  void OnDeviceHostnamePropertyChangedAndMachineStatisticsLoaded();

  chromeos::CrosSettings* cros_settings_;
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      policy_subscription_;
  base::WeakPtrFactory<HostnameHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HostnameHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_HOSTNAME_HANDLER_H_
