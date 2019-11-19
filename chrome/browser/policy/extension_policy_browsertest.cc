// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_management_constants.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/forced_extensions/installation_reporter.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/policy_constants.h"
#include "components/version_info/channel.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/scoped_ignore_content_verifier_for_test.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/updater/extension_cache_fake.h"
#include "extensions/common/constants.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/value_builder.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

using testing::AtLeast;
using testing::Sequence;

namespace policy {

namespace {

const base::FilePath::CharType kGoodCrxName[] = FILE_PATH_LITERAL("good.crx");
const base::FilePath::CharType kSimpleWithIconCrxName[] =
    FILE_PATH_LITERAL("simple_with_icon.crx");
const base::FilePath::CharType kHostedAppCrxName[] =
    FILE_PATH_LITERAL("hosted_app.crx");

const char kGoodCrxId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char kSimpleWithIconCrxId[] = "dehdlahnlebladnfleagmjdapdjdcnlp";
const char kHostedAppCrxId[] = "kbmnembihfiondgfjekmnmcbddelicoi";

const base::FilePath::CharType kGoodV1CrxName[] =
    FILE_PATH_LITERAL("good_v1.crx");
const base::FilePath::CharType kSimpleWithPopupExt[] =
    FILE_PATH_LITERAL("simple_with_popup");
const base::FilePath::CharType kAppUnpackedExt[] = FILE_PATH_LITERAL("app");

// Registers a handler to respond to requests whose path matches |match_path|.
// The response contents are generated from |template_file|, by replacing all
// "${URL_PLACEHOLDER}" substrings in the file with the request URL excluding
// filename, query values and fragment.
void RegisterURLReplacingHandler(net::EmbeddedTestServer* test_server,
                                 const std::string& match_path,
                                 const base::FilePath& template_file) {
  test_server->RegisterRequestHandler(base::Bind(
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
 protected:
  void SetUp() override {
    PolicyTest::SetUp();
    test_extension_cache_ = std::make_unique<extensions::ExtensionCacheFake>();
  }

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    if (extension_service()->updater()) {
      extension_service()->updater()->SetExtensionCacheForTesting(
          test_extension_cache_.get());
    }
  }

  extensions::ExtensionService* extension_service() {
    extensions::ExtensionSystem* system =
        extensions::ExtensionSystem::Get(browser()->profile());
    return system->extension_service();
  }

  extensions::ExtensionRegistry* extension_registry() {
    return extensions::ExtensionRegistry::Get(browser()->profile());
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
    web_app.title = base::ASCIIToUTF16("Bookmark App");
    web_app.app_url = GURL("http://www.google.com");

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

  void UninstallExtension(const std::string& id, bool expect_success) {
    if (expect_success) {
      extensions::TestExtensionRegistryObserver observer(extension_registry());
      extension_service()->UninstallExtension(
          id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
      observer.WaitForExtensionUninstalled();
    } else {
      content::WindowedNotificationObserver observer(
          extensions::NOTIFICATION_EXTENSION_UNINSTALL_NOT_ALLOWED,
          content::NotificationService::AllSources());
      extension_service()->UninstallExtension(
          id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
      observer.Wait();
    }
  }

  void DisableExtension(const std::string& id) {
    extensions::TestExtensionRegistryObserver observer(extension_registry());
    extension_service()->DisableExtension(
        id, extensions::disable_reason::DISABLE_USER_ACTION);
    observer.WaitForExtensionUnloaded();
  }

  std::unique_ptr<extensions::ExtensionCacheFake> test_extension_cache_;
  extensions::ScopedIgnoreContentVerifierForTest ignore_content_verifier_;
  extensions::ExtensionUpdater::ScopedSkipScheduledCheckForTest
      skip_scheduled_extension_checks_;
};

}  // namespace

#if defined(OS_CHROMEOS)
// Check that component extension can't be blacklisted, besides the camera app
// that can be disabled by extension policy. This is a temporary solution until
// there's a dedicated policy to disable the camera, at which point the special
// check should be removed.
// TODO(http://crbug.com/1002935)
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       ExtensionInstallBlacklistComponentApps) {
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(browser()->profile());

  // Load all component extensions.
  extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  extension_service()->component_loader()->AddDefaultComponentExtensions(false);
  base::RunLoop().RunUntilIdle();

  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_TRUE(
      registry->enabled_extensions().GetByID(extension_misc::kCameraAppId));
  ASSERT_TRUE(
      registry->enabled_extensions().GetByID(extensions::kWebStoreAppId));
  const size_t enabled_count = registry->enabled_extensions().size();

  // Verify that only Camera app can be blacklisted.
  base::ListValue blacklist;
  blacklist.AppendString(extension_misc::kCameraAppId);
  blacklist.AppendString(extensions::kWebStoreAppId);
  PolicyMap policies;
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               blacklist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);

  ASSERT_FALSE(
      registry->enabled_extensions().GetByID(extension_misc::kCameraAppId));
  ASSERT_TRUE(
      registry->disabled_extensions().GetByID(extension_misc::kCameraAppId));
  EXPECT_EQ(1u, registry->disabled_extensions().size());
  EXPECT_EQ(extensions::disable_reason::DISABLE_BLOCKED_BY_POLICY,
            extension_prefs->GetDisableReasons(extension_misc::kCameraAppId));
  ASSERT_TRUE(
      registry->enabled_extensions().GetByID(extensions::kWebStoreAppId));
  EXPECT_EQ(enabled_count - 1, registry->enabled_extensions().size());
}
#endif  // defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       ExtensionInstallBlacklistSelective) {
  // Verifies that blacklisted extensions can't be installed.
  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));
  ASSERT_FALSE(registry->GetExtensionById(
      kSimpleWithIconCrxId, extensions::ExtensionRegistry::EVERYTHING));
  base::ListValue blacklist;
  blacklist.AppendString(kGoodCrxId);
  PolicyMap policies;
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               blacklist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);

  // "good.crx" is blacklisted.
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

// Ensure that bookmark apps are not blocked by the ExtensionInstallBlacklist
// policy.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       ExtensionInstallBlacklist_BookmarkApp) {
  const extensions::Extension* bookmark_app = InstallBookmarkApp();
  ASSERT_TRUE(bookmark_app);
  EXPECT_TRUE(InstallExtension(kGoodCrxName));

  extensions::ExtensionService* service = extension_service();
  EXPECT_TRUE(service->IsExtensionEnabled(kGoodCrxId));
  EXPECT_TRUE(service->IsExtensionEnabled(bookmark_app->id()));

  // Now set ExtensionInstallBlacklist policy to block all extensions.
  PolicyMap policies;
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               extensions::ListBuilder().Append("*").Build(), nullptr);
  UpdateProviderPolicy(policies);

  // The bookmark app should still be enabled, with |kGoodCrxId| being disabled.
  EXPECT_FALSE(service->IsExtensionEnabled(kGoodCrxId));
  EXPECT_TRUE(service->IsExtensionEnabled(bookmark_app->id()));
}

// Ensure that when INSTALLATION_REMOVED is set
// that blacklisted extensions are removed from the device.
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
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               dict_value.CreateDeepCopy(), nullptr);
  extensions::TestExtensionRegistryObserver observer(registry);
  UpdateProviderPolicy(policies);
  observer.WaitForExtensionUnloaded();

  EXPECT_FALSE(registry->GetInstalledExtension(kGoodCrxId));
}

// Ensure that when INSTALLATION_REMOVED is set for wildcard
// that blacklisted extensions are removed from the device.
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
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               dict_value.CreateDeepCopy(), nullptr);
  extensions::TestExtensionRegistryObserver observer(registry);
  UpdateProviderPolicy(policies);
  observer.WaitForExtensionUnloaded();

  EXPECT_FALSE(registry->GetInstalledExtension(kGoodCrxId));
}

// Ensure that bookmark apps are not blocked by the ExtensionAllowedTypes
// policy.
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
  policies.Set(key::kExtensionAllowedTypes, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               extensions::ListBuilder().Append("theme").Build(), nullptr);
  UpdateProviderPolicy(policies);

  // The bookmark app should still be enabled, with |kGoodCrxId| being disabled.
  EXPECT_FALSE(service->IsExtensionEnabled(kGoodCrxId));
  EXPECT_TRUE(service->IsExtensionEnabled(bookmark_app->id()));
}

// Ensure that bookmark apps are not blocked by the ExtensionSettings
// policy.
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest, ExtensionSettings_BookmarkApp) {
  const extensions::Extension* bookmark_app = InstallBookmarkApp();
  ASSERT_TRUE(bookmark_app);
  EXPECT_TRUE(InstallExtension(kGoodCrxName));

  extensions::ExtensionService* service = extension_service();
  EXPECT_TRUE(service->IsExtensionEnabled(kGoodCrxId));
  EXPECT_TRUE(service->IsExtensionEnabled(bookmark_app->id()));

  // Now set policy to block all extensions.
  PolicyMap policies;
  policies.Set(key::kExtensionSettings, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               extensions::DictionaryBuilder()
                   .Set("*", extensions::DictionaryBuilder()
                                 .Set("installation_mode", "blocked")
                                 .Build())
                   .Build(),
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
  policies.Set(
      key::kExtensionSettings, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD,
      extensions::DictionaryBuilder()
          .Set("*", extensions::DictionaryBuilder()
                        .Set("allowed_types",
                             extensions::ListBuilder().Append("theme").Build())
                        .Build())
          .Build(),
      nullptr);
  UpdateProviderPolicy(policies);

  // The bookmark app should still be enabled, with |kGoodCrxId| being disabled.
  EXPECT_FALSE(service->IsExtensionEnabled(kGoodCrxId));
  EXPECT_TRUE(service->IsExtensionEnabled(bookmark_app->id()));
}

// Flaky on windows; http://crbug.com/307994.
#if defined(OS_WIN)
#define MAYBE_ExtensionInstallBlacklistWildcard \
  DISABLED_ExtensionInstallBlacklistWildcard
#else
#define MAYBE_ExtensionInstallBlacklistWildcard \
  ExtensionInstallBlacklistWildcard
#endif
IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       MAYBE_ExtensionInstallBlacklistWildcard) {
  // Verify that a wildcard blacklist takes effect.
  EXPECT_TRUE(InstallExtension(kSimpleWithIconCrxName));
  extensions::ExtensionService* service = extension_service();
  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));
  ASSERT_TRUE(registry->enabled_extensions().GetByID(kSimpleWithIconCrxId));
  base::ListValue blacklist;
  blacklist.AppendString("*");
  PolicyMap policies;
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               blacklist.CreateDeepCopy(), nullptr);
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
                       ExtensionInstallBlacklistSharedModules) {
  // Verifies that shared_modules are not affected by the blacklist.

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

  // Blacklist "*" but force-install the importer extension. The shared module
  // should be automatically installed too.
  base::ListValue blacklist;
  blacklist.AppendString("*");
  base::ListValue forcelist;
  forcelist.AppendString(
      base::StringPrintf("%s;%s", kImporterId, update_xml_url.spec().c_str()));
  PolicyMap policies;
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               blacklist.CreateDeepCopy(), nullptr);
  policies.Set(key::kExtensionInstallForcelist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               forcelist.CreateDeepCopy(), nullptr);

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

IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest, ExtensionInstallWhitelist) {
  // Verifies that the whitelist can open exceptions to the blacklist.
  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));
  ASSERT_FALSE(registry->GetExtensionById(
      kSimpleWithIconCrxId, extensions::ExtensionRegistry::EVERYTHING));
  base::ListValue blacklist;
  blacklist.AppendString("*");
  base::ListValue whitelist;
  whitelist.AppendString(kGoodCrxId);
  PolicyMap policies;
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               blacklist.CreateDeepCopy(), nullptr);
  policies.Set(key::kExtensionInstallWhitelist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               whitelist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);
  // "simple_with_icon.crx" is blacklisted.
  EXPECT_FALSE(InstallExtension(kSimpleWithIconCrxName));
  EXPECT_FALSE(registry->GetExtensionById(
      kSimpleWithIconCrxId, extensions::ExtensionRegistry::EVERYTHING));
  // "good.crx" has a whitelist exception.
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

    return false;
  }

  content::URLLoaderInterceptor::InterceptCallback callback_;
  content::URLLoaderInterceptor interceptor_;
};

class MockedInstallationReporterObserver
    : public extensions::InstallationReporter::TestObserver {
 public:
  explicit MockedInstallationReporterObserver(
      const content::BrowserContext* context)
      : context_(context) {}
  ~MockedInstallationReporterObserver() override = default;

  MOCK_METHOD1(ExtensionStageChanged,
               void(extensions::InstallationReporter::Stage));

  void OnExtensionDataChanged(
      const extensions::ExtensionId& id,
      const content::BrowserContext* context,
      const extensions::InstallationReporter::InstallationData& data) override {
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
  extensions::InstallationReporter::Stage stage_ =
      extensions::InstallationReporter::Stage::CREATED;
  const content::BrowserContext* context_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest, ExtensionInstallForcelist) {
  // Verifies that extensions that are force-installed by policies are
  // installed and can't be uninstalled.

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

  // Setting the forcelist extension should install "good_v1.crx".
  base::ListValue forcelist;
  forcelist.AppendString(
      base::StringPrintf("%s;%s", kGoodCrxId, url.spec().c_str()));
  PolicyMap policies;
  policies.Set(key::kExtensionInstallForcelist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               forcelist.CreateDeepCopy(), nullptr);
  extensions::TestExtensionRegistryObserver observer(extension_registry());
  MockedInstallationReporterObserver reporter_observer(browser()->profile());
  // CREATED is the default stage in MockedInstallationReporterObserver, so it
  // wouldn't be reported here.
  Sequence sequence;
  EXPECT_CALL(
      reporter_observer,
      ExtensionStageChanged(
          extensions::InstallationReporter::Stage::NOTIFIED_FROM_MANAGEMENT))
      .InSequence(sequence);
  EXPECT_CALL(
      reporter_observer,
      ExtensionStageChanged(
          extensions::InstallationReporter::Stage::SEEN_BY_POLICY_LOADER))
      .InSequence(sequence);
  EXPECT_CALL(
      reporter_observer,
      ExtensionStageChanged(
          extensions::InstallationReporter::Stage::SEEN_BY_EXTERNAL_PROVIDER))
      .InSequence(sequence);
  EXPECT_CALL(
      reporter_observer,
      ExtensionStageChanged(extensions::InstallationReporter::Stage::PENDING))
      .InSequence(sequence);
  EXPECT_CALL(reporter_observer,
              ExtensionStageChanged(
                  extensions::InstallationReporter::Stage::DOWNLOADING))
      .InSequence(sequence);
  EXPECT_CALL(reporter_observer,
              ExtensionStageChanged(
                  extensions::InstallationReporter::Stage::INSTALLING))
      .InSequence(sequence);
  EXPECT_CALL(
      reporter_observer,
      ExtensionStageChanged(extensions::InstallationReporter::Stage::COMPLETE))
      .InSequence(sequence);
  extensions::InstallationReporter::SetTestObserver(&reporter_observer);
  UpdateProviderPolicy(policies);
  observer.WaitForExtensionWillBeInstalled();
  extensions::InstallationReporter::SetTestObserver(nullptr);
  // Note: Cannot check that the notification details match the expected
  // exception, since the details object has already been freed prior to
  // the completion of observer.WaitForExtensionWillBeInstalled().

  EXPECT_TRUE(registry->enabled_extensions().GetByID(kGoodCrxId));

  // The user is not allowed to uninstall force-installed extensions.
  UninstallExtension(kGoodCrxId, false);

  scoped_refptr<extensions::UnpackedInstaller> installer =
      extensions::UnpackedInstaller::Create(extension_service());

  // The user is not allowed to load an unpacked extension with the
  // same ID as a force-installed extension.
  base::FilePath good_extension_path(ui_test_utils::GetTestFilePath(
      base::FilePath(kTestExtensionsDir), base::FilePath(kSimpleWithPopupExt)));
  content::WindowedNotificationObserver extension_load_error_observer(
      extensions::NOTIFICATION_EXTENSION_LOAD_ERROR,
      content::NotificationService::AllSources());
  installer->Load(good_extension_path);
  extension_load_error_observer.Wait();

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

IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       ExtensionInstallForcelist_DefaultedUpdateUrl) {
  // Verifies the ExtensionInstallForcelist policy with an empty (defaulted)
  // "update" URL.

  ExtensionRequestInterceptor interceptor;

  extensions::ExtensionRegistry* registry = extension_registry();
  ASSERT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));

  // Setting the forcelist extension should install "good_v1.crx".
  base::ListValue forcelist;
  forcelist.AppendString(kGoodCrxId);
  PolicyMap policies;
  policies.Set(key::kExtensionInstallForcelist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               forcelist.CreateDeepCopy(), nullptr);
  extensions::TestExtensionRegistryObserver observer(registry);
  UpdateProviderPolicy(policies);
  observer.WaitForExtensionWillBeInstalled();

  EXPECT_TRUE(registry->enabled_extensions().GetByID(kGoodCrxId));
}

IN_PROC_BROWSER_TEST_F(ExtensionPolicyTest,
                       ExtensionRecommendedInstallationMode) {
  // Verifies that extensions that are recommended-installed by policies are
  // installed, can be disabled but not uninstalled.

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
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               dict_value.CreateDeepCopy(), nullptr);
  extensions::TestExtensionRegistryObserver observer(registry);
  UpdateProviderPolicy(policies);
  observer.WaitForExtensionWillBeInstalled();

  // TODO(crbug.com/1006342): There is a race condition here where the extension
  // may or may not be enabled by the time we get here.
  EXPECT_TRUE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::ENABLED |
                      extensions::ExtensionRegistry::DISABLED));

  // The user is not allowed to uninstall recommended-installed extensions.
  UninstallExtension(kGoodCrxId, false);

  // Explicitly re-enables the extension.
  service->EnableExtension(kGoodCrxId);

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
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               allowed_types.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);

  // "good.crx" is blocked.
  EXPECT_FALSE(InstallExtension(kGoodCrxName));
  EXPECT_FALSE(registry->GetExtensionById(
      kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING));

  // "hosted_app.crx" is of a whitelisted type.
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
// whitelisted by policy.
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
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               install_sources.CreateDeepCopy(), nullptr);
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
    management_policy.SetMinimumVersionRequired(kGoodCrxId, "1.0.0.1");
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
  EXPECT_EQ("1.0.0.1",
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
  EXPECT_EQ("1.0.0.1",
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
    management_policy.SetMinimumVersionRequired(kGoodCrxId, "1.0.0.1");
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
  WebAppInstallForceListPolicyTest() {}
  ~WebAppInstallForceListPolicyTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionPolicyTest::SetUpInProcessBrowserTestFixture();
    ASSERT_TRUE(embedded_test_server()->Start());

    policy_app_url_ =
        embedded_test_server()->GetURL("/banners/manifest_test_page.html");
    base::Value url(policy_app_url_.spec());
    base::Value launch_container("window");

    base::Value item(base::Value::Type::DICTIONARY);
    item.SetKey("url", std::move(url));
    item.SetKey("default_launch_container", std::move(launch_container));

    base::Value list(base::Value::Type::LIST);
    list.Append(std::move(item));

    PolicyMap policies;
    SetPolicy(&policies, key::kWebAppInstallForceList,
              base::Value::ToUniquePtrValue(std::move(list)));
    provider_.UpdateChromePolicy(policies);
  }

 protected:
  GURL policy_app_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebAppInstallForceListPolicyTest);
};

IN_PROC_BROWSER_TEST_F(WebAppInstallForceListPolicyTest, StartUpInstallation) {
  extensions::TestExtensionRegistryObserver observer(extension_registry());
  const extensions::Extension* installed_extension =
      observer.WaitForExtensionWillBeInstalled();

  ASSERT_TRUE(installed_extension);
  const GURL installed_app_url =
      extensions::AppLaunchInfo::GetFullLaunchURL(installed_extension);
  EXPECT_EQ(policy_app_url_, installed_app_url);
}

}  // namespace policy
