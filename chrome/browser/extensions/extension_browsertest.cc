// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_message_bubble_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/web_application_info.h"
#include "components/crx_file/crx_verifier.h"
#include "components/sync/model/string_ordinal.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/browser/updater/extension_cache_fake.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/file_test_util.h"
#include "extensions/common/file_util.h"
#include "extensions/common/switches.h"
#include "extensions/common/value_builder.h"

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_switches.h"
#endif

namespace extensions {

namespace {

// Maps all chrome-extension://<id>/_test_resources/foo requests to
// chrome/test/data/extensions/foo. This is what allows us to share code between
// tests without needing to duplicate files in each extension.
void ExtensionProtocolTestResourcesHandler(const base::FilePath& test_dir_root,
                                           base::FilePath* directory_path,
                                           base::FilePath* relative_path) {
  // Only map paths that begin with _test_resources.
  if (!base::FilePath(FILE_PATH_LITERAL("_test_resources"))
           .IsParent(*relative_path)) {
    return;
  }

  // Replace _test_resources/foo with chrome/test/data/extensions/foo.
  *directory_path = test_dir_root;
  std::vector<base::FilePath::StringType> components;
  relative_path->GetComponents(&components);
  DCHECK_GT(components.size(), 1u);
  base::FilePath new_relative_path;
  for (size_t i = 1u; i < components.size(); ++i)
    new_relative_path = new_relative_path.Append(components[i]);

  *relative_path = new_relative_path;
}

}  // namespace

ExtensionBrowserTest::ExtensionBrowserTest()
    : loaded_(false),
      installed_(false),
#if defined(OS_CHROMEOS)
      set_chromeos_user_(true),
#endif
      // Default channel is STABLE but override with UNKNOWN so that unlaunched
      // or incomplete APIs can write tests.
      current_channel_(version_info::Channel::UNKNOWN),
      override_prompt_for_external_extensions_(
          FeatureSwitch::prompt_for_external_extensions(),
          false),
#if defined(OS_WIN)
      user_desktop_override_(base::DIR_USER_DESKTOP),
      common_desktop_override_(base::DIR_COMMON_DESKTOP),
      user_quick_launch_override_(base::DIR_USER_QUICK_LAUNCH),
      start_menu_override_(base::DIR_START_MENU),
      common_start_menu_override_(base::DIR_COMMON_START_MENU),
#endif
      profile_(NULL),
      verifier_format_override_(crx_file::VerifierFormat::CRX3) {
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
}

ExtensionBrowserTest::~ExtensionBrowserTest() {
}

Profile* ExtensionBrowserTest::profile() {
  if (!profile_) {
    if (browser())
      profile_ = browser()->profile();
    else
      profile_ = ProfileManager::GetActiveUserProfile();
  }
  return profile_;
}

bool ExtensionBrowserTest::ShouldEnableContentVerification() {
  return false;
}

bool ExtensionBrowserTest::ShouldEnableInstallVerification() {
  return false;
}

base::FilePath ExtensionBrowserTest::GetTestResourcesParentDir() {
  // Don't use |test_data_dir_| here (even though it points to
  // chrome/test/data/extensions by default) because subclasses have the ability
  // to alter it by overriding the SetUpCommandLine() method.
  base::FilePath test_root_path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_root_path);
  return test_root_path.AppendASCII("extensions");
}

// static
const Extension* ExtensionBrowserTest::GetExtensionByPath(
    const ExtensionSet& extensions,
    const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath extension_path = base::MakeAbsoluteFilePath(path);
  EXPECT_TRUE(!extension_path.empty());
  for (const scoped_refptr<const Extension>& extension : extensions) {
    if (extension->path() == extension_path) {
      return extension.get();
    }
  }
  return NULL;
}

void ExtensionBrowserTest::SetUp() {
  test_extension_cache_.reset(new ExtensionCacheFake());
  InProcessBrowserTest::SetUp();
}

void ExtensionBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII("extensions");

  // We don't want any warning bubbles for, e.g., unpacked extensions.
  ExtensionMessageBubbleFactory::set_override_for_tests(
      ExtensionMessageBubbleFactory::OVERRIDE_DISABLED);

  if (!ShouldEnableContentVerification()) {
    ignore_content_verification_.reset(
        new ScopedIgnoreContentVerifierForTest());
  }

  if (!ShouldEnableInstallVerification()) {
    ignore_install_verification_.reset(
        new ScopedInstallVerifierBypassForTest());
  }

#if defined(OS_CHROMEOS)
  if (set_chromeos_user_) {
    // This makes sure that we create the Default profile first, with no
    // ExtensionService and then the real profile with one, as we do when
    // running on chromeos.
    command_line->AppendSwitchASCII(chromeos::switches::kLoginUser,
                                    "testuser@gmail.com");
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
  }
#endif
}

void ExtensionBrowserTest::SetUpOnMainThread() {
  observer_.reset(new ChromeExtensionTestNotificationObserver(browser()));
  if (extension_service()->updater()) {
    extension_service()->updater()->SetExtensionCacheForTesting(
        test_extension_cache_.get());
  }

  test_protocol_handler_ = base::Bind(&ExtensionProtocolTestResourcesHandler,
                                      GetTestResourcesParentDir());
  SetExtensionProtocolTestHandler(&test_protocol_handler_);
}

void ExtensionBrowserTest::TearDownOnMainThread() {
  ExtensionMessageBubbleFactory::set_override_for_tests(
      ExtensionMessageBubbleFactory::NO_OVERRIDE);
  SetExtensionProtocolTestHandler(nullptr);
}

const Extension* ExtensionBrowserTest::LoadExtension(
    const base::FilePath& path) {
  return LoadExtensionWithFlags(path, kFlagEnableFileAccess);
}

const Extension* ExtensionBrowserTest::LoadExtensionIncognito(
    const base::FilePath& path) {
  return LoadExtensionWithFlags(path,
                                kFlagEnableFileAccess | kFlagEnableIncognito);
}

const Extension* ExtensionBrowserTest::LoadExtensionWithFlags(
    const base::FilePath& path, int flags) {
  base::FilePath extension_path = path;
  if (flags & kFlagRunAsServiceWorkerBasedExtension) {
    if (!CreateServiceWorkerBasedExtension(path, &extension_path))
      return nullptr;
  }
  return LoadExtensionWithInstallParam(extension_path, flags, std::string());
}

const Extension* ExtensionBrowserTest::LoadExtensionWithInstallParam(
    const base::FilePath& path,
    int flags,
    const std::string& install_param) {
  ChromeTestExtensionLoader loader(profile());
  loader.set_require_modern_manifest_version(
      (flags & kFlagAllowOldManifestVersions) == 0);
  loader.set_ignore_manifest_warnings(
      (flags & kFlagIgnoreManifestWarnings) != 0);
  loader.set_allow_incognito_access((flags & kFlagEnableIncognito) != 0);
  loader.set_allow_file_access((flags & kFlagEnableFileAccess) != 0);
  loader.set_install_param(install_param);
  if ((flags & kFlagLoadForLoginScreen) != 0) {
    loader.add_creation_flag(Extension::FOR_LOGIN_SCREEN);
    loader.set_location(Manifest::EXTERNAL_POLICY);
  }
  scoped_refptr<const Extension> extension = loader.LoadExtension(path);
  if (extension)
    observer_->set_last_loaded_extension_id(extension->id());
  return extension.get();
}

bool ExtensionBrowserTest::CreateServiceWorkerBasedExtension(
    const base::FilePath& path,
    base::FilePath* out_path) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // This dir will contain all files for the Service Worker based extension.
  base::FilePath temp_extension_container;
  if (!base::CreateTemporaryDirInDir(temp_dir_.GetPath(),
                                     base::FilePath::StringType(),
                                     &temp_extension_container)) {
    ADD_FAILURE() << "Could not create temporary dir for test under "
                  << temp_dir_.GetPath();
    return false;
  }

  // Copy all files from test dir to temp dir.
  if (!base::CopyDirectory(path, temp_extension_container,
                           true /* recursive */)) {
    ADD_FAILURE() << path.value() << " could not be copied to "
                  << temp_extension_container.value();
    return false;
  }

  const base::FilePath extension_root =
      temp_extension_container.Append(path.BaseName());

  std::string error;
  std::unique_ptr<base::DictionaryValue> manifest_dict =
      file_util::LoadManifest(extension_root, &error);
  if (!manifest_dict) {
    ADD_FAILURE() << path.value() << " could not load manifest: " << error;
    return false;
  }

  // Retrieve the value of the "background" key and verify that it is
  // non-persistent and specifies JS files.
  // Persistent background pages or background pages that specify HTML files
  // are not supported.
  base::Value* background_dict =
      manifest_dict->FindKeyOfType("background", base::Value::Type::DICTIONARY);
  if (!background_dict) {
    ADD_FAILURE() << path.value()
                  << " 'background' key not found in manifest.json";
    return false;
  }
  {
    base::Value* background_persistent = background_dict->FindKeyOfType(
        "persistent", base::Value::Type::BOOLEAN);
    if (!background_persistent || background_persistent->GetBool()) {
      ADD_FAILURE() << path.value()
                    << ": Only event pages can be loaded as SW extension.";
      return false;
    }
  }
  base::Value* background_scripts_list =
      background_dict->FindKeyOfType("scripts", base::Value::Type::LIST);
  if (!background_scripts_list) {
    ADD_FAILURE() << path.value()
                  << ": Only event pages with JS script(s) can be loaded "
                     "as SW extension.";
    return false;
  }

  // Number of JS scripts must be > 1.
  base::Value::ListStorage& scripts_list = background_scripts_list->GetList();
  if (scripts_list.size() < 1) {
    ADD_FAILURE() << path.value()
                  << ": Only event pages with JS script(s) can be loaded "
                     " as SW extension.";
    return false;
  }

  // Generate combined script as Service Worker script using importScripts().
  constexpr const char kGeneratedSWFileName[] = "generated_service_worker__.js";

  std::vector<std::string> script_filenames;
  for (const base::Value& script : scripts_list)
    script_filenames.push_back(base::StrCat({"'", script.GetString(), "'"}));

  base::FilePath combined_script_filepath =
      extension_root.AppendASCII(kGeneratedSWFileName);
  // Collision with generated script filename.
  if (base::PathExists(combined_script_filepath)) {
    ADD_FAILURE() << combined_script_filepath.value()
                  << " already exists, make sure " << path.value()
                  << " does not contained file named " << kGeneratedSWFileName;
    return false;
  }
  std::string generated_sw_script_content = base::StringPrintf(
      "importScripts(%s);", base::JoinString(script_filenames, ",").c_str());
  if (!file_test_util::WriteFile(combined_script_filepath,
                                 generated_sw_script_content)) {
    ADD_FAILURE() << "Could not write combined Service Worker script to: "
                  << combined_script_filepath.value();
    return false;
  }

  // Remove the existing background specification and replace it with a service
  // worker.
  background_dict->RemoveKey("persistent");
  background_dict->RemoveKey("scripts");
  background_dict->SetStringPath("service_worker", kGeneratedSWFileName);

  // Write out manifest.json.
  DictionaryBuilder manifest_builder(*manifest_dict);
  std::string manifest_contents = manifest_builder.ToJSON();
  base::FilePath manifest_path = extension_root.Append(kManifestFilename);
  if (!file_test_util::WriteFile(manifest_path, manifest_contents)) {
    ADD_FAILURE() << "Could not write manifest file to "
                  << manifest_path.value();
    return false;
  }

  *out_path = extension_root;
  return true;
}

const Extension* ExtensionBrowserTest::LoadExtensionAsComponentWithManifest(
    const base::FilePath& path,
    const base::FilePath::CharType* manifest_relative_path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string manifest;
  if (!base::ReadFileToString(path.Append(manifest_relative_path), &manifest)) {
    return NULL;
  }

  extension_service()->component_loader()->set_ignore_whitelist_for_testing(
      true);
  std::string extension_id =
      extension_service()->component_loader()->Add(manifest, path);
  const Extension* extension =
      extension_registry()->enabled_extensions().GetByID(extension_id);
  if (!extension)
    return NULL;
  observer_->set_last_loaded_extension_id(extension->id());
  return extension;
}

const Extension* ExtensionBrowserTest::LoadExtensionAsComponent(
    const base::FilePath& path) {
  return LoadExtensionAsComponentWithManifest(path, kManifestFilename);
}

const Extension* ExtensionBrowserTest::LoadAndLaunchApp(
    const base::FilePath& path) {
  const Extension* app = LoadExtension(path);
  CHECK(app);
  content::WindowedNotificationObserver app_loaded_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());
  apps::AppLaunchParams params(app->id(), LaunchContainer::kLaunchContainerNone,
                               WindowOpenDisposition::NEW_WINDOW,
                               AppLaunchSource::kSourceTest);
  params.command_line = *base::CommandLine::ForCurrentProcess();
  apps::LaunchService::Get(profile())->OpenApplication(params);
  app_loaded_observer.Wait();

  return app;
}

Browser* ExtensionBrowserTest::LaunchAppBrowser(const Extension* extension) {
  return browsertest_util::LaunchAppBrowser(profile(), extension);
}

Browser* ExtensionBrowserTest::LaunchBrowserForAppInTab(
    const Extension* extension) {
  return browsertest_util::LaunchBrowserForAppInTab(profile(), extension);
}

base::FilePath ExtensionBrowserTest::PackExtension(
    const base::FilePath& dir_path,
    int extra_run_flags) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath crx_path = temp_dir_.GetPath().AppendASCII("temp.crx");
  if (!base::DeleteFile(crx_path, false)) {
    ADD_FAILURE() << "Failed to delete crx: " << crx_path.value();
    return base::FilePath();
  }

  // Look for PEM files with the same name as the directory.
  base::FilePath pem_path =
      dir_path.ReplaceExtension(FILE_PATH_LITERAL(".pem"));
  base::FilePath pem_path_out;

  if (!base::PathExists(pem_path)) {
    pem_path = base::FilePath();
    pem_path_out = crx_path.DirName().AppendASCII("temp.pem");
    if (!base::DeleteFile(pem_path_out, false)) {
      ADD_FAILURE() << "Failed to delete pem: " << pem_path_out.value();
      return base::FilePath();
    }
  }

  return PackExtensionWithOptions(dir_path, crx_path, pem_path, pem_path_out,
                                  extra_run_flags);
}

base::FilePath ExtensionBrowserTest::PackExtensionWithOptions(
    const base::FilePath& dir_path,
    const base::FilePath& crx_path,
    const base::FilePath& pem_path,
    const base::FilePath& pem_out_path,
    int extra_run_flags) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (!base::PathExists(dir_path)) {
    ADD_FAILURE() << "Extension dir not found: " << dir_path.value();
    return base::FilePath();
  }

  if (!base::PathExists(pem_path) && pem_out_path.empty()) {
    ADD_FAILURE() << "Must specify a PEM file or PEM output path";
    return base::FilePath();
  }

  std::unique_ptr<ExtensionCreator> creator(new ExtensionCreator());
  if (!creator->Run(dir_path, crx_path, pem_path, pem_out_path,
                    extra_run_flags | ExtensionCreator::kOverwriteCRX)) {
    ADD_FAILURE() << "ExtensionCreator::Run() failed: "
                  << creator->error_message();
    return base::FilePath();
  }

  if (!base::PathExists(crx_path)) {
    ADD_FAILURE() << crx_path.value() << " was not created.";
    return base::FilePath();
  }
  return crx_path;
}

const Extension* ExtensionBrowserTest::UpdateExtensionWaitForIdle(
    const std::string& id,
    const base::FilePath& path,
    int expected_change) {
  return InstallOrUpdateExtension(id, path, INSTALL_UI_TYPE_NONE,
                                  expected_change, Manifest::INTERNAL,
                                  browser(), Extension::NO_FLAGS, false, false);
}

const Extension* ExtensionBrowserTest::InstallBookmarkApp(
    WebApplicationInfo info) {
  return browsertest_util::InstallBookmarkApp(profile(), std::move(info));
}

const Extension* ExtensionBrowserTest::InstallExtensionFromWebstore(
    const base::FilePath& path,
    int expected_change) {
  return InstallOrUpdateExtension(
      std::string(), path, INSTALL_UI_TYPE_AUTO_CONFIRM, expected_change,
      Manifest::INTERNAL, browser(), Extension::FROM_WEBSTORE, true, false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const std::string& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    int expected_change) {
  return InstallOrUpdateExtension(id, path, ui_type, expected_change,
                                  Manifest::INTERNAL, browser(),
                                  Extension::NO_FLAGS, true, false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const std::string& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    int expected_change,
    Browser* browser,
    Extension::InitFromValueFlags creation_flags) {
  return InstallOrUpdateExtension(id, path, ui_type, expected_change,
                                  Manifest::INTERNAL, browser, creation_flags,
                                  true, false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const std::string& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    int expected_change,
    Manifest::Location install_source) {
  return InstallOrUpdateExtension(id, path, ui_type, expected_change,
                                  install_source, browser(),
                                  Extension::NO_FLAGS, true, false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const std::string& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    int expected_change,
    Manifest::Location install_source,
    Browser* browser,
    Extension::InitFromValueFlags creation_flags,
    bool install_immediately,
    bool grant_permissions) {
  ExtensionRegistry* registry = extension_registry();
  size_t num_before = registry->enabled_extensions().size();

  {
    std::unique_ptr<ScopedTestDialogAutoConfirm> prompt_auto_confirm;
    if (ui_type == INSTALL_UI_TYPE_CANCEL) {
      prompt_auto_confirm.reset(new ScopedTestDialogAutoConfirm(
          ScopedTestDialogAutoConfirm::CANCEL));
    } else if (ui_type == INSTALL_UI_TYPE_NORMAL) {
      prompt_auto_confirm.reset(new ScopedTestDialogAutoConfirm(
          ScopedTestDialogAutoConfirm::NONE));
    } else if (ui_type == INSTALL_UI_TYPE_AUTO_CONFIRM) {
      prompt_auto_confirm.reset(new ScopedTestDialogAutoConfirm(
          ScopedTestDialogAutoConfirm::ACCEPT));
    }

    // TODO(tessamac): Update callers to always pass an unpacked extension
    //                 and then always pack the extension here.
    base::FilePath crx_path = path;
    if (crx_path.Extension() != FILE_PATH_LITERAL(".crx")) {
      int run_flags = ExtensionCreator::kNoRunFlags;
      if (creation_flags & Extension::FROM_BOOKMARK) {
        run_flags = ExtensionCreator::kBookmarkApp;
        if (install_source == Manifest::EXTERNAL_COMPONENT)
          run_flags |= ExtensionCreator::kSystemApp;
      }

      crx_path = PackExtension(path, run_flags);
    }
    if (crx_path.empty())
      return NULL;

    std::unique_ptr<ExtensionInstallPrompt> install_ui;
    if (prompt_auto_confirm) {
      install_ui.reset(new ExtensionInstallPrompt(
         browser->tab_strip_model()->GetActiveWebContents()));
    }
    scoped_refptr<CrxInstaller> installer(
        CrxInstaller::Create(extension_service(), std::move(install_ui)));
    installer->set_expected_id(id);
    installer->set_creation_flags(creation_flags);
    installer->set_install_source(install_source);
    installer->set_install_immediately(install_immediately);
    installer->set_allow_silent_install(grant_permissions);
    if (!installer->is_gallery_install()) {
      installer->set_off_store_install_allow_reason(
          CrxInstaller::OffStoreInstallAllowedInTest);
    }

    observer_->Watch(NOTIFICATION_CRX_INSTALLER_DONE,
                     content::Source<CrxInstaller>(installer.get()));

    installer->InstallCrx(crx_path);

    observer_->Wait();
  }

  size_t num_after = registry->enabled_extensions().size();
  EXPECT_EQ(num_before + expected_change, num_after);
  if (num_before + expected_change != num_after) {
    VLOG(1) << "Num extensions before: " << base::NumberToString(num_before)
            << " num after: " << base::NumberToString(num_after)
            << " Installed extensions follow:";

    for (const scoped_refptr<const Extension>& extension :
         registry->enabled_extensions())
      VLOG(1) << "  " << extension->id();

    VLOG(1) << "Errors follow:";
    const std::vector<base::string16>* errors =
        LoadErrorReporter::GetInstance()->GetErrors();
    for (auto iter = errors->begin(); iter != errors->end(); ++iter)
      VLOG(1) << *iter;

    return NULL;
  }

  if (!observer_->WaitForExtensionViewsToLoad())
    return NULL;
  return registry->GetExtensionById(last_loaded_extension_id(),
                                    ExtensionRegistry::ENABLED);
}

void ExtensionBrowserTest::ReloadExtension(const std::string& extension_id) {
  const Extension* extension =
      extension_registry()->GetInstalledExtension(extension_id);
  ASSERT_TRUE(extension);
  TestExtensionRegistryObserver observer(extension_registry(), extension_id);
  extension_service()->ReloadExtension(extension_id);
  observer.WaitForExtensionLoaded();

  // We need to let other ExtensionRegistryObservers handle the extension load
  // in order to finish initialization. This has to be done before waiting for
  // extension views to load, since we only register views after observing
  // extension load.
  base::RunLoop().RunUntilIdle();
  observer_->WaitForExtensionViewsToLoad();
}

void ExtensionBrowserTest::UnloadExtension(const std::string& extension_id) {
  extension_service()->UnloadExtension(extension_id,
                                       UnloadedExtensionReason::DISABLE);
}

void ExtensionBrowserTest::UninstallExtension(const std::string& extension_id) {
  extension_service()->UninstallExtension(
      extension_id, UNINSTALL_REASON_FOR_TESTING, nullptr);
}

void ExtensionBrowserTest::DisableExtension(const std::string& extension_id) {
  extension_service()->DisableExtension(extension_id,
                                        disable_reason::DISABLE_USER_ACTION);
}

void ExtensionBrowserTest::EnableExtension(const std::string& extension_id) {
  extension_service()->EnableExtension(extension_id);
}

void ExtensionBrowserTest::OpenWindow(content::WebContents* contents,
                                      const GURL& url,
                                      bool newtab_process_should_equal_opener,
                                      bool should_succeed,
                                      content::WebContents** newtab_result) {
  content::WebContentsAddedObserver tab_added_observer;
  ASSERT_TRUE(content::ExecuteScript(contents,
                                     "window.open('" + url.spec() + "');"));
  content::WebContents* newtab = tab_added_observer.GetWebContents();
  ASSERT_TRUE(newtab);
  WaitForLoadStop(newtab);

  if (should_succeed) {
    EXPECT_EQ(url, newtab->GetLastCommittedURL());
    EXPECT_EQ(content::PAGE_TYPE_NORMAL,
              newtab->GetController().GetLastCommittedEntry()->GetPageType());
  } else {
    // "Failure" comes in two forms: redirecting to about:blank or showing an
    // error page. At least one should be true.
    EXPECT_TRUE(
        newtab->GetLastCommittedURL() == GURL(url::kAboutBlankURL) ||
        newtab->GetController().GetLastCommittedEntry()->GetPageType() ==
            content::PAGE_TYPE_ERROR);
  }

  if (newtab_process_should_equal_opener) {
    EXPECT_EQ(contents->GetMainFrame()->GetSiteInstance(),
              newtab->GetMainFrame()->GetSiteInstance());
  } else {
    EXPECT_NE(contents->GetMainFrame()->GetSiteInstance(),
              newtab->GetMainFrame()->GetSiteInstance());
  }

  if (newtab_result)
    *newtab_result = newtab;
}

void ExtensionBrowserTest::NavigateInRenderer(content::WebContents* contents,
                                              const GURL& url) {
  // Note: We use ExecuteScript instead of ExecJS here, because ExecuteScript
  // works on pages with a Content Security Policy.
  EXPECT_TRUE(content::ExecuteScript(
      contents, "window.location = '" + url.spec() + "';"));
  content::WaitForLoadStop(contents);
  EXPECT_EQ(url, contents->GetController().GetLastCommittedEntry()->GetURL());
}

ExtensionHost* ExtensionBrowserTest::FindHostWithPath(ProcessManager* manager,
                                                      const std::string& path,
                                                      int expected_hosts) {
  ExtensionHost* result_host = nullptr;
  int num_hosts = 0;
  for (ExtensionHost* host : manager->background_hosts()) {
    if (host->GetURL().path() == path) {
      EXPECT_FALSE(result_host);
      result_host = host;
    }
    num_hosts++;
  }
  EXPECT_EQ(expected_hosts, num_hosts);
  return result_host;
}

std::string ExtensionBrowserTest::ExecuteScriptInBackgroundPage(
    const std::string& extension_id,
    const std::string& script,
    browsertest_util::ScriptUserActivation script_user_activation) {
  return browsertest_util::ExecuteScriptInBackgroundPage(
      profile(), extension_id, script, script_user_activation);
}

bool ExtensionBrowserTest::ExecuteScriptInBackgroundPageNoWait(
    const std::string& extension_id,
    const std::string& script) {
  return browsertest_util::ExecuteScriptInBackgroundPageNoWait(
      profile(), extension_id, script);
}

}  // namespace extensions
