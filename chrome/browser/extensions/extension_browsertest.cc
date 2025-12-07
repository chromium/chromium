// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_future.h"
#include "base/version_info/channel.h"
#include "chrome/browser/extensions/chrome_extension_test_notification_observer.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_browser_test_util.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/crx_file/crx_verifier.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/scoped_ignore_content_verifier_for_test.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/updater/extension_cache_fake.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/test/extension_background_page_waiter.h"
#include "extensions/test/extension_test_notification_observer.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/check.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/test/base/android/android_ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
namespace {

using ContextType = extensions::browser_test_util::ContextType;
using extensions::service_worker_test_utils::TestServiceWorkerContextObserver;

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  NotificationDisplayServiceTester::EnsureFactoryBuilt();
}

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
  for (size_t i = 1u; i < components.size(); ++i) {
    new_relative_path = new_relative_path.Append(components[i]);
  }
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

#if BUILDFLAG(IS_ANDROID)
// ActivityType that doesn't restore tabs on cold start. Any type other than
// kTabbed is fine.
const auto kTestActivityType = chrome::android::ActivityType::kCustomTab;
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

ExtensionBrowserTest::ExtensionBrowserTest(ContextType context_type)
    : context_type_(context_type),
      platform_delegate_(*this),
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
      verifier_format_override_(crx_file::VerifierFormat::CRX3) {
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
}

ExtensionBrowserTest::~ExtensionBrowserTest() = default;

bool ExtensionBrowserTest::ShouldEnableContentVerification() {
  return false;
}

bool ExtensionBrowserTest::ShouldEnableInstallVerification() {
  return false;
}

bool ExtensionBrowserTest::ShouldAllowMV2Extensions() {
  return true;
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

  EnsureBrowserContextKeyedServiceFactoriesBuilt();
  PlatformBrowserTest::SetUp();
}

void ExtensionBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  PlatformBrowserTest::SetUpCommandLine(command_line);

  // On Android, these are handled in SetUpOnMainThread().
#if !BUILDFLAG(IS_ANDROID)
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII("extensions");
#endif

  if (!ShouldEnableContentVerification()) {
    ignore_content_verification_ =
        std::make_unique<ScopedIgnoreContentVerifierForTest>();
  }

  if (!ShouldEnableInstallVerification()) {
    ignore_install_verification_ =
        std::make_unique<ScopedInstallVerifierBypassForTest>();
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (set_chromeos_user_) {
    // This makes sure that we create the Default profile first, with no
    // ExtensionService and then the real profile with one, as we do when
    // running on chromeos.
    command_line->AppendSwitchASCII(ash::switches::kLoginUser,
                                    "testuser@gmail.com");
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile, "user");
  }
#endif

  if (ShouldAllowMV2Extensions()) {
    mv2_enabler_.emplace();
  }
}

void ExtensionBrowserTest::SetUpOnMainThread() {
  PlatformBrowserTest::SetUpOnMainThread();

  // On non-Android, these are handled in SetUpCommandLine().
#if BUILDFLAG(IS_ANDROID)
  RegisterPathProvider();
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII("extensions");
#endif

  SetUpTestProtocolHandler();
  registry_observation_.Observe(ExtensionRegistry::Get(profile()));
  content::WebContents* web_contents = GetActiveWebContents();
  // `web_contents` may be null if the test doesn't open an immediate browser
  // window.
  if (web_contents) {
    web_contents_ = web_contents->GetWeakPtr();
  }

  test_notification_observer_ =
      std::make_unique<ExtensionTestNotificationObserver>(profile());

  ExtensionUpdater* updater = ExtensionUpdater::Get(profile());
  if (updater->enabled()) {
    updater->SetExtensionCacheForTesting(test_extension_cache_.get());
  }

  platform_delegate_.SetUpOnMainThread();
}

void ExtensionBrowserTest::TearDown() {
  web_contents_.reset();
  PlatformBrowserTest::TearDown();
}

void ExtensionBrowserTest::TearDownOnMainThread() {
  TearDownTestProtocolHandler();

#if BUILDFLAG(IS_ANDROID)
  // Close any incognito tabs.
  incognito_tab_model_.reset();
#endif

  // Stop observing any notifications when we're tearing down the test.
  test_notification_observer_.reset();

  registry_observation_.Reset();
  PlatformBrowserTest::TearDownOnMainThread();
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

ExtensionRegistry* ExtensionBrowserTest::extension_registry() {
  return ExtensionRegistry::Get(profile());
}

ExtensionRegistrar* ExtensionBrowserTest::extension_registrar() {
  return ExtensionRegistrar::Get(profile());
}

base::FilePath ExtensionBrowserTest::GetTestResourcesParentDir() {
  // Don't use |test_data_dir_| here (even though it points to
  // chrome/test/data/extensions by default) because subclasses have the ability
  // to alter it by overriding the SetUpCommandLine() method.
  base::FilePath test_root_path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_root_path);
  return test_root_path.AppendASCII("extensions");
}

const Extension* ExtensionBrowserTest::LoadExtension(
    const base::FilePath& path) {
  return LoadExtension(path, {});
}

const Extension* ExtensionBrowserTest::LoadExtension(
    const base::FilePath& path,
    const LoadOptions& options) {
  base::FilePath extension_path;
  if (!extensions::browser_test_util::ModifyExtensionIfNeeded(
          options, context_type_, GetTestPreCount(), temp_dir_.GetPath(), path,
          &extension_path)) {
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
        std::make_unique<TestServiceWorkerContextObserver>(profile());
  }

  scoped_refptr<const Extension> extension =
      loader.LoadExtension(extension_path);
  if (!extension) {
    return nullptr;
  }

  last_loaded_extension_id_ = extension->id();

  // Note: `options.wait_for_registration_stored` may be set even if an
  // extension isn't service worker-based if the test is using LoadExtension()
  // in a parameterized test exercising both MV2 and MV3 extensions.
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

  auto* component_loader = ComponentLoader::Get(profile());
  component_loader->set_ignore_allowlist_for_testing(true);
  extensions::ExtensionId extension_id = component_loader->Add(manifest, path);
  const Extension* extension =
      extension_registry()->enabled_extensions().GetByID(extension_id);
  if (!extension) {
    return nullptr;
  }
  set_last_loaded_extension_id(extension->id());
  return extension;
}

const Extension* ExtensionBrowserTest::LoadExtensionAsComponent(
    const base::FilePath& path) {
  return LoadExtensionAsComponentWithManifest(path, kManifestFilename);
}

const Extension* ExtensionBrowserTest::InstallExtension(
    const base::FilePath& path,
    std::optional<int> expected_change) {
  return InstallOrUpdateExtension(
      std::string(), path, InstallUIType::kNone, std::move(expected_change),
      mojom::ManifestLocation::kInternal, GetActiveWebContents(),
      Extension::NO_FLAGS, /*wait_for_idle=*/true,
      /*grant_permissions=*/false, /*was_triggered_by_user_download=*/false);
}

const Extension* ExtensionBrowserTest::InstallExtension(
    const base::FilePath& path,
    std::optional<int> expected_change,
    mojom::ManifestLocation install_source) {
  return InstallOrUpdateExtension(
      std::string(), path, InstallUIType::kNone, std::move(expected_change),
      install_source, GetActiveWebContents(), Extension::NO_FLAGS,
      /*wait_for_idle=*/true, /*grant_permissions=*/false,
      /*was_triggered_by_user_download=*/false);
}

const Extension* ExtensionBrowserTest::InstallExtensionWithPermissionsGranted(
    const base::FilePath& file_path,
    std::optional<int> expected_change) {
  return ExtensionBrowserTest::InstallOrUpdateExtension(
      std::string(), file_path, InstallUIType::kNone,
      std::move(expected_change), mojom::ManifestLocation::kInternal,
      GetActiveWebContents(), Extension::NO_FLAGS,
      /*wait_for_idle=*/false, /*grant_permissions=*/true,
      /*was_triggered_by_user_download=*/false);
}

const Extension* ExtensionBrowserTest::InstallExtensionFromWebstore(
    const base::FilePath& path,
    std::optional<int> expected_change) {
  return InstallOrUpdateExtension(
      std::string(), path, InstallUIType::kAutoConfirm,
      std::move(expected_change), mojom::ManifestLocation::kInternal,
      GetActiveWebContents(), Extension::FROM_WEBSTORE,
      /*wait_for_idle=*/true, /*grant_permissions=*/false,
      /*installed_by_user_download*/ false);
}

const Extension*
ExtensionBrowserTest::InstallExtensionFromWebstoreTriggeredByUserDownload(
    const base::FilePath& path,
    std::optional<int> expected_change) {
  return InstallOrUpdateExtension(
      std::string(), path, InstallUIType::kAutoConfirm,
      std::move(expected_change), mojom::ManifestLocation::kInternal,
      GetActiveWebContents(), Extension::FROM_WEBSTORE,
      /*wait_for_idle=*/true, /*grant_permissions=*/false,
      /*was_triggered_by_user_download=*/true);
}

const Extension* ExtensionBrowserTest::InstallExtensionWithUIAutoConfirm(
    const base::FilePath& path,
    std::optional<int> expected_change) {
  return InstallOrUpdateExtension(
      std::string(), path, InstallUIType::kAutoConfirm,
      std::move(expected_change), mojom::ManifestLocation::kInternal,
      GetActiveWebContents(), Extension::NO_FLAGS, /*wait_for_idle=*/true,
      /*grant_permissions=*/false, /*was_triggered_by_user_download=*/false);
}

const Extension* ExtensionBrowserTest::InstallExtensionWithSourceAndFlags(
    const base::FilePath& path,
    std::optional<int> expected_change,
    mojom::ManifestLocation install_source,
    Extension::InitFromValueFlags creation_flags) {
  return ExtensionBrowserTest::InstallOrUpdateExtension(
      std::string(), path, InstallUIType::kNone, std::move(expected_change),
      install_source, GetActiveWebContents(), creation_flags,
      /*wait_for_idle=*/false, /*grant_permissions=*/false,
      /*was_triggered_by_user_download=*/false);
}

const Extension* ExtensionBrowserTest::StartInstallButCancel(
    const base::FilePath& path) {
  return InstallOrUpdateExtension(std::string(), path, InstallUIType::kCancel,
                                  0, mojom::ManifestLocation::kInternal,
                                  GetActiveWebContents(), Extension::NO_FLAGS,
                                  /*wait_for_idle=*/true,
                                  /*grant_permissions=*/false,
                                  /*was_triggered_by_user_download=*/false);
}

const Extension* ExtensionBrowserTest::UpdateExtension(
    const extensions::ExtensionId& id,
    const base::FilePath& path,
    std::optional<int> expected_change) {
  return InstallOrUpdateExtension(
      id, path, InstallUIType::kNone, std::move(expected_change),
      mojom::ManifestLocation::kInternal, GetActiveWebContents(),
      Extension::NO_FLAGS,
      /*wait_for_idle=*/true, /*grant_permissions=*/false,
      /*was_triggered_by_user_download=*/false);
}

const Extension* ExtensionBrowserTest::UpdateExtensionWaitForIdle(
    const extensions::ExtensionId& id,
    const base::FilePath& path,
    std::optional<int> expected_change) {
  return InstallOrUpdateExtension(
      id, path, InstallUIType::kNone, std::move(expected_change),
      mojom::ManifestLocation::kInternal, GetActiveWebContents(),
      Extension::NO_FLAGS,
      /*wait_for_idle=*/false, /*grant_permissions=*/false,
      /*was_triggered_by_user_download=*/false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const extensions::ExtensionId& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    std::optional<int> expected_change,
    mojom::ManifestLocation install_source,
    content::WebContents* active_web_contents,
    Extension::InitFromValueFlags creation_flags,
    bool install_immediately,
    bool grant_permissions,
    bool was_triggered_by_user_download) {
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
      install_ui =
          std::make_unique<ExtensionInstallPrompt>(active_web_contents);
    }
    installer = CrxInstaller::Create(profile(), std::move(install_ui));
    installer->set_expected_id(id);
    installer->set_creation_flags(creation_flags);
    installer->set_install_source(install_source);
    installer->set_install_immediately(install_immediately);
    installer->set_allow_silent_install(grant_permissions);
    if (!installer->is_gallery_install()) {
      installer->set_off_store_install_allow_reason(
          CrxInstaller::OffStoreInstallAllowedInTest);
    }
    if (was_triggered_by_user_download) {
      installer->set_was_triggered_by_user_download();
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

  // If possible, wait for the extension's background context to be loaded.
  // `WaitForExtensionViewsToLoad()` by itself is insufficient for this, since
  // it only waits for existent views registered in the process manager, and
  // the background context may not be registered yet.
  std::string reason_unused;
  bool extension_enabled =
      !install_error &&
      registry->enabled_extensions().Contains(installer->extension()->id());
  if (extension_enabled && ExtensionBackgroundPageWaiter::CanWaitFor(
                               *installer->extension(), reason_unused)) {
    ExtensionBackgroundPageWaiter(profile(), *installer->extension())
        .WaitForBackgroundInitialized();
  }

  if (!test_notification_observer()->WaitForExtensionViewsToLoad()) {
    return nullptr;
  }

  if (install_error) {
    return nullptr;
  }

  // Even though we can already get the Extension from the CrxInstaller,
  // ensure it's also in the list of enabled extensions.
  return registry->enabled_extensions().GetByID(installer->extension()->id());
}

void ExtensionBrowserTest::DisableExtension(const ExtensionId& extension_id) {
  DisableExtension(extension_id, {disable_reason::DISABLE_USER_ACTION});
}

void ExtensionBrowserTest::DisableExtension(
    const ExtensionId& extension_id,
    const DisableReasonSet& disable_reasons) {
  extension_registrar()->DisableExtension(extension_id, disable_reasons);
}

void ExtensionBrowserTest::UnloadExtension(
    const extensions::ExtensionId& extension_id) {
  extension_registrar()->RemoveExtension(extension_id,
                                         UnloadedExtensionReason::DISABLE);
}

void ExtensionBrowserTest::UninstallExtension(
    const extensions::ExtensionId& extension_id) {
  extension_registrar()->UninstallExtension(
      extension_id, UNINSTALL_REASON_FOR_TESTING, nullptr);
}

void ExtensionBrowserTest::EnableExtension(
    const extensions::ExtensionId& extension_id) {
  extension_registrar()->EnableExtension(extension_id);
}

void ExtensionBrowserTest::ReloadExtension(
    const extensions::ExtensionId& extension_id) {
  scoped_refptr<const Extension> extension =
      extension_registry()->GetInstalledExtension(extension_id);
  ASSERT_TRUE(extension);
  TestExtensionRegistryObserver observer(extension_registry(), extension_id);
  extension_registrar()->ReloadExtension(extension_id);
  // Re-grab the extension after the reload to get the updated copy.
  extension = observer.WaitForExtensionLoaded();
  // We need to let other ExtensionRegistryObservers handle the extension load
  // in order to finish initialization.
  base::RunLoop().RunUntilIdle();

  // Wait for the background context, if any, to start up.
  std::string reason_unused;
  if (extension_registry()->enabled_extensions().Contains(extension_id) &&
      ExtensionBackgroundPageWaiter::CanWaitFor(*extension, reason_unused)) {
    ExtensionBackgroundPageWaiter(profile(), *extension)
        .WaitForBackgroundInitialized();
  }

  // Wait for any additionally-registered extension views to load.
  test_notification_observer_->WaitForExtensionViewsToLoad();
}

content::WebContents* ExtensionBrowserTest::GetActiveWebContents() {
  if (!browser_window_interface()) {
    return nullptr;
  }
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser_window_interface())->GetActiveTab();
  return active_tab ? active_tab->GetContents() : nullptr;
}

content::WebContents* ExtensionBrowserTest::GetWebContentsAt(int index) {
  if (!browser_window_interface()) {
    return nullptr;
  }
  return TabListInterface::From(browser_window_interface())
      ->GetTab(index)
      ->GetContents();
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

  ExtensionCreator creator;
  if (!creator.Run(dir_path, crx_path, pem_path, pem_out_path,
                   extra_run_flags | ExtensionCreator::kOverwriteCRX)) {
    ADD_FAILURE() << "ExtensionCreator::Run() failed: "
                  << creator.error_message();
    return base::FilePath();
  }

  if (!base::PathExists(crx_path)) {
    ADD_FAILURE() << crx_path.value() << " was not created.";
    return base::FilePath();
  }
  return crx_path;
}

bool ExtensionBrowserTest::NavigateToURL(content::WebContents* web_contents,
                                         const GURL& url) {
  return chrome_test_utils::NavigateToURL(web_contents, url);
}

bool ExtensionBrowserTest::NavigateToURL(BrowserWindowInterface* browser_window,
                                         const GURL& url) {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED() << "Not supported on Android.";
#else
  auto* web_contents =
      browser_window->GetTabStripModel()->GetActiveWebContents();
  return NavigateToURL(web_contents, url);
#endif
}

bool ExtensionBrowserTest::GetCurrentTabTitle(std::u16string* title) {
  content::WebContents* web_contents = GetActiveWebContents();
  if (!web_contents) {
    return false;
  }
  content::NavigationEntry* last_entry =
      web_contents->GetController().GetActiveEntry();
  if (!last_entry) {
    return false;
  }
  title->assign(last_entry->GetTitleForDisplay());
  return true;
}

content::WebContents* ExtensionBrowserTest::PlatformOpenURLOffTheRecord(
    Profile* profile,
    const GURL& url) {
#if BUILDFLAG(IS_ANDROID)
  // Android doesn't have an OpenURLOffTheRecord() helper so we roll our own.
  // TODO(crbug.com/424860292): Delete this code when CreateBrowserWindow()
  // works on desktop Android for incognito windows.
  Profile* incognito_profile =
      this->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  // Close any old incognito tabs before creating the new tab model.
  incognito_tab_model_.reset();
  // Create a tab model for the incognito profile.
  incognito_tab_model_ = std::make_unique<OwningTestTabModel>(
      incognito_profile, kTestActivityType);
  incognito_tab_model_->SetIsActiveModel(true);
  incognito_tab_model_->AddEmptyTab(0, /*select=*/true);
  content::WebContents* web_contents =
      incognito_tab_model_->GetActiveWebContents();
  TabAndroid::AttachTabHelpers(web_contents);
  // This blocks until the navigation completes. The return value is ignored
  // because some tests intentionally navigate to blocked URLs which fail to
  // load.
  (void)content::NavigateToURL(web_contents, url);
  return web_contents;
#else
  Browser* otr_browser = OpenURLOffTheRecord(profile, url);
  return otr_browser->tab_strip_model()->GetActiveWebContents();
#endif
}

content::RenderFrameHost* ExtensionBrowserTest::NavigateToURLInNewTab(
    const GURL& url) {
#if BUILDFLAG(IS_ANDROID)
  // Navigate and block until navigation finishes.
  android_ui_test_utils::OpenUrlInNewTab(profile(), GetActiveWebContents(),
                                         url);
  content::WebContents* new_web_contents = GetActiveWebContents();
  // Mimic BROWSER_TEST_WAIT_FOR_LOAD_STOP like above.
  content::WaitForLoadStop(new_web_contents);
  return content::ConvertToRenderFrameHost(new_web_contents);
#else
  return ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
#endif
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
    if (host->GetLastCommittedURL().GetPath() == path) {
      EXPECT_FALSE(result_host);
      result_host = host;
    }
    num_hosts++;
  }
  EXPECT_EQ(expected_hosts, num_hosts);
  return result_host;
}

content::ServiceWorkerContext* ExtensionBrowserTest::GetServiceWorkerContext() {
  return GetServiceWorkerContext(profile());
}

// static
content::ServiceWorkerContext* ExtensionBrowserTest::GetServiceWorkerContext(
    content::BrowserContext* browser_context) {
  return service_worker_test_utils::GetServiceWorkerContext(browser_context);
}

int ExtensionBrowserTest::GetTabCount() {
  return TabListInterface::From(browser_window_interface())->GetTabCount();
}

bool ExtensionBrowserTest::IsTabSelected(int index) {
  return TabListInterface::From(browser_window_interface())->GetActiveIndex() ==
         index;
}

void ExtensionBrowserTest::CloseTabForWebContents(
    content::WebContents* web_contents) {
  content::WebContentsDestroyedWatcher destroyed_watcher(web_contents);
  TabListInterface* tab_list = nullptr;
  int tab_index = -1;
  ASSERT_TRUE(ExtensionTabUtil::GetTabListInterface(*web_contents, &tab_list,
                                                    &tab_index));
  tab_list->CloseTab(tab_list->GetTab(tab_index)->GetHandle());
  destroyed_watcher.Wait();
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

void ExtensionBrowserTest::SetUpTestProtocolHandler() {
  test_protocol_handler_ = base::BindRepeating(
      &ExtensionProtocolTestResourcesHandler, GetTestResourcesParentDir());
  SetExtensionProtocolTestHandler(&test_protocol_handler_);
}

void ExtensionBrowserTest::TearDownTestProtocolHandler() {
  SetExtensionProtocolTestHandler(nullptr);
}

bool ExtensionBrowserTest::WaitForExtensionViewsToLoad() {
  return test_notification_observer_->WaitForExtensionViewsToLoad();
}

bool ExtensionBrowserTest::WaitForExtensionIdle(
    const ExtensionId& extension_id) {
  return test_notification_observer_->WaitForExtensionIdle(extension_id);
}

bool ExtensionBrowserTest::WaitForExtensionNotIdle(
    const ExtensionId& extension_id) {
  return test_notification_observer_->WaitForExtensionNotIdle(extension_id);
}

bool ExtensionBrowserTest::WaitForPageActionVisibilityChangeTo(int count) {
  // Note: It's okay if the visibility is already at `count` (i.e., that we're
  // constructing this observer "late"); the observer handles that case
  // gracefully.
  ChromeExtensionTestNotificationObserver observer(GetProfile());
  return observer.WaitForPageActionVisibilityChangeTo(GetActiveWebContents(),
                                                      count);
}

const Extension* ExtensionBrowserTest::LoadAndLaunchApp(
    const base::FilePath& path,
    bool uses_guest_view) {
  return platform_delegate_.LoadAndLaunchApp(path, uses_guest_view);
}

Profile* ExtensionBrowserTest::profile() {
  return platform_delegate_.GetProfile();
}

content::WebContents* ExtensionBrowserTest::web_contents() {
  return web_contents_.get();
}

BrowserWindowInterface* ExtensionBrowserTest::browser_window_interface() {
#if BUILDFLAG(IS_ANDROID)
  std::vector<BrowserWindowInterface*> all_browsers =
      GetAllBrowserWindowInterfaces();
  return all_browsers.empty() ? nullptr : all_browsers.front();
#else
  return browser();
#endif
}

ExtensionService* ExtensionBrowserTest::extension_service() {
  return ExtensionSystem::Get(profile())->extension_service();
}

void ExtensionBrowserTest::UseHttpsTestServer(
    net::EmbeddedTestServer::ServerCertificate server_certificate) {
  https_test_server_ = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server_.get()->AddDefaultHandlers(GetChromeTestDataDir());
  https_test_server_.get()->SetSSLConfig(server_certificate);
}

}  // namespace extensions
