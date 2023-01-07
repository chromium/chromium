// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_TEST_UTIL_H_

#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/platform_util.h"

class Profile;

namespace file_manager {
namespace test {

// A dummy folder in a temporary path that is automatically mounted as a
// Profile's Downloads folder.
class FolderInMyFiles {
 public:
  explicit FolderInMyFiles(Profile* profile);
  ~FolderInMyFiles();

  // Copies additional files into |folder_|, appending to |files_|.
  void Add(const std::vector<base::FilePath>& files);

  // Copies the contents of |file| to |folder_| with the given |new_base_name|.
  void AddWithName(const base::FilePath& file,
                   const base::FilePath& new_base_name);

  // Use platform_util::OpenItem() on the file with basename matching |path| to
  // simulate a user request to open that path, e.g., from the Files app or
  // chrome://downloads.
  platform_util::OpenOperationResult Open(const base::FilePath& path);

  // Refreshes `files_` by re-reading directory contents, sorting by name.
  void Refresh();

  const std::vector<base::FilePath> files() { return files_; }

 private:
  FolderInMyFiles(const FolderInMyFiles&) = delete;
  FolderInMyFiles& operator=(const FolderInMyFiles&) = delete;

  Profile* const profile_;
  base::FilePath folder_;
  std::vector<base::FilePath> files_;
};

// Load the default set of component extensions used on ChromeOS. This should be
// done in an override of InProcessBrowserTest::SetUpOnMainThread().
void AddDefaultComponentExtensionsOnMainThread(Profile* profile);

// Installs the chrome app at the provided |test_path_ascii| under DIR_TEST_DATA
// and waits for the background page to start up.
scoped_refptr<const extensions::Extension> InstallTestingChromeApp(
    Profile* profile,
    const char* test_path_ascii);

// Installs a test File System Provider chrome app that provides a file system
// containing readwrite.gif and readonly.png files, and wait for the file system
// to be mounted. Returns a base::WeakPtr<file_manager::Volume> to the mounted
// file system.
base::WeakPtr<file_manager::Volume> InstallFileSystemProviderChromeApp(
    Profile* profile);

// Gets the list of available tasks for the provided `file`. Note only the path
// string is used for this helper, so it must have a well-known MIME type
// according to net::GetMimeTypeFromFile().
std::vector<file_tasks::FullTaskDescriptor> GetTasksForFile(
    Profile* profile,
    const base::FilePath& file);

// Add a fake web app with to the |app_service_proxy| with
// |intent_filters|.
void AddFakeAppWithIntentFilters(
    const std::string& app_id,
    std::vector<apps::IntentFilterPtr> intent_filters,
    apps::AppType app_type,
    absl::optional<bool> handles_intents,
    apps::AppServiceProxy* app_service_proxy);

// Add a fake web app with to the |app_service_proxy|.
void AddFakeWebApp(const std::string& app_id,
                   const std::string& mime_type,
                   const std::string& file_extension,
                   const std::string& activity_label,
                   absl::optional<bool> handles_intents,
                   apps::AppServiceProxy* app_service_proxy);

}  // namespace test
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_TEST_UTIL_H_
