// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/chrome_extension_test_notification_observer.h"
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
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/crx_file/crx_verifier.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/sync/model/string_ordinal.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/browser/updater/extension_cache_fake.h"
#include "extensions/common/api/web_accessible_resources.h"
#include "extensions/common/api/web_accessible_resources_mv2.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#endif

using extensions::mojom::ManifestLocation;

namespace extensions {

using extensions::service_worker_test_utils::TestServiceWorkerContextObserver;

namespace {

// Maps all chrome-extension://<id>/_test_resources/foo requests to
// <test_dir_root>/foo or <test_dir_gen_root>/foo, where |test_dir_gen_root| is
// inferred from <test_dir_root>. The latter is triggered only if the first path
// does not correspond to an existing file. This is what allows us to share code
// between tests without needing to duplicate files in each extension.
// Example invocation #1, where the requested file exists in |test_dir_root|
//   Input:
//     test_dir_root: /abs/path/src/chrome/test/data
//     directory_path: /abs/path/src/out/<out_dir>/resources/pdf
//     relative_path: _test_resources/webui/test_browser_proxy.js
//   Output:
//     directory_path: /abs/path/src/chrome/test/data
//     relative_path: webui/test_browser_proxy.js
//
// Example invocation #2, where the requested file exists in |test_dir_gen_root|
//   Input:
//     test_dir_root: /abs/path/src/chrome/test/data
//     directory_path: /abs/path/src/out/<out_dir>/resources/pdf
//     relative_path: _test_resources/webui/test_browser_proxy.js
//   Output:
//     directory_path: /abs/path/src/out/<out_dir>/gen/chrome/test/data
//     relative_path: webui/test_browser_proxy.js
void ExtensionProtocolTestResourcesHandler(const base::FilePath& test_dir_root,
                                           base::FilePath* directory_path,
                                           base::FilePath* relative_path) {
  // Only map paths that begin with _test_resources.
  if (!base::FilePath(FILE_PATH_LITERAL("_test_resources"))
           .IsParent(*relative_path)) {
    return;
  }

  // Strip the '_test_resources/' prefix from |relative_path|.
  std::vector<base::FilePath::StringType> components =
      relative_path->GetComponents();
  DCHECK_GT(components.size(), 1u);
  base::FilePath new_relative_path;
  for (size_t i = 1u; i < components.size(); ++i)
    new_relative_path = new_relative_path.Append(components[i]);
  *relative_path = new_relative_path;

  // Check if the file exists in the |test_dir_root| folder first.
  base::FilePath src_path = test_dir_root.Append(new_relative_path);
  // Replace _test_resources/foo with <test_dir_root>/foo.
  *directory_path = test_dir_root;
  {
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    if (base::PathExists(src_path)) {
      return;
    }
  }

  // Infer |test_dir_gen_root| from |test_dir_root|.
  // E.g., if |test_dir_root| is /abs/path/src/chrome/test/data,
  // |test_dir_gen_root| will be /abs/path/out/<out_dir>/gen/chrome/test/data.
  base::FilePath dir_src_test_data_root;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &dir_src_test_data_root);
  base::FilePath gen_test_data_root_dir;
  base::PathService::Get(base::DIR_GEN_TEST_DATA_ROOT, &gen_test_data_root_dir);
  base::FilePath relative_root_path;
  dir_src_test_data_root.AppendRelativePath(test_dir_root, &relative_root_path);
  base::FilePath test_dir_gen_root =
      gen_test_data_root_dir.Append(relative_root_path);

  // Then check if the file exists in the |test_dir_gen_root| folder
  // covering cases where the test file is generated at build time.
  base::FilePath gen_path = test_dir_gen_root.Append(new_relative_path);
  {
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    if (base::PathExists(gen_path)) {
      *directory_path = test_dir_gen_root;
    }
  }
}

// Creates a copy of `source` within `temp_dir` and populates `out` with the
// destination path. Returns true on success.
bool CreateTempDirectoryCopy(const base::FilePath& temp_dir,
                             const base::FilePath& source,
                             base::FilePath* out) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath temp_subdir;
  if (!base::CreateTemporaryDirInDir(temp_dir, base::FilePath::StringType(),
                                     &temp_subdir)) {
    ADD_FAILURE() << "Could not create temporary dir for test under "
                  << temp_dir;
    return false;
  }

  // Copy all files from `source` to `temp_subdir`.
  if (!base::CopyDirectory(source, temp_subdir, true /* recursive */)) {
    ADD_FAILURE() << source.value() << " could not be copied to "
                  << temp_subdir.value();
    return false;
  }

  *out = temp_subdir.Append(source.BaseName());
  return true;
}

// Moves match patterns from the `permissions_key` list to the
// `host_permissions_key` list.
void DoMoveHostPermissions(base::Value::Dict& manifest_dict,
                           const char* permissions_key,
                           const char* host_permissions_key) {
  base::Value::List* const permissions =
      manifest_dict.FindList(permissions_key);
  if (!permissions) {
    return;
  }

  // Add the permissions to the appropriate destinations, then update/add
  // the target lists as appropriate.
  base::Value::List permissions_list;
  base::Value::List host_permissions_list;
  for (auto& value : *permissions) {
    CHECK(value.is_string());
    const std::string& str_value = value.GetString();
    if (str_value == "<all_urls>" ||
        str_value.find("://") != std::string::npos) {
      host_permissions_list.Append(std::move(value));
    } else {
      permissions_list.Append(std::move(value));
    }
  }

  if (permissions_list.empty()) {
    manifest_dict.Remove(permissions_key);
  } else {
    *permissions = std::move(permissions_list);
  }

  if (!host_permissions_list.empty()) {
    manifest_dict.Set(host_permissions_key, std::move(host_permissions_list));
  }
}

// Moves match patterns from permissions/optional_permissions to the
// host_permissions/optional_host_permissions.
void MoveHostPermissions(base::Value::Dict& manifest_dict) {
  DoMoveHostPermissions(manifest_dict, manifest_keys::kPermissions,
                        manifest_keys::kHostPermissions);
  DoMoveHostPermissions(manifest_dict, manifest_keys::kOptionalPermissions,
                        manifest_keys::kOptionalHostPermissions);
}

using web_accessible_resource =
    api::web_accessible_resources::WebAccessibleResource;

// Upgrades MV2 format to MV3 format.
void UpgradeWebAccessibleResources(base::Value::Dict& manifest_dict) {
  base::Value::List* const web_accessible_resources = manifest_dict.FindList(
      api::web_accessible_resources::ManifestKeys::kWebAccessibleResources);
  if (!web_accessible_resources) {
    return;
  }

  // Copy all of the entries to a single dictionary entry that matches all
  // URLs.
  auto war_dict = base::Value::Dict()
                      .Set(web_accessible_resource::kResources,
                           web_accessible_resources->Clone())
                      .Set(web_accessible_resource::kMatches,
                           base::Value::List().Append("<all_urls>"));

  // Clear the list and append the dictionary.
  web_accessible_resources->clear();
  web_accessible_resources->Append(std::move(war_dict));
}

// Modifies `manifest_dict` changing its manifest version to 3.
bool ModifyManifestForManifestVersion3(base::Value::Dict& manifest_dict) {
  // This should only be used for manifest v2 extension.
  std::optional<int> current_manifest_version =
      manifest_dict.FindInt(manifest_keys::kManifestVersion);
  if (!current_manifest_version || *current_manifest_version != 2) {
    ADD_FAILURE() << manifest_dict << " should have a manifest version of 2.";
    return false;
  }

  UpgradeWebAccessibleResources(manifest_dict);
  MoveHostPermissions(manifest_dict);

  manifest_dict.Set(manifest_keys::kManifestVersion, 3);

  return true;
}

// Modifies extension at `extension_root` and its `manifest_dict` converting it
// to a service worker based extension.
// NOTE: The conversion works only for extensions with background.scripts and
// requires the background.persistent key. The background.page key is not
// supported.
bool ModifyExtensionForServiceWorker(const base::FilePath& extension_root,
                                     base::Value::Dict& manifest_dict) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Retrieve the value of the `background` key and verify that it has
  // the `persistent` key and specifies JS files.
  // Background pages that specify HTML files are not supported.
  base::Value::Dict* background_dict = manifest_dict.FindDict("background");
  if (!background_dict) {
    ADD_FAILURE() << extension_root.value()
                  << " 'background' key not found in manifest.json";
    return false;
  }
  {
    std::optional<bool> background_persistent =
        background_dict->FindBool("persistent");
    if (!background_persistent.has_value()) {
      ADD_FAILURE() << extension_root.value()
                    << ": The \"persistent\" key must be specified to run as a "
                       "Service Worker-based extension.";
      return false;
    }
  }
  base::Value::List* background_scripts_list =
      background_dict->FindList("scripts");
  // Number of JS scripts must be >= 1.
  if (!background_scripts_list || background_scripts_list->empty()) {
    ADD_FAILURE() << extension_root.value()
                  << ": Only extensions with JS script(s) can be loaded "
                     "as a sw-based extension.";
    return false;
  }

  // Generate combined script as Service Worker script using importScripts().
  static constexpr const char kGeneratedSWFileName[] =
      "generated_service_worker__.js";

  std::vector<std::string> script_filenames;
  for (const base::Value& script : *background_scripts_list)
    script_filenames.push_back(base::StrCat({"'", script.GetString(), "'"}));

  base::FilePath combined_script_filepath =
      extension_root.AppendASCII(kGeneratedSWFileName);
  // Collision with generated script filename.
  if (base::PathExists(combined_script_filepath)) {
    ADD_FAILURE() << combined_script_filepath.value()
                  << " already exists, make sure " << extension_root.value()
                  << " does not contained file named " << kGeneratedSWFileName;
    return false;
  }
  std::string generated_sw_script_content = base::StringPrintf(
      "importScripts(%s);", base::JoinString(script_filenames, ",").c_str());
  if (!base::WriteFile(combined_script_filepath, generated_sw_script_content)) {
    ADD_FAILURE() << "Could not write combined Service Worker script to: "
                  << combined_script_filepath.value();
    return false;
  }

  // Remove the existing background specification and replace it with a service
  // worker.
  background_dict->Remove("persistent");
  background_dict->Remove("scripts");
  background_dict->Set("service_worker", kGeneratedSWFileName);

  return true;
}

}  // namespace

ExtensionBrowserTest::ExtensionBrowserTest(ContextType context_type)
    :
#if BUILDFLAG(IS_CHROMEOS_ASH)
      set_chromeos_user_(true),
#endif
      context_type_(context_type),
      // TODO(crbug.com/40261741): Move this ScopedCurrentChannel down into
      // tests that specifically require it.
      current_channel_(version_info::Channel::UNKNOWN),
      override_prompt_for_external_extensions_(
          FeatureSwitch::prompt_for_external_extensions(),
          false),
#if BUILDFLAG(IS_WIN)
      user_desktop_override_(base::DIR_USER_DESKTOP),
      common_desktop_override_(base::DIR_COMMON_DESKTOP),
      user_quick_launch_override_(base::DIR_USER_QUICK_LAUNCH),
      start_menu_override_(base::DIR_START_MENU),
      common_start_menu_override_(base::DIR_COMMON_START_MENU),
#endif
      profile_(nullptr),
      verifier_format_override_(crx_file::VerifierFormat::CRX3) {
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
}

ExtensionBrowserTest::~ExtensionBrowserTest() = default;

ExtensionService* ExtensionBrowserTest::extension_service() {
  return ExtensionSystem::Get(profile())->extension_service();
}

ExtensionRegistry* ExtensionBrowserTest::extension_registry() {
  return ExtensionRegistry::Get(profile());
}

void ExtensionBrowserTest::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  last_loaded_extension_id_ = extension->id();
  VLOG(1) << "Got EXTENSION_LOADED notification.";
}

void ExtensionBrowserTest::OnShutdown(ExtensionRegistry* registry) {
  registry_observation_.Reset();
}

Profile* ExtensionBrowserTest::profile() {
  if (!profile_) {
    if (browser()) {
      profile_ = browser()->profile();
    } else {
      profile_ = ProfileManager::GetLastUsedProfile();
    }
  }
  return profile_;
}

bool ExtensionBrowserTest::ShouldEnableContentVerification() {
  return false;
}

bool ExtensionBrowserTest::ShouldEnableInstallVerification() {
  return false;
}

bool ExtensionBrowserTest::ShouldAllowMV2Extensions() {
  return true;
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
  return nullptr;
}

void ExtensionBrowserTest::SetUp() {
  test_extension_cache_ = std::make_unique<ExtensionCacheFake>();
  InProcessBrowserTest::SetUp();
}

void ExtensionBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII("extensions");

  if (!ShouldEnableContentVerification()) {
    ignore_content_verification_ =
        std::make_unique<ScopedIgnoreContentVerifierForTest>();
  }

  if (!ShouldEnableInstallVerification()) {
    ignore_install_verification_ =
        std::make_unique<ScopedInstallVerifierBypassForTest>();
  }

  if (ShouldAllowMV2Extensions()) {
    mv2_enabler_.emplace();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (set_chromeos_user_) {
    // This makes sure that we create the Default profile first, with no
    // ExtensionService and then the real profile with one, as we do when
    // running on chromeos.
    command_line->AppendSwitchASCII(ash::switches::kLoginUser,
                                    "testuser@gmail.com");
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile, "user");
  }
#endif
}

void ExtensionBrowserTest::SetUpOnMainThread() {
  observer_ =
      std::make_unique<ChromeExtensionTestNotificationObserver>(browser());
  if (extension_service()->updater()) {
    extension_service()->updater()->SetExtensionCacheForTesting(
        test_extension_cache_.get());
  }

  test_protocol_handler_ = base::BindRepeating(
      &ExtensionProtocolTestResourcesHandler, GetTestResourcesParentDir());
  SetExtensionProtocolTestHandler(&test_protocol_handler_);
  content::URLDataSource::Add(profile(),
                              std::make_unique<ThemeSource>(profile()));
  registry_observation_.Observe(ExtensionRegistry::Get(profile()));
}

void ExtensionBrowserTest::TearDownOnMainThread() {
  SetExtensionProtocolTestHandler(nullptr);
  registry_observation_.Reset();
}

const Extension* ExtensionBrowserTest::LoadExtension(
    const base::FilePath& path) {
  return LoadExtension(path, {});
}

const Extension* ExtensionBrowserTest::LoadExtension(
    const base::FilePath& path,
    const LoadOptions& options) {
  base::FilePath extension_path;
  if (!ModifyExtensionIfNeeded(options, path, &extension_path)) {
    return nullptr;
  }

  if (options.load_as_component) {
    // TODO(crbug.com/40166157): Decide if other load options
    // can/should be supported when load_as_component is true.
    DCHECK(!options.allow_in_incognito);
    DCHECK(!options.allow_file_access);
    DCHECK(!options.ignore_manifest_warnings);
    DCHECK(options.wait_for_renderers);
    DCHECK(options.install_param == nullptr);
    DCHECK(!options.wait_for_registration_stored);
    return LoadExtensionAsComponent(extension_path);
  }
  ChromeTestExtensionLoader loader(profile());
  loader.set_allow_incognito_access(options.allow_in_incognito);
  loader.set_allow_file_access(options.allow_file_access);
  loader.set_ignore_manifest_warnings(options.ignore_manifest_warnings);
  loader.set_wait_for_renderers(options.wait_for_renderers);

  if (options.install_param != nullptr) {
    loader.set_install_param(options.install_param);
  }

  std::unique_ptr<TestServiceWorkerContextObserver> registration_observer;

  if (options.wait_for_registration_stored) {
    registration_observer =
        std::make_unique<TestServiceWorkerContextObserver>(profile_);
  }

  scoped_refptr<const Extension> extension =
      loader.LoadExtension(extension_path);
  if (extension) {
    last_loaded_extension_id_ = extension->id();
  }

  if (options.wait_for_registration_stored &&
      BackgroundInfo::IsServiceWorkerBased(extension.get())) {
    registration_observer->WaitForRegistrationStored();
  }

  return extension.get();
}

const Extension* ExtensionBrowserTest::LoadExtensionAsComponentWithManifest(
    const base::FilePath& path,
    const base::FilePath::CharType* manifest_relative_path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string manifest;
  if (!base::ReadFileToString(path.Append(manifest_relative_path), &manifest)) {
    return nullptr;
  }

  extension_service()->component_loader()->set_ignore_allowlist_for_testing(
      true);
  extensions::ExtensionId extension_id =
      extension_service()->component_loader()->Add(manifest, path);
  const Extension* extension =
      extension_registry()->enabled_extensions().GetByID(extension_id);
  if (!extension) {
    return nullptr;
  }
  last_loaded_extension_id_ = extension->id();
  return extension;
}

const Extension* ExtensionBrowserTest::LoadExtensionAsComponent(
    const base::FilePath& path) {
  return LoadExtensionAsComponentWithManifest(path, kManifestFilename);
}

const Extension* ExtensionBrowserTest::LoadAndLaunchApp(
    const base::FilePath& path,
    bool uses_guest_view) {
  const Extension* app = LoadExtension(path);
  CHECK(app);
  content::CreateAndLoadWebContentsObserver app_loaded_observer(
      /*num_expected_contents=*/uses_guest_view ? 2 : 1);
  apps::AppLaunchParams params(
      app->id(), apps::LaunchContainer::kLaunchContainerNone,
      WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest);
  params.command_line = *base::CommandLine::ForCurrentProcess();
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->BrowserAppLauncher()
      ->LaunchAppWithParamsForTesting(std::move(params));
  app_loaded_observer.Wait();

  return app;
}

Browser* ExtensionBrowserTest::LaunchAppBrowser(const Extension* extension) {
  return browsertest_util::LaunchAppBrowser(profile(), extension);
}

base::FilePath ExtensionBrowserTest::PackExtension(
    const base::FilePath& dir_path,
    int extra_run_flags) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath crx_path = temp_dir_.GetPath().AppendASCII("temp.crx");
  if (!base::DeleteFile(crx_path)) {
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
    if (!base::DeleteFile(pem_path_out)) {
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
    const extensions::ExtensionId& id,
    const base::FilePath& path,
    std::optional<int> expected_change) {
  return InstallOrUpdateExtension(
      id, path, InstallUIType::kNone, std::move(expected_change),
      ManifestLocation::kInternal, GetWindowController(), Extension::NO_FLAGS,
      false, false);
}

const Extension* ExtensionBrowserTest::InstallExtensionWithUIAutoConfirm(
    const base::FilePath& path,
    std::optional<int> expected_change,
    Browser* browser) {
  return InstallOrUpdateExtension(
      std::string(), path, InstallUIType::kAutoConfirm,
      std::move(expected_change), browser->extension_window_controller(),
      Extension::NO_FLAGS);
}

const Extension* ExtensionBrowserTest::InstallExtensionFromWebstore(
    const base::FilePath& path,
    std::optional<int> expected_change) {
  return InstallOrUpdateExtension(
      std::string(), path, InstallUIType::kAutoConfirm,
      std::move(expected_change), ManifestLocation::kInternal,
      GetWindowController(), Extension::FROM_WEBSTORE, true, false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const extensions::ExtensionId& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    std::optional<int> expected_change) {
  return InstallOrUpdateExtension(id, path, ui_type, std::move(expected_change),
                                  ManifestLocation::kInternal,
                                  GetWindowController(), Extension::NO_FLAGS,
                                  true, false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const extensions::ExtensionId& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    std::optional<int> expected_change,
    WindowController* window_controller,
    Extension::InitFromValueFlags creation_flags) {
  return InstallOrUpdateExtension(id, path, ui_type, std::move(expected_change),
                                  ManifestLocation::kInternal,
                                  window_controller, creation_flags, true,
                                  false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const extensions::ExtensionId& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    std::optional<int> expected_change,
    ManifestLocation install_source) {
  return InstallOrUpdateExtension(id, path, ui_type, std::move(expected_change),
                                  install_source, GetWindowController(),
                                  Extension::NO_FLAGS, true, false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const extensions::ExtensionId& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    std::optional<int> expected_change,
    ManifestLocation install_source,
    WindowController* window_controller,
    Extension::InitFromValueFlags creation_flags,
    bool install_immediately,
    bool grant_permissions) {
  ExtensionRegistry* registry = extension_registry();
  size_t num_before = registry->enabled_extensions().size();

  scoped_refptr<CrxInstaller> installer;
  std::optional<CrxInstallError> install_error;
  {
    std::unique_ptr<ScopedTestDialogAutoConfirm> prompt_auto_confirm;
    if (ui_type == InstallUIType::kCancel) {
      prompt_auto_confirm = std::make_unique<ScopedTestDialogAutoConfirm>(
          ScopedTestDialogAutoConfirm::CANCEL);
    } else if (ui_type == InstallUIType::kNormal) {
      prompt_auto_confirm = std::make_unique<ScopedTestDialogAutoConfirm>(
          ScopedTestDialogAutoConfirm::NONE);
    } else if (ui_type == InstallUIType::kAutoConfirm) {
      prompt_auto_confirm = std::make_unique<ScopedTestDialogAutoConfirm>(
          ScopedTestDialogAutoConfirm::ACCEPT);
    }

    // TODO(tessamac): Update callers to always pass an unpacked extension
    //                 and then always pack the extension here.
    base::FilePath crx_path = path;
    if (crx_path.Extension() != FILE_PATH_LITERAL(".crx")) {
      crx_path = PackExtension(path, ExtensionCreator::kNoRunFlags);
    }
    if (crx_path.empty()) {
      return nullptr;
    }

    std::unique_ptr<ExtensionInstallPrompt> install_ui;
    if (prompt_auto_confirm) {
      install_ui = std::make_unique<ExtensionInstallPrompt>(
          window_controller->GetActiveTab());
    }
    installer =
        CrxInstaller::Create(extension_service(), std::move(install_ui));
    installer->set_expected_id(id);
    installer->set_creation_flags(creation_flags);
    installer->set_install_source(install_source);
    installer->set_install_immediately(install_immediately);
    installer->set_allow_silent_install(grant_permissions);
    if (!installer->is_gallery_install()) {
      installer->set_off_store_install_allow_reason(
          CrxInstaller::OffStoreInstallAllowedInTest);
    }

    base::test::TestFuture<std::optional<CrxInstallError>>
        installer_done_future;
    installer->AddInstallerCallback(
        installer_done_future
            .GetCallback<const std::optional<CrxInstallError>&>());

    installer->InstallCrx(crx_path);

    install_error = installer_done_future.Get();
  }

  if (expected_change.has_value()) {
    size_t num_after = registry->enabled_extensions().size();
    EXPECT_EQ(num_before + expected_change.value(), num_after);
    if (num_before + expected_change.value() != num_after) {
      VLOG(1) << "Num extensions before: " << base::NumberToString(num_before)
              << " num after: " << base::NumberToString(num_after)
              << " Installed extensions follow:";

      for (const scoped_refptr<const Extension>& extension :
           registry->enabled_extensions()) {
        VLOG(1) << "  " << extension->id();
      }

      VLOG(1) << "Errors follow:";
      const std::vector<std::u16string>* errors =
          LoadErrorReporter::GetInstance()->GetErrors();
      for (const auto& error : *errors) {
        VLOG(1) << error;
      }

      return nullptr;
    }
  }

  if (!observer_->WaitForExtensionViewsToLoad()) {
    return nullptr;
  }

  if (install_error) {
    return nullptr;
  }

  // Even though we can already get the Extension from the CrxInstaller,
  // ensure it's also in the list of enabled extensions.
  return registry->enabled_extensions().GetByID(installer->extension()->id());
}

void ExtensionBrowserTest::ReloadExtension(
    const extensions::ExtensionId& extension_id) {
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

void ExtensionBrowserTest::UnloadExtension(
    const extensions::ExtensionId& extension_id) {
  extension_service()->UnloadExtension(extension_id,
                                       UnloadedExtensionReason::DISABLE);
}

void ExtensionBrowserTest::UninstallExtension(
    const extensions::ExtensionId& extension_id) {
  extension_service()->UninstallExtension(
      extension_id, UNINSTALL_REASON_FOR_TESTING, nullptr);
}

void ExtensionBrowserTest::DisableExtension(
    const extensions::ExtensionId& extension_id) {
  extension_service()->DisableExtension(extension_id,
                                        disable_reason::DISABLE_USER_ACTION);
}

void ExtensionBrowserTest::EnableExtension(
    const extensions::ExtensionId& extension_id) {
  extension_service()->EnableExtension(extension_id);
}

bool ExtensionBrowserTest::WaitForPageActionVisibilityChangeTo(int count) {
  return observer_->WaitForPageActionVisibilityChangeTo(count);
}

bool ExtensionBrowserTest::WaitForExtensionViewsToLoad() {
  return observer_->WaitForExtensionViewsToLoad();
}

bool ExtensionBrowserTest::WaitForExtensionIdle(
    const extensions::ExtensionId& extension_id) {
  return observer_->WaitForExtensionIdle(extension_id);
}

bool ExtensionBrowserTest::WaitForExtensionNotIdle(
    const extensions::ExtensionId& extension_id) {
  return observer_->WaitForExtensionNotIdle(extension_id);
}

void ExtensionBrowserTest::OpenWindow(content::WebContents* contents,
                                      const GURL& url,
                                      bool newtab_process_should_equal_opener,
                                      bool should_succeed,
                                      content::WebContents** newtab_result) {
  content::WebContentsAddedObserver tab_added_observer;
  ASSERT_TRUE(content::ExecJs(contents, "window.open('" + url.spec() + "');"));
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
    EXPECT_EQ(contents->GetPrimaryMainFrame()->GetSiteInstance(),
              newtab->GetPrimaryMainFrame()->GetSiteInstance());
  } else {
    EXPECT_NE(contents->GetPrimaryMainFrame()->GetSiteInstance(),
              newtab->GetPrimaryMainFrame()->GetSiteInstance());
  }

  if (newtab_result) {
    *newtab_result = newtab;
  }
}

bool ExtensionBrowserTest::NavigateInRenderer(content::WebContents* contents,
                                              const GURL& url) {
  EXPECT_TRUE(
      content::ExecJs(contents, "window.location = '" + url.spec() + "';"));
  bool result = content::WaitForLoadStop(contents);
  EXPECT_EQ(url, contents->GetController().GetLastCommittedEntry()->GetURL());
  return result;
}

ExtensionHost* ExtensionBrowserTest::FindHostWithPath(ProcessManager* manager,
                                                      const std::string& path,
                                                      int expected_hosts) {
  ExtensionHost* result_host = nullptr;
  int num_hosts = 0;
  for (ExtensionHost* host : manager->background_hosts()) {
    if (host->GetLastCommittedURL().path() == path) {
      EXPECT_FALSE(result_host);
      result_host = host;
    }
    num_hosts++;
  }
  EXPECT_EQ(expected_hosts, num_hosts);
  return result_host;
}

base::Value ExtensionBrowserTest::ExecuteScriptInBackgroundPage(
    const extensions::ExtensionId& extension_id,
    const std::string& script,
    browsertest_util::ScriptUserActivation script_user_activation) {
  return browsertest_util::ExecuteScriptInBackgroundPage(
      profile(), extension_id, script, script_user_activation);
}

std::string ExtensionBrowserTest::ExecuteScriptInBackgroundPageDeprecated(
    const extensions::ExtensionId& extension_id,
    const std::string& script,
    browsertest_util::ScriptUserActivation script_user_activation) {
  return browsertest_util::ExecuteScriptInBackgroundPageDeprecated(
      profile(), extension_id, script, script_user_activation);
}

bool ExtensionBrowserTest::ExecuteScriptInBackgroundPageNoWait(
    const extensions::ExtensionId& extension_id,
    const std::string& script,
    browsertest_util::ScriptUserActivation script_user_activation) {
  return browsertest_util::ExecuteScriptInBackgroundPageNoWait(
      profile(), extension_id, script, script_user_activation);
}

content::ServiceWorkerContext* ExtensionBrowserTest::GetServiceWorkerContext() {
  return GetServiceWorkerContext(profile());
}

// static
content::ServiceWorkerContext* ExtensionBrowserTest::GetServiceWorkerContext(
    content::BrowserContext* browser_context) {
  return service_worker_test_utils::GetServiceWorkerContext(browser_context);
}

bool ExtensionBrowserTest::ModifyExtensionIfNeeded(
    const LoadOptions& options,
    const base::FilePath& input_path,
    base::FilePath* out_path) {
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;

  const ContextType context_type_to_use =
      options.context_type == ContextType::kNone ? context_type_
                                                 : options.context_type;

  // Use context_type_ if LoadOptions.context_type is unspecified.
  // Otherwise, use LoadOptions.context_type.
  const bool load_as_service_worker =
      IsServiceWorkerContext(context_type_to_use);

  // Early return if no modification is needed.
  if (!load_as_service_worker && !options.load_as_manifest_version_3) {
    *out_path = input_path;
    return true;
  }

  // Tests that have a PRE_ stage need to exist in a temporary directory that
  // persists after the test fixture is destroyed. The test bots are configured
  // to use a unique temp directory that's cleaned up after the tests run, so
  // this won't pollute the system tmp directory.
  base::FilePath temp_dir;
  if (GetTestPreCount() == 0) {
    temp_dir = temp_dir_.GetPath();
  } else if (!base::GetTempDir(&temp_dir)) {
    ADD_FAILURE() << "Could not get temporary dir for test.";
    return false;
  }

  base::FilePath extension_root;
  if (!CreateTempDirectoryCopy(temp_dir, input_path, &extension_root)) {
    return false;
  }

  std::string error;
  std::optional<base::Value::Dict> manifest_dict =
      file_util::LoadManifest(extension_root, &error);
  if (!manifest_dict) {
    ADD_FAILURE() << extension_root.value()
                  << " could not load manifest: " << error;
    return false;
  }

  if (load_as_service_worker &&
      !ModifyExtensionForServiceWorker(extension_root, *manifest_dict)) {
    return false;
  }

  const bool is_service_worker_mv3 =
      context_type_to_use == ContextType::kServiceWorker;

  // Update the manifest if converting to a service worker MV3-based
  // extension or if requested in `options`.
  if ((is_service_worker_mv3 || options.load_as_manifest_version_3) &&
      !ModifyManifestForManifestVersion3(*manifest_dict)) {
    return false;
  }

  // Write out manifest.json.
  base::FilePath manifest_path = extension_root.Append(kManifestFilename);
  if (!JSONFileValueSerializer(manifest_path).Serialize(*manifest_dict)) {
    ADD_FAILURE() << "Could not write manifest file to "
                  << manifest_path.value();
    return false;
  }

  *out_path = extension_root;
  return true;
}

WindowController* ExtensionBrowserTest::GetWindowController() {
#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
  // TODO(b/361838438): Provide an implementation for the desktop android build.
  return nullptr;
#else
  return browser()->extension_window_controller();
#endif
}

}  // namespace extensions
