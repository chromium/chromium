// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_util.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/load_error_waiter.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_creator.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/install_prefs_helper.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

ExtensionService* GetExtensionService(
    content::BrowserContext* browser_context) {
  return ExtensionSystem::Get(browser_context)->extension_service();
}

class ExtensionLoadedObserver : public ExtensionRegistryObserver {
 public:
  using ExtensionCallback = base::OnceCallback<void(const Extension*)>;

  ExtensionLoadedObserver(ExtensionRegistry* registry,
                          const base::FilePath& file_path)
      : file_path_(file_path) {
    extension_registry_observation_.Observe(registry);
  }

  static void ObserveOnce(std::unique_ptr<ExtensionLoadedObserver> observer,
                          ExtensionCallback callback) {
    auto* observer_ptr = observer.get();
    observer_ptr->ObserveOnce(std::move(callback).Then(
        // Keep |observer| alive until it made its observation.
        base::BindOnce([](std::unique_ptr<ExtensionLoadedObserver>) {},
                       std::move(observer))));
  }

 private:
  void ObserveOnce(ExtensionCallback callback) {
    if (extension_) {
      std::move(callback).Run(extension_.get());
      // |this| will be deleted here.
      return;
    }
    callback_ = std::move(callback);
  }

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override {
    if (extension->path() == file_path_) {
      extension_registry_observation_.Reset();
      if (callback_) {
        std::move(callback_).Run(extension);
        // |this| will be deleted here.
        return;
      }
      extension_ = extension;
    }
  }

  const base::FilePath file_path_;
  scoped_refptr<const Extension> extension_;
  ExtensionCallback callback_;
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
};

}  // namespace

void ChromeTestExtensionLoader::LoadUnpackedExtensionAsync(
    const base::FilePath& file_path,
    base::OnceCallback<void(const Extension*)> callback) {
  auto observer =
      std::make_unique<ExtensionLoadedObserver>(extension_registry_, file_path);
  UnpackedInstaller::Create(GetExtensionService(browser_context_))
      ->Load(file_path);
  ExtensionLoadedObserver::ObserveOnce(std::move(observer),
                                       std::move(callback));
}

scoped_refptr<const Extension> ChromeTestExtensionLoader::LoadExtension(
    const base::FilePath& path) {
  scoped_refptr<const Extension> extension;
  bool is_unpacked = false;
  if (path.MatchesExtension(FILE_PATH_LITERAL(".crx"))) {
    extension = LoadCrx(path);
  } else if (pack_extension_) {
    base::FilePath crx_path = PackExtension(path);
    if (crx_path.empty()) {
      return nullptr;
    }
    extension = LoadCrx(crx_path);
  } else {
    is_unpacked = true;
    extension = LoadUnpacked(path);
  }

  if (should_fail_ && extension) {
    ADD_FAILURE() << "Expected extension load failure, but succeeded";
  } else if (!should_fail_ && !extension) {
    ADD_FAILURE() << "Failed to load extension";
  }

  if (!extension) {
    return nullptr;
  }

  extension_id_ = extension->id();

  // Permissions and the install param are handled by the unpacked installer
  // before the extension is installed.
  // TODO(crbug.com/40160904): Fix CrxInstaller to enable this for
  // packed extensions.
  if (!is_unpacked) {
    AdjustPackedExtension(*extension);
    // Make `extension` null since it may have been reloaded invalidating
    // pointers to it.
    extension = nullptr;
  }

  extension = extension_registry_->enabled_extensions().GetByID(extension_id_);
  if (!extension) {
    return nullptr;
  }
  if (!VerifyPermissions(extension.get())) {
    ADD_FAILURE() << "The extension did not get the requested permissions.";
    return nullptr;
  }
  if (!CheckInstallWarnings(*extension)) {
    return nullptr;
  }

  if (!WaitForExtensionReady(*extension)) {
    ADD_FAILURE() << "Failed to wait for extension ready";
    return nullptr;
  }
  return extension;
}

base::FilePath ChromeTestExtensionLoader::PackExtension(
    const base::FilePath& unpacked_path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (!base::PathExists(unpacked_path)) {
    ADD_FAILURE() << "Unpacked path does not exist: " << unpacked_path.value();
    return base::FilePath();
  }

  if (!temp_dir_.CreateUniqueTempDir()) {
    ADD_FAILURE() << "Could not create unique temp dir.";
    return base::FilePath();
  }
  base::FilePath crx_path = temp_dir_.GetPath().AppendASCII("temp.crx");
  if (base::PathExists(crx_path)) {
    ADD_FAILURE()
        << "Crx path exists: " << crx_path.value()
        << ", are you trying to reuse the same ChromeTestExtensionLoader?";
    return base::FilePath();
  }
  base::FilePath fallback_pem_path =
      temp_dir_.GetPath().AppendASCII("temp.pem");
  if (base::PathExists(fallback_pem_path)) {
    ADD_FAILURE()
        << "PEM path exists: " << fallback_pem_path.value()
        << ", are you trying to reuse the same ChromeTestExtensionLoader?";
    return base::FilePath();
  }

  base::FilePath empty_path;
  base::FilePath* pem_path_to_use = &empty_path;
  if (!pem_path_.empty()) {
    pem_path_to_use = &pem_path_;
    if (!base::PathExists(pem_path_)) {
      ADD_FAILURE() << "Provided PEM path does not exist: "
                    << pem_path_.value();
      return base::FilePath();
    }
  }

  ExtensionCreator creator;
  if (!creator.Run(unpacked_path, crx_path, *pem_path_to_use, fallback_pem_path,
                   ExtensionCreator::kOverwriteCRX)) {
    ADD_FAILURE() << "ExtensionCreator::Run() failed: "
                  << creator.error_message();
    return base::FilePath();
  }

  CHECK(base::PathExists(crx_path));

  return crx_path;
}

scoped_refptr<const Extension> ChromeTestExtensionLoader::LoadCrx(
    const base::FilePath& file_path) {
  if (!file_path.MatchesExtension(FILE_PATH_LITERAL(".crx"))) {
    ADD_FAILURE() << "Must pass a crx path to LoadCrx()";
    return nullptr;
  }

  scoped_refptr<const Extension> extension;
  {
    // TODO(devlin): Allow consumers to specify the install ui type.
    std::unique_ptr<ExtensionInstallPrompt> install_ui;
    scoped_refptr<CrxInstaller> installer =
        CrxInstaller::Create(browser_context_, std::move(install_ui));
    installer->set_expected_id(expected_id_);
    installer->set_creation_flags(creation_flags_);
    installer->set_install_source(location_);
    installer->set_install_immediately(install_immediately_);
    installer->set_allow_silent_install(grant_permissions_);
    if (!installer->is_gallery_install()) {
      installer->set_off_store_install_allow_reason(
          CrxInstaller::OffStoreInstallAllowedInTest);
    }

    base::test::TestFuture<std::optional<CrxInstallError>>
        installer_done_future;
    installer->AddInstallerCallback(
        installer_done_future
            .GetCallback<const std::optional<CrxInstallError>&>());

    installer->InstallCrx(file_path);

    std::optional<CrxInstallError> error = installer_done_future.Get();
    if (error) {
      return nullptr;
    }

    extension = installer->extension();
  }

  return extension;
}

scoped_refptr<const Extension> ChromeTestExtensionLoader::LoadUnpacked(
    const base::FilePath& file_path) {
  scoped_refptr<const Extension> extension;
  TestExtensionRegistryObserver registry_observer(extension_registry_);
  scoped_refptr<UnpackedInstaller> installer =
      UnpackedInstaller::Create(GetExtensionService(browser_context_));
  installer->set_require_modern_manifest_version(
      require_modern_manifest_version_);
  if (allow_file_access_.has_value()) {
    installer->set_allow_file_access(*allow_file_access_);
  }
  if (allow_incognito_access_.has_value()) {
    installer->set_allow_incognito_access(*allow_incognito_access_);
  }
  if (install_param_.has_value()) {
    installer->set_install_param(*install_param_);
  }
  LoadErrorWaiter waiter;
  installer->Load(file_path);
  if (!should_fail_) {
    extension = registry_observer.WaitForExtensionLoaded();
  } else {
    EXPECT_TRUE(waiter.Wait()) << "No load error observed";
  }

  return extension;
}

}  // namespace extensions
