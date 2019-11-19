// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_RESOURCES_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_RESOURCES_H_

#include <list>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"

namespace chromeos {

// Loads Demo Mode Resources depending on the configuration:
// - online demo mode: loads or installs the Demo Mode resources CrOS Component
// - offline demo mode: loads the pre-installed Offline Demo Mode Resources
//   image
class DemoResources {
 public:
  // The name of the demo mode resources CrOS component.
  static const char kDemoModeResourcesComponentName[];

  // The name of the preinstalled offline demo mode resources CrOS component.
  static const char kOfflineDemoModeResourcesComponentName[];

  // Location on disk where pre-installed demo mode resources are expected to be
  // found.
  static base::FilePath GetPreInstalledPath();

  explicit DemoResources(DemoSession::DemoModeConfig config);
  ~DemoResources();

  // Converts a relative path to an absolute path under the demo
  // resources mount. Returns an empty string if the demo resources are
  // not loaded.
  base::FilePath GetAbsolutePath(const base::FilePath& relative_path) const;

  // Gets the path of the image containing demo session Android apps. The path
  // will be set when the demo resources get loaded.
  base::FilePath GetDemoAppsPath() const;

  // Gets the path under demo resources mount point that contains
  // external extensions prefs (JSON containing set of extensions to be loaded
  // as external extensions into demo sessions - expected to map extension IDs
  // to the associated CRX path and version).
  base::FilePath GetExternalExtensionsPrefsPath() const;

  // Ensures that the load of demo session resources is requested.
  // |load_callback| will be run once the resources finish loading.
  void EnsureLoaded(base::OnceClosure load_callback);

  // Fakes the demo mode resources CrOS component having been requested and
  // mounted at the given path (or not mounted if |path| is empty).
  void SetCrOSComponentLoadedForTesting(
      const base::FilePath& path,
      component_updater::CrOSComponentManager::Error);

  // Fakes the offline demo mode resources image having been requested and
  // mounted at the given path (or not mounted if |path| is empty).
  void SetPreinstalledOfflineResourcesLoadedForTesting(
      const base::FilePath& path);

  bool loaded() const { return loaded_; }
  const base::FilePath& path() const { return path_; }

  // The error from trying to load the demo mode resources CrOS component from
  // the CrOSComponentManager.
  const base::Optional<component_updater::CrOSComponentManager::Error>&
  component_error() const {
    return component_error_;
  }

 private:
  // Called after attempting to load the installed demo mode resources CrOS
  // component has finished.
  // On success, |path| is expected to contain the path as which the component
  // is loaded.
  void InstalledComponentLoaded(
      component_updater::CrOSComponentManager::Error error,
      const base::FilePath& path);

  // Loads the preinstalled offline resources image.
  void LoadPreinstalledOfflineResources();

  // Callback for the component or image loader request to load demo resources.
  // |mount_path| is the path at which the resources were loaded.
  void OnDemoResourcesLoaded(base::Optional<base::FilePath> mounted_path);

  // Which config to load resources for: online or offline.
  DemoSession::DemoModeConfig config_;

  bool load_requested_ = false;
  bool loaded_ = false;

  // Last error (or NONE) seen when trying to load the CrOS component. Has no
  // value until the load attempt has completed.
  base::Optional<component_updater::CrOSComponentManager::Error>
      component_error_;

  // Path at which demo mode resources were loaded.
  base::FilePath path_;

  // List of pending callbacks passed to EnsureLoaded().
  std::list<base::OnceClosure> load_callbacks_;

  base::WeakPtrFactory<DemoResources> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DemoResources);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_RESOURCES_H_
