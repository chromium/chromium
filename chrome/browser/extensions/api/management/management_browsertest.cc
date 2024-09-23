// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gmock/include/gmock/gmock.h"

using content::BrowserThread;
using extensions::Extension;
using extensions::ExtensionRegistry;
using extensions::ExtensionService;
using extensions::Manifest;
using extensions::mojom::ManifestLocation;
using policy::PolicyMap;
using testing::_;
using testing::Return;

namespace {

std::string BuildForceInstallPolicyValue(const char* extension_id,
                                         const char* update_url) {
  return base::StringPrintf("%s;%s", extension_id, update_url);
}

}  // namespace

class ExtensionManagementTest : public extensions::ExtensionBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

 protected:
  void UpdateProviderPolicy(const PolicyMap& policy) {
    policy_provider_.UpdateChromePolicy(policy);
    base::RunLoop().RunUntilIdle();
  }

  GURL GetUpdateUrl() {
    return embedded_test_server()->GetURL("/autoupdate/manifest");
  }

  // Prepares a CRX file for serving by EmbeddedTestServer. This includes
  // taking the unpacked extension files from |source_dir_name| in the test
  // data tree, modifying them to use the EmbeddedTestServer's URLs for
  // extension updates, packing the extension into a CRX file named |crx_name|
  // and putting it in |temp_dir|. The full path to the CRX file created is
  // returned in |crx_path|.
  void SetUpExtensionUpdatePackage(const base::FilePath& temp_dir,
                                   const std::string& source_dir_name,
                                   const std::string& crx_name,
                                   base::FilePath* crx_path) {
    ASSERT_TRUE(base::CreateDirectory(temp_dir.AppendASCII("unpacked")));

    const base::FilePath basedir = test_data_dir_.AppendASCII("autoupdate");
    ASSERT_TRUE(base::CopyDirectory(basedir.AppendASCII(source_dir_name),
                                    temp_dir.AppendASCII("unpacked"),
                                    /*recursive=*/false));

    std::string manifest_template;
    ASSERT_TRUE(
        base::ReadFileToString(basedir.AppendASCII(source_dir_name)
                                   .AppendASCII("manifest.json.template"),
                               &manifest_template));
    const std::string manifest = base::ReplaceStringPlaceholders(
        manifest_template, {GetUpdateUrl().spec()}, nullptr);
    ASSERT_TRUE(base::WriteFile(
        temp_dir.AppendASCII("unpacked").AppendASCII("manifest.json"),
        manifest));

    ASSERT_TRUE(base::CreateDirectory(temp_dir.AppendASCII("autoupdate")));
    *crx_path = PackExtensionWithOptions(
        temp_dir.AppendASCII("unpacked"),
        temp_dir.AppendASCII("autoupdate").AppendASCII(crx_name),
        basedir.AppendASCII("key.pem"), base::FilePath());
  }

  // Sets up a file to be served by EmbeddedTestServer in response to a
  // "/autoupdate/manifest" request. The response template resides in a file
  // named |manifest_template_name| in the test data tree. An
  // EmbeddedTestServer's URL pointing to |crx_name| inside |temp_dir| is
  // inserted into the template.
  void SetUpExtensionUpdateResponse(const base::FilePath& temp_dir,
                                    const std::string& crx_name,
                                    const std::string& manifest_template_name) {
    std::string manifest_template;
    ASSERT_TRUE(base::ReadFileToString(test_data_dir_.AppendASCII("autoupdate")
                                           .AppendASCII(manifest_template_name),
                                       &manifest_template));
    const GURL crx_url = embedded_test_server()->GetURL(
        base::StrCat({"/autoupdate/", crx_name}));
    const std::string manifest = base::ReplaceStringPlaceholders(
        manifest_template, {crx_url.spec()}, nullptr);
    ASSERT_TRUE(base::CreateDirectory(temp_dir.AppendASCII("autoupdate")));
    ASSERT_TRUE(
        base::WriteFile(temp_dir.AppendASCII("autoupdate/manifest"), manifest));
  }

  // Helper method that returns whether the extension is at the given version.
  // This calls version(), which must be defined in the extension's bg page,
  // as well as asking the extension itself.
  //
  // Note that 'version' here means something different than the version field
  // in the extension's manifest. We use the version as reported by the
  // background page to test how overinstalling crx files with the same
  // manifest version works.
  bool IsExtensionAtVersion(const Extension* extension,
                            const std::string& expected_version) {
    // Test that the extension's version from the manifest and reported by the
    // background page is correct.  This is to ensure that the processes are in
    // sync with the Extension.
    extensions::ProcessManager* manager =
        extensions::ProcessManager::Get(browser()->profile());
    extensions::ExtensionHost* ext_host =
        manager->GetBackgroundHostForExtension(extension->id());
    EXPECT_TRUE(ext_host);
    if (!ext_host)
      return false;

    std::string version_from_bg =
        content::EvalJs(ext_host->host_contents(), "version()").ExtractString();

    if (version_from_bg != expected_version ||
        extension->VersionString() != expected_version)
      return false;
    return true;
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  extensions::ScopedInstallVerifierBypassForTest install_verifier_bypass_;
};

// Tests that installing the same version overwrites.
IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, InstallSameVersion) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath root_path = test_data_dir_.AppendASCII("install");
  base::FilePath pem_path = root_path.AppendASCII("install.pem");
  base::FilePath first_path =
      PackExtensionWithOptions(root_path.AppendASCII("install"),
                               temp_dir.GetPath().AppendASCII("install.crx"),
                               pem_path, base::FilePath());
  base::FilePath second_path = PackExtensionWithOptions(
      root_path.AppendASCII("install_same_version"),
      temp_dir.GetPath().AppendASCII("install_same_version.crx"), pem_path,
      base::FilePath());

  const Extension* extension = InstallExtension(first_path, 1);
  ASSERT_TRUE(extension);
  base::FilePath old_path = extension->path();

  const extensions::ExtensionId extension_id = extension->id();
  {
    // Set up two observers: One to wait for the existing background page to be
    // destroyed, and a second to wait for a new one to load.
    extensions::ExtensionHost* background_host =
        extensions::ProcessManager::Get(profile())
            ->GetBackgroundHostForExtension(extension_id);
    ASSERT_TRUE(background_host);
    extensions::ExtensionHostTestHelper destruction_observer(profile());
    destruction_observer.RestrictToHost(background_host);

    extensions::ExtensionHostTestHelper first_load_observer(profile(),
                                                            extension_id);
    first_load_observer.RestrictToType(
        extensions::mojom::ViewType::kExtensionBackgroundPage);

    // Install an extension with the same version. The previous install should
    // be overwritten.
    extension = InstallExtension(second_path, 0);
    ASSERT_TRUE(extension);

    // Wait for the old ExtensionHost destruction first before waiting for the
    // new one to load.
    // Note that this is needed to ensure that |IsExtensionAtVersion| below can
    // successfully execute JS, otherwise this test becomes flaky.
    destruction_observer.WaitForHostDestroyed();
    first_load_observer.WaitForHostCompletedFirstLoad();
  }
  base::FilePath new_path = extension->path();

  EXPECT_FALSE(IsExtensionAtVersion(extension, "1.0"));
  EXPECT_NE(old_path.value(), new_path.value());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, InstallOlderVersion) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath root_path = test_data_dir_.AppendASCII("install");
  base::FilePath pem_path = root_path.AppendASCII("install.pem");
  base::FilePath modern_path =
      PackExtensionWithOptions(root_path.AppendASCII("install"),
                               temp_dir.GetPath().AppendASCII("install.crx"),
                               pem_path, base::FilePath());
  base::FilePath older_path = PackExtensionWithOptions(
      root_path.AppendASCII("install_older_version"),
      temp_dir.GetPath().AppendASCII("install_older_version.crx"), pem_path,
      base::FilePath());

  const Extension* extension = InstallExtension(modern_path, 1);
  ASSERT_TRUE(extension);
  ASSERT_FALSE(InstallExtension(older_path, 0));
  EXPECT_TRUE(IsExtensionAtVersion(extension, "1.0"));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, InstallThenCancel) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath root_path = test_data_dir_.AppendASCII("install");
  base::FilePath pem_path = root_path.AppendASCII("install.pem");
  base::FilePath v1_path =
      PackExtensionWithOptions(root_path.AppendASCII("install"),
                               temp_dir.GetPath().AppendASCII("install.crx"),
                               pem_path, base::FilePath());
  base::FilePath v2_path =
      PackExtensionWithOptions(root_path.AppendASCII("install_v2"),
                               temp_dir.GetPath().AppendASCII("install_v2.crx"),
                               pem_path, base::FilePath());

  const Extension* extension = InstallExtension(v1_path, 1);
  ASSERT_TRUE(extension);

  // Cancel this install.
  ASSERT_FALSE(StartInstallButCancel(v2_path));
  EXPECT_TRUE(IsExtensionAtVersion(extension, "1.0"));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, InstallRequiresConfirm) {
  // Installing the extension without an auto confirming UI should result in
  // it being disabled, since good.crx has permissions that require approval.
  std::string id = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
  ASSERT_FALSE(InstallExtension(test_data_dir_.AppendASCII("good.crx"), 0));
  ASSERT_TRUE(extension_registry()->disabled_extensions().GetByID(id));
  UninstallExtension(id);

  // And the install should succeed when the permissions are accepted.
  ASSERT_TRUE(InstallExtensionWithUIAutoConfirm(
      test_data_dir_.AppendASCII("good.crx"), 1, browser()));
  UninstallExtension(id);
}

// Tests that disabling and re-enabling an extension works.
IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, DisableEnable) {
  extensions::ProcessManager* manager =
      extensions::ProcessManager::Get(browser()->profile());
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  const size_t size_before = registry->enabled_extensions().size();

  // Load an extension, expect the background page to be available.
  std::string extension_id = "bjafgdebaacbbbecmhlhpofkepfkgcpa";
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII(extension_id)
                    .AppendASCII("1.0")));
  ASSERT_EQ(size_before + 1, registry->enabled_extensions().size());
  EXPECT_EQ(0u, registry->disabled_extensions().size());
  EXPECT_TRUE(manager->GetBackgroundHostForExtension(extension_id));

  // After disabling, the background page should go away.
  DisableExtension(extension_id);
  EXPECT_EQ(size_before, registry->enabled_extensions().size());
  EXPECT_EQ(1u, registry->disabled_extensions().size());
  EXPECT_FALSE(manager->GetBackgroundHostForExtension(extension_id));

  // And bring it back.
  EnableExtension(extension_id);
  EXPECT_EQ(size_before + 1, registry->enabled_extensions().size());
  EXPECT_EQ(0u, registry->disabled_extensions().size());
  EXPECT_TRUE(manager->GetBackgroundHostForExtension(extension_id));
}

#if BUILDFLAG(IS_WIN)
// Fails consistently on Windows XP, see: http://crbug.com/120640.
#define MAYBE_AutoUpdate DISABLED_AutoUpdate
#else
// See http://crbug.com/103371 and http://crbug.com/120640.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_AutoUpdate DISABLED_AutoUpdate
#else
#define MAYBE_AutoUpdate AutoUpdate
#endif
#endif

// Tests extension autoupdate.
IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, MAYBE_AutoUpdate) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  embedded_test_server()->ServeFilesFromDirectory(temp_dir.GetPath());
  ASSERT_TRUE(embedded_test_server()->Start());

  base::FilePath crx_v1_path;
  ASSERT_NO_FATAL_FAILURE(SetUpExtensionUpdatePackage(temp_dir.GetPath(), "v1",
                                                      "v1.crx", &crx_v1_path));

  base::FilePath crx_v2_path;
  ASSERT_NO_FATAL_FAILURE(SetUpExtensionUpdatePackage(temp_dir.GetPath(), "v2",
                                                      "v2.crx", &crx_v2_path));
  ASSERT_NO_FATAL_FAILURE(SetUpExtensionUpdateResponse(
      temp_dir.GetPath(), "v2.crx", "manifest_v2.xml.template"));

  // Install version 1 of the extension.
  ExtensionTestMessageListener listener1("v1 installed");
  ExtensionService* service = extension_service();
  ExtensionRegistry* registry = extension_registry();
  const size_t size_before = registry->enabled_extensions().size();
  EXPECT_TRUE(registry->disabled_extensions().empty());
  const Extension* extension = InstallExtension(crx_v1_path, 1);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(listener1.WaitUntilSatisfied());
  ASSERT_EQ(size_before + 1, registry->enabled_extensions().size());
  ASSERT_EQ("ogjcoiohnmldgjemafoockdghcjciccf", extension->id());
  ASSERT_EQ("1.0", extension->VersionString());

  // Run autoupdate and make sure version 2 of the extension was installed.
  ExtensionTestMessageListener listener2("v2 installed");

  {
    extensions::TestExtensionRegistryObserver install_observer(registry);
    extensions::ExtensionUpdater::CheckParams params1;
    bool install_finished = false;
    std::set<std::string> updates;
    params1.update_found_callback = base::BindLambdaForTesting(
        [&updates](const std::string& id, const base::Version&) {
          updates.insert(id);
        });
    params1.callback = base::BindLambdaForTesting(
        [&install_finished]() { install_finished = true; });
    service->updater()->CheckNow(std::move(params1));
    install_observer.WaitForExtensionWillBeInstalled();
    EXPECT_TRUE(listener2.WaitUntilSatisfied());
    ASSERT_EQ(size_before + 1, registry->enabled_extensions().size());
    extension = registry->enabled_extensions().GetByID(
        "ogjcoiohnmldgjemafoockdghcjciccf");
    ASSERT_TRUE(extension);
    ASSERT_EQ("2.0", extension->VersionString());
    ASSERT_TRUE(install_finished);
    ASSERT_TRUE(base::Contains(updates, "ogjcoiohnmldgjemafoockdghcjciccf"));
  }

  // Now try doing an update to version 3, which has been incorrectly
  // signed. This should fail.

  ASSERT_TRUE(base::CopyFile(
      test_data_dir_.AppendASCII("autoupdate").AppendASCII("v3.crx"),
      temp_dir.GetPath().AppendASCII("autoupdate").AppendASCII("v3.crx")));
  ASSERT_NO_FATAL_FAILURE(SetUpExtensionUpdateResponse(
      temp_dir.GetPath(), "v3.crx", "manifest_v3.xml.template"));

  {
    extensions::ExtensionUpdater::CheckParams params2;
    base::RunLoop run_loop;
    std::set<std::string> updates;
    params2.update_found_callback = base::BindLambdaForTesting(
        [&updates](const std::string& id, const base::Version&) {
          updates.insert(id);
        });
    params2.callback = run_loop.QuitClosure();
    service->updater()->CheckNow(std::move(params2));
    run_loop.Run();
    ASSERT_TRUE(base::Contains(updates, "ogjcoiohnmldgjemafoockdghcjciccf"));
  }

  // Make sure the extension state is the same as before.
  ASSERT_EQ(size_before + 1, registry->enabled_extensions().size());
  extension = registry->enabled_extensions().GetByID(
      "ogjcoiohnmldgjemafoockdghcjciccf");
  ASSERT_TRUE(extension);
  ASSERT_EQ("2.0", extension->VersionString());
}

#if BUILDFLAG(IS_WIN)
// Fails consistently on Windows XP, see: http://crbug.com/120640.
#define MAYBE_AutoUpdateDisabledExtensions DISABLED_AutoUpdateDisabledExtensions
#else
#if defined(ADDRESS_SANITIZER)
#define MAYBE_AutoUpdateDisabledExtensions DISABLED_AutoUpdateDisabledExtensions
#else
#define MAYBE_AutoUpdateDisabledExtensions AutoUpdateDisabledExtensions
#endif
#endif

// Tests extension autoupdate.
IN_PROC_BROWSER_TEST_F(ExtensionManagementTest,
                       MAYBE_AutoUpdateDisabledExtensions) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  embedded_test_server()->ServeFilesFromDirectory(temp_dir.GetPath());
  ASSERT_TRUE(embedded_test_server()->Start());

  base::FilePath crx_v1_path;
  ASSERT_NO_FATAL_FAILURE(SetUpExtensionUpdatePackage(temp_dir.GetPath(), "v1",
                                                      "v1.crx", &crx_v1_path));
  base::FilePath crx_v2_path;
  ASSERT_NO_FATAL_FAILURE(SetUpExtensionUpdatePackage(temp_dir.GetPath(), "v2",
                                                      "v2.crx", &crx_v2_path));
  ASSERT_NO_FATAL_FAILURE(SetUpExtensionUpdateResponse(
      temp_dir.GetPath(), "v2.crx", "manifest_v2.xml.template"));

  // Install version 1 of the extension.
  ExtensionTestMessageListener listener1("v1 installed");
  ExtensionService* service = extension_service();
  ExtensionRegistry* registry = extension_registry();
  const size_t enabled_size_before = registry->enabled_extensions().size();
  const size_t disabled_size_before = registry->disabled_extensions().size();
  const Extension* extension = InstallExtension(crx_v1_path, 1);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(listener1.WaitUntilSatisfied());
  DisableExtension(extension->id());
  ASSERT_EQ(disabled_size_before + 1, registry->disabled_extensions().size());
  ASSERT_EQ(enabled_size_before, registry->enabled_extensions().size());
  ASSERT_EQ("ogjcoiohnmldgjemafoockdghcjciccf", extension->id());
  ASSERT_EQ("1.0", extension->VersionString());

  ExtensionTestMessageListener listener2("v2 installed");
  extensions::TestExtensionRegistryObserver install_observer(registry);
  // Run autoupdate and make sure version 2 of the extension was installed but
  // is still disabled.
  bool install_finished = false;
  std::set<std::string> updates;
  extensions::ExtensionUpdater::CheckParams params;
  params.update_found_callback = base::BindLambdaForTesting(
      [&updates](const std::string& id, const base::Version&) {
        updates.insert(id);
      });
  params.callback = base::BindLambdaForTesting(
      [&install_finished]() { install_finished = true; });
  service->updater()->CheckNow(std::move(params));
  install_observer.WaitForExtensionWillBeInstalled();
  ASSERT_EQ(disabled_size_before + 1, registry->disabled_extensions().size());
  ASSERT_EQ(enabled_size_before, registry->enabled_extensions().size());
  extension = registry->disabled_extensions().GetByID(
      "ogjcoiohnmldgjemafoockdghcjciccf");
  ASSERT_TRUE(extension);
  ASSERT_FALSE(registry->enabled_extensions().GetByID(
      "ogjcoiohnmldgjemafoockdghcjciccf"));
  ASSERT_EQ("2.0", extension->VersionString());

  // The extension should have not made the callback because it is disabled.
  // When we enabled it, it should then make the callback.
  ASSERT_FALSE(listener2.was_satisfied());
  EnableExtension(extension->id());
  EXPECT_TRUE(listener2.WaitUntilSatisfied());
  ASSERT_TRUE(install_finished);
  ASSERT_TRUE(base::Contains(updates, "ogjcoiohnmldgjemafoockdghcjciccf"));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, ExternalUrlUpdate) {
  ExtensionService* service = extension_service();
  const char kExtensionId[] = "ogjcoiohnmldgjemafoockdghcjciccf";

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  embedded_test_server()->ServeFilesFromDirectory(temp_dir.GetPath());
  ASSERT_TRUE(embedded_test_server()->Start());

  base::FilePath crx_path;
  ASSERT_NO_FATAL_FAILURE(SetUpExtensionUpdatePackage(temp_dir.GetPath(), "v2",
                                                      "v2.crx", &crx_path));
  ASSERT_NO_FATAL_FAILURE(SetUpExtensionUpdateResponse(
      temp_dir.GetPath(), "v2.crx", "manifest_v2.xml.template"));

  ExtensionRegistry* registry = extension_registry();
  const size_t size_before = registry->enabled_extensions().size();
  EXPECT_TRUE(registry->disabled_extensions().empty());

  extensions::PendingExtensionManager* pending_extension_manager =
      service->pending_extension_manager();

  // The code that reads external_extensions.json uses this method to inform
  // the extensions::ExtensionService of an extension to download.  Using the
  // real code is race-prone, because instantating the
  // extensions::ExtensionService starts a read of external_extensions.json
  // before this test function starts.

  EXPECT_TRUE(pending_extension_manager->AddFromExternalUpdateUrl(
      kExtensionId, std::string(), GetUpdateUrl(),
      ManifestLocation::kExternalPrefDownload, Extension::NO_FLAGS, false));

  extensions::TestExtensionRegistryObserver install_observer(registry);
  // Run autoupdate and make sure version 2 of the extension was installed.
  service->updater()->CheckNow(extensions::ExtensionUpdater::CheckParams());
  install_observer.WaitForExtensionWillBeInstalled();
  ASSERT_EQ(size_before + 1, registry->enabled_extensions().size());
  const Extension* extension =
      registry->enabled_extensions().GetByID(kExtensionId);
  ASSERT_TRUE(extension);
  ASSERT_EQ("2.0", extension->VersionString());

  // Uninstalling the extension should set a pref that keeps the extension from
  // being installed again the next time external_extensions.json is read.

  UninstallExtension(kExtensionId);

  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(browser()->profile());
  EXPECT_TRUE(extension_prefs->IsExternalExtensionUninstalled(kExtensionId))
      << "Uninstalling should set kill bit on externaly installed extension.";

  // Try to install the extension again from an external source. It should fail
  // because of the killbit.
  EXPECT_FALSE(pending_extension_manager->AddFromExternalUpdateUrl(
      kExtensionId, std::string(), GetUpdateUrl(),
      ManifestLocation::kExternalPrefDownload, Extension::NO_FLAGS, false));
  EXPECT_FALSE(pending_extension_manager->IsIdPending(kExtensionId))
      << "External reinstall of a killed extension shouldn't work.";
  EXPECT_TRUE(extension_prefs->IsExternalExtensionUninstalled(kExtensionId))
      << "External reinstall of a killed extension should leave it killed.";

  // Installing from non-external source.
  ASSERT_TRUE(InstallExtension(crx_path, 1));

  EXPECT_FALSE(extension_prefs->IsExternalExtensionUninstalled(kExtensionId))
      << "Reinstalling should clear the kill bit.";

  // Uninstalling from a non-external source should not set the kill bit.
  UninstallExtension(kExtensionId);

  EXPECT_FALSE(extension_prefs->IsExternalExtensionUninstalled(kExtensionId))
      << "Uninstalling non-external extension should not set kill bit.";
}

namespace {

const char kForceInstallNotEmptyHelp[] =
    "A policy may already be controlling the list of force-installed "
    "extensions. Please remove all policy settings from your computer "
    "before running tests. E.g. from /etc/chromium/policies Linux or "
    "from the registry on Windows, etc.";

}

// See http://crbug.com/57378 for flakiness details.
IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, ExternalPolicyRefresh) {
  // Mark as enterprise managed.
  policy::ScopedDomainEnterpriseManagement scoped_domain;
  const char kExtensionId[] = "ogjcoiohnmldgjemafoockdghcjciccf";

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  embedded_test_server()->ServeFilesFromDirectory(temp_dir.GetPath());
  ASSERT_TRUE(embedded_test_server()->Start());

  base::FilePath crx_path;
  ASSERT_NO_FATAL_FAILURE(SetUpExtensionUpdatePackage(temp_dir.GetPath(), "v2",
                                                      "v2.crx", &crx_path));
  ASSERT_NO_FATAL_FAILURE(SetUpExtensionUpdateResponse(
      temp_dir.GetPath(), "v2.crx", "manifest_v2.xml.template"));

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  const size_t size_before = registry->enabled_extensions().size();
  EXPECT_TRUE(registry->disabled_extensions().empty());

  ASSERT_TRUE(extensions::ExtensionManagementFactory::GetForBrowserContext(
                  browser()->profile())
                  ->GetForceInstallList()
                  .empty())
      << kForceInstallNotEmptyHelp;

  base::Value::List forcelist;
  forcelist.Append(BuildForceInstallPolicyValue(kExtensionId,
                                                GetUpdateUrl().spec().c_str()));
  PolicyMap policies;
  policies.Set(policy::key::kExtensionInstallForcelist,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(std::move(forcelist)),
               nullptr);
  extensions::TestExtensionRegistryObserver install_observer(registry);
  UpdateProviderPolicy(policies);
  install_observer.WaitForExtensionWillBeInstalled();

  // Check if the extension got installed.
  ASSERT_EQ(size_before + 1, registry->enabled_extensions().size());
  const Extension* extension =
      registry->enabled_extensions().GetByID(kExtensionId);
  ASSERT_TRUE(extension);
  ASSERT_EQ("2.0", extension->VersionString());
  EXPECT_EQ(ManifestLocation::kExternalPolicyDownload, extension->location());

  // Try to disable and uninstall the extension which should fail.
  DisableExtension(kExtensionId);
  EXPECT_EQ(size_before + 1, registry->enabled_extensions().size());
  EXPECT_EQ(0u, registry->disabled_extensions().size());
  UninstallExtension(kExtensionId);
  EXPECT_EQ(size_before + 1, registry->enabled_extensions().size());
  EXPECT_EQ(0u, registry->disabled_extensions().size());

  // Now try to disable it through the management api, again failing.
  ExtensionTestMessageListener listener1("ready");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/uninstall_extension")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());
  EXPECT_EQ(size_before + 2, registry->enabled_extensions().size());
  EXPECT_EQ(0u, registry->disabled_extensions().size());

  // Check that emptying the list triggers uninstall.
  policies.Erase(policy::key::kExtensionInstallForcelist);
  UpdateProviderPolicy(policies);
  EXPECT_EQ(size_before + 1, registry->enabled_extensions().size());
  EXPECT_FALSE(
      registry->GetExtensionById(kExtensionId, ExtensionRegistry::EVERYTHING));
}

// Tests that non-CWS extensions are disabled when force-installed in a low
// trust environment. See https://b/283274398.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(ExtensionManagementTest,
                       NonCWSForceInstalledDisabledInLowTrustEnvironment) {
  // Mark enterprise management authority for platform as COMPUTER_LOCAL, and
  // for profile as NONE.
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::COMPUTER_LOCAL);
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::NONE);
  static constexpr char kExtensionId[] = "ogjcoiohnmldgjemafoockdghcjciccf";

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  embedded_test_server()->ServeFilesFromDirectory(temp_dir.GetPath());
  ASSERT_TRUE(embedded_test_server()->Start());

  base::FilePath crx_path;
  ASSERT_NO_FATAL_FAILURE(SetUpExtensionUpdatePackage(temp_dir.GetPath(), "v2",
                                                      "v2.crx", &crx_path));
  ASSERT_NO_FATAL_FAILURE(SetUpExtensionUpdateResponse(
      temp_dir.GetPath(), "v2.crx", "manifest_v2.xml.template"));

  ExtensionRegistry* registry = extension_registry();

  base::Value::List forcelist;
  forcelist.Append(BuildForceInstallPolicyValue(kExtensionId,
                                                GetUpdateUrl().spec().c_str()));
  PolicyMap policies;
  policies.Set(policy::key::kExtensionInstallForcelist,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(std::move(forcelist)),
               nullptr);
  extensions::TestExtensionRegistryObserver install_observer(registry);
  UpdateProviderPolicy(policies);
  install_observer.WaitForExtensionWillBeInstalled();

  // Extension should be disabled.
  EXPECT_EQ(1u, registry->disabled_extensions().size());
  EXPECT_TRUE(registry->disabled_extensions().GetByID(kExtensionId));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementTest,
                       NonCWSForceInstalledEnabledOnManagedPlatform) {
  // Mark enterprise management authority for platform as CLOUD, and for profile
  // as NONE.
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::CLOUD);
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::NONE);
  static constexpr char kExtensionId[] = "ogjcoiohnmldgjemafoockdghcjciccf";

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  embedded_test_server()->ServeFilesFromDirectory(temp_dir.GetPath());
  ASSERT_TRUE(embedded_test_server()->Start());

  base::FilePath crx_path;
  ASSERT_NO_FATAL_FAILURE(SetUpExtensionUpdatePackage(temp_dir.GetPath(), "v2",
                                                      "v2.crx", &crx_path));
  ASSERT_NO_FATAL_FAILURE(SetUpExtensionUpdateResponse(
      temp_dir.GetPath(), "v2.crx", "manifest_v2.xml.template"));

  base::Value::List forcelist;
  forcelist.Append(BuildForceInstallPolicyValue(kExtensionId,
                                                GetUpdateUrl().spec().c_str()));
  PolicyMap policies;
  policies.Set(policy::key::kExtensionInstallForcelist,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(std::move(forcelist)),
               nullptr);

  ExtensionRegistry* registry = extension_registry();
  const size_t size_before = registry->enabled_extensions().size();
  extensions::TestExtensionRegistryObserver install_observer(registry);
  UpdateProviderPolicy(policies);
  install_observer.WaitForExtensionWillBeInstalled();

  // Extension is enabled.
  EXPECT_EQ(size_before + 1, registry->enabled_extensions().size());
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kExtensionId));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementTest,
                       NonCWSForceInstalledEnabledOnManagedProfile) {
  // Mark enterprise management authority for platform as NONE, and for profile
  // as CLOUD.
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::NONE);
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD);
  static constexpr char kExtensionId[] = "ogjcoiohnmldgjemafoockdghcjciccf";

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  embedded_test_server()->ServeFilesFromDirectory(temp_dir.GetPath());
  ASSERT_TRUE(embedded_test_server()->Start());

  base::FilePath crx_path;
  ASSERT_NO_FATAL_FAILURE(SetUpExtensionUpdatePackage(temp_dir.GetPath(), "v2",
                                                      "v2.crx", &crx_path));
  ASSERT_NO_FATAL_FAILURE(SetUpExtensionUpdateResponse(
      temp_dir.GetPath(), "v2.crx", "manifest_v2.xml.template"));

  base::Value::List forcelist;
  forcelist.Append(BuildForceInstallPolicyValue(kExtensionId,
                                                GetUpdateUrl().spec().c_str()));
  PolicyMap policies;
  policies.Set(policy::key::kExtensionInstallForcelist,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(std::move(forcelist)),
               nullptr);

  ExtensionRegistry* registry = extension_registry();
  const size_t size_before = registry->enabled_extensions().size();
  extensions::TestExtensionRegistryObserver install_observer(registry);
  UpdateProviderPolicy(policies);
  install_observer.WaitForExtensionWillBeInstalled();

  // Extension is enabled.
  EXPECT_EQ(size_before + 1, registry->enabled_extensions().size());
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kExtensionId));
}
#endif

// See http://crbug.com/103371 and http://crbug.com/120640.
#if defined(ADDRESS_SANITIZER) || BUILDFLAG(IS_WIN)
#define MAYBE_PolicyOverridesUserInstall DISABLED_PolicyOverridesUserInstall
#else
#define MAYBE_PolicyOverridesUserInstall PolicyOverridesUserInstall
#endif

// Tests the behavior of force-installing extensions that the user has already
// installed.
IN_PROC_BROWSER_TEST_F(ExtensionManagementTest,
                       MAYBE_PolicyOverridesUserInstall) {
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  const char kExtensionId[] = "ogjcoiohnmldgjemafoockdghcjciccf";
  const size_t size_before = registry->enabled_extensions().size();
  EXPECT_TRUE(registry->disabled_extensions().empty());

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  embedded_test_server()->ServeFilesFromDirectory(temp_dir.GetPath());
  ASSERT_TRUE(embedded_test_server()->Start());

  base::FilePath crx_path;
  ASSERT_NO_FATAL_FAILURE(SetUpExtensionUpdatePackage(temp_dir.GetPath(), "v2",
                                                      "v2.crx", &crx_path));
  ASSERT_NO_FATAL_FAILURE(SetUpExtensionUpdateResponse(
      temp_dir.GetPath(), "v2.crx", "manifest_v2.xml.template"));

  // Check that the policy is initially empty.
  ASSERT_TRUE(extensions::ExtensionManagementFactory::GetForBrowserContext(
                  browser()->profile())
                  ->GetForceInstallList()
                  .empty())
      << kForceInstallNotEmptyHelp;

  // User install of the extension.
  ASSERT_TRUE(InstallExtension(crx_path, 1));
  ASSERT_EQ(size_before + 1, registry->enabled_extensions().size());
  const Extension* extension =
      registry->enabled_extensions().GetByID(kExtensionId);
  ASSERT_TRUE(extension);
  EXPECT_EQ(ManifestLocation::kInternal, extension->location());
  EXPECT_TRUE(service->IsExtensionEnabled(kExtensionId));

  // Setup the force install policy. It should override the location.
  base::Value::List forcelist;
  forcelist.Append(BuildForceInstallPolicyValue(kExtensionId,
                                                GetUpdateUrl().spec().c_str()));
  PolicyMap policies;
  policies.Set(policy::key::kExtensionInstallForcelist,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(forcelist.Clone()),
               nullptr);
  extensions::TestExtensionRegistryObserver install_observer(registry);
  UpdateProviderPolicy(policies);

  ASSERT_EQ(size_before + 1, registry->enabled_extensions().size());
  extension = registry->enabled_extensions().GetByID(kExtensionId);
  ASSERT_TRUE(extension);
  EXPECT_EQ(ManifestLocation::kExternalPolicyDownload, extension->location());
  EXPECT_TRUE(service->IsExtensionEnabled(kExtensionId));

  // Remove the policy, and verify that the extension was uninstalled.
  // TODO(joaodasilva): it would be nicer if the extension was kept instead,
  // and reverted location to INTERNAL or whatever it was before the policy
  // was applied.
  policies.Erase(policy::key::kExtensionInstallForcelist);
  UpdateProviderPolicy(policies);
  ASSERT_EQ(size_before, registry->enabled_extensions().size());
  extension =
      registry->GetExtensionById(kExtensionId, ExtensionRegistry::EVERYTHING);
  EXPECT_FALSE(extension);

  // User install again, but have it disabled too before setting the policy.
  ASSERT_TRUE(InstallExtension(crx_path, 1));
  ASSERT_EQ(size_before + 1, registry->enabled_extensions().size());
  extension = registry->enabled_extensions().GetByID(kExtensionId);
  ASSERT_TRUE(extension);
  EXPECT_EQ(ManifestLocation::kInternal, extension->location());
  EXPECT_TRUE(service->IsExtensionEnabled(kExtensionId));
  EXPECT_TRUE(registry->disabled_extensions().empty());

  DisableExtension(kExtensionId);
  EXPECT_EQ(1u, registry->disabled_extensions().size());
  extension = registry->disabled_extensions().GetByID(kExtensionId);
  EXPECT_TRUE(extension);
  EXPECT_FALSE(service->IsExtensionEnabled(kExtensionId));

  // Install the policy again. It should overwrite the extension's location,
  // and force enable it too.
  policies.Set(policy::key::kExtensionInstallForcelist,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(std::move(forcelist)),
               nullptr);

  extensions::TestExtensionRegistryObserver extension_observer(registry);
  UpdateProviderPolicy(policies);

  ASSERT_EQ(size_before + 1, registry->enabled_extensions().size());
  extension = registry->enabled_extensions().GetByID(kExtensionId);
  ASSERT_TRUE(extension);
  EXPECT_EQ(ManifestLocation::kExternalPolicyDownload, extension->location());
  EXPECT_TRUE(service->IsExtensionEnabled(kExtensionId));
  EXPECT_TRUE(registry->disabled_extensions().empty());
}
