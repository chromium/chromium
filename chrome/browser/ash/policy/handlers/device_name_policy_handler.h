// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_H_

#include <optional>
#include <string>

namespace policy {

// Provides the current device name policy, and provides
// hostname only if the template policy is active. Also notifies
// observers when the policy type and/or hostname changes.
class DeviceNamePolicyHandler {
 public:
  // Types of policies for device name functionality.
  enum class DeviceNamePolicy {
    // No device name policy in place.
    kNoPolicy,

    // Policy in place allowing administrator to specify a template
    // used to generate and format the hostname.
    kPolicyHostnameChosenByAdmin,

    // Policy in place which allows managed users to configure the device name.
    kPolicyHostnameConfigurableByManagedUser,

    // Policy in place which prohibits users from configuring device name.
    kPolicyHostnameNotConfigurable,
  };

  virtual ~DeviceNamePolicyHandler();

  // Provides hostname if requested by administrator.
  // Returns null if no hostname was requested by administrator.
  virtual std::optional<std::string> GetHostnameChosenByAdministrator()
      const = 0;

 protected:
  DeviceNamePolicyHandler();
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_H_
