// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_FEATURES_H_
#define CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_FEATURES_H_

#include <string>

#include "base/functional/callback.h"

class Profile;

namespace plugin_vm {

// PluginVmFeatures provides an interface for querying which parts of Plugin VM
// are enabled or allowed.
class PluginVmFeatures {
 public:
  static PluginVmFeatures* Get();

  enum class ProfileSupported {
    kOk,
    kErrorNonPrimary,
    kErrorChildAccount,
    kErrorOffTheRecord,
    kErrorEphemeral,
    // This is for all the other cases where the profile is not supported.
    kErrorNotSupported,
  };

  enum class PolicyConfigured {
    kOk,
    kErrorUnableToCheckPolicy,
    kErrorNotEnterpriseEnrolled,
    kErrorUserNotAffiliated,
    kErrorUnableToCheckDevicePolicy,
    kErrorNotAllowedByDevicePolicy,
    kErrorNotAllowedByUserPolicy,
    kErrorLicenseNotSetUp,
    kErrorVirtualMachinesNotAllowed,
  };

  // Remember to update `plugin_vm::GetDiagnostics()` when this struct or the
  // members change.
  struct IsAllowedDiagnostics {
    bool device_supported;
    ProfileSupported profile_supported;
    PolicyConfigured policy_configured;

    bool IsOk() const;
    // An empty string is returned if there is no error (i.e. `IsOk()`
    // returns true).
    std::string GetTopError() const;
  };

  // Check if Plugin VM is allowed for the current profile and return
  // diagnostics.
  virtual IsAllowedDiagnostics GetIsAllowedDiagnostics(const Profile* profile);

  // Checks if Plugin VM is allowed for the current profile and provides
  // a reason if it is not allowed. The reason string is to only be used
  // in crosh/vmc error messages.
  virtual bool IsAllowed(const Profile* profile, std::string* reason = nullptr);

  // Returns whether Plugin VM is installed. Using vmc may cause this to return
  // an incorrect value, e.g. by renaming or deleting VMs.
  virtual bool IsConfigured(const Profile* profile);

  // Returns true if Plugin VM is allowed and configured for the current
  // profile.
  virtual bool IsEnabled(const Profile* profile);

 protected:
  static void SetForTesting(PluginVmFeatures* features);

  PluginVmFeatures();
  virtual ~PluginVmFeatures();

 private:
  PluginVmFeatures(PluginVmFeatures&& registration) = delete;
  PluginVmFeatures& operator=(PluginVmFeatures&& registration) = delete;
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_FEATURES_H_
