// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <string_view>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_handlers/file_handler_info.h"
#include "extensions/common/manifest_handlers/web_file_handlers_info.h"
#include "extensions/common/web_file_handler_constants.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/base/filename_util.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/types/display_constants.h"

namespace apps {

namespace {

// Write file to disk.
base::FilePath WriteFile(const base::FilePath& directory,
                         std::string_view name,
                         std::string_view content) {
  const base::FilePath path = directory.Append(std::string_view(name));
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::WriteFile(path, content);
  return path;
}

}  // namespace

class ExtensionAppsChromeOsBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  ExtensionAppsChromeOsBrowserTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionWebFileHandlers);
  }

 protected:
  // Launch the extension from an intent and wait for a result from chrome.test.
  void LaunchExtensionAndCatchResult(const extensions::Extension& extension) {
    std::unique_ptr<Intent> intent = SetupLaunchAndGetIntent(extension);
    ASSERT_TRUE(intent);

    // Prepare to verify launch.
    extensions::ResultCatcher catcher;

    // Launch app with intent.
    Profile* const profile = browser()->profile();
    const int32_t event_flags =
        apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                            /*prefer_container=*/true);
    apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithIntent(
        extension.id(), event_flags, std::move(intent),
        apps::LaunchSource::kFromFileManager, nullptr, base::DoNothing());

    // Verify launch.
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // Install extension as a default installed extension. The permission UI isn't
  // presented in these cases, and as such there is no need to fake clicks.
  const extensions::Extension* InstallDefaultInstalledExtension(
      base::FilePath file_path) {
    return InstallExtensionWithSourceAndFlags(
        file_path,
        /*expected_change=*/true,
        extensions::mojom::ManifestLocation::kInternal,
        extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  }

 private:
  apps::IntentPtr SetupLaunchAndGetIntent(
      const extensions::Extension& extension) {
    auto* file_handlers =
        extensions::WebFileHandlers::GetFileHandlers(extension);
    EXPECT_EQ(1u, file_handlers->size());

    // Create file(s).
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ScopedTempDir scoped_temp_dir;
    EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
    auto intent = std::make_unique<apps::Intent>(apps_util::kIntentActionView);
    intent->mime_type = "text/csv";
    intent->activity_name = "open-csv.html";
    const base::FilePath file_path =
        WriteFile(scoped_temp_dir.GetPath(), "a.csv", "1,2,3");

    // Add file(s) to intent.
    int64_t file_size = 0;
    base::GetFileSize(file_path, &file_size);

    // Create a virtual file in the file system, as required for AppService.
    scoped_refptr<storage::FileSystemContext> file_system_context =
        storage::CreateFileSystemContextForTesting(
            /*quota_manager_proxy=*/nullptr, base::FilePath());
    auto file_system_url = file_system_context->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFromStringForTesting("chrome://file-manager"),
        storage::kFileSystemTypeTest, file_path);

    // Update the intent with the file.
    auto file = std::make_unique<apps::IntentFile>(file_system_url.ToGURL());
    file->file_name = base::SafeBaseName::Create("a.csv");
    file->file_size = file_size;
    file->mime_type = "text/csv";
    intent->files.push_back(std::move(file));
    return intent;
  }

  base::test::ScopedFeatureList feature_list_;
};

// Open the extension action url when opening a matching file type.
IN_PROC_BROWSER_TEST_F(ExtensionAppsChromeOsBrowserTest, LaunchWithFileIntent) {
  // Load extension.
  static constexpr char kManifest[] = R"({
    "name": "Test",
    "version": "0.0.1",
    "manifest_version": 3,
    "file_handlers": [
      {
        "name": "Comma separated values",
        "action": "/open-csv.html",
        "accept": {"text/csv": [".csv"]}
      }
    ]
  })";
  extensions::TestExtensionDir extension_dir;
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile("open-csv.js", "chrome.test.succeed();");
  extension_dir.WriteFile("open-csv.html",
                          R"(<script src="/open-csv.js"></script>)");
  const extensions::Extension* extension =
      InstallDefaultInstalledExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ASSERT_TRUE(extensions::WebFileHandlers::SupportsWebFileHandlers(*extension));
  LaunchExtensionAndCatchResult(*extension);
}

// Test WFH (Web File Handlers) without a key and again with a QuickOffice key.
// WFH (Web File Handlers) are an MV3 concept. QO (QuickOffice) should use WFH
// to open files for Extensions, instead of ChromeApps.
// Verify window.launchQueue presence.
IN_PROC_BROWSER_TEST_F(ExtensionAppsChromeOsBrowserTest, SetConsumerCalled) {
  struct {
    const char* title;
    const std::string manifest_part;
  } test_cases[] = {
      {"Default", ""},
      {"QuickOffice",
       base::StringPrintf(R"(, "key": "%s")",
                          extensions::web_file_handlers::kQuickOfficeKey)},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.title);
    const std::string manifest =
        base::StringPrintf(R"({
      "name": "Test",
      "version": "0.0.1",
      "manifest_version": 3,
      "file_handlers": [
        {
          "name": "Comma separated values",
          "action": "/open-csv.html",
          "accept": {"text/csv": [".csv"]}
        }
      ]
      %s
    })",
                           test_case.manifest_part.c_str());

    // Load extension.
    extensions::TestExtensionDir extension_dir;
    extension_dir.WriteManifest(manifest);
    extension_dir.WriteFile("open-csv.js", R"(
      launchQueue.setConsumer((launchParams) => {
        chrome.test.assertTrue('launchQueue' in window);
        chrome.test.succeed();
      });
    )");
    extension_dir.WriteFile("open-csv.html",
                            R"(<script src="/open-csv.js"></script>"
                              "<body>Test</body>)");
    const extensions::Extension* extension =
        InstallDefaultInstalledExtension(extension_dir.UnpackedPath());
    ASSERT_TRUE(extension);

    // TODO(crbug.com/40169582): setConsumer is called, but launchParams is
    // empty in the test. However, it is populated when run manually. Find a
    // better way to automate launchParams testing such that it's populated in
    // the test, like it is when executed manually.
    LaunchExtensionAndCatchResult(*extension);
  }
}

// Verify that a new tab prefers opening in an existing window.
IN_PROC_BROWSER_TEST_F(ExtensionAppsChromeOsBrowserTest, NavigateExisting) {
  // Create an extension.
  static constexpr char const kManifest[] = R"({
    "name": "Test",
    "version": "0.0.1",
    "manifest_version": 3,
    "file_handlers": [
      {
        "name": "Comma separated values",
        "action": "/open-csv.html",
        "accept": {"text/csv": [".csv"]}
      }
    ]
  })";

  // Load extension.
  extensions::TestExtensionDir extension_dir;
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile("open-csv.js", R"(
    chrome.test.assertTrue('launchQueue' in window);
    launchQueue.setConsumer((launchParams) => {
      chrome.test.assertEq(1, launchParams.files.length);
      chrome.test.assertEq("a.csv", launchParams.files[0].name);
      chrome.test.assertEq("file", launchParams.files[0].kind);
      chrome.test.succeed();
    });
  )");
  extension_dir.WriteFile("open-csv.html",
                          R"(<script src="/open-csv.js"></script>"
                            "<body>Test</body>)");
  const extensions::Extension* extension =
      InstallDefaultInstalledExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Open a file twice by launching the file handler each time.
  content::WebContents* web_contents[2];
  for (unsigned short i = 0; i < 2; i++) {
    LaunchExtensionAndCatchResult(*extension);
    web_contents[i] = browser()->tab_strip_model()->GetActiveWebContents();
  }

  // GetWindowIdOfTab() returns -1 for SessionID::InvalidValue().
  ASSERT_NE(extensions::ExtensionTabUtil::GetWindowIdOfTab(web_contents[0]),
            -1);

  // The Window ID should match for both launches.
  ASSERT_EQ(extensions::ExtensionTabUtil::GetWindowIdOfTab(web_contents[0]),
            extensions::ExtensionTabUtil::GetWindowIdOfTab(web_contents[1]));

  // Tab ids should differ.
  ASSERT_NE(extensions::ExtensionTabUtil::GetTabId(web_contents[0]),
            extensions::ExtensionTabUtil::GetTabId(web_contents[1]));
}

}  // namespace apps
