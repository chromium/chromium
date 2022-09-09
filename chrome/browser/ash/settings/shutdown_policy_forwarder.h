// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SETTINGS_SHUTDOWN_POLICY_FORWARDER_H_
#define CHROME_BROWSER_ASH_SETTINGS_SHUTDOWN_POLICY_FORWARDER_H_

#include "chrome/browser/ash/settings/shutdown_policy_handler.h"

namespace ash {

// Forwards the current DeviceRebootOnShutdown policy.
class ShutdownPolicyForwarder : public ShutdownPolicyHandler::Delegate {
 public:
  ShutdownPolicyForwarder();

  ShutdownPolicyForwarder(const ShutdownPolicyForwarder&) = delete;
  ShutdownPolicyForwarder& operator=(const ShutdownPolicyForwarder&) = delete;

  ~ShutdownPolicyForwarder() override;

 private:
  // ShutdownPolicyHandler::Delegate:
  void OnShutdownPolicyChanged(bool reboot_on_shutdown) override;

  ShutdownPolicyHandler shutdown_policy_handler_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SETTINGS_SHUTDOWN_POLICY_FORWARDER_H_
