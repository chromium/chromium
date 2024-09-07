// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_COMPONENTS_H_
#define CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_COMPONENTS_H_

#include <list>
#include <optional>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"

namespace base {
class Version;
}

namespace ash {

// Loads Demo Mode ChromeOS components and exposes component-related data such
// as install paths.
// Components:
// - demo-mode-resources: Contains Android APKs, sample photos for Google
//     Photos, and splash screen images.
// - demo-mode-app: Contains app content (html/css/js/assets) to render in the
//     Demo Mode System Web App.
//
// TODO(b/255679902): Consider removing this class entirely and returning
//  component loading responsibilities to callers.
class DemoComponents {
 public:
  // The name of the demo mode resources ChromeOS component.
  static const char kDemoModeResourcesComponentName[];

  // The name of the demo mode app ChromeOS component.
  static const char kDemoModeAppComponentName[];

  // The name of the preinstalled offline demo mode resources CrOS component.
  static const char kOfflineDemoModeResourcesComponentName[];

  // Location on disk where pre-installed demo mode resources are expected to be
  // found.
  static base::FilePath GetPreInstalledPath();

  static void OverridePreinstalledResourcesRootPathForTesting(
      const base::FilePath* path);

  explicit DemoComponents(DemoSession::DemoModeConfig config);

  DemoComponents(const DemoComponents&) = delete;
  DemoComponents& operator=(const DemoComponents&) = delete;

  ~DemoComponents();

  // Converts a relative path to an absolute path under the demo
  // resources mount. Returns an empty string if the demo resources are
  // not loaded.
  base::FilePath GetAbsolutePath(const base::FilePath& relative_path) const;

  // Gets the path of the image containing demo session Android apps. The path
  // will be set when the demo resources get loaded.
  base::FilePath GetDemoAndroidAppsPath() const;

  // Gets the path under demo resources mount point that contains
  // external extensions prefs (JSON containing set of extensions to be loaded
  // as external extensions into demo sessions - expected to map extension IDs
  // to the associated CRX path and version).
  base::FilePath GetExternalExtensionsPrefsPath() const;

  // Requests the load of the demo-mode-resources component.
  // |load_callback| will be run once the component finishes loading.
  void LoadResourcesComponent(base::OnceClosure load_callback);

  // Requests the load of the demo-mode-app component.
  // |load_callback| will be run once the component finishes loading.
  void LoadAppComponent(base::OnceClosure load_callback);

  // Fakes the demo mode resources CrOS component having been requested and
  // mounted at the given path (or not mounted if `path` is empty).
  void SetCrOSComponentLoadedForTesting(
      const base::FilePath& path,
      component_updater::ComponentManagerAsh::Error);

  // Fakes the offline demo mode resources image having been requested and
  // mounted at the given path (or not mounted if `path` is empty).
  void SetPreinstalledOfflineResourcesLoadedForTesting(
      const base::FilePath& path);

  bool resources_component_loaded() const { return resources_loaded_; }
  const base::FilePath& resources_component_path() const {
    return resources_component_path_;
  }
  const base::FilePath& default_app_component_path() const {
    return default_app_component_path_;
  }

  // The error from trying to load the demo mode resources CrOS component from
  // the ComponentManagerAsh.
  const std::optional<component_updater::ComponentManagerAsh::Error>&
  resources_component_error() const {
    return resources_component_error_;
  }

  // The error from trying to load the demo mode app CrOS component from
  // the ComponentManagerAsh.
  const std::optional<component_updater::ComponentManagerAsh::Error>&
  app_component_error() const {
    return app_component_error_;
  }

  const std::optional<base::Version>& app_component_version() const {
    return app_component_version_;
  }

  const std::optional<base::Version>& resources_component_version() const {
    return resources_component_version_;
  }

 private:
  void OnAppVersionReady(base::OnceClosure callback,
                         const base::Version& version);

  void OnResourcesVersionReady(const base::FilePath& path,
                               const base::Version& version);

  void OnAppComponentLoaded(base::OnceClosure load_callback,
                            component_updater::ComponentManagerAsh::Error error,
                            const base::FilePath& path);

  // Called after attempting to load the installed demo mode resources CrOS
  // component has finished.
  // On success, `path` is expected to contain the path as which the component
  // is loaded.
  void InstalledComponentLoaded(
      component_updater::ComponentManagerAsh::Error error,
      const base::FilePath& path);

  // Callback for the component or image loader request to load demo resources.
  // `mount_path` is the path at which the resources were loaded.
  void OnDemoResourcesLoaded(std::optional<base::FilePath> mounted_path);

  // Which config to load resources for: online or offline.
  DemoSession::DemoModeConfig config_;

  bool resources_load_requested_ = false;
  bool resources_loaded_ = false;

  // Last error (or NONE) seen when trying to load the demo-mode-resources CrOS
  // component. Has no value until the load attempt has completed.
  std::optional<component_updater::ComponentManagerAsh::Error>
      resources_component_error_;

  // Last error (or NONE) seen when trying to load the demo-mode-app CrOS
  // component. Has no value until the load attempt has completed.
  std::optional<component_updater::ComponentManagerAsh::Error>
      app_component_error_;

  // Path at which the demo-mode-resources component was loaded.
  base::FilePath resources_component_path_;

  // Path at which the demo-mode-app component was loaded.
  // This path can be overridden for local testing of component content with the
  // demo-mode-swa-content-directory switch.
  base::FilePath default_app_component_path_;

  // List of pending callbacks passed to EnsureLoaded().
  std::list<base::OnceClosure> load_callbacks_;

  std::optional<base::Version> app_component_version_;
  std::optional<base::Version> resources_component_version_;

  base::WeakPtrFactory<DemoComponents> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_COMPONENTS_H_
