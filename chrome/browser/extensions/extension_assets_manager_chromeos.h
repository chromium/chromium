// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ASSETS_MANAGER_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ASSETS_MANAGER_CHROMEOS_H_

#include <map>

#include "base/values.h"
#include "chrome/browser/extensions/extension_assets_manager.h"

namespace base {
template <typename T> struct DefaultSingletonTraits;
}

class PrefRegistrySimple;

namespace extensions {

// Chrome OS specific implementation of assets manager that shares default apps
// between all users on the machine.
class ExtensionAssetsManagerChromeOS : public ExtensionAssetsManager {
 public:
  ExtensionAssetsManagerChromeOS(const ExtensionAssetsManagerChromeOS&) =
      delete;
  ExtensionAssetsManagerChromeOS& operator=(
      const ExtensionAssetsManagerChromeOS&) = delete;

  static ExtensionAssetsManagerChromeOS* GetInstance();

  // A dictionary that maps shared extension IDs to version/paths/users.
  static const char kSharedExtensions[];

  // Name of path attribute in shared extensions map.
  static const char kSharedExtensionPath[];

  // Name of users attribute (list of user emails) in shared extensions map.
  static const char kSharedExtensionUsers[];

  // Register shared assets related preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Override from ExtensionAssetsManager.
  void InstallExtension(
      const Extension* extension,
      const base::FilePath& unpacked_extension_root,
      const base::FilePath& local_install_dir,
      Profile* profile,
      InstallExtensionCallback callback,
      bool updates_from_webstore_or_empty_update_url) override;
  void UninstallExtension(const std::string& id,
                          const std::string& profile_user_name,
                          const base::FilePath& extensions_install_dir,
                          const base::FilePath& extension_dir_to_delete,
                          const base::FilePath& profile_dir) override;

  // Return shared install dir.
  static base::FilePath GetSharedInstallDir();

  // Return true if |extension| was installed to shared location.
  static bool IsSharedInstall(const Extension* extension);

  // Cleans up shared extensions list in preferences and returns list of
  // extension IDs and version paths that are in use in |live_extension_paths|.
  // Files on disk are not removed. Must be called on UI thread.
  // Returns |false| in case of errors.
  static bool CleanUpSharedExtensions(
      std::multimap<std::string, base::FilePath>* live_extension_paths);

  static void SetSharedInstallDirForTesting(const base::FilePath& install_dir);

 private:
  friend struct base::DefaultSingletonTraits<ExtensionAssetsManagerChromeOS>;

  ExtensionAssetsManagerChromeOS();
  ~ExtensionAssetsManagerChromeOS() override;

  // Return |true| if |extension| can be installed in a shared place for all
  // users on the device.
  static bool CanShareAssets(const Extension* extension,
                             const base::FilePath& unpacked_extension_root,
                             bool updates_from_webstore_or_empty_update_url);

  // Called on the UI thread to check if a given version of the |extension|
  // already exists at the shared location.
  static void CheckSharedExtension(
      const std::string& id,
      const std::string& version,
      const base::FilePath& unpacked_extension_root,
      const base::FilePath& local_install_dir,
      Profile* profile,
      InstallExtensionCallback callback);

  // Called on task runner thread to install extension to shared location.
  static void InstallSharedExtension(
      const std::string& id,
      const std::string& version,
      const base::FilePath& unpacked_extension_root);

  // Called on UI thread to process shared install result.
  static void InstallSharedExtensionDone(
      const std::string& id,
      const std::string& version,
      const base::FilePath& shared_version_dir);

  // Called on task runner thread to install the extension to local dir call
  // callback with the result.
  static void InstallLocalExtension(
      const std::string& id,
      const std::string& version,
      const base::FilePath& unpacked_extension_root,
      const base::FilePath& local_install_dir,
      InstallExtensionCallback callback);

  // Called on UI thread to mark that shared version is not used.
  static void MarkSharedExtensionUnused(const std::string& id,
                                        const std::string& profile_user_name);

  // Called on task runner thread to remove shared version.
  static void DeleteSharedVersion(const base::FilePath& shared_version_dir);

  // Clean shared extension with given |id|.
  static bool CleanUpExtension(
      const std::string& id,
      base::Value::Dict& extension_info,
      std::multimap<std::string, base::FilePath>* live_extension_paths);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ASSETS_MANAGER_CHROMEOS_H_
