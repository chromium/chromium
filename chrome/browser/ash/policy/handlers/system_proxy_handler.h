// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_SYSTEM_PROXY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_SYSTEM_PROXY_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/settings/cros_settings.h"

namespace ash {
class SystemProxyManager;
}

namespace policy {

// This class observes the device setting |SystemProxySettings|, and controls
// the availability of the System-proxy service and the configuration of the web
// proxy credentials for system services connecting through System-proxy.
class SystemProxyHandler {
 public:
  explicit SystemProxyHandler(ash::CrosSettings* cros_settings);

  SystemProxyHandler(const SystemProxyHandler&) = delete;
  SystemProxyHandler& operator=(const SystemProxyHandler&) = delete;

  ~SystemProxyHandler();

  void SetSystemProxyManagerForTesting(
      ash::SystemProxyManager* system_proxy_manager);

 private:
  void OnSystemProxySettingsPolicyChanged();

  ash::SystemProxyManager* GetSystemProxyManager();

  // Owned by the test fixture.
  raw_ptr<ash::SystemProxyManager, DanglingUntriaged>
      system_proxy_manager_for_testing_ = nullptr;
  raw_ptr<ash::CrosSettings> cros_settings_;
  base::CallbackListSubscription system_proxy_subscription_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_SYSTEM_PROXY_HANDLER_H_
