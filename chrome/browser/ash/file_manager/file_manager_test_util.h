// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_TEST_UTIL_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/ash/file_system_provider/provider_interface.h"
#include "chrome/browser/platform_util.h"
#include "chromeos/ash/components/drivefs/fake_drivefs.h"

class Profile;

namespace file_manager {

class Volume;

namespace test {

static const char kODFSSampleUrl[] = "https://1drv.ms/123";
static const char kSampleUserEmail1[] = "user.1@gmail.com";
static const char kSampleUserUpperCaseEmail1[] = "USER.1@gmail.com";
static const char kSampleUserEmail2[] = "user.2@gmail.com";

// A dummy folder in a temporary path that is automatically mounted as a
// Profile's Downloads folder.
class FolderInMyFiles {
 public:
  explicit FolderInMyFiles(Profile* profile);
  ~FolderInMyFiles();

  // Copies additional files into `folder_`, appending to `files_`.
  void Add(const std::vector<base::FilePath>& files);

  // Copies the contents of `file` to `folder_` with the given `new_base_name`.
  void AddWithName(const base::FilePath& file,
                   const base::FilePath& new_base_name);

  // Use platform_util::OpenItem() on the file with basename matching `path` to
  // simulate a user request to open that path, e.g., from the Files app or
  // chrome://downloads.
  platform_util::OpenOperationResult Open(const base::FilePath& path);

  // Refreshes `files_` by re-reading directory contents, sorting by name.
  void Refresh();

  const std::vector<base::FilePath> files() { return files_; }

 private:
  FolderInMyFiles(const FolderInMyFiles&) = delete;
  FolderInMyFiles& operator=(const FolderInMyFiles&) = delete;

  const raw_ptr<Profile, DanglingUntriaged> profile_;
  base::FilePath folder_;
  std::vector<base::FilePath> files_;
};

// Take test files from the chromeos/file_manager/ test directory and copy them
// into a temp folder mounted within MyFiles.
std::vector<storage::FileSystemURL> CopyTestFilesIntoMyFiles(
    Profile* profile,
    std::vector<std::string> file_names);

// Load the default set of component extensions used on ChromeOS. This should be
// done in an override of InProcessBrowserTest::SetUpOnMainThread().
void AddDefaultComponentExtensionsOnMainThread(Profile* profile);

// Installs the chrome app at the provided `test_path_ascii` under DIR_TEST_DATA
// and waits for the background page to start up.
scoped_refptr<const extensions::Extension> InstallTestingChromeApp(
    Profile* profile,
    const char* test_path_ascii);

// Uses InstallTestingChromeApp to install a test File System Provider chrome
// app that provides a file system containing readwrite.gif and readonly.png
// files, and wait for the file system to be mounted. Returns a
// base::WeakPtr<file_manager::Volume> to the mounted file system.
base::WeakPtr<file_manager::Volume> InstallFileSystemProviderChromeApp(
    Profile* profile);
// Like above but uses the provided chrome app installation function
// `install_fn` instead of InstallTestingChromeApp. `install_fn` receives the
// chrome app's path (relative to DIR_TEST_DATA) as argument.
base::WeakPtr<file_manager::Volume> InstallFileSystemProviderChromeApp(
    Profile* profile,
    base::OnceCallback<void(const char*)> install_fn);

// Gets the list of available tasks for the provided `file`. Note only the path
// string is used for this helper, so it must have a well-known MIME type
// according to net::GetMimeTypeFromFile().
std::vector<file_tasks::FullTaskDescriptor> GetTasksForFile(
    Profile* profile,
    const base::FilePath& file);

// Add a fake web app with to the `app_service_proxy` with
// `intent_filters`.
void AddFakeAppWithIntentFilters(
    const std::string& app_id,
    std::vector<apps::IntentFilterPtr> intent_filters,
    apps::AppType app_type,
    std::optional<bool> handles_intents,
    apps::AppServiceProxy* app_service_proxy);

// Add a fake web app with to the `app_service_proxy`.
void AddFakeWebApp(const std::string& app_id,
                   const std::string& mime_type,
                   const std::string& file_extension,
                   const std::string& activity_label,
                   std::optional<bool> handles_intents,
                   apps::AppServiceProxy* app_service_proxy);

// Fake DriveFs specific to the `DriveTest`. The alternate URL is the only piece
// of metadata stored for a file which is identified by its relative path.
class FakeSimpleDriveFs : public drivefs::FakeDriveFs {
 public:
  explicit FakeSimpleDriveFs(const base::FilePath& mount_path);

  ~FakeSimpleDriveFs() override;

  // Sets the `alternate_urls_` entry for the given path.
  void SetMetadata(const drivefs::FakeMetadata& metadata);

 private:
  // This is a simplified version of `FakeDriveFs::GetMetadata()` that returns a
  // `metadata` with `alternate_url ` field set as the `alternate_urls_` entry
  // for `path`. The other metadata fields are constructed with default values.
  // If there is no `alternate_urls_` entry for `path`, return with
  // `FILE_ERROR_NOT_FOUND`.
  void GetMetadata(const base::FilePath& path,
                   GetMetadataCallback callback) override;

  // Each file in this DriveFs has an entry.
  std::unordered_map<base::FilePath, std::string> alternate_urls_;
};

// Fake DriveFs helper specific to the `DriveTest`. Implements the
// functions to create a `FakeSimpleDriveFs`.
class FakeSimpleDriveFsHelper : public drive::FakeDriveFsHelper {
 public:
  FakeSimpleDriveFsHelper(Profile* profile, const base::FilePath& mount_path);

  base::RepeatingCallback<std::unique_ptr<drivefs::DriveFsBootstrapListener>()>
  CreateFakeDriveFsListenerFactory();

  const base::FilePath& mount_path() { return mount_path_; }
  FakeSimpleDriveFs& fake_drivefs() { return fake_drivefs_; }

 private:
  const base::FilePath mount_path_;
  FakeSimpleDriveFs fake_drivefs_;
};

// Fake provided file system implementation specific to mimic ODFS. Override
// `CreateFile` method to fail with a specific error, if set. Override
// `GetActions` method to return fake actions and to fail with a specific error
// for a non-root entry, if set.
class FakeProvidedFileSystemOneDrive
    : public ash::file_system_provider::FakeProvidedFileSystem {
 public:
  explicit FakeProvidedFileSystemOneDrive(
      const ash::file_system_provider::ProvidedFileSystemInfo&
          file_system_info);
  ~FakeProvidedFileSystemOneDrive() override;

  // Fail the create file request with `create_file_error_` if it exists.
  // Otherwise, create a file as normal. Tests can run a callback on
  // `CreateFile` via `SetCreateFileCallback()`.
  ash::file_system_provider::AbortCallback CreateFile(
      const base::FilePath& file_path,
      storage::AsyncFileUtil::StatusCallback callback) override;

  // Parallel what ODFS does but fail to get non-root entry metadata if
  // `get_actions_error_` is set:
  // - If the number of entries requested is not 1, fail.
  // - If the root is requested, return (test) ODFS metadata.
  // - If `get_actions_error_` is set, fail request with it.
  // - If the entry is found, return (test) entry metadata.
  // - Otherwise, fail.
  ash::file_system_provider::AbortCallback GetActions(
      const std::vector<base::FilePath>& entry_paths,
      GetActionsCallback callback) override;

  // Set error for the `CreateFile` to fail with.
  void SetCreateFileError(base::File::Error error);

  // Set a callback to be called when `CreateFile` is called.
  void SetCreateFileCallback(base::OnceClosure callback);

  // Set error for the `GetActions` to fail with when non-root entry actions are
  // requested.
  void SetGetActionsError(base::File::Error error);

  // Set value for the `kReauthenticationRequiredId` ODFS metadata action.
  void SetReauthenticationRequired(bool reauthentication_required);

 private:
  base::File::Error create_file_error_ = base::File::Error::FILE_OK;
  base::File::Error get_actions_error_ = base::File::Error::FILE_OK;
  bool reauthentication_required_ = false;
  base::OnceClosure create_file_callback_;
};

// Fake extension provider to create a `FakeProvidedFileSystemOneDrive`.
class FakeExtensionProviderOneDrive
    : public ash::file_system_provider::FakeExtensionProvider {
 public:
  static std::unique_ptr<ProviderInterface> Create(
      const extensions::ExtensionId& extension_id);

  std::unique_ptr<ash::file_system_provider::ProvidedFileSystemInterface>
  CreateProvidedFileSystem(
      Profile* profile,
      const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
      ash::file_system_provider::CacheManager* cache_manager) override;

  // Calls `request_mount_callback` if set.
  bool RequestMount(
      Profile* profile,
      ash::file_system_provider::RequestMountCallback callback) override;

  // `RequestMount()` will call this callback as its implementation.
  void SetRequestMountImpl(
      base::OnceCallback<
          void(ash::file_system_provider::RequestMountCallback)>);

 private:
  FakeExtensionProviderOneDrive(
      const extensions::ExtensionId& extension_id,
      const ash::file_system_provider::Capabilities& capabilities);
  ~FakeExtensionProviderOneDrive() override;

  base::OnceCallback<void(ash::file_system_provider::RequestMountCallback)>
      request_mount_impl_;
};

// Mount a provider and return a `ProvidedFileSystemInterface`.
ash::file_system_provider::ProvidedFileSystemInterface* MountProvidedFileSystem(
    Profile* profile,
    const extensions::ExtensionId& extension_id,
    ash::file_system_provider::MountOptions options,
    std::unique_ptr<ash::file_system_provider::ProviderInterface> provider);

// Only call this after `MountProvidedFileSystem()`.
ash::file_system_provider::ProviderInterface* GetProvider(
    Profile* profile,
    const extensions::ExtensionId& extension_id);

// Mount a `FakeProvidedFileSystemOneDrive`.
FakeProvidedFileSystemOneDrive* MountFakeProvidedFileSystemOneDrive(
    Profile* profile);

// Only call this after `MountFakeProvidedFileSystemOneDrive()`.
FakeExtensionProviderOneDrive* GetFakeProviderOneDrive(Profile* profile);

}  // namespace test
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_TEST_UTIL_H_
