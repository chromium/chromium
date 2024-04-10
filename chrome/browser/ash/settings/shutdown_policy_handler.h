// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SETTINGS_SHUTDOWN_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_SETTINGS_SHUTDOWN_POLICY_HANDLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/settings/cros_settings.h"

namespace ash {

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

  ShutdownPolicyHandler(const ShutdownPolicyHandler&) = delete;
  ShutdownPolicyHandler& operator=(const ShutdownPolicyHandler&) = delete;

  ~ShutdownPolicyHandler();

  // Once a trusted set of policies is established, this function notifies
  // |delegate_| with the trusted state of the |DeviceRebootOnShutdown| policy.
  void NotifyDelegateWithShutdownPolicy();

 private:
  raw_ptr<CrosSettings> cros_settings_;

  raw_ptr<Delegate> delegate_;

  base::CallbackListSubscription shutdown_policy_subscription_;

  base::WeakPtrFactory<ShutdownPolicyHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SETTINGS_SHUTDOWN_POLICY_HANDLER_H_
