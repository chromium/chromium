// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_test_extension_loader.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/chrome_extension_test_notification_observer.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/load_error_waiter.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_creator.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/install_prefs_helper.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/user_script_loader.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/test/extension_background_page_waiter.h"
#include "extensions/test/test_content_script_load_waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

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

ChromeTestExtensionLoader::ChromeTestExtensionLoader(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      extension_system_(ExtensionSystem::Get(browser_context)),
      extension_service_(extension_system_->extension_service()),
      extension_registry_(ExtensionRegistry::Get(browser_context)) {}

ChromeTestExtensionLoader::~ChromeTestExtensionLoader() {
  // If there was a temporary directory created for a CRX, we need to clean it
  // up before the member is destroyed so we can explicitly allow IO.
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (temp_dir_.IsValid())
    EXPECT_TRUE(temp_dir_.Delete());
}

void ChromeTestExtensionLoader::LoadUnpackedExtensionAsync(
    const base::FilePath& file_path,
    base::OnceCallback<void(const Extension*)> callback) {
  auto observer =
      std::make_unique<ExtensionLoadedObserver>(extension_registry_, file_path);
  UnpackedInstaller::Create(extension_service_)->Load(file_path);
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
    if (crx_path.empty())
      return nullptr;
    extension = LoadCrx(crx_path);
  } else {
    is_unpacked = true;
    extension = LoadUnpacked(path);
  }

  if (should_fail_ && extension)
    ADD_FAILURE() << "Expected extension load failure, but succeeded";
  else if (!should_fail_ && !extension)
    ADD_FAILURE() << "Failed to load extension";

  if (!extension)
    return nullptr;

  extension_id_ = extension->id();

  // Permissions and the install param are handled by the unpacked installer
  // before the extension is installed.
  // TODO(crbug.com/40160904): Fix CrxInstaller to enable this for
  // packed extensions.
  if (!is_unpacked) {
    // Trying to reload a shared module (as we do when adjusting extension
    // permissions) causes ExtensionService to crash. Only adjust permissions
    // for non-shared modules.
    // TODO(devlin): That's not good; we shouldn't be crashing.
    if (!SharedModuleInfo::IsSharedModule(extension.get())) {
      CheckPermissions(extension.get());
      // Make |extension| null since it may have been reloaded invalidating
      // pointers to it.
      extension = nullptr;
    }

    if (install_param_.has_value()) {
      DCHECK(!install_param_->empty());
      SetInstallParam(ExtensionPrefs::Get(browser_context_), extension_id_,
                      *install_param_);
      // Reload the extension so listeners of the loaded notification have
      // access to the install param.
      TestExtensionRegistryObserver registry_observer(extension_registry_,
                                                      extension_id_);
      extension_service_->ReloadExtension(extension_id_);
      registry_observer.WaitForExtensionLoaded();
    }
  }

  extension = extension_registry_->enabled_extensions().GetByID(extension_id_);
  if (!extension)
    return nullptr;
  if (!VerifyPermissions(extension.get())) {
    ADD_FAILURE() << "The extension did not get the requested permissions.";
    return nullptr;
  }
  if (!CheckInstallWarnings(*extension))
    return nullptr;

  if (!WaitForExtensionReady(*extension)) {
    ADD_FAILURE() << "Failed to wait for extension ready";
    return nullptr;
  }
  return extension;
}

bool ChromeTestExtensionLoader::WaitForExtensionReady(
    const Extension& extension) {
  UserScriptManager* user_script_manager =
      ExtensionSystem::Get(browser_context_)->user_script_manager();
  // Note: |user_script_manager| can be null in tests.
  if (user_script_manager &&
      !ContentScriptsInfo::GetContentScripts(&extension).empty()) {
    ExtensionUserScriptLoader* user_script_loader =
        user_script_manager->GetUserScriptLoaderForExtension(extension_id_);
    if (!user_script_loader->HasLoadedScripts()) {
      ContentScriptLoadWaiter waiter(user_script_loader);
      waiter.Wait();
    }
  }

  const int num_processes =
      content::RenderProcessHost::GetCurrentRenderProcessCountForTesting();
  // Determine whether or not to wait for extension renderers. By default, we
  // base this on whether any renderer processes exist (which is also a proxy
  // for whether this is a browser test, since MockRenderProcessHosts and
  // similar don't count towards the render process host count), but we allow
  // tests to override this behavior.
  const bool should_wait_for_ready =
      wait_for_renderers_.value_or(num_processes > 0);

  if (!should_wait_for_ready)
    return true;

  content::BrowserContext* context_to_use =
      IncognitoInfo::IsSplitMode(&extension)
          ? browser_context_.get()
          : Profile::FromBrowserContext(browser_context_)->GetOriginalProfile();

  // If possible, wait for the extension's background context to be loaded.
  std::string reason_unused;
  if (ExtensionBackgroundPageWaiter::CanWaitFor(extension, reason_unused)) {
    ExtensionBackgroundPageWaiter(context_to_use, extension)
        .WaitForBackgroundInitialized();
  }

  // TODO(devlin): Should this use |context_to_use|? Or should
  // WaitForExtensionViewsToLoad check both contexts if one is OTR?
  if (!ChromeExtensionTestNotificationObserver(browser_context_)
           .WaitForExtensionViewsToLoad()) {
    return false;
  }

  return true;
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
    ADD_FAILURE() << "Crx path exists: " << crx_path.value()
        << ", are you trying to reuse the same ChromeTestExtensionLoader?";
    return base::FilePath();
  }
  base::FilePath fallback_pem_path =
      temp_dir_.GetPath().AppendASCII("temp.pem");
  if (base::PathExists(fallback_pem_path)) {
    ADD_FAILURE() << "PEM path exists: " << fallback_pem_path.value()
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
        CrxInstaller::Create(extension_service_, std::move(install_ui));
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

void ChromeTestExtensionLoader::CheckPermissions(const Extension* extension) {
  std::string id = extension->id();

  // If the client explicitly set |allow_file_access_|, use that value. Else
  // use the default as per the extensions manifest location.
  if (!allow_file_access_) {
    allow_file_access_ =
        Manifest::ShouldAlwaysAllowFileAccess(extension->location());
  }

  // |extension| may be reloaded subsequently, invalidating the pointer. Hence
  // make it null.
  extension = nullptr;

  // Toggling incognito or file access will reload the extension, so wait for
  // the reload.
  if (*allow_file_access_ != util::AllowFileAccess(id, browser_context_)) {
    TestExtensionRegistryObserver registry_observer(extension_registry_, id);
    util::SetAllowFileAccess(id, browser_context_, *allow_file_access_);
    registry_observer.WaitForExtensionLoaded();
  }

  if (allow_incognito_access_.has_value() &&
      *allow_incognito_access_ !=
          util::IsIncognitoEnabled(id, browser_context_)) {
    TestExtensionRegistryObserver registry_observer(extension_registry_, id);
    util::SetIsIncognitoEnabled(id, browser_context_, true);
    registry_observer.WaitForExtensionLoaded();
  }
}

bool ChromeTestExtensionLoader::VerifyPermissions(const Extension* extension) {
  const ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context_);
  if (allow_file_access_.has_value() &&
      prefs->AllowFileAccess(extension->id()) != *allow_file_access_) {
    return false;
  }
  if (allow_incognito_access_.has_value() &&
      prefs->IsIncognitoEnabled(extension->id()) != *allow_incognito_access_) {
    return false;
  }
  return true;
}

scoped_refptr<const Extension> ChromeTestExtensionLoader::LoadUnpacked(
    const base::FilePath& file_path) {
  scoped_refptr<const Extension> extension;
  TestExtensionRegistryObserver registry_observer(extension_registry_);
  scoped_refptr<UnpackedInstaller> installer =
      UnpackedInstaller::Create(extension_service_);
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

bool ChromeTestExtensionLoader::CheckInstallWarnings(
    const Extension& extension) {
  if (ignore_manifest_warnings_)
    return true;

  const std::vector<InstallWarning>& install_warnings =
      extension.install_warnings();
  std::string install_warnings_string;
  for (const InstallWarning& warning : install_warnings) {
    // Don't fail on the manifest v2 deprecation warning in tests for now.
    // TODO(crbug.com/40804030): Stop skipping this warning when all
    // tests are updated to MV3.
    if (warning.message == manifest_errors::kManifestV2IsDeprecatedWarning)
      continue;
    install_warnings_string += "  " + warning.message + "\n";
  }

  if (install_warnings_string.empty())
    return true;

  ADD_FAILURE() << "Unexpected warnings for extension:\n"
                << install_warnings_string;
  return false;
}

}  // namespace extensions
