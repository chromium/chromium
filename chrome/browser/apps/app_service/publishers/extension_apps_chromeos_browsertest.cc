// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_handlers/file_handler_info.h"
#include "extensions/common/manifest_handlers/web_file_handlers_info.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/base/filename_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/types/display_constants.h"

namespace apps {

class ExtensionAppsChromeOsBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  ExtensionAppsChromeOsBrowserTest() {
    feature_list_.InitAndEnableFeature(extensions_features::kWebFileHandlers);
  }

 protected:
  base::FilePath StoreSharedFile(const base::FilePath& directory,
                                 const base::StringPiece& name,
                                 const base::StringPiece& content) {
    const base::FilePath path = directory.Append(base::StringPiece(name));
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::File file(path,
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    EXPECT_EQ(file.WriteAtCurrentPos(content.begin(), content.size()),
              static_cast<int>(content.size()));
    return path;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  extensions::ScopedCurrentChannel current_channel_{version_info::Channel::DEV};
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
  extension_dir.WriteFile("open-csv.js",
                          R"(chrome.test.sendMessage("launched");)");
  extension_dir.WriteFile("open-csv.html",
                          R"(<script src="/open-csv.js"></script>)");
  ExtensionTestMessageListener listener("launched");
  const extensions::Extension* extension =
      LoadExtension(extension_dir.UnpackedPath());
  DCHECK(extension);
  auto* file_handlers =
      extensions::WebFileHandlers::GetFileHandlers(*extension);
  EXPECT_EQ(1u, file_handlers->size());

  // Open app.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  auto intent = std::make_unique<apps::Intent>("view");
  intent->mime_type = "text/csv";
  intent->activity_name = "open-csv.html";
  const base::FilePath file_path =
      StoreSharedFile(scoped_temp_dir.GetPath(), "a.csv", "1,2,3");

  // Add file(s) to intent.
  int64_t file_size = 0;
  base::GetFileSize(file_path, &file_size);
  auto file =
      std::make_unique<apps::IntentFile>(net::FilePathToFileURL(file_path));
  file->file_name = base::SafeBaseName::Create(file_path);
  file->file_size = file_size;
  file->mime_type = "text/csv";
  intent->files.push_back(std::move(file));

  // Launch app with intent.
  Profile* const profile = browser()->profile();
  const int32_t event_flags =
      apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                          /*prefer_container=*/true);
  apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithIntent(
      extension->id(), event_flags, std::move(intent),
      apps::LaunchSource::kFromFileManager, nullptr, base::DoNothing());

  // Verify launch.
  ASSERT_TRUE(listener.WaitUntilSatisfied());
}

}  // namespace apps
