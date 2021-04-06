// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_SYSTEM_PROXY_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_SYSTEM_PROXY_HANDLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/settings/cros_settings.h"

namespace chromeos {
class SystemProxyManager;
}

namespace policy {

// This class observes the device setting |SystemProxySettings|, and controls
// the availability of the System-proxy service and the configuration of the web
// proxy credentials for system services connecting through System-proxy.
class SystemProxyHandler {
 public:
  SystemProxyHandler(chromeos::CrosSettings* cros_settings);

  SystemProxyHandler(const SystemProxyHandler&) = delete;
  SystemProxyHandler& operator=(const SystemProxyHandler&) = delete;

  ~SystemProxyHandler();

  void SetSystemProxyManagerForTesting(
      chromeos::SystemProxyManager* system_proxy_manager);

 private:
  void OnSystemProxySettingsPolicyChanged();

  chromeos::SystemProxyManager* GetSystemProxyManager();

  // Owned by the test fixture.
  chromeos::SystemProxyManager* system_proxy_manager_for_testing_ = nullptr;
  chromeos::CrosSettings* cros_settings_;
  base::CallbackListSubscription system_proxy_subscription_;

  base::WeakPtrFactory<SystemProxyHandler> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_SYSTEM_PROXY_HANDLER_H_
