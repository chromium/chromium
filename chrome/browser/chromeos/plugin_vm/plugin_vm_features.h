// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_FEATURES_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_FEATURES_H_

#include "base/callback.h"

class Profile;

namespace plugin_vm {

// PluginVmFeatures provides an interface for querying which parts of Plugin VM
// are enabled or allowed.
class PluginVmFeatures {
 public:
  static PluginVmFeatures* Get();

  // Checks if Plugin VM is allowed for the current profile.
  virtual bool IsAllowed(const Profile* profile);

  // Returns whether Plugin VM has been installed.
  // TODO(timloh): We should detect installations via VMC, currently the user
  // needs to manually launch the installer once for the pref to get set.
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

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_FEATURES_H_
