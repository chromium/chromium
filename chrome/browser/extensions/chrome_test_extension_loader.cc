// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_test_extension_loader.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/chrome_extension_test_notification_observer.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_creator.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/shared_user_script_master.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/user_script_loader.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/test/extension_test_notification_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// A single-use class to wait for content scripts to be loaded.
class ContentScriptLoadWaiter : public UserScriptLoader::Observer {
 public:
  ContentScriptLoadWaiter(UserScriptLoader* loader, const HostID& host_id)
      : host_id_(host_id), loader_(loader), scoped_observer_(this) {
    scoped_observer_.Add(loader_);
  }
  ~ContentScriptLoadWaiter() = default;

  void Wait() { run_loop_.Run(); }

 private:
  // UserScriptLoader::Observer:
  void OnScriptsLoaded(UserScriptLoader* loader,
                       content::BrowserContext* browser_context) override {
    if (loader_->HasLoadedScripts(host_id_)) {
      // Quit when idle in order to allow other observers to run.
      run_loop_.QuitWhenIdle();
    }
  }
  void OnUserScriptLoaderDestroyed(UserScriptLoader* loader) override {}

  const HostID host_id_;
  UserScriptLoader* const loader_;
  base::RunLoop run_loop_;
  ScopedObserver<UserScriptLoader, UserScriptLoader::Observer> scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(ContentScriptLoadWaiter);
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

scoped_refptr<const Extension> ChromeTestExtensionLoader::LoadExtension(
    const base::FilePath& path) {
  scoped_refptr<const Extension> extension;
  if (path.MatchesExtension(FILE_PATH_LITERAL(".crx"))) {
    extension = LoadCrx(path);
  } else if (pack_extension_) {
    base::FilePath crx_path = PackExtension(path);
    if (crx_path.empty())
      return nullptr;
    extension = LoadCrx(crx_path);
  } else {
    extension = LoadUnpacked(path);
  }

  if (should_fail_ && extension)
    ADD_FAILURE() << "Expected extension load failure, but succeeded";
  else if (!should_fail_ && !extension)
    ADD_FAILURE() << "Failed to load extension";

  if (!extension)
    return nullptr;

  extension_id_ = extension->id();

  // Trying to reload a shared module (as we do when adjusting extension
  // permissions) causes ExtensionService to crash. Only adjust permissions for
  // non-shared modules.
  // TODO(devlin): That's not good; we shouldn't be crashing.
  if (!SharedModuleInfo::IsSharedModule(extension.get())) {
    CheckPermissions(extension.get());
    // Make |extension| null since it may have been reloaded invalidating
    // pointers to it.
    extension = nullptr;
  }

  if (!install_param_.empty()) {
    ExtensionPrefs::Get(browser_context_)
        ->SetInstallParam(extension_id_, install_param_);
    // Reload the extension so listeners of the loaded notification have access
    // to the install param.
    TestExtensionRegistryObserver registry_observer(extension_registry_,
                                                    extension_id_);
    extension_service_->ReloadExtension(extension_id_);
    registry_observer.WaitForExtensionLoaded();
  }

  extension = extension_registry_->enabled_extensions().GetByID(extension_id_);
  if (!extension)
    return nullptr;
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
  SharedUserScriptMaster* user_script_master =
      ExtensionSystem::Get(browser_context_)->shared_user_script_master();
  // Note: |user_script_master| can be null in tests.
  if (user_script_master &&
      !ContentScriptsInfo::GetContentScripts(&extension).empty()) {
    UserScriptLoader* user_script_loader = user_script_master->script_loader();
    HostID host_id(HostID::EXTENSIONS, extension_id_);
    if (!user_script_loader->HasLoadedScripts(host_id))
      ContentScriptLoadWaiter(user_script_loader, host_id).Wait();
  }

  return ChromeExtensionTestNotificationObserver(browser_context_)
      .WaitForExtensionViewsToLoad();
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

    content::WindowedNotificationObserver install_observer(
        NOTIFICATION_CRX_INSTALLER_DONE,
        content::Source<CrxInstaller>(installer.get()));
    installer->InstallCrx(file_path);
    install_observer.Wait();

    extension =
        content::Details<const Extension>(install_observer.details()).ptr();
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

  if (allow_incognito_access_ !=
      util::IsIncognitoEnabled(id, browser_context_)) {
    TestExtensionRegistryObserver registry_observer(extension_registry_, id);
    util::SetIsIncognitoEnabled(id, browser_context_, true);
    registry_observer.WaitForExtensionLoaded();
  }
}

scoped_refptr<const Extension> ChromeTestExtensionLoader::LoadUnpacked(
    const base::FilePath& file_path) {
  const Extension* extension = nullptr;
  TestExtensionRegistryObserver registry_observer(extension_registry_);
  scoped_refptr<UnpackedInstaller> installer =
      UnpackedInstaller::Create(extension_service_);
  installer->set_require_modern_manifest_version(
      require_modern_manifest_version_);
  installer->Load(file_path);
  if (!should_fail_) {
    extension = registry_observer.WaitForExtensionLoaded();
  } else {
    EXPECT_TRUE(ExtensionTestNotificationObserver(browser_context_)
                    .WaitForExtensionLoadError())
        << "No load error observed";
  }

  return extension;
}

bool ChromeTestExtensionLoader::CheckInstallWarnings(
    const Extension& extension) {
  if (ignore_manifest_warnings_)
    return true;
  const std::vector<InstallWarning>& install_warnings =
      extension.install_warnings();
  if (install_warnings.empty())
    return true;

  std::string install_warnings_message = "Unexpected warnings for extension:\n";
  for (const InstallWarning& warning : install_warnings)
    install_warnings_message += "  " + warning.message + "\n";

  ADD_FAILURE() << install_warnings_message;
  return false;
}

}  // namespace extensions
