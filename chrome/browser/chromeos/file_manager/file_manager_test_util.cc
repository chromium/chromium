// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_test_util.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_observer.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/entry_info.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "net/base/mime_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using platform_util::OpenOperationResult;

namespace file_manager {
namespace test {

FolderInMyFiles::FolderInMyFiles(Profile* profile) : profile_(profile) {
  const base::FilePath root = util::GetMyFilesFolderForProfile(profile);
  VolumeManager::Get(profile)->RegisterDownloadsDirectoryForTesting(root);

  base::ScopedAllowBlockingForTesting allow_blocking;
  constexpr base::FilePath::CharType kPrefix[] = FILE_PATH_LITERAL("a_folder");
  CHECK(CreateTemporaryDirInDir(root, kPrefix, &folder_));
}

FolderInMyFiles::~FolderInMyFiles() = default;

void FolderInMyFiles::Add(const std::vector<base::FilePath>& files) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  for (const auto& file : files) {
    files_.push_back(folder_.Append(file.BaseName()));
    base::CopyFile(file, files_.back());
  }
}

OpenOperationResult FolderInMyFiles::Open(const base::FilePath& file) {
  const auto& it = std::find_if(files_.begin(), files_.end(),
                                [file](const base::FilePath& i) {
                                  return i.BaseName() == file.BaseName();
                                });
  EXPECT_FALSE(it == files_.end());
  if (it == files_.end())
    return platform_util::OPEN_FAILED_PATH_NOT_FOUND;

  const base::FilePath& path = *it;
  base::RunLoop run_loop;
  OpenOperationResult open_result;
  platform_util::OpenItem(
      profile_, path, platform_util::OPEN_FILE,
      base::BindLambdaForTesting([&](OpenOperationResult result) {
        open_result = result;
        run_loop.Quit();
      }));
  run_loop.Run();

  // On ChromeOS, the OpenOperationResult is determined in
  // OpenFileMimeTypeAfterTasksListed() which also invokes
  // ExecuteFileTaskForUrl(). For WebApps like chrome://media-app, that invokes
  // WebApps::LaunchAppWithFiles() via AppServiceProxy.
  // Depending how the mime type of |path| is determined (e.g. extension,
  // metadata sniffing), there may be a number of asynchronous steps involved
  // before the call to ExecuteFileTaskForUrl(). After that, the OpenItem
  // callback is invoked, which exits the RunLoop above.
  // That used to be enough to also launch a Browser for the WebApp. However,
  // since https://crrev.com/c/2121860, ExecuteFileTaskForUrl() goes through the
  // mojoAppService, so it's necessary to flush those calls for WebApps to open.
  apps::AppServiceProxyFactory::GetForProfileRedirectInIncognito(profile_)
      ->FlushMojoCallsForTesting();

  return open_result;
}

void FolderInMyFiles::Refresh() {
  constexpr bool kRecursive = false;
  files_.clear();

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FileEnumerator e(folder_, kRecursive, base::FileEnumerator::FILES);
  for (base::FilePath path = e.Next(); !path.empty(); path = e.Next()) {
    files_.push_back(path);
  }

  std::sort(files_.begin(), files_.end(),
            [](const base::FilePath& l, const base::FilePath& r) {
              return l.BaseName() < r.BaseName();
            });
}

void AddDefaultComponentExtensionsOnMainThread(Profile* profile) {
  CHECK(profile);

  extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  service->component_loader()->AddDefaultComponentExtensions(false);

  // The File Manager component extension should have been added for loading
  // into the user profile, but not into the sign-in profile.
  CHECK(extensions::ExtensionSystem::Get(profile)
            ->extension_service()
            ->component_loader()
            ->Exists(kFileManagerAppId));
  CHECK(!extensions::ExtensionSystem::Get(
             chromeos::ProfileHelper::GetSigninProfile())
             ->extension_service()
             ->component_loader()
             ->Exists(kFileManagerAppId));
}

namespace {
// Helper to exit a RunLoop the next time OnVolumeMounted() is invoked.
class VolumeWaiter : public VolumeManagerObserver {
 public:
  VolumeWaiter(Profile* profile, const base::RepeatingClosure& on_mount)
      : profile_(profile), on_mount_(on_mount) {
    VolumeManager::Get(profile_)->AddObserver(this);
  }

  ~VolumeWaiter() override {
    VolumeManager::Get(profile_)->RemoveObserver(this);
  }

  void OnVolumeMounted(chromeos::MountError error_code,
                       const Volume& volume) override {
    on_mount_.Run();
  }

 private:
  Profile* profile_;
  base::RepeatingClosure on_mount_;
};
}  // namespace

scoped_refptr<const extensions::Extension> InstallTestingChromeApp(
    Profile* profile,
    const char* test_path_ascii) {
  base::ScopedAllowBlockingForTesting allow_io;
  content::WindowedNotificationObserver handler_ready(
      extensions::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
      content::NotificationService::AllSources());
  extensions::ChromeTestExtensionLoader loader(profile);

  base::FilePath path;
  CHECK(base::PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII(test_path_ascii);

  auto extension = loader.LoadExtension(path);
  CHECK(extension);
  handler_ready.Wait();
  return extension;
}

base::WeakPtr<file_manager::Volume> InstallFileSystemProviderChromeApp(
    Profile* profile) {
  static constexpr char kFileSystemProviderFilesystemId[] =
      "test-image-provider-fs";
  base::RunLoop run_loop;
  file_manager::test::VolumeWaiter waiter(profile, run_loop.QuitClosure());
  auto extension = InstallTestingChromeApp(
      profile, "extensions/api_test/file_browser/image_provider");
  run_loop.Run();

  auto* volume_manager = file_manager::VolumeManager::Get(profile);
  CHECK(volume_manager);
  base::WeakPtr<file_manager::Volume> volume;
  for (auto& v : volume_manager->GetVolumeList()) {
    if (v->file_system_id() == kFileSystemProviderFilesystemId)
      volume = v;
  }

  CHECK(volume);
  return volume;
}

std::vector<file_tasks::FullTaskDescriptor> GetTasksForFile(
    Profile* profile,
    const base::FilePath& file) {
  std::string mime_type;
  net::GetMimeTypeFromFile(file, &mime_type);
  CHECK(!mime_type.empty());

  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(file, mime_type, false);

  // Use empty URLs in this helper (i.e. only support files backed by volumes).
  // FindExtensionAndAppTasks() does not pass them to FindWebTasks() or
  // FindFileHandlerTasks() in any case.
  std::vector<GURL> file_urls(entries.size(), GURL());

  std::vector<file_tasks::FullTaskDescriptor> result;
  bool invoked_synchronously = false;
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<std::vector<file_tasks::FullTaskDescriptor>> tasks) {
        result = *tasks;
        invoked_synchronously = true;
      });

  FindAllTypesOfTasks(profile, entries, file_urls, callback);

  // MIME sniffing requires a run loop, but the mime type must be explicitly
  // available, and is provided in this helper.
  CHECK(invoked_synchronously);
  return result;
}

}  // namespace test
}  // namespace file_manager
