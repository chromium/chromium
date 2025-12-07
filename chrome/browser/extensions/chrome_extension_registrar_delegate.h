// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_REGISTRAR_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_REGISTRAR_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class Profile;

namespace base {
class FilePath;
}

namespace extensions {
class ComponentLoader;
class ExtensionPrefs;
class ExtensionRegistry;
class ExtensionSystem;

// The ExtensionRegistrar::Delegate for the //chrome/browser layer.
class ChromeExtensionRegistrarDelegate : public ExtensionRegistrar::Delegate {
 public:
  explicit ChromeExtensionRegistrarDelegate(Profile* profile);
  ChromeExtensionRegistrarDelegate(const ChromeExtensionRegistrarDelegate&) =
      delete;
  ChromeExtensionRegistrarDelegate& operator=(
      const ChromeExtensionRegistrarDelegate&) = delete;
  ~ChromeExtensionRegistrarDelegate() override;

  // Provides pointers to objects that are constructed after this one.
  void Init(ExtensionRegistrar* registrar);

  // Clears member pointers. Call this during KeyedService two-phase shutdown to
  // avoid dangling pointers.
  void Shutdown();

  // ExtensionRegistrar::Delegate:
  void PreAddExtension(const Extension* extension,
                       const Extension* old_extension) override;
  void OnAddNewOrUpdatedExtension(const Extension* extension) override;
  void PostActivateExtension(scoped_refptr<const Extension> extension) override;
  void PostDeactivateExtension(
      scoped_refptr<const Extension> extension) override;
  void PreUninstallExtension(scoped_refptr<const Extension> extension) override;
  void PostUninstallExtension(scoped_refptr<const Extension> extension,
                              base::OnceClosure done_callback) override;
  void LoadExtensionForReload(const ExtensionId& extension_id,
                              const base::FilePath& path) override;
  void LoadExtensionForReloadWithQuietFailure(
      const ExtensionId& extension_id,
      const base::FilePath& path) override;
  void ShowExtensionDisabledError(const Extension* extension,
                                  bool is_remote_install) override;
  bool CanEnableExtension(const Extension* extension) override;
  bool CanDisableExtension(const Extension* extension) override;
  void GrantActivePermissions(const Extension* extension) override;
  void UpdateExternalExtensionAlert() override;
  void OnExtensionInstalled(const Extension* extension,
                            const syncer::StringOrdinal& page_ordinal,
                            int install_flags,
                            base::Value::Dict ruleset_install_prefs) override;

  Profile* profile() { return profile_; }

 private:
  // Disables the extension if the privilege level has increased
  // (e.g., due to an upgrade).
  void CheckPermissionsIncrease(const Extension* extension,
                                bool is_extension_loaded);

  // Given an extension ID and/or path, loads that extension as a reload.
  void DoLoadExtensionForReload(const ExtensionId& extension_id,
                                const base::FilePath& path,
                                bool load_error_behavior_noisy);

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
                               const std::u16string& error);

  void RecordInstallHistograms(const Extension* extension);

  // The normal profile associated with this delegate.
  raw_ptr<Profile> profile_ = nullptr;

  raw_ptr<ExtensionSystem> system_ = nullptr;
  raw_ptr<ExtensionPrefs> extension_prefs_ = nullptr;
  raw_ptr<ExtensionRegistry> registry_ = nullptr;
  raw_ptr<ExtensionRegistrar> extension_registrar_ = nullptr;
  raw_ptr<ComponentLoader> component_loader_ = nullptr;

  base::WeakPtrFactory<ChromeExtensionRegistrarDelegate> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_REGISTRAR_DELEGATE_H_
