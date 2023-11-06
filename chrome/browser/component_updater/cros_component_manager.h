// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_MANAGER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_MANAGER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/version.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
class Version;
}

namespace component_updater {

// Contains the path and version of a compatible component.
struct CompatibleComponentInfo {
  CompatibleComponentInfo();
  CompatibleComponentInfo(const base::FilePath& path_in,
                          const absl::optional<base::Version>& version_in);
  CompatibleComponentInfo(const CompatibleComponentInfo& rhs) = delete;
  CompatibleComponentInfo& operator=(const CompatibleComponentInfo& rhs) =
      delete;
  CompatibleComponentInfo(CompatibleComponentInfo&& rhs);
  CompatibleComponentInfo& operator=(CompatibleComponentInfo&& rhs);
  ~CompatibleComponentInfo();
  base::FilePath path;
  absl::optional<base::Version> version;
};

// This class contains functions used to register and install a component.
//
// This innocuous looking class unfortunately cannot be pure abstract and must
// inherit from base::RefCountedThreadSafe. This class has two concrete
// subclasses: FakeCrOSComponentManager (for tests) and CrOSComponentInstaller
// (for production). The implementation of
// CrOSComponentInstaller::IsRegisteredMayBlock must be called from a thread
// that allows disk IO, whereas all other methods are intended to be called from
// the main thread. There are only two ways to ensure that the base pointer and
// corresponding vtable are still valid from another thread: using a lock or
// ensuring the object is not destroyed. We can't use the former for performance
// reasons -- the entire reason we're dispatching onto another thread is to
// avoid blocking the main thread on disk IO. So the only remaining option is to
// ensure the object stays alive, which we do by using ref-counting. The
// implementation of CrOSComponentInstaller::IsRegisteredMayBlock looks like it
// could be made a static method, but FakeCrOSComponentManager requires a
// non-static method.
//
// This class, with the exception of the method IsRegisteredMayBlock() must only
// be used from the main thread. IsRegisteredMayBlock() must only be used from a
// thread that allows disk IO.
class CrOSComponentManager
    : public base::RefCountedThreadSafe<CrOSComponentManager> {
 public:
  CrOSComponentManager();

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
    UPDATE_IN_PROGRESS = 6,  // Component update in progress.
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
    // Force component update. The component will not be available until the
    // Omaha update check is complete.
    kForce,
    // Do not update if a compatible component is installed. The compatible
    // component will load now and any updates will download in the background.
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
    virtual ~Delegate() = default;
    // Broadcasts a D-Bus signal for a successful component installation.
    virtual void EmitInstalledSignal(const std::string& component) = 0;
  };

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

  // Gets version of a component. `version_callback` runs on the calling thread.
  // Return invalid base::Version() as `version` if the error occurs.
  virtual void GetVersion(const std::string& name,
                          base::OnceCallback<void(const base::Version& version)>
                              version_callback) const = 0;

  // Saves the information related to a compatible component.
  virtual void RegisterCompatiblePath(const std::string& name,
                                      CompatibleComponentInfo info) = 0;

  // Removes the name and install path entry of a component.
  virtual void UnregisterCompatiblePath(const std::string& name) = 0;

  // Returns installed path of a compatible component given |name|. Returns an
  // empty path if the component isn't compatible.
  virtual base::FilePath GetCompatiblePath(const std::string& name) const = 0;

  // Returns true if any previously registered version of a component exists,
  // even if it is incompatible.
  // This method must only be called on a thread that allows disk IO.
  virtual bool IsRegisteredMayBlock(const std::string& name) = 0;

  // Register all installed components.
  virtual void RegisterInstalled() = 0;

 protected:
  virtual ~CrOSComponentManager();

 private:
  friend class base::RefCountedThreadSafe<CrOSComponentManager>;
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_MANAGER_H_
