// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "base/memory/raw_ptr.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager_observer.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/common/chrome_paths.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
#include "extensions/browser/entry_info.h"
#include "extensions/browser/extension_system.h"
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

void FolderInMyFiles::AddWithName(const base::FilePath& file,
                                  const base::FilePath& new_base_name) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  files_.push_back(folder_.Append(new_base_name));
  base::CopyFile(file, files_.back());
}

OpenOperationResult FolderInMyFiles::Open(const base::FilePath& file) {
  const auto& it =
      base::ranges::find(files_, file.BaseName(), &base::FilePath::BaseName);
  EXPECT_FALSE(it == files_.end());
  if (it == files_.end()) {
    return platform_util::OPEN_FAILED_PATH_NOT_FOUND;
  }

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
  // AddDefaultComponentExtensions() is normally invoked during
  // ExtensionService::Init() which also invokes UninstallMigratedExtensions().
  // Invoke it here as well, otherwise migrated extensions will remain installed
  // for the duration of the test. Note this may result in immediately
  // uninstalling an extension just installed above.
  service->UninstallMigratedExtensionsForTest();
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

  void OnVolumeMounted(ash::MountError error_code,
                       const Volume& volume) override {
    on_mount_.Run();
  }

 private:
  raw_ptr<Profile, ExperimentalAsh> profile_;
  base::RepeatingClosure on_mount_;
};
}  // namespace

scoped_refptr<const extensions::Extension> InstallTestingChromeApp(
    Profile* profile,
    const char* test_path_ascii) {
  base::ScopedAllowBlockingForTesting allow_io;
  extensions::ChromeTestExtensionLoader loader(profile);

  base::FilePath path;
  CHECK(base::PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII(test_path_ascii);

  // ChromeTestExtensionLoader waits for the background page to load before
  // returning.
  auto extension = loader.LoadExtension(path);
  CHECK(extension);
  return extension;
}

base::WeakPtr<file_manager::Volume> InstallFileSystemProviderChromeApp(
    Profile* profile) {
  static constexpr char kFileSystemProviderFilesystemId[] =
      "test-image-provider-fs";
  base::RunLoop run_loop;

  // Incognito profiles don't have VolumeManager events (so a
  // file_manager::test::VolumeWaiter won't work with them). Switch to the
  // original profile in that case. For regular (not incognito) profiles,
  // p->GetOriginalProfile() returns p.
  Profile* profile_with_volume_manager_events = profile->GetOriginalProfile();

  file_manager::test::VolumeWaiter waiter(profile_with_volume_manager_events,
                                          run_loop.QuitClosure());
  auto extension = InstallTestingChromeApp(
      profile, "extensions/api_test/file_browser/image_provider");
  run_loop.Run();

  auto* volume_manager =
      file_manager::VolumeManager::Get(profile_with_volume_manager_events);
  CHECK(volume_manager);
  base::WeakPtr<file_manager::Volume> volume;
  for (auto& v : volume_manager->GetVolumeList()) {
    if (v->file_system_id() == kFileSystemProviderFilesystemId) {
      volume = v;
    }
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

  // Use fake URLs in this helper. This is needed because FindAppServiceTasks
  // uses the file extension found in the URL to do file extension matching.
  std::vector<GURL> file_urls;
  GURL url = GURL(base::JoinString(
      {"filesystem:https://site.com/isolated/foo", file.Extension()}, ""));
  CHECK(url.is_valid());
  file_urls.emplace_back(url);

  std::vector<file_tasks::FullTaskDescriptor> result;
  bool invoked_synchronously = false;
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<file_tasks::ResultingTasks> resulting_tasks) {
        result = std::move(resulting_tasks->tasks);
        invoked_synchronously = true;
      });

  FindAllTypesOfTasks(profile, entries, file_urls, {""}, callback);

  // MIME sniffing requires a run loop, but the mime type must be explicitly
  // available, and is provided in this helper.
  CHECK(invoked_synchronously);
  return result;
}

void AddFakeAppWithIntentFilters(
    const std::string& app_id,
    std::vector<apps::IntentFilterPtr> intent_filters,
    apps::AppType app_type,
    absl::optional<bool> handles_intents,
    apps::AppServiceProxy* app_service_proxy) {
  std::vector<apps::AppPtr> apps;
  auto app = std::make_unique<apps::App>(app_type, app_id);
  app->app_id = app_id;
  app->app_type = app_type;
  app->handles_intents = handles_intents;
  app->readiness = apps::Readiness::kReady;
  app->intent_filters = std::move(intent_filters);
  apps.push_back(std::move(app));
  app_service_proxy->AppRegistryCache().OnApps(
      std::move(apps), app_type, false /* should_notify_initialized */);
}

void AddFakeWebApp(const std::string& app_id,
                   const std::string& mime_type,
                   const std::string& file_extension,
                   const std::string& activity_label,
                   absl::optional<bool> handles_intents,
                   apps::AppServiceProxy* app_service_proxy) {
  std::vector<apps::IntentFilterPtr> filters;
  filters.push_back(apps_util::MakeFileFilterForView(mime_type, file_extension,
                                                     activity_label));
  AddFakeAppWithIntentFilters(app_id, std::move(filters), apps::AppType::kWeb,
                              handles_intents, app_service_proxy);
}

}  // namespace test
}  // namespace file_manager
