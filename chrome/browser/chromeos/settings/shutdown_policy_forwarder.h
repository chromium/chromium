// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_SHUTDOWN_POLICY_FORWARDER_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_SHUTDOWN_POLICY_FORWARDER_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/settings/shutdown_policy_handler.h"

namespace chromeos {

// Forwards the current DeviceRebootOnShutdown policy to ash.
class ShutdownPolicyForwarder : public ShutdownPolicyHandler::Delegate {
 public:
  ShutdownPolicyForwarder();
  ~ShutdownPolicyForwarder() override;

 private:
  // ShutdownPolicyHandler::Delegate:
  void OnShutdownPolicyChanged(bool reboot_on_shutdown) override;

  ShutdownPolicyHandler shutdown_policy_handler_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownPolicyForwarder);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_SHUTDOWN_POLICY_FORWARDER_H_
