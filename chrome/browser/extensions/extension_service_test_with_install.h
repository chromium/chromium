// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_TEST_WITH_INSTALL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_TEST_WITH_INSTALL_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/extension_service_user_test_base.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace base {
class FilePath;
}

namespace content {
class BrowserTaskEnvironment;
}

namespace extensions {

// An enhancement of ExtensionServiceUserTestBase that provides helpers to
// install, update, and uninstall extensions.
class ExtensionServiceTestWithInstall : public ExtensionServiceUserTestBase,
                                        public ExtensionRegistryObserver {
 public:
  ExtensionServiceTestWithInstall();
  explicit ExtensionServiceTestWithInstall(
      std::unique_ptr<content::BrowserTaskEnvironment> task_environment);

  ExtensionServiceTestWithInstall(const ExtensionServiceTestWithInstall&) =
      delete;
  ExtensionServiceTestWithInstall& operator=(
      const ExtensionServiceTestWithInstall&) = delete;

  ~ExtensionServiceTestWithInstall() override;

 protected:
  void InitializeExtensionService(ExtensionServiceInitParams params) override;

  static std::vector<std::u16string> GetErrors();

  void PackCRX(const base::FilePath& dir_path,
               const base::FilePath& pem_path,
               const base::FilePath& crx_path);

  enum InstallState {
    INSTALL_FAILED,
    INSTALL_UPDATED,
    INSTALL_NEW,
    INSTALL_WITHOUT_LOAD,
  };

  const Extension* PackAndInstallCRX(const base::FilePath& dir_path,
                                     const base::FilePath& pem_path,
                                     InstallState install_state,
                                     int creation_flags,
                                     mojom::ManifestLocation install_location);
  const Extension* PackAndInstallCRX(const base::FilePath& dir_path,
                                     const base::FilePath& pem_path,
                                     InstallState install_state);
  const Extension* PackAndInstallCRX(const base::FilePath& dir_path,
                                     InstallState install_state);
  const Extension* PackAndInstallCRX(const base::FilePath& dir_path,
                                     mojom::ManifestLocation install_location,
                                     InstallState install_state);
  const Extension* InstallCRX(const base::FilePath& path,
                              InstallState install_state,
                              int creation_flags,
                              const std::string& expected_old_name);
  const Extension* InstallCRX(const base::FilePath& path,
                              mojom::ManifestLocation install_location,
                              InstallState install_state,
                              int creation_flags);
  const Extension* InstallCRX(const base::FilePath& path,
                              InstallState install_state,
                              int creation_flags);
  const Extension* InstallCRX(const base::FilePath& path,
                              InstallState install_state);
  const Extension* InstallCRXFromWebStore(const base::FilePath& path,
                                          InstallState install_state);

  // Verifies the result of a CRX installation. Used by InstallCRX. Set the
  // `install_state` to INSTALL_FAILED if the installation is expected to fail.
  // Returns an Extension pointer if the install succeeded, null otherwise.
  const Extension* VerifyCrxInstall(const base::FilePath& path,
                                    InstallState install_state);

  // Verifies the result of a CRX installation. Used by InstallCRX. Set the
  // `install_state` to INSTALL_FAILED if the installation is expected to fail.
  // If `install_state` is INSTALL_UPDATED, and `expected_old_name` is
  // non-empty, expects that the existing extension's title was
  // `expected_old_name`.
  // Returns an Extension pointer if the install succeeded, null otherwise.
  const Extension* VerifyCrxInstall(const base::FilePath& path,
                                    InstallState install_state,
                                    const std::string& expected_old_name);

  enum UpdateState {
    FAILED_SILENTLY,
    FAILED,
    UPDATED,
    INSTALLED,
    DISABLED,
    ENABLED
  };

  void PackCRXAndUpdateExtension(const std::string& id,
                                 const base::FilePath& dir_path,
                                 const base::FilePath& pem_path,
                                 UpdateState expected_state);

  void UpdateExtension(const std::string& id,
                       const base::FilePath& in_path,
                       UpdateState expected_state);

  enum UninstallExtensionFileDeleteType {
    kDeletePath,         // Delete the exact path of the extension install.
    kDeleteAllVersions,  // Delete all version of the extension (e.g. delete the
                         // root of the install folder).
    kDoNotDelete,        // Do not delete any of the extension's files.
  };

  // Uninstalls extension with `id` and expects deletion of the extension's
  // files according to `delete_type`.
  void UninstallExtension(
      const std::string& id,
      UninstallExtensionFileDeleteType delete_type = kDeleteAllVersions,
      extensions::UninstallReason reason =
          extensions::UNINSTALL_REASON_FOR_TESTING);

  void TerminateExtension(const std::string& id);

  void BlockAllExtensions();

  void ClearLoadedExtensions();

  const ExtensionList& loaded_extensions() const { return loaded_extensions_; }
  const Extension* installed_extension() const { return installed_extension_; }
  bool was_update() const { return was_update_; }
  UnloadedExtensionReason unloaded_reason() const { return unloaded_reason_; }

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;
  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const Extension* extension,
                                  bool is_update,
                                  const std::string& old_name) override;

 private:
  void InstallCRXInternal(const base::FilePath& crx_path,
                          mojom::ManifestLocation install_location,
                          InstallState install_state,
                          int creation_flags);

  extensions::ExtensionList loaded_extensions_;
  raw_ptr<const Extension, DanglingUntriaged> installed_extension_;
  bool was_update_;
  std::string old_name_;
  std::string unloaded_id_;
  UnloadedExtensionReason unloaded_reason_;
  size_t expected_extensions_count_;

  FeatureSwitch::ScopedOverride override_external_install_prompt_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};
};

// This is a FakeSyncChangeProcessor specialization that maintains a store of
// SyncData items, treating it like a map keyed by the extension id from the
// SyncData. Each instance of this class should only be used for one data type
// (which should be either extensions or apps) to match how the real sync
// system handles things.
class StatefulChangeProcessor : public syncer::FakeSyncChangeProcessor {
 public:
  explicit StatefulChangeProcessor(syncer::DataType expected_type);

  StatefulChangeProcessor(const StatefulChangeProcessor&) = delete;
  StatefulChangeProcessor& operator=(const StatefulChangeProcessor&) = delete;

  ~StatefulChangeProcessor() override;

  // We let our parent class, FakeSyncChangeProcessor, handle saving the
  // changes for us, but in addition we "apply" these changes by treating
  // the FakeSyncChangeProcessor's SyncDataList as a map keyed by extension
  // id.
  std::optional<syncer::ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

  // This is a helper which offers a wrapped version of this object suitable for
  // passing in to MergeDataAndStartSyncing, which takes a
  // std::unique_ptr<SyncChangeProcessor>, since in tests we typically don't
  // want to give up ownership of a local change processor.
  std::unique_ptr<syncer::SyncChangeProcessor> GetWrapped();

  const syncer::SyncDataList& data() const { return data_; }

 private:
  // The expected DataType of changes that this processor will see.
  const syncer::DataType expected_type_;
  syncer::SyncDataList data_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_TEST_WITH_INSTALL_H_
