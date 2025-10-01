// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/initial_external_extension_loader.h"

#include <string>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/external_provider_manager.h"
#include "chrome/browser/extensions/external_testing_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/updater/extension_downloader_test_helper.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/extensions_client.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {
namespace {

constexpr char kTestExtensionId[] = "ogjcoiohnmldgjemafoockdghcjciccf";

// Paths under the temporary directory that the EmbeddedTestServer will serve.
constexpr char kAutoupdateDir[] = "autoupdate";
constexpr char kUnpackedDir[] = "unpacked";
constexpr char kManifestRelpath[] = "manifest";
constexpr char kManifestJson[] = "manifest.json";

// This test verifies the initial external extension flow across browser
// restarts. The scenario:
//   1) PRE_PRE_: Simulate first run by seeding `kInitialInstallList` with a
//      known extension ID and verifying installation via the webstore update
//      URL.
//   2) PRE_:     Verify the extension is present, then uninstall it and record
//      that it is an *external* uninstallation (so the loader should not
//      reinstall it on next restart).
//   3) Main:     Ensure the extension remains uninstalled after a further
//      restart even if the updater runs again.
class InitialExternalExtensionLoaderRestartBrowserTest
    : public ExtensionBrowserTest {
 protected:
  InitialExternalExtensionLoaderRestartBrowserTest() = default;

  ~InitialExternalExtensionLoaderRestartBrowserTest() override = default;

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    // Browser tests disable default apps; re-enable as this test relies on the
    // initial external extension installation path.
    ExtensionBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->RemoveSwitch(switches::kDisableDefaultApps);
  }

  std::string GetExtensionUpdateURLSpec() {
    return embedded_test_server()
        ->GetURL(base::StrCat({"/", kAutoupdateDir, "/", kManifestRelpath}))
        .spec();
  }

  // Returns the installed extension under test if present, otherwise nullptr.
  Extension* GetInstalledExtension() {
    return const_cast<Extension*>(
        extension_registry()->GetInstalledExtension(kTestExtensionId));
  }

  // Blocks until `extension_id` is reported as installed. The installation
  // trigger is executed only after the observer is attached to avoid races.
  void WaitForInstallOf(const std::string& extension_id) {
    extensions::TestExtensionRegistryObserver observer(extension_registry(),
                                                       extension_id);
    // Trigger an updater cycle and wait for the specific extension ID.
    ExtensionUpdater::Get(profile())->CheckNow(ExtensionUpdater::CheckParams());

    // If the action completed synchronously and the extension is already there,
    // this returns immediately; otherwise it waits for the install event.
    if (!extension_registry()->GetInstalledExtension(extension_id)) {
      observer.WaitForExtensionInstalled();
    }
  }

  // Creates a CRX from test data (under test_data_dir_/autoupdate/<source>)
  // while rewriting its manifest to point to our EmbeddedTestServer update URL.
  void SetUpExtensionUpdatePackage(const base::FilePath& temp_dir,
                                   const std::string& source_dir_name,
                                   const std::string& crx_name) {
    ASSERT_TRUE(base::CreateDirectory(temp_dir.AppendASCII(kUnpackedDir)));

    const base::FilePath basedir = test_data_dir_.AppendASCII("autoupdate");
    ASSERT_TRUE(base::CopyDirectory(basedir.AppendASCII(source_dir_name),
                                    temp_dir.AppendASCII(kUnpackedDir),
                                    /*recursive=*/false));

    // Render manifest.json.template -> manifest.json with the server's update
    // URL.
    std::string manifest_template;
    ASSERT_TRUE(
        base::ReadFileToString(basedir.AppendASCII(source_dir_name)
                                   .AppendASCII("manifest.json.template"),
                               &manifest_template));
    const std::string manifest = base::ReplaceStringPlaceholders(
        manifest_template, {GetExtensionUpdateURLSpec()},
        /*offsets=*/nullptr);
    ASSERT_TRUE(base::WriteFile(
        temp_dir.AppendASCII(kUnpackedDir).AppendASCII(kManifestJson),
        manifest));

    // Pack to CRX placed under /autoupdate so the server can serve it.
    ASSERT_TRUE(base::CreateDirectory(temp_dir.AppendASCII(kAutoupdateDir)));
    base::FilePath crx_path = PackExtensionWithOptions(
        temp_dir.AppendASCII(kUnpackedDir),
        temp_dir.AppendASCII(kAutoupdateDir).AppendASCII(crx_name),
        basedir.AppendASCII("key.pem"), /*pem_out_path=*/base::FilePath());
    ASSERT_FALSE(crx_path.empty());
  }

  // Emits the "update manifest" the updater fetches at /autoupdate/manifest,
  // pointing to the CRX we created above.
  void SetUpExtensionUpdateResponse(const base::FilePath& temp_dir,
                                    const std::string& crx_name,
                                    const std::string& manifest_template_name) {
    std::string manifest_template;
    ASSERT_TRUE(
        base::ReadFileToString(test_data_dir_.AppendASCII(kAutoupdateDir)
                                   .AppendASCII(manifest_template_name),
                               &manifest_template));
    const GURL crx_url = embedded_test_server()->GetURL(
        base::StrCat({"/", kAutoupdateDir, "/", crx_name}));
    const std::string manifest = base::ReplaceStringPlaceholders(
        manifest_template, {crx_url.spec()}, /*offsets=*/nullptr);

    ASSERT_TRUE(base::CreateDirectory(temp_dir.AppendASCII(kAutoupdateDir)));
    ASSERT_TRUE(base::WriteFile(
        temp_dir.AppendASCII(kAutoupdateDir).AppendASCII(kManifestRelpath),
        manifest));
  }

  // Returns current initial install list from the prefs.
  const base::Value::List& InitialInstallList() {
    return profile()->GetPrefs()->GetList(pref_names::kInitialInstallList);
  }

  // Adds `kTestExtensionId` to the initial install list (overwriting any
  // previous list to ensure deterministic state).
  void SeedInitialInstallListWithTestExtension() {
    base::Value::List ids;
    ids.Append(kTestExtensionId);
    profile()->GetPrefs()->SetList(pref_names::kInitialInstallList,
                                   std::move(ids));
    profile()->GetPrefs()->CommitPendingWrite();
  }

  // Setup extension installation environment so that a request to install
  // `kTestExtensionId` may succeed when it should.
  void PrepareExtensionInstallation() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Serve all files we place into `temp_dir_`, especially /autoupdate/*
    // resources needed by the CRX update manifest.
    embedded_test_server()->ServeFilesFromDirectory(temp_dir_.GetPath());
    ASSERT_TRUE(embedded_test_server()->Start());

    // Point the "webstore" update URL to the EmbeddedTestServer.
    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kAppsGalleryUpdateURL,
                                    GetExtensionUpdateURLSpec());
    ExtensionsClient::Get()->InitializeWebStoreUrls(command_line);

    // Prepare a CRX package and the update response the updater will fetch.
    ASSERT_NO_FATAL_FAILURE(SetUpExtensionUpdatePackage(
        temp_dir_.GetPath(), /*source_dir_name=*/"v2", /*crx_name=*/"v2.crx"));
    ASSERT_NO_FATAL_FAILURE(SetUpExtensionUpdateResponse(
        temp_dir_.GetPath(), /*crx_name=*/"v2.crx",
        /*manifest_template_name=*/"manifest_v2.xml.template"));
  }

 protected:
  base::test::ScopedFeatureList feature_list_{
      features::kInitialExternalExtensions};
  FeatureSwitch::ScopedOverride feature_override{
      FeatureSwitch::prompt_for_external_extensions(), true};
  base::ScopedTempDir temp_dir_;
};

// Phase 1 (First run): seed initial preferences and verify installation occurs.
IN_PROC_BROWSER_TEST_F(InitialExternalExtensionLoaderRestartBrowserTest,
                       PRE_PRE_InitialExternalExtension) {
  PrepareExtensionInstallation();
  SeedInitialInstallListWithTestExtension();

  const size_t enabled_before =
      extension_registry()->enabled_extensions().size();
  EXPECT_TRUE(extension_registry()->disabled_extensions().empty());

  WaitForInstallOf(kTestExtensionId);

  Extension* extension = GetInstalledExtension();
  ASSERT_TRUE(extension);
  EXPECT_EQ(extension->id(), kTestExtensionId);

  // Verify the extension is in the expected state (disabled for being
  // unacknowledged).
  EXPECT_FALSE(
      extension_registry()->enabled_extensions().Contains(kTestExtensionId));
  EXPECT_EQ(enabled_before, extension_registry()->enabled_extensions().size());
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_FALSE(prefs->IsExternalExtensionAcknowledged(kTestExtensionId));
  EXPECT_THAT(prefs->GetDisableReasons(kTestExtensionId),
              testing::UnorderedElementsAre(
                  disable_reason::DISABLE_EXTERNAL_EXTENSION));
}

// Phase 2 (Restart): confirm installation, then uninstall so a future restart
// must *not* reinstall it.
IN_PROC_BROWSER_TEST_F(InitialExternalExtensionLoaderRestartBrowserTest,
                       PRE_InitialExternalExtension) {
  EXPECT_TRUE(
      base::Contains(InitialInstallList(), base::Value(kTestExtensionId)));

  Extension* extension = GetInstalledExtension();
  ASSERT_TRUE(extension);
  EXPECT_EQ(extension->id(), kTestExtensionId);

  UninstallExtension(extension->id());
  EXPECT_FALSE(GetInstalledExtension());

  auto* extension_prefs = extensions::ExtensionPrefs::Get(profile());
  EXPECT_TRUE(
      extension_prefs->IsExternalExtensionUninstalled(kTestExtensionId));
}

// Phase 3 (Second restart): the loader/updater must respect the external
// uninstall (user preference) and not reinstall the extension.
IN_PROC_BROWSER_TEST_F(InitialExternalExtensionLoaderRestartBrowserTest,
                       InitialExternalExtension) {
  // Ensures that extension installation environment is correctly set to verify
  // that the user preference is respected.
  PrepareExtensionInstallation();

  EXPECT_TRUE(
      base::Contains(InitialInstallList(), base::Value(kTestExtensionId)));

  EXPECT_FALSE(GetInstalledExtension());

  auto* extension_prefs = extensions::ExtensionPrefs::Get(profile());
  EXPECT_TRUE(
      extension_prefs->IsExternalExtensionUninstalled(kTestExtensionId));

  // Even if the updater runs, the external-uninstalled marker should prevent
  // reinstallation to respect user choice.
  ExtensionUpdater::CheckParams params;
  params.install_immediately = true;
  ExtensionUpdater::Get(profile())->CheckNow(std::move(params));

  EXPECT_FALSE(GetInstalledExtension());
  EXPECT_TRUE(
      extension_prefs->IsExternalExtensionUninstalled(kTestExtensionId));
}

}  // namespace
}  // namespace extensions
