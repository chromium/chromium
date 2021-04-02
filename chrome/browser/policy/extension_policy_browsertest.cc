// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_content_verifier_delegate.h"
#include "chrome/browser/extensions/chrome_extension_test_notification_observer.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_management_constants.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/load_error_waiter.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/test/web_app_install_observer.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/policy_constants.h"
#include "components/version_info/channel.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/content_verifier/test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/scoped_ignore_content_verifier_for_test.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/updater/extension_cache_fake.h"
#include "extensions/common/constants.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/value_builder.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/switches.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/extensions/updater/local_extension_cache.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/web_app_id_constants.h"
#endif

using extensions::mojom::ManifestLocation;
using testing::AtLeast;
using testing::Sequence;

namespace policy {

// Called when an additional profile has been created.
// The created profile is stored in *|out_created_profile|.
void OnProfileInitialized(Profile** out_created_profile,
                          base::RunLoop* run_loop,
                          Profile* profile,
                          Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED) {
    *out_created_profile = profile;
    run_loop->Quit();
  }
}
namespace {

const base::FilePath::CharType kGoodCrxName[] = FILE_PATH_LITERAL("good.crx");
const base::FilePath::CharType kSimpleWithIconCrxName[] =
    FILE_PATH_LITERAL("simple_with_icon.crx");
const base::FilePath::CharType kHostedAppCrxName[] =
    FILE_PATH_LITERAL("hosted_app.crx");

const char kGoodCrxId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char kSimpleWithIconCrxId[] = "dehdlahnlebladnfleagmjdapdjdcnlp";
const char kHostedAppCrxId[] = "kbmnembihfiondgfjekmnmcbddelicoi";
// Different versions of this extension Id at
// {DIR_TEST_DATA}/extensions/pinning/ are used in extension pinning tests.
const char kPinnedExtensionCrxId[] = "fdlpamochgodkfemfnickdlkabcfmbln";

const char kGoodCrxVersion[] = "1.0.0.1";

const base::FilePath::CharType kGoodV1CrxName[] =
    FILE_PATH_LITERAL("good_v1.crx");
const base::FilePath::CharType kSimpleWithPopupExt[] =
    FILE_PATH_LITERAL("simple_with_popup");
const base::FilePath::CharType kAppUnpackedExt[] = FILE_PATH_LITERAL("app");

// This is to enforce zero initial delay.
constexpr net::BackoffEntry::Policy kDefaultBackOffPolicyForTesting = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms.
    0,

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.1,

    // Maximum amount of time we are willing to delay our request in ms.
    600000,  // Ten minutes.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

// Registers a handler to respond to requests whose path matches |match_path|.
// The response contents are generated from |template_file|, by replacing all
// "${URL_PLACEHOLDER}" substrings in the file with the request URL excluding
// filename, query values and fragment.
void RegisterURLReplacingHandler(net::EmbeddedTestServer* test_server,
                                 const std::string& match_path,
                                 const base::FilePath& template_file) {
  test_server->RegisterRequestHandler(base::BindRepeating(
      [](net::EmbeddedTestServer* test_server, const std::string& match_path,
         const base::FilePath& template_file,
         const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        GURL url = test_server->GetURL(request.relative_url);
        if (url.path() != match_path)
          return nullptr;

        std::string contents;
        CHECK(base::ReadFileToString(template_file, &contents));

        GURL url_base = url.GetWithoutFilename();
        base::ReplaceSubstringsAfterOffset(&contents, 0, "${URL_PLACEHOLDER}",
                                           url_base.spec());

        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_content(contents);
        response->set_content_type("text/plain");
        return response;
      },
      base::Unretained(test_server), match_path, template_file));
}

class ExtensionPolicyTest : public PolicyTest {
 public:
  ExtensionPolicyTest() = default;

 protected:
  void SetUp() override {
    // Set default verification mode for content verifier to be enabled.
    extensions::ChromeContentVerifierDelegate::SetDefaultModeForTesting(
        extensions::ChromeContentVerifierDelegate::VerifyInfo::Mode::
            ENFORCE_STRICT);
    ignore_content_verifier_ =
        std::make_unique<extensions::ScopedIgnoreContentVerifierForTest>();
    test_extension_cache_ = std::make_unique<extensions::ExtensionCacheFake>();
    // Base class SetUp() should be invoked at the end as it runs the test body.
    PolicyTest::SetUp();
  }

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    if (extension_service()->updater()) {
      extension_service()->updater()->SetExtensionCacheForTesting(
          test_extension_cache_.get());
    }

    os_hooks_suppress_ =
        web_app::OsIntegrationManager::ScopedSuppressOsHooksForTesting();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PolicyTest::SetUpCommandLine(command_line);
    // Some bots are flaky due to slower loading interacting with
    // deferred commits.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  extensions::ExtensionCacheFake* extension_cache() {
    return test_extension_cache_.get();
  }

  extensions::ExtensionService* extension_service() {
    extensions::ExtensionSystem* system =
        extensions::ExtensionSystem::Get(browser()->profile());
    return system->extension_service();
  }

  extensions::ExtensionRegistry* extension_registry() {
    return extensions::ExtensionRegistry::Get(browser()->profile());
  }

  web_app::WebAppProviderBase* web_app_provider_base() {
    return web_app::WebAppProviderBase::GetProviderBase(browser()->profile());
  }

  const extensions::Extension* InstallExtension(
      const base::FilePath::StringType& name) {
    base::FilePath extension_path(ui_test_utils::GetTestFilePath(
        base::FilePath(kTestExtensionsDir), base::FilePath(name)));
    scoped_refptr<extensions::CrxInstaller> installer =
        extensions::CrxInstaller::CreateSilent(extension_service());
    installer->set_allow_silent_install(true);
    installer->set_install_cause(extension_misc::INSTALL_CAUSE_UPDATE);
    installer->set_creation_flags(extensions::Extension::FROM_WEBSTORE);
    installer->set_off_store_install_allow_reason(
        extensions::CrxInstaller::OffStoreInstallAllowReason::
            OffStoreInstallAllowedInTest);

    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        content::NotificationService::AllSources());
    installer->InstallCrx(extension_path);
    observer.Wait();
    content::Details<const extensions::Extension> details = observer.details();
    return details.ptr();
  }

  const extensions::Extension* InstallBookmarkApp() {
    WebApplicationInfo web_app;
    web_app.title = u"Bookmark App";
    web_app.start_url = GURL("http://www.google.com");

    scoped_refptr<extensions::CrxInstaller> installer =
        extensions::CrxInstaller::CreateSilent(extension_service());

    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        content::NotificationService::AllSources());
    installer->InstallWebApp(web_app);
    observer.Wait();
    content::Details<const extensions::Extension> details = observer.details();
    return details.ptr();
  }

  const web_app::AppId InstallWebApp() {
    std::unique_ptr<WebApplicationInfo> web_application =
        std::make_unique<WebApplicationInfo>();
    web_application->title = u"Web App";
    web_application->start_url = GURL("http://www.google.com");
    base::RunLoop loop;
    web_app::AppId return_app_id;
    web_app_provider_base()->install_manager().InstallWebAppFromInfo(
        std::move(web_application), web_app::ForInstallableSite::kYes,
        webapps::WebappInstallSource::SYNC,
        base::BindLambdaForTesting(
            [&](const web_app::AppId& app_id, web_app::InstallResultCode code) {
              EXPECT_EQ(code, web_app::InstallResultCode::kSuccessNewInstall);
              return_app_id = app_id;
              loop.Quit();
            }));
    loop.Run();


    return return_app_id;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const extensions::Extension* InstallOSSettings() {
    WebApplicationInfo web_app;
    web_app.title = u"Settings";
    web_app.start_url = GURL("chrome://os-settings/");

    scoped_refptr<extensions::CrxInstaller> installer =
        extensions::CrxInstaller::CreateSilent(extension_service());
    installer->set_install_source(ManifestLocation::kExternalComponent);
    installer->set_creation_flags(
        extensions::Extension::WAS_INSTALLED_BY_DEFAULT);

    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        content::NotificationService::AllSources());
    installer->InstallWebApp(web_app);
    observer.Wait();

    web_app::ExternallyInstalledWebAppPrefs web_app_prefs(
        browser()->profile()->GetPrefs());
    web_app_prefs.Insert(GURL("chrome://os-settings/"),
                         web_app::kOsSettingsAppId,
                         web_app::ExternalInstallSource::kSystemInstalled);

    content::Details<const extensions::Extension> details = observer.details();
    return details.ptr();
  }
#endif

  void UninstallExtension(const std::string& id, bool expect_success) {
    if (expect_success) {
      extensions::TestExtensionRegistryObserver observer(extension_registry());
      extension_service()->UninstallExtension(
          id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
      observer.WaitForExtensionUninstalled();
    } else {
      extensions::TestExtensionRegistryObserver observer(extension_registry());
      extension_service()->UninstallExtension(
          id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
      observer.WaitForExtensionUninstallationDenied();
    }
  }

  void DisableExtension(const std::string& id) {
    extensions::TestExtensionRegistryObserver observer(extension_registry());
    extension_service()->DisableExtension(
        id, extensions::disable_reason::DISABLE_USER_ACTION);
    observer.WaitForExtensionUnloaded();
  }

  void AddExtensionToForceList(PolicyMap* policies,
                               const std::string& id,
                               const GURL& update_url) {
    // Setting the forcelist extension should install extension with ExtensionId
    // equal to id.
    base::ListValue forcelist;
    forcelist.AppendString(
        base::StringPrintf(update_url.is_empty() ? "%s" : "%s;%s", id.c_str(),
                           update_url.spec().c_str()));
    policies->Set(key::kExtensionInstallForcelist, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, std::move(forcelist),
                  nullptr);
  }

  const extensions::Extension* InstallForceListExtension(
      const std::string& update_url_suffix,
      const std::string& id) {
    extensions::ExtensionRegistry* registry = extension_registry();
    if (registry->GetExtensionById(id,
                                   extensions::ExtensionRegistry::EVERYTHING)) {
      return nullptr;
    }

    GURL update_url = embedded_test_server()->GetURL(update_url_suffix);
    PolicyMap policies;
    AddExtensionToForceList(&policies, id, update_url);

    extensions::TestExtensionRegistryObserver observer(extension_registry());
    UpdateProviderPolicy(policies);
    observer.WaitForExtensionWillBeInstalled();

    return registry->enabled_extensions().GetByID(id);
  }

  std::unique_ptr<extensions::ExtensionCacheFake> test_extension_cache_;
  std::unique_ptr<extensions::ScopedIgnoreContentVerifierForTest>
      ignore_content_verifier_;
  extensions::ExtensionUpdater::ScopedSkipScheduledCheckForTest
      skip_scheduled_extension_checks_;

 private:
  web_app::ScopedOsHooksSuppress os_hooks_suppress_;
};

}  // namespace

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Check that component extension can't be blocklisted.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       ExtensionInstallBlocklistComponentApps) {
  // Load all component extensions.
  extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  extension_service()->component_loader()->AddDefaultComponentExtensions(false);
  base::RunLoop().RunUntilIdle();

  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_TRUE(
      registry->enabled_extensions().GetByID(extensions::kWebStoreAppId));

  base::ListValue blocklist;
  blocklist.AppendString(extensions::kWebStoreAppId);
  PolicyMap policies;
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, blocklist.Clone(),
               nullptr);
  UpdateProviderPolicy(policies);
  ASSERT_TRUE(
      registry->enabled_extensions().GetByID(extensions::kWebStoreAppId));
}

// Ensures that OS Settings can't be disabled by ExtensionInstallBlocklist
// policy.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       ExtensionInstallBlocklistOsSettings) {
  extensions::ExtensionRegistry* registry = extension_registry();
  const extensions::Extension* bookmark_app = InstallOSSettings();
  ASSERT_TRUE(bookmark_app);
  ASSERT_TRUE(
      registry->enabled_extensions().GetByID(web_app::kOsSettingsAppId));

  base::ListValue blocklist;
  blocklist.AppendString(web_app::kOsSettingsAppId);
  PolicyMap policies;
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, blocklist.Clone(),
               nullptr);
  UpdateProviderPolicy(policies);

  extensions::ExtensionService* service = extension_service();
  EXPECT_TRUE(service->IsExtensionEnabled(web_app::kOsSettingsAppId));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       ExtensionInstallBlocklistSelective) {
  // Verifies that blocklisted extensions can't be installed.
  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));
  ASSERT_FALSE(registry->GetExtensionById(
      kSimpleWithIconCrxId, extensions::ExtensionRegistry::EVERYTHING));
  base::ListValue blocklist;
  blocklist.AppendString(kGoodCrxId);
  PolicyMap policies;
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, blocklist.Clone(),
               nullptr);
  UpdateProviderPolicy(policies);

  // "good.crx" is blocklisted.
  EXPECT_FALSE(InstallExtension(kGoodCrxName));
  EXPECT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));

  // "simple_with_icon.crx" is not.
  const extensions::Extension* simple_with_icon =
      InstallExtension(kSimpleWithIconCrxName);
  ASSERT_TRUE(simple_with_icon);
  EXPECT_EQ(kSimpleWithIconCrxId, simple_with_icon->id());
  EXPECT_EQ(simple_with_icon,
            registry->enabled_extensions().GetByID(kSimpleWithIconCrxId));
}

// Ensure that bookmark apps are not blocked by the ExtensionInstallBlocklist
// policy.
// Also see ExtensionInstallBlocklist_WebApp counterpart.
// TODO(https://crbug.com/1079435): Delete this test for bookmark apps removal.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       ExtensionInstallBlocklist_BookmarkApp) {
  const extensions::Extension* bookmark_app = InstallBookmarkApp();
  ASSERT_TRUE(bookmark_app);
  EXPECT_TRUE(InstallExtension(kGoodCrxName));

  extensions::ExtensionService* service = extension_service();
  EXPECT_TRUE(service->IsExtensionEnabled(kGoodCrxId));
  EXPECT_TRUE(service->IsExtensionEnabled(bookmark_app->id()));

  // Now set ExtensionInstallBlocklist policy to block all extensions.
  PolicyMap policies;
  base::Value list(base::Value::Type::LIST);
  list.Append("*");
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, std::move(list),
               nullptr);
  UpdateProviderPolicy(policies);

  // The bookmark app should still be enabled, with |kGoodCrxId| being disabled.
  EXPECT_FALSE(service->IsExtensionEnabled(kGoodCrxId));
  EXPECT_TRUE(service->IsExtensionEnabled(bookmark_app->id()));
}

// Ensure that web apps are not blocked by the ExtensionInstallBlocklist policy.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest, ExtensionInstallBlocklist_WebApp) {
  web_app::AppId web_app_id = InstallWebApp();
  EXPECT_TRUE(InstallExtension(kGoodCrxName));

  web_app::WebAppProviderBase* provider = web_app_provider_base();
  EXPECT_TRUE(provider->registrar().IsInstalled(web_app_id));
  extensions::ExtensionService* service = extension_service();
  EXPECT_TRUE(service->IsExtensionEnabled(kGoodCrxId));

  // Now set ExtensionInstallBlocklist policy to block all extensions.
  PolicyMap policies;
  base::Value list(base::Value::Type::LIST);
  list.Append("*");
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, std::move(list),
               nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_FALSE(service->IsExtensionEnabled(kGoodCrxId));
  EXPECT_TRUE(provider->registrar().IsInstalled(web_app_id));
  EXPECT_TRUE(provider->registrar().IsLocallyInstalled(web_app_id));
}

// Ensure that when INSTALLATION_REMOVED is set
// that blocklisted extensions are removed from the device.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest, ExtensionInstallRemovedPolicy) {
  EXPECT_TRUE(InstallExtension(kGoodCrxName));

  extensions::ExtensionRegistry* registry = extension_registry();
  EXPECT_TRUE(registry->GetInstalledExtension(kGoodCrxId));

  // Should uninstall good_v1.crx.
  base::DictionaryValue dict_value;
  dict_value.SetString(std::string(kGoodCrxId) + "." +
                           extensions::schema_constants::kInstallationMode,
                       extensions::schema_constants::kRemoved);
  PolicyMap policies;
  policies.Set(key::kExtensionSettings, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, dict_value.Clone(),
               nullptr);
  extensions::TestExtensionRegistryObserver observer(registry);
  UpdateProviderPolicy(policies);
  observer.WaitForExtensionUnloaded();

  EXPECT_FALSE(registry->GetInstalledExtension(kGoodCrxId));
}

// Ensure that when INSTALLATION_REMOVED is set for wildcard
// that blocklisted extensions are removed from the device.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest, ExtensionWildcardRemovedPolicy) {
  EXPECT_TRUE(InstallExtension(kGoodCrxName));

  extensions::ExtensionRegistry* registry = extension_registry();
  EXPECT_TRUE(registry->GetInstalledExtension(kGoodCrxId));

  // Should uninstall good_v1.crx.
  base::DictionaryValue dict_value;
  dict_value.SetString(
      std::string("*") + "." + extensions::schema_constants::kInstallationMode,
      extensions::schema_constants::kRemoved);
  PolicyMap policies;
  policies.Set(key::kExtensionSettings, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, dict_value.Clone(),
               nullptr);
  extensions::TestExtensionRegistryObserver observer(registry);
  UpdateProviderPolicy(policies);
  observer.WaitForExtensionUnloaded();

  EXPECT_FALSE(registry->GetInstalledExtension(kGoodCrxId));
}

// Ensure that bookmark apps are not blocked by the ExtensionAllowedTypes
// policy.
// Also see ExtensionAllowedTypes_WebApp counterpart.
// TODO(https://crbug.com/1079435): Delete this test for bookmark apps removal.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest, ExtensionAllowedTypes_BookmarkApp) {
  const extensions::Extension* bookmark_app = InstallBookmarkApp();
  ASSERT_TRUE(bookmark_app);
  EXPECT_TRUE(InstallExtension(kGoodCrxName));

  extensions::ExtensionService* service = extension_service();
  EXPECT_TRUE(service->IsExtensionEnabled(kGoodCrxId));
  EXPECT_TRUE(service->IsExtensionEnabled(bookmark_app->id()));

  // Now set policy to only allow themes. Note: Bookmark apps are hosted
  // apps.
  PolicyMap policies;
  base::Value list(base::Value::Type::LIST);
  list.Append("theme");
  policies.Set(key::kExtensionAllowedTypes, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, std::move(list),
               nullptr);
  UpdateProviderPolicy(policies);

  // The bookmark app should still be enabled, with |kGoodCrxId| being disabled.
  EXPECT_FALSE(service->IsExtensionEnabled(kGoodCrxId));
  EXPECT_TRUE(service->IsExtensionEnabled(bookmark_app->id()));
}

// Ensure that web apps are not blocked by the ExtensionInstallBlocklist policy.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest, ExtensionAllowedTypes_WebApp) {
  web_app::AppId web_app_id = InstallWebApp();
  EXPECT_TRUE(InstallExtension(kGoodCrxName));

  web_app::WebAppProviderBase* provider = web_app_provider_base();
  EXPECT_TRUE(provider->registrar().IsInstalled(web_app_id));
  extensions::ExtensionService* service = extension_service();
  EXPECT_TRUE(service->IsExtensionEnabled(kGoodCrxId));

  // Now set policy to only allow themes.
  PolicyMap policies;
  base::Value list(base::Value::Type::LIST);
  list.Append("theme");
  policies.Set(key::kExtensionAllowedTypes, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, std::move(list),
               nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_FALSE(service->IsExtensionEnabled(kGoodCrxId));
  EXPECT_TRUE(provider->registrar().IsInstalled(web_app_id));
  EXPECT_TRUE(provider->registrar().IsLocallyInstalled(web_app_id));
}

// Ensure that bookmark apps are not blocked by the ExtensionSettings
// policy.
// Also see ExtensionSettings_WebApp counterpart.
// TODO(https://crbug.com/1079435): Delete this test for bookmark apps removal.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest, ExtensionSettings_BookmarkApp) {
  const extensions::Extension* bookmark_app = InstallBookmarkApp();
  ASSERT_TRUE(bookmark_app);
  EXPECT_TRUE(InstallExtension(kGoodCrxName));

  extensions::ExtensionService* service = extension_service();
  EXPECT_TRUE(service->IsExtensionEnabled(kGoodCrxId));
  EXPECT_TRUE(service->IsExtensionEnabled(bookmark_app->id()));

  // Now set policy to block all extensions.
  PolicyMap policies;
  base::Value dict(base::Value::Type::DICTIONARY),
      key_dict(base::Value::Type::DICTIONARY);
  key_dict.SetStringKey("installation_mode", "blocked");
  dict.SetKey("*", std::move(key_dict));
  policies.Set(key::kExtensionSettings, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, std::move(dict),
               nullptr);
  UpdateProviderPolicy(policies);

  // The bookmark app should still be enabled, with |kGoodCrxId| being disabled.
  EXPECT_FALSE(service->IsExtensionEnabled(kGoodCrxId));
  EXPECT_TRUE(service->IsExtensionEnabled(bookmark_app->id()));

  // Clear all policies.
  policies.Clear();
  UpdateProviderPolicy(policies);

  EXPECT_TRUE(service->IsExtensionEnabled(kGoodCrxId));
  EXPECT_TRUE(service->IsExtensionEnabled(bookmark_app->id()));

  // Now set policy to only allow themes. Note: Bookmark apps are hosted
  // apps.
  base::Value dict2(base::Value::Type::DICTIONARY),
      key_dict2(base::Value::Type::DICTIONARY);
  base::Value list(base::Value::Type::LIST);
  list.Append("theme");
  key_dict2.SetKey("allowed_types", std::move(list));
  dict2.SetKey("*", std::move(key_dict2));
  policies.Set(key::kExtensionSettings, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, std::move(dict2),
               nullptr);
  UpdateProviderPolicy(policies);

  // The bookmark app should still be enabled, with |kGoodCrxId| being disabled.
  EXPECT_FALSE(service->IsExtensionEnabled(kGoodCrxId));
  EXPECT_TRUE(service->IsExtensionEnabled(bookmark_app->id()));
}

// Ensure that web apps are not blocked by the ExtensionInstallBlocklist policy.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest, ExtensionSettings_WebApp) {
  web_app::AppId web_app_id = InstallWebApp();
  EXPECT_TRUE(InstallExtension(kGoodCrxName));

  web_app::WebAppProviderBase* provider = web_app_provider_base();
  EXPECT_TRUE(provider->registrar().IsInstalled(web_app_id));
  extensions::ExtensionService* service = extension_service();
  EXPECT_TRUE(service->IsExtensionEnabled(kGoodCrxId));

  // Now set policy to block all extensions.
  PolicyMap policies;
  base::Value dict(base::Value::Type::DICTIONARY),
      key_dict(base::Value::Type::DICTIONARY);
  key_dict.SetStringKey("installation_mode", "blocked");
  dict.SetKey("*", std::move(key_dict));
  policies.Set(key::kExtensionSettings, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, std::move(dict),
               nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_FALSE(service->IsExtensionEnabled(kGoodCrxId));
  EXPECT_TRUE(provider->registrar().IsInstalled(web_app_id));
  EXPECT_TRUE(provider->registrar().IsLocallyInstalled(web_app_id));
}

// Flaky on windows; http://crbug.com/307994.
#if defined(OS_WIN)
#define MAYBE_ExtensionInstallBlocklistWildcard \
  DISABLED_ExtensionInstallBlocklistWildcard
#else
#define MAYBE_ExtensionInstallBlocklistWildcard \
  ExtensionInstallBlocklistWildcard
#endif
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       MAYBE_ExtensionInstallBlocklistWildcard) {
  // Verify that a wildcard blocklist takes effect.
  EXPECT_TRUE(InstallExtension(kSimpleWithIconCrxName));
  extensions::ExtensionService* service = extension_service();
  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));
  ASSERT_TRUE(registry->enabled_extensions().GetByID(kSimpleWithIconCrxId));
  base::ListValue blocklist;
  blocklist.AppendString("*");
  PolicyMap policies;
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, blocklist.Clone(),
               nullptr);
  UpdateProviderPolicy(policies);

  // "simple_with_icon" should be disabled.
  EXPECT_TRUE(registry->disabled_extensions().GetByID(kSimpleWithIconCrxId));
  EXPECT_FALSE(service->IsExtensionEnabled(kSimpleWithIconCrxId));

  // It shouldn't be possible to re-enable "simple_with_icon", until it
  // satisfies management policy.
  service->EnableExtension(kSimpleWithIconCrxId);
  EXPECT_FALSE(service->IsExtensionEnabled(kSimpleWithIconCrxId));

  // It shouldn't be possible to install good.crx.
  EXPECT_FALSE(InstallExtension(kGoodCrxName));
  EXPECT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));
}

IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       ExtensionInstallBlocklistSharedModules) {
  // Verifies that shared_modules are not affected by the blocklist.

  base::FilePath base_path;
  GetTestDataDirectory(&base_path);
  base::FilePath update_xml_template_path =
      base_path.Append(kTestExtensionsDir)
          .AppendASCII("policy_shared_module")
          .AppendASCII("update_template.xml");

  std::string update_xml_path =
      "/" + base::FilePath(kTestExtensionsDir).MaybeAsASCII() +
      "/policy_shared_module/gen_update.xml";
  RegisterURLReplacingHandler(embedded_test_server(), update_xml_path,
                              update_xml_template_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  const char kImporterId[] = "pchakhniekfaeoddkifplhnfbffomabh";
  const char kSharedModuleId[] = "nfgclafboonjbiafbllihiailjlhelpm";

  // Make sure that "import" and "export" are available to these extension IDs
  // by mocking the release channel.
  extensions::ScopedCurrentChannel channel(version_info::Channel::DEV);

  // Verify that the extensions are not installed initially.
  extensions::ExtensionService* service = extension_service();
  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_FALSE(registry->GetExtensionById(
      kImporterId, extensions::ExtensionRegistry::EVERYTHING));
  ASSERT_FALSE(registry->GetExtensionById(
      kSharedModuleId, extensions::ExtensionRegistry::EVERYTHING));

  // Mock the webstore update URL. This is where the shared module extension
  // will be installed from.
  GURL update_xml_url = embedded_test_server()->GetURL(update_xml_path);
  extension_test_util::SetGalleryUpdateURL(update_xml_url);
  ui_test_utils::NavigateToURL(browser(), update_xml_url);

  // Blocklist "*" but force-install the importer extension. The shared module
  // should be automatically installed too.
  base::ListValue blocklist;
  blocklist.AppendString("*");
  PolicyMap policies;
  AddExtensionToForceList(&policies, kImporterId, update_xml_url);
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, blocklist.Clone(),
               nullptr);

  extensions::TestExtensionRegistryObserver observe_importer(registry,
                                                             kImporterId);
  extensions::TestExtensionRegistryObserver observe_shared_module(
      registry, kSharedModuleId);
  UpdateProviderPolicy(policies);
  observe_importer.WaitForExtensionLoaded();
  observe_shared_module.WaitForExtensionLoaded();

  // Verify that both extensions got installed.
  const extensions::Extension* importer =
      registry->enabled_extensions().GetByID(kImporterId);
  ASSERT_TRUE(importer);
  EXPECT_EQ(kImporterId, importer->id());
  const extensions::Extension* shared_module =
      registry->enabled_extensions().GetByID(kSharedModuleId);
  ASSERT_TRUE(shared_module);
  EXPECT_EQ(kSharedModuleId, shared_module->id());
  EXPECT_TRUE(shared_module->is_shared_module());

  // Verify the dependency.
  std::unique_ptr<extensions::ExtensionSet> set =
      service->shared_module_service()->GetDependentExtensions(shared_module);
  ASSERT_TRUE(set);
  EXPECT_EQ(1u, set->size());
  EXPECT_TRUE(set->Contains(importer->id()));

  std::vector<extensions::SharedModuleInfo::ImportInfo> imports =
      extensions::SharedModuleInfo::GetImports(importer);
  ASSERT_EQ(1u, imports.size());
  EXPECT_EQ(kSharedModuleId, imports[0].extension_id);
}

IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest, ExtensionInstallAllowlist) {
  // Verifies that the allowlist can open exceptions to the blocklist.
  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));
  ASSERT_FALSE(registry->GetExtensionById(
      kSimpleWithIconCrxId, extensions::ExtensionRegistry::EVERYTHING));
  base::ListValue blocklist;
  blocklist.AppendString("*");
  base::ListValue allowlist;
  allowlist.AppendString(kGoodCrxId);
  PolicyMap policies;
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, blocklist.Clone(),
               nullptr);
  policies.Set(key::kExtensionInstallAllowlist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, allowlist.Clone(),
               nullptr);
  UpdateProviderPolicy(policies);
  // "simple_with_icon.crx" is blocklisted.
  EXPECT_FALSE(InstallExtension(kSimpleWithIconCrxName));
  EXPECT_FALSE(registry->GetExtensionById(
      kSimpleWithIconCrxId, extensions::ExtensionRegistry::EVERYTHING));
  // "good.crx" has a allowlist exception.
  const extensions::Extension* good = InstallExtension(kGoodCrxName);
  ASSERT_TRUE(good);
  EXPECT_EQ(kGoodCrxId, good->id());
  EXPECT_EQ(good, registry->enabled_extensions().GetByID(kGoodCrxId));
  // The user can also remove this extension.
  UninstallExtension(kGoodCrxId, true);
}

namespace {

class ExtensionRequestInterceptor {
 public:
  ExtensionRequestInterceptor()
      : interceptor_(
            base::BindRepeating(&ExtensionRequestInterceptor::OnRequest,
                                base::Unretained(this))) {}

  void set_interceptor_hook(
      content::URLLoaderInterceptor::InterceptCallback callback) {
    callback_ = std::move(callback);
  }

 private:
  bool OnRequest(content::URLLoaderInterceptor::RequestParams* params) {
    if (callback_ && callback_.Run(params))
      return true;
    // Mock out requests to the Web Store.
    if (params->url_request.url.host() == "clients2.google.com" &&
        params->url_request.url.path() == "/service/update2/crx") {
      content::URLLoaderInterceptor::WriteResponse(
          "chrome/test/data/extensions/good2_update_manifest.xml",
          params->client.get());
      return true;
    }

    if (params->url_request.url.path() == "/good_update_manifest.xml") {
      content::URLLoaderInterceptor::WriteResponse(
          "chrome/test/data/extensions/good2_update_manifest.xml",
          params->client.get());
      return true;
    }

    if (params->url_request.url.path() ==
        "/good_prodversionmin_update_manifest.xml") {
      content::URLLoaderInterceptor::WriteResponse(
          "chrome/test/data/extensions/good_prodversionmin_update_manifest.xml",
          params->client.get());
      return true;
    }

    if (params->url_request.url.path() == "/extensions/good_v1.crx") {
      content::URLLoaderInterceptor::WriteResponse(
          "chrome/test/data/extensions/good_v1.crx", params->client.get());
      return true;
    }
    if (params->url_request.url.path() == "/extensions/good2.crx") {
      content::URLLoaderInterceptor::WriteResponse(
          "chrome/test/data/extensions/good2.crx", params->client.get());
      return true;
    }
    if (params->url_request.url.path() == "/extensions/good3.crx") {
      content::URLLoaderInterceptor::WriteResponse(
          "chrome/test/data/extensions/good3.crx", params->client.get());
      return true;
    }

    return false;
  }

  content::URLLoaderInterceptor::InterceptCallback callback_;
  content::URLLoaderInterceptor interceptor_;
};

class MockedInstallationCollectorObserver
    : public extensions::InstallStageTracker::Observer {
 public:
  explicit MockedInstallationCollectorObserver(
      const content::BrowserContext* context)
      : context_(context) {}
  ~MockedInstallationCollectorObserver() override = default;

  MOCK_METHOD1(ExtensionStageChanged,
               void(extensions::InstallStageTracker::Stage));
  MOCK_METHOD2(OnExtensionInstallationFailed,
               void(const extensions::ExtensionId&,
                    extensions::InstallStageTracker::FailureReason));

  void OnExtensionDataChangedForTesting(
      const extensions::ExtensionId& id,
      const content::BrowserContext* context,
      const extensions::InstallStageTracker::InstallationData& data) override {
    // For simplicity policies are pushed into all profiles, so we need to track
    // only one here.
    if (context != context_) {
      return;
    }
    if (data.install_stage && stage_ != data.install_stage.value()) {
      stage_ = data.install_stage.value();
      ExtensionStageChanged(stage_);
    }
  }

 private:
  extensions::InstallStageTracker::Stage stage_ =
      extensions::InstallStageTracker::Stage::CREATED;
  const content::BrowserContext* context_;
};

}  // namespace

// Verifies that if extension is installed manually by user and then added to
// force-installed policy, it can't be uninstalled. And then if it removed
// from the force installed list, it should be uninstalled.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       ExtensionAddedAndRemovedFromForceInstalledList) {
  ExtensionRequestInterceptor interceptor;

  ASSERT_FALSE(extension_registry()->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));

  ASSERT_TRUE(InstallExtension(kGoodCrxName));
  EXPECT_TRUE(extension_registry()->enabled_extensions().GetByID(kGoodCrxId));

  EXPECT_EQ(extension_registry()
                ->enabled_extensions()
                .GetByID(kGoodCrxId)
                ->location(),
            ManifestLocation::kInternal);

  // The user is allowed to disable the added extension.
  EXPECT_TRUE(extension_service()->IsExtensionEnabled(kGoodCrxId));
  DisableExtension(kGoodCrxId);
  EXPECT_FALSE(extension_service()->IsExtensionEnabled(kGoodCrxId));

  // Explicitly re-enable the extension.
  extension_service()->EnableExtension(kGoodCrxId);
  // Extensions that are force-installed come from an update URL, which defaults
  // to the webstore. Use a test URL for this test with an update manifest
  // that includes "good_v1.crx".
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/extensions/good_v1_update_manifest.xml");

  PolicyMap policies;
  AddExtensionToForceList(&policies, kGoodCrxId, url);
  UpdateProviderPolicy(policies);
  const extensions::Extension* extension =
      extension_registry()->enabled_extensions().GetByID(kGoodCrxId);
  EXPECT_TRUE(extension);

  // The user is not allowed to uninstall force-installed extensions.
  UninstallExtension(kGoodCrxId, /*expect_success=*/false);
  EXPECT_EQ(extension->location(), ManifestLocation::kExternalPolicyDownload);

  // Remove the force installed policy.
  policies.Erase(policy::key::kExtensionInstallForcelist);
  UpdateProviderPolicy(policies);

  // TODO(crbug.com/1042187)
  // Extension should be uninstalled now. It would be better to keep it, but it
  // doesn't happen for now.
  ASSERT_FALSE(extension_registry()->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));
}

// Verifies that extension is not installed if its version does not match
// with that in the update manifest.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       CrxVersionInconsistencyFromManifest) {
  // Intercepts the call to download the crx file and responds with the test crx
  // file.
  ExtensionRequestInterceptor interceptor;
  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));

  // Allow caching for the extension in case it is inserted in the cache.
  extension_cache()->AllowCaching(kGoodCrxId);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/extensions/good_v1_wrong_version_update_manifest.xml");
  PolicyMap policies;
  // Add an entry in the extension force list policy.
  AddExtensionToForceList(&policies, kGoodCrxId, url);

  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());

  // Updating the policy triggers the extension installation process.
  UpdateProviderPolicy(policies);
  // Wait till the installer has finished by receiving the notification
  // NOTIFICATION_CRX_INSTALLER_DONE.
  observer.Wait();
  content::Details<const extensions::Extension> details = observer.details();
  // Check the extension is not installed.
  EXPECT_FALSE(details.ptr());
  EXPECT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));
  // Check the extension in not inserted in the cache.
  EXPECT_FALSE(
      extension_cache()->GetExtension(kGoodCrxId, "", nullptr, nullptr));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Verifies that if the cache entry contains inconsistent extension version,
// the crx installation fails and download of a new crx file is attempted.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest, CrxVersionInconsistencyInCache) {
  base::ScopedAllowBlockingForTesting allow_io;
  // Intercepts the call to download the crx file and responds with the test crx
  // file.
  ExtensionRequestInterceptor interceptor;
  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));

  // Override the fake extension cache set in SetUpOnMainThread() as the test
  // requires real extension cache to retry download of crx file when
  // installation fails due to version mismatch.
  extensions::ExtensionCache* cache =
      extensions::ExtensionsBrowserClient::Get()->GetExtensionCache();
  extension_service()->updater()->SetExtensionCacheForTesting(cache);

  base::FilePath extension_path(ui_test_utils::GetTestFilePath(
      base::FilePath(kTestExtensionsDir), base::FilePath(kGoodCrxName)));
  cache->AllowCaching(kGoodCrxId);

  // Copy the crx file to a temp directory so that the test file is not deleted
  // when cache entry is removed on version mismatch.
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  const base::FilePath tmp_path = tmp_dir.GetPath();
  const base::FilePath filename =
      tmp_path.Append(extensions::LocalExtensionCache::ExtensionFileName(
          kGoodCrxId, kGoodCrxVersion, "" /* hash */));
  EXPECT_TRUE(CopyFile(extension_path, filename));

  // Wait for the extension cache to get ready.
  base::RunLoop cache_init_run_loop;
  cache->Start(cache_init_run_loop.QuitClosure());
  cache_init_run_loop.Run();

  base::RunLoop put_extension_run_loop;
  // Insert a cache entry with version "1.0.0.1" while the crx file it points to
  // belongs to version "1.0.0.0".
  cache->PutExtension(
      kGoodCrxId, "" /* expected hash */, filename, kGoodCrxVersion,
      base::BindLambdaForTesting(
          [&put_extension_run_loop](const base::FilePath& file_path,
                                    bool file_ownership_passed) {
            put_extension_run_loop.Quit();
          }));
  put_extension_run_loop.Run();
  EXPECT_TRUE(cache->GetExtension(kGoodCrxId, "", nullptr, nullptr));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/extensions/good2_update_manifest.xml");
  PolicyMap policies;
  // Add an entry in the extension force list policy.
  AddExtensionToForceList(&policies, kGoodCrxId, url);

  // Observes notification for failed crx installation.
  content::WindowedNotificationObserver failed_installation_observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      base::BindLambdaForTesting(
          [&](const content::NotificationSource& source,
              const content::NotificationDetails& details) {
            return content::Details<const extensions::Extension>(details)
                       .ptr() == nullptr;
          }));
  // Observes notification for passed crx installation.
  content::WindowedNotificationObserver passed_installation_observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      base::BindLambdaForTesting(
          [&](const content::NotificationSource& source,
              const content::NotificationDetails& details) {
            return content::Details<const extensions::Extension>(details)
                       .ptr() != nullptr;
          }));
  // Updating the policy triggers the extension installation process.
  UpdateProviderPolicy(policies);
  // Wait till extension entry is found in the cache and installation fails due
  // to version mismatch as the cache entry informs extension version as
  // "1.0.0.1" while the crx file it points to belongs to "1.0.0.0".
  failed_installation_observer.Wait();

  // Wait till extension is freshly downloaded from the server and installation
  // succeeds.
  passed_installation_observer.Wait();

  EXPECT_TRUE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));
  std::string version;
  base::FilePath file_path;
  // Check extension is inserted in the cache with a new filepath to the
  // downloaded correct crx.
  EXPECT_TRUE(cache->GetExtension(kGoodCrxId, "", &file_path, &version));
  EXPECT_EQ(version, kGoodCrxVersion);
  EXPECT_NE(file_path, filename);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Verifies that extensions that are force-installed by policies are
// installed and can't be uninstalled.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest, ExtensionInstallForcelist) {
  ExtensionRequestInterceptor interceptor;
  extensions::ExtensionService* service = extension_service();
  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));

  // Extensions that are force-installed come from an update URL, which defaults
  // to the webstore. Use a test URL for this test with an update manifest
  // that includes "good_v1.crx".
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/extensions/good_v1_update_manifest.xml");

  PolicyMap policies;
  AddExtensionToForceList(&policies, kGoodCrxId, url);
  extension_cache()->AllowCaching(kGoodCrxId);
  EXPECT_FALSE(
      extension_cache()->GetExtension(kGoodCrxId, "", nullptr, nullptr));

  extensions::TestExtensionRegistryObserver registry_observer(
      extension_registry());
  MockedInstallationCollectorObserver collector_observer(browser()->profile());
  // CREATED is the default stage in MockedInstallationCollectorObserver, so it
  // wouldn't be reported here.
  Sequence sequence;
  EXPECT_CALL(
      collector_observer,
      ExtensionStageChanged(extensions::InstallStageTracker::Stage::PENDING))
      .InSequence(sequence);
  EXPECT_CALL(collector_observer,
              ExtensionStageChanged(
                  extensions::InstallStageTracker::Stage::DOWNLOADING))
      .InSequence(sequence);
  EXPECT_CALL(
      collector_observer,
      ExtensionStageChanged(extensions::InstallStageTracker::Stage::INSTALLING))
      .InSequence(sequence);
  EXPECT_CALL(
      collector_observer,
      ExtensionStageChanged(extensions::InstallStageTracker::Stage::COMPLETE))
      .InSequence(sequence);

  extensions::InstallStageTracker* install_stage_tracker =
      extensions::InstallStageTracker::Get(browser()->profile());
  install_stage_tracker->AddObserver(&collector_observer);
  UpdateProviderPolicy(policies);
  registry_observer.WaitForExtensionWillBeInstalled();
  install_stage_tracker->RemoveObserver(&collector_observer);
  // Note: Cannot check that the notification details match the expected
  // exception, since the details object has already been freed prior to
  // the completion of registry_observer.WaitForExtensionWillBeInstalled().
  EXPECT_TRUE(
      extension_cache()->GetExtension(kGoodCrxId, "", nullptr, nullptr));
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kGoodCrxId));

  // The user is not allowed to uninstall force-installed extensions.
  UninstallExtension(kGoodCrxId, false);

  scoped_refptr<extensions::UnpackedInstaller> installer =
      extensions::UnpackedInstaller::Create(extension_service());

  // The user is not allowed to load an unpacked extension with the
  // same ID as a force-installed extension.
  base::FilePath good_extension_path(ui_test_utils::GetTestFilePath(
      base::FilePath(kTestExtensionsDir), base::FilePath(kSimpleWithPopupExt)));
  extensions::LoadErrorWaiter waiter;
  installer->Load(good_extension_path);
  waiter.Wait();

  // Loading other unpacked extensions are not blocked.
  scoped_refptr<const extensions::Extension> extension =
      LoadUnpackedExtension(kAppUnpackedExt);
  ASSERT_TRUE(extension);

  const std::string old_version_number =
      registry->enabled_extensions().GetByID(kGoodCrxId)->version().GetString();

  content::WindowedNotificationObserver new_process_observer(
      content::NOTIFICATION_RENDERER_PROCESS_CREATED,
      content::NotificationService::AllSources());

  // Updating the force-installed extension.
  extensions::ExtensionUpdater* updater = service->updater();
  extensions::ExtensionUpdater::CheckParams params;
  params.install_immediately = true;
  extensions::TestExtensionRegistryObserver update_observer(
      extension_registry());
  updater->CheckNow(std::move(params));
  update_observer.WaitForExtensionWillBeInstalled();

  const base::Version& new_version =
      registry->enabled_extensions().GetByID(kGoodCrxId)->version();
  ASSERT_TRUE(new_version.IsValid());
  base::Version old_version(old_version_number);
  ASSERT_TRUE(old_version.IsValid());

  EXPECT_EQ(1, new_version.CompareTo(old_version));

  // Wait for the new extension process to launch.
  new_process_observer.Wait();

  // Wait until any background pages belonging to force-installed extensions
  // have been loaded.
  extensions::ProcessManager* manager =
      extensions::ProcessManager::Get(browser()->profile());
  extensions::ProcessManager::FrameSet all_frames = manager->GetAllFrames();
  for (auto iter = all_frames.begin(); iter != all_frames.end();) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(*iter);
    ASSERT_TRUE(web_contents);
    if (!web_contents->IsLoading()) {
      ++iter;
    } else {
      base::RunLoop().RunUntilIdle();

      // Test activity may have modified the set of extension processes during
      // message processing, so re-start the iteration to catch added/removed
      // processes.
      all_frames = manager->GetAllFrames();
      iter = all_frames.begin();
    }
  }

  // Test policy-installed extensions are reloaded when killed.
  {
    BackgroundContentsService::
        SetRestartDelayForForceInstalledAppsAndExtensionsForTesting(0);
    content::WindowedNotificationObserver extension_crashed_observer(
        extensions::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
        content::NotificationService::AllSources());
    extensions::TestExtensionRegistryObserver extension_loaded_observer(
        extension_registry(), kGoodCrxId);
    extensions::ExtensionHost* extension_host =
        extensions::ProcessManager::Get(browser()->profile())
            ->GetBackgroundHostForExtension(kGoodCrxId);
    content::RenderProcessHost* process = extension_host->render_process_host();
    content::ScopedAllowRendererCrashes allow_renderer_crashes(process);
    process->Shutdown(content::RESULT_CODE_KILLED);
    extension_crashed_observer.Wait();
    extension_loaded_observer.WaitForExtensionLoaded();
  }
}

// Verifies that "prodversionmin" attribute in update manifest is used to
// select the version to which an already installed extension should be updated.
// "Prodversionmin" specifies the minimum browser version which supports the
// corresponding extension version.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest, UpdateExtensionWithProdversionmin) {
  ExtensionRequestInterceptor interceptor;
  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));

  // Install the extension with an older version. The extension manifest of this
  // extension points to an update manifest which contains multiple <app> tags
  // with different values of "prodversionmin" attribute.
  const extensions::Extension* extension =
      InstallExtension(FILE_PATH_LITERAL("good_prodversion_v1.crx"));
  ASSERT_TRUE(extension);
  EXPECT_EQ(kGoodCrxId, extension->id());
  ASSERT_TRUE(registry->enabled_extensions().GetByID(kGoodCrxId));
  auto installed_version =
      registry->enabled_extensions().GetByID(kGoodCrxId)->version();
  EXPECT_EQ(installed_version.CompareTo(base::Version("1.0.0.0")), 0);

  // Update the extension and verify the version according to "prodversionmin"
  // in the update manifest.
  extensions::ExtensionService* service = extension_service();
  extensions::ExtensionUpdater* updater = service->updater();
  extensions::ExtensionUpdater::CheckParams params;
  params.install_immediately = true;
  extensions::TestExtensionRegistryObserver update_observer(
      extension_registry());
  updater->CheckNow(std::move(params));
  update_observer.WaitForExtensionWillBeInstalled();

  ASSERT_TRUE(registry->enabled_extensions().GetByID(kGoodCrxId));
  auto updated_version =
      registry->enabled_extensions().GetByID(kGoodCrxId)->version();
  EXPECT_EQ(updated_version.CompareTo(base::Version("1.0.0.1")), 0);
}

// Verifies that "prodversionmin" attribute in update manifest is used to
// select the extension version which should be installed. "Prodversionmin"
// specifies the minimum browser version which supports the corresponding
// extension version.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       InstallExtensionWithProdversionmin) {
  ExtensionRequestInterceptor interceptor;
  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));

  // Extensions that are force-installed come from an update URL, which defaults
  // to the webstore. Use a custom URL for this test for the update manifest.
  ASSERT_TRUE(embedded_test_server()->Start());
  const extensions::Extension* extension = InstallForceListExtension(
      "/extensions/good_prodversionmin_update_manifest.xml", kGoodCrxId);
  ASSERT_TRUE(extension);

  auto installed_version = extension->version();
  EXPECT_EQ(installed_version.CompareTo(base::Version("1.0.0.1")), 0);
}

class ExtensionPinningTest : public extensions::ExtensionBrowserTest {
 public:
  ExtensionPinningTest() = default;
  ~ExtensionPinningTest() override = default;

 protected:
  // Sets the ExtensionSettings policy so that extension |id| will be
  // force-installed from update URL pointing to test server's file
  // |update_url_suffix|. It also sets |override_update_url| flag as true for
  // the |id|.
  void SetExtensionSettingsPolicy(const std::string& update_url_suffix,
                                  const std::string& id) {
#if defined(OS_WIN)
    // Unless enterprise managed, policy handler only allows extensions from the
    // Chrome Webstore to be force installed. Mark enterprise managed for
    // windows.
    base::win::ScopedDomainStateForTesting scoped_domain(true);
#endif

    ASSERT_TRUE(embedded_test_server()->Started());
    GURL update_url = embedded_test_server()->GetURL(update_url_suffix);

    PolicyMap policies;
    base::Value dict(base::Value::Type::DICTIONARY),
        key_dict(base::Value::Type::DICTIONARY);
    key_dict.SetStringKey(extensions::schema_constants::kInstallationMode,
                          extensions::schema_constants::kForceInstalled);
    key_dict.SetStringKey(extensions::schema_constants::kUpdateUrl,
                          update_url.spec());
    key_dict.SetBoolKey(extensions::schema_constants::kOverrideUpdateUrl, true);
    dict.SetKey(id, std::move(key_dict));
    policies.Set(key::kExtensionSettings, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, std::move(dict),
                 nullptr);
    provider_.UpdateChromePolicy(policies);
  }

  // Triggers extension update process and waits for either extension
  // installation success if |expected_install_success| is true or else
  // extension installation failure.
  const extensions::Extension* TriggerExtensionUpdate(
      const std::string& id,
      bool expected_install_success) {
    extensions::ExtensionRegistry* registry = extension_registry();
    if (registry->GetExtensionById(
            id, extensions::ExtensionRegistry::EVERYTHING) == nullptr) {
      return nullptr;
    }

    extensions::ExtensionService* service = extension_service();
    extensions::ExtensionUpdater* updater = service->updater();
    extensions::ExtensionUpdater::CheckParams params;
    params.install_immediately = true;

    if (expected_install_success) {
      extensions::TestExtensionRegistryObserver update_observer(
          extension_registry());
      updater->CheckNow(std::move(params));
      update_observer.WaitForExtensionWillBeInstalled();
    } else {
      base::RunLoop run_loop;
      MockedInstallationCollectorObserver collector_observer(
          browser()->profile());
      extensions::InstallStageTracker* install_stage_tracker =
          extensions::InstallStageTracker::Get(browser()->profile());
      install_stage_tracker->AddObserver(&collector_observer);

      // We expect install failure only due to no update for the extension.
      EXPECT_CALL(
          collector_observer,
          OnExtensionInstallationFailed(
              testing::_,
              extensions::InstallStageTracker::FailureReason::NO_UPDATE))
          .WillOnce(testing::Invoke([&]() { run_loop.Quit(); }));
      updater->CheckNow(std::move(params));
      run_loop.Run();
    }

    return registry->enabled_extensions().GetByID(id);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();

    EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(provider_, IsFirstPolicyLoadComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  std::string GetUpdateManifestBody(const std::string& id,
                                    const std::string& crx_name,
                                    const std::string& version) {
    // "example.com" is a  placeholder that gets substituted with the test
    // server address at runtime.
    std::string crx_path = "http://example.com/" + crx_name;
    return "<?xml version='1.0' encoding='UTF-8'?>"
           "<gupdate xmlns='http://www.google.com/update2/response' "
           "protocol='2.0'>"
           " <app appid='" +
           id +
           "'>"
           "  <updatecheck status='ok' codebase='" +
           crx_path + "' version='" + version +
           "' />"
           " </app>"
           "</gupdate>";
  }

  std::string GetUpdateManifestHeader() {
    return "HTTP/1.1 200 OK\nContent-Type: application/json; "
           "charset=utf-8\n";
  }

  base::FilePath PackLocalExtension(const std::string& relative_dir_path,
                                    const std::string& relative_pem_path,
                                    const base::FilePath& crx_path) {
    base::FilePath base_path;
    GetTestDataDirectory(&base_path);
    auto extension_path =
        base_path.Append(kTestExtensionsDir).AppendASCII(relative_dir_path);
    base::FilePath pem_path =
        base_path.Append(kTestExtensionsDir).AppendASCII(relative_pem_path);
    return PackExtensionWithOptions(extension_path, crx_path, pem_path,
                                    base::FilePath());
  }

 private:
  MockConfigurationPolicyProvider provider_;
};

// Extension without update_url in manifest gets updated through update_url in
// policy.
IN_PROC_BROWSER_TEST_F(ExtensionPinningTest,
                       UpdateExtensionWithNoUrlInManifest) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::FilePath install_crx_path =
      scoped_temp_dir.GetPath().AppendASCII("v1.crx");
  ASSERT_EQ(
      PackLocalExtension("pinning/no_update_url/v1",
                         "pinning/no_update_url/key.pem", install_crx_path),
      install_crx_path);

  // Intercept requests to install the extension and return false for any
  // unexpected request.
  ExtensionRequestInterceptor interceptor;
  ASSERT_TRUE(embedded_test_server()->Start());
  // TODO(b/172205862): Find a way to move these requests out of the test.
  interceptor.set_interceptor_hook(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.path() == "/update_manifest_v1.xml") {
          content::URLLoaderInterceptor::WriteResponse(
              GetUpdateManifestHeader(),
              GetUpdateManifestBody(kPinnedExtensionCrxId, "v1.crx",
                                    /*version=*/"1"),
              params->client.get());
          return true;
        }

        if (params->url_request.url.path() == "/v1.crx") {
          content::URLLoaderInterceptor::WriteResponse(install_crx_path,
                                                       params->client.get());
          return true;
        }
        return false;
      }));

  extensions::TestExtensionRegistryObserver observer(extension_registry(),
                                                     kPinnedExtensionCrxId);
  SetExtensionSettingsPolicy("/update_manifest_v1.xml", kPinnedExtensionCrxId);
  auto installed_extension = observer.WaitForExtensionWillBeInstalled();
  ASSERT_TRUE(installed_extension);
  EXPECT_EQ(installed_extension->version().CompareTo(base::Version("1")), 0);

  base::FilePath updated_crx_path =
      scoped_temp_dir.GetPath().AppendASCII("v2.crx");
  ASSERT_EQ(
      PackLocalExtension("pinning/no_update_url/v2",
                         "pinning/no_update_url/key.pem", updated_crx_path),
      updated_crx_path);

  // Override the interceptor hook to only accept new requests for updated
  // extension.
  interceptor.set_interceptor_hook(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.path() == "/update_manifest_v2.xml") {
          content::URLLoaderInterceptor::WriteResponse(
              GetUpdateManifestHeader(),
              GetUpdateManifestBody(kPinnedExtensionCrxId, "v2.crx",
                                    /*version=*/"2"),
              params->client.get());
          return true;
        }

        if (params->url_request.url.path() == "/v2.crx") {
          content::URLLoaderInterceptor::WriteResponse(updated_crx_path,
                                                       params->client.get());
          return true;
        }
        return false;
      }));
  // Change |update_url| to point to version 2 of the extension.
  SetExtensionSettingsPolicy("/update_manifest_v2.xml", kPinnedExtensionCrxId);

  // Extension is updated from |update_url| in the policy.
  const extensions::Extension* updated_extension = TriggerExtensionUpdate(
      kPinnedExtensionCrxId, /*expected_install_success=*/true);
  ASSERT_TRUE(updated_extension);
  EXPECT_EQ(updated_extension->version().CompareTo(base::Version("2")), 0);
}

// Extension with one update_url in manifest gets updated through another
// update_url in policy.
IN_PROC_BROWSER_TEST_F(ExtensionPinningTest, UpdateExtensionWithUrlInManifest) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::FilePath install_crx_path =
      scoped_temp_dir.GetPath().AppendASCII("v1.crx");
  ASSERT_EQ(PackLocalExtension("pinning/update_url/v1",
                               "pinning/update_url/key.pem", install_crx_path),
            install_crx_path);

  // Intercept requests to update the extension and return false for any
  // unexpected request.
  ExtensionRequestInterceptor interceptor;
  ASSERT_TRUE(embedded_test_server()->Start());
  // TODO(b/172205862): Find a way to move these requests out of the test.
  interceptor.set_interceptor_hook(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.path() == "/update_manifest_v1.xml") {
          content::URLLoaderInterceptor::WriteResponse(
              GetUpdateManifestHeader(),
              GetUpdateManifestBody(kPinnedExtensionCrxId, "v1.crx",
                                    /*version=*/"1"),
              params->client.get());
          return true;
        }

        if (params->url_request.url.path() == "/v1.crx") {
          content::URLLoaderInterceptor::WriteResponse(install_crx_path,
                                                       params->client.get());
          return true;
        }
        return false;
      }));

  extensions::TestExtensionRegistryObserver observer(extension_registry(),
                                                     kPinnedExtensionCrxId);
  SetExtensionSettingsPolicy("/update_manifest_v1.xml", kPinnedExtensionCrxId);
  auto installed_extension = observer.WaitForExtensionWillBeInstalled();
  ASSERT_TRUE(installed_extension);
  EXPECT_EQ(installed_extension->version().CompareTo(base::Version("1")), 0);

  base::FilePath updated_crx_path =
      scoped_temp_dir.GetPath().AppendASCII("v2.crx");
  ASSERT_EQ(PackLocalExtension("pinning/update_url/v2",
                               "pinning/update_url/key.pem", updated_crx_path),
            updated_crx_path);

  // Override the interceptor hook to only accept new requests for updated
  // extension.
  interceptor.set_interceptor_hook(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.path() == "/update_manifest_v2.xml") {
          content::URLLoaderInterceptor::WriteResponse(
              GetUpdateManifestHeader(),
              GetUpdateManifestBody(kPinnedExtensionCrxId, "v2.crx",
                                    /*version=*/"2"),
              params->client.get());
          return true;
        }

        if (params->url_request.url.path() == "/v2.crx") {
          content::URLLoaderInterceptor::WriteResponse(updated_crx_path,
                                                       params->client.get());
          return true;
        }
        return false;
      }));
  // Change |update_url| to point to version 2 of the extension.
  SetExtensionSettingsPolicy("/update_manifest_v2.xml", kPinnedExtensionCrxId);

  // Extension is updated from |update_url| in the policy.
  const extensions::Extension* updated_extension = TriggerExtensionUpdate(
      kPinnedExtensionCrxId, /*expected_install_success=*/true);
  ASSERT_TRUE(updated_extension);
  EXPECT_EQ(updated_extension->version().CompareTo(base::Version("2")), 0);
}

// Extension with one update_url in manifest is not updated through it.
IN_PROC_BROWSER_TEST_F(ExtensionPinningTest, IgnoreUpdateUrlInManifest) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::FilePath install_crx_path =
      scoped_temp_dir.GetPath().AppendASCII("v1.crx");
  ASSERT_EQ(PackLocalExtension("pinning/update_url/v1",
                               "pinning/update_url/key.pem", install_crx_path),
            install_crx_path);

  // Intercept requests to update the extension and return false for any
  // unexpected request.
  ExtensionRequestInterceptor interceptor;
  ASSERT_TRUE(embedded_test_server()->Start());
  // TODO(b/172205862): Find a way to move these requests out of the test.
  interceptor.set_interceptor_hook(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.path() == "/update_manifest_v1.xml") {
          content::URLLoaderInterceptor::WriteResponse(
              GetUpdateManifestHeader(),
              GetUpdateManifestBody(kPinnedExtensionCrxId, "v1.crx",
                                    /*version=*/"1"),
              params->client.get());
          return true;
        }

        if (params->url_request.url.path() == "/v1.crx") {
          content::URLLoaderInterceptor::WriteResponse(install_crx_path,
                                                       params->client.get());
          return true;
        }
        return false;
      }));

  extensions::TestExtensionRegistryObserver observer(extension_registry(),
                                                     kPinnedExtensionCrxId);
  SetExtensionSettingsPolicy("/update_manifest_v1.xml", kPinnedExtensionCrxId);
  auto installed_extension = observer.WaitForExtensionWillBeInstalled();
  ASSERT_TRUE(installed_extension);
  EXPECT_EQ(installed_extension->version().CompareTo(base::Version("1")), 0);

  // Extension is not updated from |update_url| in extension manifest. The
  // installation fails due to no updates as |update_url| in the
  // ExtensionSettings policy is used for updates which points to the installed
  // version.
  const extensions::Extension* updated_extension = TriggerExtensionUpdate(
      kPinnedExtensionCrxId, /*expected_install_success=*/false);
  ASSERT_TRUE(updated_extension);
  EXPECT_EQ(updated_extension->version().CompareTo(base::Version("1")), 0);
}

// Self hosted extension from the Chrome Web Store is not updated through the
// update_url in it's manifest even though a new version is available on the
// Store.
IN_PROC_BROWSER_TEST_F(ExtensionPinningTest,
                       SelfHostedCWSExtensionNotUpdatedFromStore) {
  // Sample Google calendar extension from Chrome Web Store, for which we have
  // an old CRX.
  const char kGoogleCalendarCrxId[] = "gmbgaklkmjakoegficnlkhebmhkjfich";
  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_FALSE(registry->GetExtensionById(
      kGoogleCalendarCrxId, extensions::ExtensionRegistry::EVERYTHING));

  // Intercept requests to update the extension and return false for any
  // unexpected request.
  ExtensionRequestInterceptor interceptor;
  ASSERT_TRUE(embedded_test_server()->Start());
  // TODO(b/172205862): Find a way to move these requests out of the test.
  interceptor.set_interceptor_hook(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.path() == "/update_manifest.xml") {
          content::URLLoaderInterceptor::WriteResponse(
              GetUpdateManifestHeader(),
              GetUpdateManifestBody(kGoogleCalendarCrxId, "calendar.crx",
                                    "3.1.0"),
              params->client.get());
          return true;
        }

        if (params->url_request.url.path() == "/calendar.crx") {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/extensions/pinning/calendar/"
              "gmbgaklkmjakoegficnlkhebmhkjfich-google-calendar-3.1.0.crx",
              params->client.get());
          return true;
        }
        return false;
      }));

  extensions::TestExtensionRegistryObserver observer(extension_registry(),
                                                     kGoogleCalendarCrxId);
  SetExtensionSettingsPolicy("/update_manifest.xml", kGoogleCalendarCrxId);
  auto installed_extension = observer.WaitForExtensionWillBeInstalled();
  ASSERT_TRUE(installed_extension);
  EXPECT_EQ(installed_extension->version().CompareTo(base::Version("3.1.0")),
            0);

  // Extension is not updated from |update_url| in extension manifest. The
  // installation fails due to no updates as |update_url| in the
  // ExtensionSettings policy is used for updates which points to the installed
  // version.
  const extensions::Extension* updated_extension = TriggerExtensionUpdate(
      kGoogleCalendarCrxId, /*expected_install_success=*/false);
  ASSERT_TRUE(updated_extension);
  EXPECT_EQ(updated_extension->version().CompareTo(base::Version("3.1.0")), 0);
}

// Verifies that if multiple <app> tags for an extension are specified in the
// update manifest, the first valid tag is selected for crx download. This
// test helps to notify of any change in this behaviour as there might be
// extensions relying on it.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest, UpdateManifestOrderedAppTags) {
  ExtensionRequestInterceptor interceptor;
  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));

  // Extensions that are force-installed come from an update URL, which defaults
  // to the webstore. Use a custom URL for this test for the update manifest.
  ASSERT_TRUE(embedded_test_server()->Start());
  const extensions::Extension* extension = InstallForceListExtension(
      "/extensions/good_ordered_app_tags_update_manifest.xml", kGoodCrxId);
  ASSERT_TRUE(extension);

  auto installed_version = extension->version();
  EXPECT_EQ(installed_version.CompareTo(base::Version("1.0.0.0")), 0);
}

// Verifies that corrupted non-webstore policy-based extension is automatically
// repaired (reinstalled).
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       CorruptedNonWebstoreExtensionRepaired) {
  ignore_content_verifier_.reset();
  ExtensionRequestInterceptor interceptor;
  ASSERT_TRUE(embedded_test_server()->Start());

  const base::FilePath kResourcePath(FILE_PATH_LITERAL("script1.js"));

  extensions::ExtensionService* service = extension_service();

  // Step 1: Setup a policy and force-install an extension.
  const extensions::Extension* extension = InstallForceListExtension(
      "/extensions/good_v1_update_manifest.xml", kGoodCrxId);
  ASSERT_TRUE(extension);

  // Step 2: Corrupt extension's resource.
  {
    base::FilePath resource_path = extension->path().Append(kResourcePath);
    base::ScopedAllowBlockingForTesting allow_blocking;
    // Temporarily disable extension, we don't want to tackle with resources of
    // enabled one. Not using command DISABLE_USER_ACTION reason since
    // force-installed extension may not be disabled by user action.
    service->DisableExtension(kGoodCrxId,
                              extensions::disable_reason::DISABLE_RELOAD);

    const std::string kCorruptedContent("// corrupted\n");
    ASSERT_TRUE(base::WriteFile(resource_path, kCorruptedContent));

    service->EnableExtension(kGoodCrxId);
  }

  extensions::TestContentVerifyJobObserver content_verify_job_observer;
  extensions::TestExtensionRegistryObserver registry_observer(
      extension_registry());

  // Step 3: Fetch resource to trigger corruption check and wait for content
  // verify job completion.
  {
    content_verify_job_observer.ExpectJobResult(
        kGoodCrxId, kResourcePath,
        extensions::TestContentVerifyJobObserver::Result::FAILURE);

    GURL resource_url = extension->GetResourceURL("script1.js");
    FetchSubresource(browser()->tab_strip_model()->GetActiveWebContents(),
                     resource_url);

    EXPECT_TRUE(content_verify_job_observer.WaitForExpectedJobs());
  }

  // Step 4: Check that we are going to reinstall the extension and wait for
  // extension reinstall.
  EXPECT_TRUE(service->pending_extension_manager()
                  ->IsPolicyReinstallForCorruptionExpected(kGoodCrxId));
  registry_observer.WaitForExtensionWillBeInstalled();

  // Extension was reloaded, old extension object is invalid.
  extension = extension_registry()->enabled_extensions().GetByID(kGoodCrxId);

  // Step 5: Check that resource has its original contents.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath resource_path = extension->path().Append(kResourcePath);
    std::string contents;
    ASSERT_TRUE(base::ReadFileToString(resource_path, &contents));
    EXPECT_EQ("// script1\n", contents);
  }
}

// Verifies that corrupted non-webstore policy-based extension is automatically
// repaired (reinstalled) even if hashes file is damaged too.
// crbug.com/1131634: flaky on win
#if defined(OS_WIN)
#define MAYBE_CorruptedNonWebstoreExtensionWithDamagedHashesRepaired \
  DISABLED_CorruptedNonWebstoreExtensionWithDamagedHashesRepaired
#else
#define MAYBE_CorruptedNonWebstoreExtensionWithDamagedHashesRepaired \
  CorruptedNonWebstoreExtensionWithDamagedHashesRepaired
#endif
IN_PROC_BROWSER_TEST_F(
    ExtensionPolicyTest,
    MAYBE_CorruptedNonWebstoreExtensionWithDamagedHashesRepaired) {
  ignore_content_verifier_.reset();
  ExtensionRequestInterceptor interceptor;
  ASSERT_TRUE(embedded_test_server()->Start());

  const base::FilePath kResourcePath(FILE_PATH_LITERAL("script1.js"));

  extensions::ExtensionService* service = extension_service();

  // Step 1: Setup a policy and force-install an extension.
  const extensions::Extension* extension = InstallForceListExtension(
      "/extensions/good_v1_update_manifest.xml", kGoodCrxId);
  ASSERT_TRUE(extension);

  // Step 2: Corrupt extension's resource and hashes file.
  {
    base::FilePath resource_path = extension->path().Append(kResourcePath);
    base::ScopedAllowBlockingForTesting allow_blocking;
    // Temporarily disable extension, we don't want to tackle with resources of
    // enabled one. Not using command DISABLE_USER_ACTION reason since
    // force-installed extension may not be disabled by user action.
    service->DisableExtension(kGoodCrxId,
                              extensions::disable_reason::DISABLE_RELOAD);

    const std::string kCorruptedContent("// corrupted\n");
    ASSERT_TRUE(base::WriteFile(resource_path, kCorruptedContent));

    const std::string kInvalidJson("not a json");
    ASSERT_TRUE(base::WriteFile(
        extensions::file_util::GetComputedHashesPath(extension->path()),
        kInvalidJson));

    service->EnableExtension(kGoodCrxId);
  }

  extensions::TestExtensionRegistryObserver observer(extension_registry());

  // Step 3: Fetch resource to trigger corruption check and wait for content
  // verify job completion.
  {
    extensions::TestContentVerifyJobObserver content_verify_job_observer;
    content_verify_job_observer.ExpectJobResult(
        kGoodCrxId, kResourcePath,
        extensions::TestContentVerifyJobObserver::Result::FAILURE);

    GURL resource_url = extension->GetResourceURL("script1.js");
    FetchSubresource(browser()->tab_strip_model()->GetActiveWebContents(),
                     resource_url);

    EXPECT_TRUE(content_verify_job_observer.WaitForExpectedJobs());
  }

  // Step 4: Check that we are going to reinstall the extension and wait for
  // extension reinstall.
  EXPECT_TRUE(service->pending_extension_manager()
                  ->IsPolicyReinstallForCorruptionExpected(kGoodCrxId));
  observer.WaitForExtensionWillBeInstalled();

  // Extension was reloaded, old extension object is invalid.
  extension = extension_registry()->enabled_extensions().GetByID(kGoodCrxId);

  // Step 5: Check that resource has its original contents.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath resource_path = extension->path().Append(kResourcePath);
    std::string contents;
    ASSERT_TRUE(base::ReadFileToString(resource_path, &contents));
    EXPECT_EQ("// script1\n", contents);
  }
}

// Verifies that corrupted non-webstore policy-based extension is not repaired
// if there are no computed_hashes.json for it. Note that this behavior will
// change in the future.
// See https://crbug.com/958794#c22 for details.
// TODO(https://crbug.com/1044572): Change this test so extension without hashes
// will be also reinstalled.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       CorruptedNonWebstoreExtensionWithoutHashesRemained) {
  ignore_content_verifier_.reset();
  ExtensionRequestInterceptor interceptor;
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histogram_tester;

  const base::FilePath kResourcePath(FILE_PATH_LITERAL("script1.js"));

  extensions::ExtensionService* service = extension_service();

  // Step 1: Setup a policy and force-install an extension.
  const extensions::Extension* extension = InstallForceListExtension(
      "/extensions/good_v1_update_manifest.xml", kGoodCrxId);
  ASSERT_TRUE(extension);

  // Step 2: Corrupt extension's resource and remove hashes.
  {
    base::FilePath resource_path = extension->path().Append(kResourcePath);
    base::ScopedAllowBlockingForTesting allow_blocking;
    // Temporarily disable extension, we don't want to tackle with resources of
    // enabled one. Not using command DISABLE_USER_ACTION reason since
    // force-installed extension may not be disabled by user action.
    service->DisableExtension(kGoodCrxId,
                              extensions::disable_reason::DISABLE_RELOAD);

    const std::string kCorruptedContent("// corrupted\n");
    ASSERT_TRUE(base::WriteFile(resource_path, kCorruptedContent));
    ASSERT_TRUE(base::DeleteFile(
        extensions::file_util::GetComputedHashesPath(extension->path())));

    service->EnableExtension(kGoodCrxId);
  }

  extensions::TestContentVerifyJobObserver content_verify_job_observer;

  // Step 3: Fetch resource to trigger corruption check and wait for content
  // verify job completion.
  {
    content_verify_job_observer.ExpectJobResult(
        kGoodCrxId, kResourcePath,
        extensions::TestContentVerifyJobObserver::Result::FAILURE);

    GURL resource_url = extension->GetResourceURL("script1.js");
    FetchSubresource(browser()->tab_strip_model()->GetActiveWebContents(),
                     resource_url);

    EXPECT_TRUE(content_verify_job_observer.WaitForExpectedJobs());
  }

  // Step 4: Check that we are not going to reinstall the extension, but we have
  // detected a corruption.
  EXPECT_FALSE(service->pending_extension_manager()
                   ->IsPolicyReinstallForCorruptionExpected(kGoodCrxId));
  histogram_tester.ExpectUniqueSample(
      "Extensions.CorruptPolicyExtensionDetected3",
      extensions::PendingExtensionManager::PolicyReinstallReason::
          NO_UNSIGNED_HASHES_FOR_NON_WEBSTORE_SKIP,
      1);
}

// Verifies that the extension is installed when the manifest is not fetched in
// case the remote update server is down.
// TODO(http://crbug.com/1086148): Add a check whether the cache entry is
// recorded in ForceInstalledMetrics after the bug is fixed.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       ExtensionInstallForcelistServerShutDown) {
  base::HistogramTester histogram_tester;
  ExtensionRequestInterceptor interceptor;

  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/extensions/good_v1_update_manifest.xml");

  extension_service()->updater()->SetBackoffPolicyForTesting(
      &kDefaultBackOffPolicyForTesting);

  base::FilePath extension_path(ui_test_utils::GetTestFilePath(
      base::FilePath(kTestExtensionsDir), base::FilePath(kGoodV1CrxName)));

  test_extension_cache_->AllowCaching(kGoodCrxId);
  test_extension_cache_->PutExtension(
      kGoodCrxId, "" /* expected hash, ignored by ExtensionCacheFake */,
      extension_path, "1.0", base::DoNothing());
  // Shut down test update server to make update manifest and CRX queries fail
  // with network error code net::ERR_CONNECTION_REFUSED.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  PolicyMap policies;
  AddExtensionToForceList(&policies, kGoodCrxId, url);
  extensions::TestExtensionRegistryObserver observer(registry);
  UpdateProviderPolicy(policies);

  observer.WaitForExtensionInstalled();

  EXPECT_TRUE(registry->enabled_extensions().GetByID(kGoodCrxId));
}

// Verifies that the extension is installed when the manifest is not fetched in
// case the device is offline. This test mimics a server providing
// ERR_INTERNET_DISCONNECTED response instead of actually having the device
// offline.
// TODO(http://crbug.com/1086148): Add a check whether the cache entry is
// recorded in ForceInstalledMetrics after the bug is fixed.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest, ExtensionInstallForcelistOffline) {
  base::HistogramTester histogram_tester;
  ExtensionRequestInterceptor interceptor;

  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/extensions/good_v1_update_manifest.xml");
  // This simulates inability to make network requests for fetching the
  // extension update manifest and CRX files.
  {
    interceptor.set_interceptor_hook(base::BindLambdaForTesting(
        [&](content::URLLoaderInterceptor::RequestParams* params) {
          if (params->url_request.url.path() !=
              "/extensions/good_v1_update_manifest.xml")
            return false;
          params->client->OnComplete(network::URLLoaderCompletionStatus(
              net::ERR_INTERNET_DISCONNECTED));
          return true;
        }));
  }
  extension_service()->updater()->SetBackoffPolicyForTesting(
      &kDefaultBackOffPolicyForTesting);

  base::FilePath extension_path(ui_test_utils::GetTestFilePath(
      base::FilePath(kTestExtensionsDir), base::FilePath(kGoodV1CrxName)));

  test_extension_cache_->AllowCaching(kGoodCrxId);
  test_extension_cache_->PutExtension(
      kGoodCrxId, "" /* expected hash, ignored by ExtensionCacheFake */,
      extension_path, "1.0", base::DoNothing());
  PolicyMap policies;
  AddExtensionToForceList(&policies, kGoodCrxId, url);

  extensions::TestExtensionRegistryObserver observer(registry);
  UpdateProviderPolicy(policies);
  observer.WaitForExtensionInstalled();

  EXPECT_TRUE(registry->enabled_extensions().GetByID(kGoodCrxId));
}

IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       ExtensionInstallForcelist_DefaultedUpdateUrl) {
  // Verifies the ExtensionInstallForcelist policy with an empty (defaulted)
  // "update" URL.

  ExtensionRequestInterceptor interceptor;

  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));

  PolicyMap policies;
  AddExtensionToForceList(&policies, kGoodCrxId, GURL());
  extensions::TestExtensionRegistryObserver observer(registry);
  UpdateProviderPolicy(policies);
  observer.WaitForExtensionWillBeInstalled();

  EXPECT_TRUE(registry->enabled_extensions().GetByID(kGoodCrxId));
}

// Verifies that the browser doesn't crash on shutdown. If the extensions are
// being installed, and the browser is shutdown, it should not lead to a crash
// as in (crbug/1114191).
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       ExtensionInstallForcelistShutdownBeforeInstall) {
  ExtensionRequestInterceptor interceptor;

  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/extensions/good_v1_update_manifest.xml");

  PolicyMap policies;
  AddExtensionToForceList(&policies, kGoodCrxId, url);
  UpdateProviderPolicy(policies);
  // The extension is not yet installed, shutdown the browser now and there
  // should be no crash.
}

IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       ExtensionRecommendedInstallationMode) {
  // Verifies that extensions that are recommended-installed by policies are
  // installed, can be disabled but not uninstalled.

  // Recommended-installed extensions should auto-enable on install without a
  // user prompt.
  extensions::FeatureSwitch::ScopedOverride external_prompt_override(
      extensions::FeatureSwitch::prompt_for_external_extensions(), true);

  ExtensionRequestInterceptor interceptor;

  // Extensions that are force-installed come from an update URL, which defaults
  // to the webstore. Use a test URL for this test with an update manifest
  // that includes "good_v1.crx".
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/extensions/good_v1_update_manifest.xml");

// Mark as enterprise managed.
#if defined(OS_WIN)
  base::win::ScopedDomainStateForTesting scoped_domain(true);
#endif

  extensions::ExtensionService* service = extension_service();
  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));

  // Setting the forcelist extension should install "good_v1.crx".
  base::DictionaryValue dict_value;
  dict_value.SetString(std::string(kGoodCrxId) + "." +
                           extensions::schema_constants::kInstallationMode,
                       extensions::schema_constants::kNormalInstalled);
  dict_value.SetString(
      std::string(kGoodCrxId) + "." + extensions::schema_constants::kUpdateUrl,
      url.spec());
  PolicyMap policies;
  policies.Set(key::kExtensionSettings, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, dict_value.Clone(),
               nullptr);
  extensions::TestExtensionRegistryObserver observer(registry);
  UpdateProviderPolicy(policies);
  observer.WaitForExtensionInstalled();
  EXPECT_TRUE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::ENABLED));

  // The user is not allowed to uninstall recommended-installed extensions.
  UninstallExtension(kGoodCrxId, false);

  // But the user is allowed to disable them.
  EXPECT_TRUE(service->IsExtensionEnabled(kGoodCrxId));
  DisableExtension(kGoodCrxId);
  EXPECT_FALSE(service->IsExtensionEnabled(kGoodCrxId));
}

IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest, ExtensionAllowedTypes) {
  // Verifies that extensions are blocked if policy specifies an allowed types
  // list and the extension's type is not on that list.
  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));
  ASSERT_FALSE(registry->GetExtensionById(
      kHostedAppCrxId, extensions::ExtensionRegistry::EVERYTHING));

  base::ListValue allowed_types;
  allowed_types.AppendString("hosted_app");
  PolicyMap policies;
  policies.Set(key::kExtensionAllowedTypes, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, allowed_types.Clone(),
               nullptr);
  UpdateProviderPolicy(policies);

  // "good.crx" is blocked.
  EXPECT_FALSE(InstallExtension(kGoodCrxName));
  EXPECT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));

  // "hosted_app.crx" is of a allowlisted type.
  const extensions::Extension* hosted_app = InstallExtension(kHostedAppCrxName);
  ASSERT_TRUE(hosted_app);
  EXPECT_EQ(kHostedAppCrxId, hosted_app->id());
  EXPECT_EQ(hosted_app,
            registry->enabled_extensions().GetByID(kHostedAppCrxId));

  // The user can remove the extension.
  UninstallExtension(kHostedAppCrxId, true);
}

// Checks that a click on an extension CRX download triggers the extension
// installation prompt without further user interaction when the source is
// allowlisted by policy.
// Flaky on windows; http://crbug.com/295729 .
#if defined(OS_WIN)
#define MAYBE_ExtensionInstallSources DISABLED_ExtensionInstallSources
#else
#define MAYBE_ExtensionInstallSources ExtensionInstallSources
#endif
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest, MAYBE_ExtensionInstallSources) {
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  extensions::ScopedInstallVerifierBypassForTest install_verifier_bypass;

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL download_page_url = embedded_test_server()->GetURL(
      "/policy/extension_install_sources_test.html");
  ui_test_utils::NavigateToURL(browser(), download_page_url);

  const GURL install_source_url(
      embedded_test_server()->GetURL("/extensions/*"));
  const GURL referrer_url(embedded_test_server()->GetURL("/policy/*"));

  // As long as the policy is not present, extensions are considered dangerous.
  content::DownloadTestObserverTerminal download_observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()), 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_DENY);
  PerformClick(0, 0);
  download_observer.WaitForFinished();

  // Install the policy and trigger another download.
  base::ListValue install_sources;
  install_sources.AppendString(install_source_url.spec());
  install_sources.AppendString(referrer_url.spec());
  PolicyMap policies;
  policies.Set(key::kExtensionInstallSources, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, install_sources.Clone(),
               nullptr);
  UpdateProviderPolicy(policies);

  extensions::TestExtensionRegistryObserver observer(extension_registry());
  PerformClick(1, 0);
  observer.WaitForExtensionWillBeInstalled();
  // Note: Cannot check that the notification details match the expected
  // exception, since the details object has already been freed prior to
  // the completion of observer.WaitForExtensionWillBeInstalled().

  // The first extension shouldn't be present, the second should be there.
  EXPECT_FALSE(extension_registry()->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));
  EXPECT_TRUE(
      extension_registry()->enabled_extensions().GetByID(kSimpleWithIconCrxId));
}

// Verifies that extensions with version older than the minimum version required
// by policy will get disabled, and will be auto-updated and/or re-enabled upon
// policy changes as well as regular auto-updater scheduled updates.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest, ExtensionMinimumVersionRequired) {
  ExtensionRequestInterceptor interceptor;

  base::AtomicRefCount update_extension_count;
  base::RunLoop first_update_extension_runloop;
  interceptor.set_interceptor_hook(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.host() != "update.extension")
          return false;

        if (!update_extension_count.IsZero() && !update_extension_count.IsOne())
          return false;

        if (update_extension_count.IsZero()) {
          content::URLLoaderInterceptor::WriteResponse(
              "400 Bad request", std::string(), params->client.get());
        } else {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/extensions/good2_update_manifest.xml",
              params->client.get());
        }
        if (update_extension_count.IsZero())
          first_update_extension_runloop.Quit();
        update_extension_count.Increment();
        return true;
      }));

  extensions::ExtensionService* service = extension_service();
  extensions::ExtensionRegistry* registry = extension_registry();
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(browser()->profile());

  // Install the extension.
  EXPECT_TRUE(InstallExtension(kGoodV1CrxName));
  EXPECT_TRUE(registry->enabled_extensions().Contains(kGoodCrxId));

  // Update policy to set a minimum version of 1.0.0.0, the extension (with
  // version 1.0.0.0) should still be enabled.
  {
    extensions::ExtensionManagementPolicyUpdater management_policy(&provider_);
    management_policy.SetMinimumVersionRequired(kGoodCrxId, "1.0.0.0");
  }
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(registry->enabled_extensions().Contains(kGoodCrxId));

  // Update policy to set a minimum version of 1.0.0.1, the extension (with
  // version 1.0.0.0) should now be disabled.
  EXPECT_TRUE(update_extension_count.IsZero());
  {
    extensions::ExtensionManagementPolicyUpdater management_policy(&provider_);
    management_policy.SetMinimumVersionRequired(kGoodCrxId, kGoodCrxVersion);
  }
  first_update_extension_runloop.Run();
  EXPECT_TRUE(update_extension_count.IsOne());

  EXPECT_TRUE(registry->disabled_extensions().Contains(kGoodCrxId));
  EXPECT_EQ(extensions::disable_reason::DISABLE_UPDATE_REQUIRED_BY_POLICY,
            extension_prefs->GetDisableReasons(kGoodCrxId));

  // Provide a new version (1.0.0.1) which is expected to be auto updated to
  // via the update URL in the manifest of the older version.
  EXPECT_TRUE(update_extension_count.IsOne());
  {
    extensions::TestExtensionRegistryObserver update_observer(registry);
    service->updater()->CheckSoon();
    update_observer.WaitForExtensionWillBeInstalled();
  }
  EXPECT_EQ(2, update_extension_count.SubtleRefCountForDebug());

  // The extension should be auto-updated to newer version and re-enabled.
  EXPECT_EQ(kGoodCrxVersion,
            registry->GetInstalledExtension(kGoodCrxId)->version().GetString());
  EXPECT_TRUE(registry->enabled_extensions().Contains(kGoodCrxId));
}

// Similar to ExtensionMinimumVersionRequired test, but with different settings
// and orders.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       ExtensionMinimumVersionRequiredAlt) {
  ExtensionRequestInterceptor interceptor;

  base::AtomicRefCount update_extension_count;
  interceptor.set_interceptor_hook(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.host() == "update.extension" &&
            update_extension_count.IsZero()) {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/extensions/good2_update_manifest.xml",
              params->client.get());
          update_extension_count.Increment();
          return true;
        }
        return false;
      }));

  extensions::ExtensionRegistry* registry = extension_registry();
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(browser()->profile());

  // Set the policy to require an even higher minimum version this time.
  {
    extensions::ExtensionManagementPolicyUpdater management_policy(&provider_);
    management_policy.SetMinimumVersionRequired(kGoodCrxId, "1.0.0.2");
  }
  base::RunLoop().RunUntilIdle();

  // Install the 1.0.0.0 version, it should be installed but disabled.
  EXPECT_TRUE(InstallExtension(kGoodV1CrxName));
  EXPECT_TRUE(registry->disabled_extensions().Contains(kGoodCrxId));
  EXPECT_EQ(extensions::disable_reason::DISABLE_UPDATE_REQUIRED_BY_POLICY,
            extension_prefs->GetDisableReasons(kGoodCrxId));
  EXPECT_EQ("1.0.0.0",
            registry->GetInstalledExtension(kGoodCrxId)->version().GetString());

  // An extension management policy update should trigger an update as well.
  EXPECT_TRUE(update_extension_count.IsZero());
  {
    extensions::TestExtensionRegistryObserver update_observer(registry);
    {
      // Set a higher minimum version, just intend to trigger a policy update.
      extensions::ExtensionManagementPolicyUpdater management_policy(
          &provider_);
      management_policy.SetMinimumVersionRequired(kGoodCrxId, "1.0.0.3");
    }
    base::RunLoop().RunUntilIdle();
    update_observer.WaitForExtensionWillBeInstalled();
  }
  EXPECT_TRUE(update_extension_count.IsOne());

  // It should be updated to 1.0.0.1 but remain disabled.
  EXPECT_EQ(kGoodCrxVersion,
            registry->GetInstalledExtension(kGoodCrxId)->version().GetString());
  EXPECT_TRUE(registry->disabled_extensions().Contains(kGoodCrxId));
  EXPECT_EQ(extensions::disable_reason::DISABLE_UPDATE_REQUIRED_BY_POLICY,
            extension_prefs->GetDisableReasons(kGoodCrxId));

  // Remove the minimum version requirement. The extension should be re-enabled.
  {
    extensions::ExtensionManagementPolicyUpdater management_policy(&provider_);
    management_policy.UnsetMinimumVersionRequired(kGoodCrxId);
  }
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(registry->enabled_extensions().Contains(kGoodCrxId));
  EXPECT_FALSE(extension_prefs->HasDisableReason(
      kGoodCrxId,
      extensions::disable_reason::DISABLE_UPDATE_REQUIRED_BY_POLICY));
}

// Verifies that a force-installed extension which does not meet a subsequently
// set minimum version requirement is handled well.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       ExtensionMinimumVersionForceInstalled) {
  ExtensionRequestInterceptor interceptor;

// Mark as enterprise managed.
#if defined(OS_WIN)
  base::win::ScopedDomainStateForTesting scoped_domain(true);
#endif
  extensions::ExtensionRegistry* registry = extension_registry();
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(browser()->profile());

  // Prepare the update URL for force installing.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/extensions/good_v1_update_manifest.xml");

  // Set policy to force-install the extension, it should be installed and
  // enabled.
  extensions::TestExtensionRegistryObserver install_observer(registry);
  EXPECT_FALSE(registry->enabled_extensions().Contains(kGoodCrxId));
  {
    extensions::ExtensionManagementPolicyUpdater management_policy(&provider_);
    management_policy.SetIndividualExtensionAutoInstalled(kGoodCrxId,
                                                          url.spec(), true);
  }
  base::RunLoop().RunUntilIdle();
  install_observer.WaitForExtensionWillBeInstalled();

  EXPECT_TRUE(registry->enabled_extensions().Contains(kGoodCrxId));

  // Set policy a minimum version of "1.0.0.1", the extension now should be
  // disabled.
  {
    extensions::ExtensionManagementPolicyUpdater management_policy(&provider_);
    management_policy.SetMinimumVersionRequired(kGoodCrxId, kGoodCrxVersion);
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(registry->enabled_extensions().Contains(kGoodCrxId));
  EXPECT_TRUE(registry->disabled_extensions().Contains(kGoodCrxId));
  EXPECT_EQ(extensions::disable_reason::DISABLE_UPDATE_REQUIRED_BY_POLICY,
            extension_prefs->GetDisableReasons(kGoodCrxId));
}

// Similar to ExtensionPolicyTest but sets the WebAppInstallForceList policy
// before the browser is started.
class WebAppInstallForceListPolicyTest : public ExtensionPolicyTest {
 public:
  WebAppInstallForceListPolicyTest()
      : test_page_("/banners/manifest_test_page.html") {}
  ~WebAppInstallForceListPolicyTest() override = default;
  WebAppInstallForceListPolicyTest(const WebAppInstallForceListPolicyTest&) =
      delete;
  WebAppInstallForceListPolicyTest& operator=(
      const WebAppInstallForceListPolicyTest&) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionPolicyTest::SetUpInProcessBrowserTestFixture();
    ASSERT_TRUE(embedded_test_server()->Start());

    policy_app_url_ = embedded_test_server()->GetURL(test_page_);
    base::Value url(policy_app_url_.spec());
    base::Value launch_container("window");

    base::Value item(base::Value::Type::DICTIONARY);
    item.SetKey("url", std::move(url));
    item.SetKey("default_launch_container", std::move(launch_container));
    if (fallback_app_name_.has_value()) {
      base::Value fallback_app_name(fallback_app_name_.value());
      item.SetKey("fallback_app_name", std::move(fallback_app_name));
    }

    base::Value list(base::Value::Type::LIST);
    list.Append(std::move(item));

    PolicyMap policies;
    SetPolicy(&policies, key::kWebAppInstallForceList, std::move(list));
    provider_.UpdateChromePolicy(policies);
  }

 protected:
  std::string test_page_;
  GURL policy_app_url_;
  base::Optional<std::string> fallback_app_name_;
};

IN_PROC_BROWSER_TEST_F(WebAppInstallForceListPolicyTest, StartUpInstallation) {
  const web_app::AppRegistrar& registrar =
      web_app::WebAppProviderBase::GetProviderBase(browser()->profile())
          ->registrar();
  web_app::WebAppInstallObserver install_observer(browser()->profile());
  base::Optional<web_app::AppId> app_id =
      registrar.FindAppWithUrlInScope(policy_app_url_);
  if (!app_id)
    app_id = install_observer.AwaitNextInstall();
  EXPECT_EQ(policy_app_url_, registrar.GetAppStartUrl(*app_id));
}

class WebAppInstallForceListPolicyWithAppFallbackNameManifestTest
    : public WebAppInstallForceListPolicyTest {
 public:
  WebAppInstallForceListPolicyWithAppFallbackNameManifestTest() {
    test_page_ = "/banners/manifest_test_page.html";
    fallback_app_name_ = "fallback app name";
  }

  ~WebAppInstallForceListPolicyWithAppFallbackNameManifestTest() override =
      default;
  WebAppInstallForceListPolicyWithAppFallbackNameManifestTest(
      const WebAppInstallForceListPolicyWithAppFallbackNameManifestTest&) =
      delete;
  WebAppInstallForceListPolicyWithAppFallbackNameManifestTest& operator=(
      const WebAppInstallForceListPolicyWithAppFallbackNameManifestTest&) =
      delete;
};

IN_PROC_BROWSER_TEST_F(
    WebAppInstallForceListPolicyWithAppFallbackNameManifestTest,
    StartUpInstallationPWAFallbackName) {
  const web_app::AppRegistrar& registrar =
      web_app::WebAppProviderBase::GetProviderBase(browser()->profile())
          ->registrar();
  web_app::WebAppInstallObserver install_observer(browser()->profile());
  base::Optional<web_app::AppId> app_id =
      registrar.FindAppWithUrlInScope(policy_app_url_);
  if (!app_id)
    app_id = install_observer.AwaitNextInstall();
  EXPECT_EQ(policy_app_url_, registrar.GetAppStartUrl(*app_id));

  // We specifically don't expect the fallback name to be used for a PWA
  // except for the placeholder app.
  EXPECT_NE(fallback_app_name_, registrar.GetAppShortName(*app_id));
}

// SAA == Site as App (a non-PWA installed as an app)
class WebAppInstallForceListPolicySAATest
    : public WebAppInstallForceListPolicyTest {
 public:
  WebAppInstallForceListPolicySAATest() {
    test_page_ = "/banners/no_manifest_test_page.html";
  }

  ~WebAppInstallForceListPolicySAATest() override = default;
  WebAppInstallForceListPolicySAATest(
      const WebAppInstallForceListPolicySAATest&) = delete;
  WebAppInstallForceListPolicySAATest& operator=(
      const WebAppInstallForceListPolicySAATest&) = delete;
};

IN_PROC_BROWSER_TEST_F(WebAppInstallForceListPolicySAATest,
                       StartUpInstallationSAA) {
  const web_app::AppRegistrar& registrar =
      web_app::WebAppProviderBase::GetProviderBase(browser()->profile())
          ->registrar();
  web_app::WebAppInstallObserver install_observer(browser()->profile());
  base::Optional<web_app::AppId> app_id =
      registrar.FindAppWithUrlInScope(policy_app_url_);
  if (!app_id)
    app_id = install_observer.AwaitNextInstall();
  EXPECT_EQ(policy_app_url_, registrar.GetAppStartUrl(*app_id));
  EXPECT_NE(fallback_app_name_, registrar.GetAppShortName(*app_id));
}

class WebAppInstallForceListPolicyWithAppFallbackNameSAATest
    : public WebAppInstallForceListPolicyTest {
 public:
  WebAppInstallForceListPolicyWithAppFallbackNameSAATest() {
    test_page_ = "/banners/no_manifest_test_page.html";
    fallback_app_name_ = "fallback app name";
  }

  ~WebAppInstallForceListPolicyWithAppFallbackNameSAATest() override = default;
  WebAppInstallForceListPolicyWithAppFallbackNameSAATest(
      const WebAppInstallForceListPolicyWithAppFallbackNameSAATest&) = delete;
  WebAppInstallForceListPolicyWithAppFallbackNameSAATest& operator=(
      const WebAppInstallForceListPolicyWithAppFallbackNameSAATest&) = delete;
};

IN_PROC_BROWSER_TEST_F(WebAppInstallForceListPolicyWithAppFallbackNameSAATest,
                       StartUpInstallationSAAFallbackName) {
  const web_app::AppRegistrar& registrar =
      web_app::WebAppProviderBase::GetProviderBase(browser()->profile())
          ->registrar();
  web_app::WebAppInstallObserver install_observer(browser()->profile());
  base::Optional<web_app::AppId> app_id =
      registrar.FindAppWithUrlInScope(policy_app_url_);
  if (!app_id)
    app_id = install_observer.AwaitNextInstall();
  EXPECT_EQ(policy_app_url_, registrar.GetAppStartUrl(*app_id));
  EXPECT_EQ(fallback_app_name_, registrar.GetAppShortName(*app_id));
}

class WebAppInstallForceListPolicyPlaceholderWithAppFallbackNameTest
    : public WebAppInstallForceListPolicyTest {
 public:
  WebAppInstallForceListPolicyPlaceholderWithAppFallbackNameTest() {
    test_page_ = "/close-socket";
    fallback_app_name_ = "fallback app name";
  }

  ~WebAppInstallForceListPolicyPlaceholderWithAppFallbackNameTest() override =
      default;
  WebAppInstallForceListPolicyPlaceholderWithAppFallbackNameTest(
      const WebAppInstallForceListPolicyPlaceholderWithAppFallbackNameTest&) =
      delete;
  WebAppInstallForceListPolicyPlaceholderWithAppFallbackNameTest& operator=(
      const WebAppInstallForceListPolicyPlaceholderWithAppFallbackNameTest&) =
      delete;
};

IN_PROC_BROWSER_TEST_F(
    WebAppInstallForceListPolicyPlaceholderWithAppFallbackNameTest,
    StartUpInstallationPlaceholderFallbackName) {
  const web_app::AppRegistrar& registrar =
      web_app::WebAppProviderBase::GetProviderBase(browser()->profile())
          ->registrar();
  web_app::WebAppInstallObserver install_observer(browser()->profile());
  base::Optional<web_app::AppId> app_id =
      registrar.FindAppWithUrlInScope(policy_app_url_);
  if (!app_id)
    app_id = install_observer.AwaitNextInstall();
  EXPECT_EQ(policy_app_url_, registrar.GetAppStartUrl(*app_id));
  EXPECT_EQ(fallback_app_name_, registrar.GetAppShortName(*app_id));

  std::unique_ptr<web_app::ExternallyInstalledWebAppPrefs>
      externally_installed_app_prefs =
          std::make_unique<web_app::ExternallyInstalledWebAppPrefs>(
              browser()->profile()->GetPrefs());
  ASSERT_TRUE(
      externally_installed_app_prefs->LookupPlaceholderAppId(policy_app_url_)
          .has_value());
}

// Fixture for tests that have two profiles with a different policy for each.
class ExtensionPolicyTest2Contexts : public PolicyTest {
 public:
  ExtensionPolicyTest2Contexts() = default;
  ExtensionPolicyTest2Contexts(const ExtensionPolicyTest2Contexts& other) =
      delete;
  ~ExtensionPolicyTest2Contexts() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
    PolicyTest::SetUpCommandLine(command_line);
  }

  void SetUp() override {
    PolicyTest::SetUp();
    test_extension_cache1_ = std::make_unique<extensions::ExtensionCacheFake>();
    test_extension_cache2_ = std::make_unique<extensions::ExtensionCacheFake>();
  }

  void TearDown() override {
    test_extension_cache1_.reset();
    test_extension_cache2_.reset();
    PolicyTest::TearDown();
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    ON_CALL(profile1_policy_, IsInitializationComplete(testing::_))
        .WillByDefault(testing::Return(true));
    ON_CALL(profile1_policy_, IsFirstPolicyLoadComplete(testing::_))
        .WillByDefault(testing::Return(true));
    policy::PushProfilePolicyConnectorProviderForTesting(&profile1_policy_);
  }

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    profile1_ = browser()->profile();

    profile2_ = CreateProfile(&profile2_policy_);

    service1_ = CreateExtensionService(profile1_);
    service2_ = CreateExtensionService(profile2_);
    service1_->updater()->SetExtensionCacheForTesting(
        test_extension_cache1_.get());
    service2_->updater()->SetExtensionCacheForTesting(
        test_extension_cache2_.get());
    registry1_ = CreateExtensionRegistry(profile1_);
    registry2_ = CreateExtensionRegistry(profile2_);
  }

 protected:
  const extensions::Extension* InstallExtension(
      const base::FilePath::StringType& path,
      const base::FilePath::StringType& name,
      content::BrowserContext* browser_context,
      extensions::ExtensionService* extension_service,
      extensions::ExtensionRegistry* extension_registry) {
    base::FilePath extension_path(ui_test_utils::GetTestFilePath(
        base::FilePath(path), base::FilePath(name)));
    scoped_refptr<extensions::CrxInstaller> installer =
        extensions::CrxInstaller::CreateSilent(extension_service);
    installer->set_allow_silent_install(true);
    installer->set_install_cause(extension_misc::INSTALL_CAUSE_AUTOMATION);
    installer->set_creation_flags(extensions::Extension::FROM_WEBSTORE);
    installer->set_off_store_install_allow_reason(
        extensions::CrxInstaller::OffStoreInstallAllowReason::
            OffStoreInstallAllowedInTest);

    extensions::ChromeExtensionTestNotificationObserver observer(
        browser_context);
    observer.Watch(extensions::NOTIFICATION_CRX_INSTALLER_DONE,
                   content::Source<extensions::CrxInstaller>(installer.get()));
    installer->InstallCrx(extension_path);
    observer.Wait();
    if (!observer.WaitForExtensionViewsToLoad())
      return nullptr;
    return extension_registry->GetExtensionById(
        observer.last_loaded_extension_id(),
        extensions::ExtensionRegistry::ENABLED);
  }

  void SetTabSpecificPermissionsForURL(const extensions::Extension* extension,
                                       int tab_id,
                                       GURL& url,
                                       int url_scheme) {
    extensions::URLPatternSet new_hosts;
    new_hosts.AddOrigin(url_scheme, url);
    extension->permissions_data()->UpdateTabSpecificPermissions(
        1, extensions::PermissionSet(extensions::APIPermissionSet(),
                                     extensions::ManifestPermissionSet(),
                                     std::move(new_hosts),
                                     extensions::URLPatternSet()));
  }

  MockConfigurationPolicyProvider* GetProfile1Policy() {
    return &profile1_policy_;
  }
  MockConfigurationPolicyProvider* GetProfile2Policy() {
    return &profile2_policy_;
  }
  Profile* GetProfile1() { return browser()->profile(); }
  Profile* GetProfile2() { return profile2_; }
  Browser* GetBrowser1() { return browser(); }
  extensions::ExtensionRegistry* GetExtensionRegistry1() { return registry1_; }
  extensions::ExtensionRegistry* GetExtensionRegistry2() { return registry2_; }
  extensions::ExtensionService* GetExtensionService1() { return service1_; }
  extensions::ExtensionService* GetExtensionService2() { return service2_; }

 private:
  // Creates a Profile for testing. The Profile is returned.
  // The policy for the profile has to be passed via policy_for_profile.
  // This method is called from SetUp and only from there.
  Profile* CreateProfile(MockConfigurationPolicyProvider* policy_for_profile) {
    ON_CALL(*policy_for_profile, IsInitializationComplete(testing::_))
        .WillByDefault(testing::Return(true));
    ON_CALL(*policy_for_profile, IsFirstPolicyLoadComplete(testing::_))
        .WillByDefault(testing::Return(true));
    Profile* profile = nullptr;
    policy::PushProfilePolicyConnectorProviderForTesting(policy_for_profile);

    ProfileManager* profile_manager = g_browser_process->profile_manager();

    // Create an additional profile.
    base::FilePath path_profile =
        profile_manager->GenerateNextProfileDirectoryPath();
    base::RunLoop run_loop;
    profile_manager->CreateProfileAsync(
        path_profile, base::BindRepeating(&policy::OnProfileInitialized,
                                          &profile, &run_loop));

    // Run the message loop to allow profile creation to take place; the loop is
    // terminated by OnProfileInitialized calling the loop's QuitClosure when
    // the profile is created.
    run_loop.Run();
    return profile;
  }

  extensions::ExtensionService* CreateExtensionService(
      content::BrowserContext* context) {
    extensions::ExtensionSystem* system =
        extensions::ExtensionSystem::Get(context);
    return system->extension_service();
  }

  extensions::ExtensionRegistry* CreateExtensionRegistry(
      content::BrowserContext* context) {
    return extensions::ExtensionRegistry::Get(context);
  }

  std::unique_ptr<extensions::ExtensionCacheFake> test_extension_cache1_;
  std::unique_ptr<extensions::ExtensionCacheFake> test_extension_cache2_;
  extensions::ScopedIgnoreContentVerifierForTest ignore_content_verifier_;
  Profile* profile1_;
  Profile* profile2_;
  MockConfigurationPolicyProvider profile1_policy_;
  MockConfigurationPolicyProvider profile2_policy_;
  extensions::ExtensionRegistry* registry1_;
  extensions::ExtensionRegistry* registry2_;
  extensions::ExtensionService* service1_;
  extensions::ExtensionService* service2_;
};

// Verifies that default policy host block/allow settings are applied as
// expected.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest2Contexts,
                       ExtensionDefaultPolicyBlockedHost) {
  GURL test_url = GURL("http://www.google.com");
  std::string* error = nullptr;
  int tab_id = 1;

  const extensions::Extension* app1 =
      InstallExtension(kTestExtensionsDir, kGoodCrxName, GetProfile1(),
                       GetExtensionService1(), GetExtensionRegistry1());
  ASSERT_TRUE(app1);
  const extensions::Extension* app2 =
      InstallExtension(kTestExtensionsDir, kGoodCrxName, GetProfile2(),
                       GetExtensionService2(), GetExtensionRegistry2());
  ASSERT_TRUE(app2);
  SetTabSpecificPermissionsForURL(app1, tab_id, test_url,
                                  URLPattern::SCHEME_ALL);
  SetTabSpecificPermissionsForURL(app2, tab_id, test_url,
                                  URLPattern::SCHEME_ALL);

  ASSERT_TRUE(GetExtensionService1()->IsExtensionEnabled(app1->id()));
  ASSERT_TRUE(GetExtensionService2()->IsExtensionEnabled(app2->id()));

  ASSERT_TRUE(app1->permissions_data()->CanAccessPage(test_url, tab_id, error));
  ASSERT_TRUE(app2->permissions_data()->CanAccessPage(test_url, tab_id, error));

  {
    extensions::ExtensionManagementPolicyUpdater pref(GetProfile1Policy());
    pref.AddPolicyBlockedHost("*", "*://*.google.com");
  }

  EXPECT_FALSE(
      app1->permissions_data()->CanAccessPage(test_url, tab_id, error));
  EXPECT_TRUE(app2->permissions_data()->CanAccessPage(test_url, tab_id, error));
}

}  // namespace policy
