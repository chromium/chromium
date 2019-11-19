// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_SHUTDOWN_POLICY_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_SHUTDOWN_POLICY_HANDLER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"

namespace chromeos {

// This class observes the device setting |DeviceRebootOnShutdown|. Changes to
// this policy are communicated to the ShutdownPolicyHandler::Delegate by
// calling its OnShutdownPolicyChanged method with the new state of the policy.
class ShutdownPolicyHandler {
 public:
  // This delegate is called when the |DeviceRebootOnShutdown| policy changes.
  // NotifyDelegateWithShutdownPolicy() can manually request a notification.
  class Delegate {
   public:
    virtual void OnShutdownPolicyChanged(bool reboot_on_shutdown) = 0;

   protected:
    virtual ~Delegate() {}
  };

  ShutdownPolicyHandler(CrosSettings* cros_settings, Delegate* delegate);
  ~ShutdownPolicyHandler();

  // Once a trusted set of policies is established, this function notifies
  // |delegate_| with the trusted state of the |DeviceRebootOnShutdown| policy.
  void NotifyDelegateWithShutdownPolicy();

 private:
  CrosSettings* cros_settings_;

  Delegate* delegate_;

  std::unique_ptr<CrosSettings::ObserverSubscription>
      shutdown_policy_subscription_;

  base::WeakPtrFactory<ShutdownPolicyHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ShutdownPolicyHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_SHUTDOWN_POLICY_HANDLER_H_
