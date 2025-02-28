// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_REGISTRAR_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_REGISTRAR_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/extension_registrar.h"

class Profile;

namespace base {
class FilePath;
}

namespace extensions {
class ComponentLoader;
class DelayedInstallManager;
class ExtensionPrefs;
class ExtensionRegistry;
class ExtensionService;
class ExtensionSystem;

// The ExtensionRegistrar::Delegate for the //chrome/browser layer.
class ChromeExtensionRegistrarDelegate : public ExtensionRegistrar::Delegate {
 public:
  ChromeExtensionRegistrarDelegate(
      Profile* profile,
      ExtensionService* extension_service,
      ComponentLoader* component_loader,
      const base::FilePath& install_directory,
      const base::FilePath& unpacked_install_directory);
  ChromeExtensionRegistrarDelegate(const ChromeExtensionRegistrarDelegate&) =
      delete;
  ChromeExtensionRegistrarDelegate& operator=(
      const ChromeExtensionRegistrarDelegate&) = delete;
  ~ChromeExtensionRegistrarDelegate() override;

  // Provides pointers to objects that are constructed after this one.
  void Init(ExtensionRegistrar* registrar,
            DelayedInstallManager* delayed_install);

  // Clears member pointers. Call this during KeyedService two-phase shutdown to
  // avoid dangling pointers.
  void Shutdown();

  // ExtensionRegistrar::Delegate:
  void PreAddExtension(const Extension* extension,
                       const Extension* old_extension) override;
  void PostActivateExtension(scoped_refptr<const Extension> extension) override;
  void PostDeactivateExtension(
      scoped_refptr<const Extension> extension) override;
  void PreUninstallExtension(scoped_refptr<const Extension> extension) override;
  void PostUninstallExtension(scoped_refptr<const Extension> extension,
                              base::OnceClosure done_callback) override;
  void PostNotifyUninstallExtension(
      scoped_refptr<const Extension> extension) override;
  void LoadExtensionForReload(
      const ExtensionId& extension_id,
      const base::FilePath& path,
      ExtensionRegistrar::LoadErrorBehavior load_error_behavior) override;
  void ShowExtensionDisabledError(const Extension* extension,
                                  bool is_remote_install) override;
  void FinishDelayedInstallationsIfAny() override;
  bool CanAddExtension(const Extension* extension) override;
  bool CanEnableExtension(const Extension* extension) override;
  bool CanDisableExtension(const Extension* extension) override;
  bool ShouldBlockExtension(const Extension* extension) override;
  void GrantActivePermissions(const Extension* extension) override;

 private:
  // Disables the extension if the privilege level has increased
  // (e.g., due to an upgrade).
  void CheckPermissionsIncrease(const Extension* extension,
                                bool is_extension_loaded);

  // Helper that updates the active extension list used for crash reporting.
  void UpdateActiveExtensionsInCrashReporter();

  // Called on file task runner thread to uninstall extension.
  static void UninstallExtensionOnFileThread(
      const std::string& id,
      const std::string& profile_user_name,
      const base::FilePath& install_dir,
      const base::FilePath& extension_path,
      const base::FilePath& profile_dir);

  // Called when reloading an unpacked extension fails.
  void OnUnpackedReloadFailure(const Extension* extension,
                               const base::FilePath& file_path,
                               const std::string& error);

  // The normal profile associated with this delegate.
  raw_ptr<Profile> profile_ = nullptr;

  raw_ptr<ExtensionSystem> system_ = nullptr;
  raw_ptr<ExtensionService> extension_service_ = nullptr;
  raw_ptr<ExtensionPrefs> extension_prefs_ = nullptr;
  raw_ptr<ExtensionRegistry> registry_ = nullptr;
  raw_ptr<ExtensionRegistrar> extension_registrar_ = nullptr;
  raw_ptr<DelayedInstallManager> delayed_install_manager_ = nullptr;
  raw_ptr<ComponentLoader> component_loader_ = nullptr;

  // The full path to the directory where extensions are installed.
  const base::FilePath install_directory_;

  // The full path to the directory where unpacked (e.g. from .zip files)
  // extensions are installed.
  const base::FilePath unpacked_install_directory_;

  base::WeakPtrFactory<ChromeExtensionRegistrarDelegate> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_REGISTRAR_DELEGATE_H_
