// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/unpacked_installer.h"

#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/search_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

class UnpackedInstallerBrowserTest : public ExtensionBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    search_test_utils::WaitForTemplateURLServiceToLoad(
        TemplateURLServiceFactory::GetForProfile(profile()));
  }

  scoped_refptr<const Extension> LoadCommandLineExtension(
      const base::FilePath& path) {
    TestExtensionRegistryObserver observer(extension_registry());
    std::string extension_id;
    UnpackedInstaller::Create(profile())->LoadFromCommandLine(
        path, &extension_id,
        /*only-allow-apps*/ false);

    return observer.WaitForExtensionLoaded();
  }
};

// Tests that `kNoOverride` is recorded for an extension with manifest overrides
// but they do not override default search engine or new tab page.
IN_PROC_BROWSER_TEST_F(UnpackedInstallerBrowserTest,
                       RecordCommandLineMetrics_NoOverrides) {
  base::HistogramTester histograms;

  // Load an extension without default search engine and new tab page overrides.
  static constexpr char kManifest[] =
      R"({
          "name": "No Override Extension",
          "version": "0.1",
          "manifest_version": 3,
          "permissions": ["history","storage"],
          "chrome_settings_overrides": {
            "search_provider": {
              "search_url": "https://www.example.__MSG_url_domain__/?q={searchTerms}",
              "name": "Example",
              "keyword": "word",
              "encoding": "UTF-8",
              "is_default": false,
              "favicon_url": "https://example.com/favicon.ico"
            }
          },
          "chrome_url_overrides": {
            "history": "history.html"
          }
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("history.html"), "");

  scoped_refptr<const Extension> loaded_extension =
      LoadCommandLineExtension(test_dir.UnpackedPath());
  EXPECT_EQ(loaded_extension->name(), "No Override Extension");

  histograms.ExpectTotalCount(
      /*name=*/"Extensions.CommandLineInstalled",
      /*expected_count=*/1);
  // Verify kNoOverride is logged.
  histograms.ExpectUniqueSample(
      /*name=*/"Extensions.CommandLineManifestSettingsOverride",
      /*sample=*/UnpackedInstaller::ManifestSettingsOverrideType::kNoOverride,
      /*expected_bucket_count=*/1);
}

// Tests that `kNewTabPage` is recorded for an extension that only overrides
// the new tab page.
IN_PROC_BROWSER_TEST_F(UnpackedInstallerBrowserTest,
                       RecordCommandLineMetrics_NewTabPageOverride) {
  base::HistogramTester histograms;

  // Load an extension with new tab page overrides.
  static constexpr char kManifest[] =
      R"({
          "name": "New Tab Page Override Extension",
          "version": "0.1",
          "manifest_version": 3,
          "permissions": ["newtab"],
          "chrome_url_overrides": {
            "newtab": "override.html"
          }
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("override.html"), "");

  // Load new tab page override extension and verify.
  scoped_refptr<const Extension> loaded_extension =
      LoadCommandLineExtension(test_dir.UnpackedPath());
  EXPECT_EQ(loaded_extension->name(), "New Tab Page Override Extension");

  histograms.ExpectTotalCount(
      /*name=*/"Extensions.CommandLineInstalled",
      /*expected_count=*/1);
  // Verify kNewTabPage is logged.
  histograms.ExpectUniqueSample(
      /*name=*/"Extensions.CommandLineManifestSettingsOverride",
      /*sample=*/UnpackedInstaller::ManifestSettingsOverrideType::kNewTabPage,
      /*expected_bucket_count=*/1);
}

// SettingsOverrides are only available on Windows and macOS.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
// Tests that `kSearchEngine` is recorded for an extension that only overrides
// the default search engine.
IN_PROC_BROWSER_TEST_F(UnpackedInstallerBrowserTest,
                       RecordCommandLineMetrics_SearchOverride) {
  base::HistogramTester histograms;

  // Load an extension with default search engine override.
  static constexpr char kManifest[] =
      R"({
          "name": "Search Engine Override Extension",
          "version": "0.1",
          "manifest_version": 3,
          "permissions": ["newtab","storage"],
          "chrome_settings_overrides": {
            "search_provider": {
              "search_url": "https://www.example.__MSG_url_domain__/?q={searchTerms}",
              "name": "Example",
              "keyword": "word",
              "encoding": "UTF-8",
              "is_default": true,
              "favicon_url": "https://example.com/favicon.ico"
            }
          }
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);

  scoped_refptr<const Extension> loaded_extension =
      LoadCommandLineExtension(test_dir.UnpackedPath());
  EXPECT_EQ(loaded_extension->name(), "Search Engine Override Extension");

  histograms.ExpectTotalCount(
      /*name=*/"Extensions.CommandLineInstalled",
      /*expected_count=*/1);
  // Verify kSearchEngine is logged.
  histograms.ExpectUniqueSample(
      /*name=*/"Extensions.CommandLineManifestSettingsOverride",
      /*sample=*/UnpackedInstaller::ManifestSettingsOverrideType::kSearchEngine,
      /*expected_bucket_count=*/1);
}

// Tests that `kSearchEngineAndNewTabPage` is recorded for an extension that
// overrides both the default search engine and new tab page.
IN_PROC_BROWSER_TEST_F(
    UnpackedInstallerBrowserTest,
    RecordCommandLineMetrics_SearchEngineAndNewTabPageOverride) {
  base::HistogramTester histograms;

  // Load an extension with default search engine and new tab page overrides.
  static constexpr char kManifest[] =
      R"({
          "name": "Search Engine And New Tab Page Override Extension",
          "version": "0.1",
          "manifest_version": 3,
          "permissions": ["history","storage"],
          "chrome_settings_overrides": {
            "search_provider": {
              "search_url": "https://www.example.__MSG_url_domain__/?q={searchTerms}",
              "name": "Example",
              "keyword": "word",
              "encoding": "UTF-8",
              "is_default": true,
              "favicon_url": "https://example.com/favicon.ico"
            }
          },
          "chrome_url_overrides": {
            "newtab": "override.html"
          }
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("override.html"), "");

  // Load no override extension and verify.
  scoped_refptr<const Extension> loaded_extension =
      LoadCommandLineExtension(test_dir.UnpackedPath());
  EXPECT_EQ(loaded_extension->name(),
            "Search Engine And New Tab Page Override Extension");

  histograms.ExpectTotalCount(
      /*name=*/"Extensions.CommandLineInstalled",
      /*expected_count=*/1);
  // Verify kSearchEngineAndNewTabPage is logged.
  histograms.ExpectUniqueSample(
      /*name=*/"Extensions.CommandLineManifestSettingsOverride",
      /*sample=*/
      UnpackedInstaller::ManifestSettingsOverrideType::
          kSearchEngineAndNewTabPage,
      /*expected_bucket_count=*/1);
}
#endif

}  // namespace extensions
