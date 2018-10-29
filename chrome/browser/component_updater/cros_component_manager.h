// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_MANAGER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_MANAGER_H_

#include <string>

#include "base/callback_forward.h"

namespace base {
class FilePath;
}

namespace component_updater {

// This class contains functions used to register and install a component.
class CrOSComponentManager {
 public:
  // Error needs to be consistent with CrosComponentManagerError in
  // src/tools/metrics/histograms/enums.xml.
  enum class Error {
    NONE = 0,
    UNKNOWN_COMPONENT = 1,  // Component requested does not exist.
    INSTALL_FAILURE = 2,    // update_client fails to install component.
    MOUNT_FAILURE = 3,      // Component can not be mounted.
    COMPATIBILITY_CHECK_FAILED = 4,  // Compatibility check failed.
    NOT_FOUND = 5,  // A component installation was not found - reported for
                    // load requests with kSkip update policy.
    ERROR_MAX
  };

  // Policy on mount operation.
  enum class MountPolicy {
    // Mount the component if installed.
    kMount,
    // Skip the mount operation.
    kDontMount,
  };

  // Policy on update operation.
  enum class UpdatePolicy {
    // Force component update.
    kForce,
    // Do not update if a compatible component is installed.
    kDontForce,
    // Do not run updater, even if a compatible component is not installed at
    // the moment.
    kSkip
  };

  // LoadCallback will always return the load result in |error|. If used in
  // conjunction with the |kMount| policy below, return the mounted FilePath in
  // |path|, or an empty |path| otherwise.
  using LoadCallback =
      base::OnceCallback<void(Error error, const base::FilePath& path)>;

  class Delegate {
   public:
    virtual ~Delegate() {}
    // Broadcasts a D-Bus signal for a successful component installation.
    virtual void EmitInstalledSignal(const std::string& component) = 0;
  };

  virtual ~CrOSComponentManager() = default;

  virtual void SetDelegate(Delegate* delegate) = 0;

  // Installs a component and keeps it up-to-date.
  // The |load_callback| is run on the calling thread.
  virtual void Load(const std::string& name,
                    MountPolicy mount_policy,
                    UpdatePolicy update_policy,
                    LoadCallback load_callback) = 0;

  // Stops updating and removes a component.
  // Returns true if the component was successfully unloaded
  // or false if it couldn't be unloaded or already wasn't loaded.
  virtual bool Unload(const std::string& name) = 0;

  // Saves the name and install path of a compatible component.
  virtual void RegisterCompatiblePath(const std::string& name,
                                      const base::FilePath& path) = 0;

  // Removes the name and install path entry of a component.
  virtual void UnregisterCompatiblePath(const std::string& name) = 0;

  // Returns installed path of a compatible component given |name|. Returns an
  // empty path if the component isn't compatible.
  virtual base::FilePath GetCompatiblePath(const std::string& name) const = 0;

  // Returns true if any previously registered version of a component exists,
  // even if it is incompatible.
  virtual bool IsRegistered(const std::string& name) const = 0;

  // Register all installed components.
  virtual void RegisterInstalled() = 0;
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_MANAGER_H_
