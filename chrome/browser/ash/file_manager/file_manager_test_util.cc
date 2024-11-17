// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_manager_test_util.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_observer.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
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

std::vector<storage::FileSystemURL> CopyTestFilesIntoMyFiles(
    Profile* profile,
    std::vector<std::string> file_names) {
  FolderInMyFiles folder(profile);
  base::FilePath test_data_path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path));

  for (const auto& file_name : file_names) {
    base::FilePath file_path =
        test_data_path.AppendASCII("chromeos/file_manager/" + file_name);
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(base::PathExists(file_path));
    }
    // Copy the file into MyFiles.
    folder.Add({file_path});
  }

  std::vector<storage::FileSystemURL> files;
  for (const auto& path_in_my_files : folder.files()) {
    GURL url;
    CHECK(util::ConvertAbsoluteFilePathToFileSystemUrl(
        profile, path_in_my_files, util::GetFileManagerURL(), &url));
    auto* file_system_context = util::GetFileManagerFileSystemContext(profile);
    files.push_back(file_system_context->CrackURLInFirstPartyContext(url));
  }
  return files;
}

void AddDefaultComponentExtensionsOnMainThread(Profile* profile) {
  CHECK(profile);

  extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  service->component_loader()->AddDefaultComponentExtensions(false);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // QuickOffice loads from rootfs at /usr/share/chromeos-assets/quickoffce
  // which does not exist on bots for tests, so load test version.
  base::FilePath data_dir;
  CHECK(base::PathService::Get(chrome::DIR_TEST_DATA, &data_dir));
  base::RunLoop run_loop;
  service->component_loader()->AddComponentFromDirWithManifestFilename(
      data_dir.Append("chromeos/file_manager/quickoffice"),
      extension_misc::kQuickOfficeComponentExtensionId,
      extensions::kManifestFilename, extensions::kManifestFilename,
      run_loop.QuitClosure());
  run_loop.Run();
#endif
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
  raw_ptr<Profile> profile_;
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
  return InstallFileSystemProviderChromeApp(
      profile, base::BindOnce(&InstallTestingChromeApp, profile)
                   .Then(base::BindOnce(
                       [](scoped_refptr<const extensions::Extension>) {})));
}

base::WeakPtr<file_manager::Volume> InstallFileSystemProviderChromeApp(
    Profile* profile,
    base::OnceCallback<void(const char*)> install_fn) {
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
  std::move(install_fn).Run("extensions/api_test/file_browser/image_provider");
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
  base::ScopedAllowBlockingForTesting allow_blocking;
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
    std::optional<bool> handles_intents,
    apps::AppServiceProxy* app_service_proxy) {
  std::vector<apps::AppPtr> apps;
  auto app = std::make_unique<apps::App>(app_type, app_id);
  app->app_id = app_id;
  app->app_type = app_type;
  app->handles_intents = handles_intents;
  app->readiness = apps::Readiness::kReady;
  app->intent_filters = std::move(intent_filters);
  apps.push_back(std::move(app));
  app_service_proxy->OnApps(std::move(apps), app_type,
                            false /* should_notify_initialized */);
}

void AddFakeWebApp(const std::string& app_id,
                   const std::string& mime_type,
                   const std::string& file_extension,
                   const std::string& activity_label,
                   std::optional<bool> handles_intents,
                   apps::AppServiceProxy* app_service_proxy) {
  std::vector<apps::IntentFilterPtr> filters;
  filters.push_back(apps_util::MakeFileFilterForView(mime_type, file_extension,
                                                     activity_label));
  AddFakeAppWithIntentFilters(app_id, std::move(filters), apps::AppType::kWeb,
                              handles_intents, app_service_proxy);
}

FakeSimpleDriveFs::FakeSimpleDriveFs(const base::FilePath& mount_path)
    : drivefs::FakeDriveFs(mount_path) {}

FakeSimpleDriveFs::~FakeSimpleDriveFs() = default;

void FakeSimpleDriveFs::SetMetadata(const drivefs::FakeMetadata& metadata) {
  alternate_urls_[metadata.path] = metadata.alternate_url;
}

void FakeSimpleDriveFs::GetMetadata(const base::FilePath& path,
                                    GetMetadataCallback callback) {
  if (!alternate_urls_.count(path)) {
    std::move(callback).Run(drive::FILE_ERROR_NOT_FOUND, nullptr);
    return;
  }
  auto metadata = drivefs::mojom::FileMetadata::New();
  metadata->alternate_url = alternate_urls_[path];
  // Fill the rest of `metadata` with default values.
  metadata->content_mime_type = "";
  const drivefs::mojom::Capabilities& capabilities = {};
  metadata->capabilities = capabilities.Clone();
  metadata->folder_feature = {};
  metadata->available_offline = false;
  metadata->shared = false;
  std::move(callback).Run(drive::FILE_ERROR_OK, std::move(metadata));
}

FakeSimpleDriveFsHelper::FakeSimpleDriveFsHelper(
    Profile* profile,
    const base::FilePath& mount_path)
    : drive::FakeDriveFsHelper(profile, mount_path),
      mount_path_(mount_path),
      fake_drivefs_(mount_path_) {}

base::RepeatingCallback<std::unique_ptr<drivefs::DriveFsBootstrapListener>()>
FakeSimpleDriveFsHelper::CreateFakeDriveFsListenerFactory() {
  return base::BindRepeating(&drivefs::FakeDriveFs::CreateMojoListener,
                             base::Unretained(&fake_drivefs_));
}

FakeProvidedFileSystemOneDrive::FakeProvidedFileSystemOneDrive(
    const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info)
    : FakeProvidedFileSystem(file_system_info) {}

ash::file_system_provider::AbortCallback
FakeProvidedFileSystemOneDrive::CreateFile(
    const base::FilePath& file_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  if (create_file_callback_) {
    std::move(create_file_callback_).Run();
  }
  if (create_file_error_ != base::File::Error::FILE_OK) {
    std::move(callback).Run(create_file_error_);
    return ash::file_system_provider::AbortCallback();
  }
  return FakeProvidedFileSystem::CreateFile(file_path, std::move(callback));
}

void FakeProvidedFileSystemOneDrive::SetCreateFileError(
    base::File::Error error) {
  create_file_error_ = error;
}

void FakeProvidedFileSystemOneDrive::SetCreateFileCallback(
    base::OnceClosure callback) {
  create_file_callback_ = std::move(callback);
}

void FakeProvidedFileSystemOneDrive::SetGetActionsError(
    base::File::Error error) {
  get_actions_error_ = error;
}

void FakeProvidedFileSystemOneDrive::SetReauthenticationRequired(
    bool reauthentication_required) {
  reauthentication_required_ = reauthentication_required;
}

ash::file_system_provider::AbortCallback
FakeProvidedFileSystemOneDrive::GetActions(
    const std::vector<base::FilePath>& entry_paths,
    GetActionsCallback callback) {
  ash::file_system_provider::Actions actions;
  // Expect only single entry requests.
  if (entry_paths.size() != 1) {
    std::move(callback).Run(actions, base::File::FILE_ERROR_NOT_FOUND);
    return ash::file_system_provider::AbortCallback();
  }
  // If root requested, return ODFS metadata.
  if (entry_paths[0].value() == ash::cloud_upload::kODFSMetadataQueryPath) {
    actions.push_back(
        {ash::cloud_upload::kUserEmailActionId, kSampleUserEmail1});
    actions.push_back({ash::cloud_upload::kReauthenticationRequiredId,
                       reauthentication_required_ ? "true" : "false"});
    actions.push_back(
        {ash::cloud_upload::kAccountStateId,
         reauthentication_required_ ? "REAUTHENTICATION_REQUIRED" : "NORMAL"});
    std::move(callback).Run(actions, base::File::FILE_OK);
    return ash::file_system_provider::AbortCallback();
  }
  // If `get_actions_error_` set, mock error for non-root entry.
  if (get_actions_error_ != base::File::Error::FILE_OK) {
    std::move(callback).Run(actions, get_actions_error_);
    return ash::file_system_provider::AbortCallback();
  }
  ash::file_system_provider::FakeEntry* entry = GetEntry(entry_paths[0]);
  // If no entry exists, return error.
  if (!entry) {
    std::move(callback).Run(actions, base::File::FILE_ERROR_NOT_FOUND);
    return ash::file_system_provider::AbortCallback();
  }
  // Otherwise, return `kODFSSampleUrl`.
  actions.push_back({ash::cloud_upload::kOneDriveUrlActionId, kODFSSampleUrl});

  std::move(callback).Run(actions, base::File::FILE_OK);
  return ash::file_system_provider::AbortCallback();
}

std::unique_ptr<ash::file_system_provider::ProviderInterface>
FakeExtensionProviderOneDrive::Create(
    const extensions::ExtensionId& extension_id) {
  ash::file_system_provider::Capabilities default_capabilities(
      false, false, false, extensions::SOURCE_NETWORK);
  return std::unique_ptr<ProviderInterface>(
      new FakeExtensionProviderOneDrive(extension_id, default_capabilities));
}

std::unique_ptr<ash::file_system_provider::ProvidedFileSystemInterface>
FakeExtensionProviderOneDrive::CreateProvidedFileSystem(
    Profile* profile,
    const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
    ash::file_system_provider::CacheManager* cache_manager) {
  DCHECK(profile);
  std::unique_ptr<FakeProvidedFileSystemOneDrive> fake_provided_file_system =
      std::make_unique<FakeProvidedFileSystemOneDrive>(file_system_info);
  return fake_provided_file_system;
}

bool FakeExtensionProviderOneDrive::RequestMount(
    Profile* profile,
    ash::file_system_provider::RequestMountCallback callback) {
  if (request_mount_impl_) {
    std::move(request_mount_impl_).Run(std::move(callback));
    return true;
  }
  return ash::file_system_provider::FakeExtensionProvider::RequestMount(
      profile, std::move(callback));
}

void FakeExtensionProviderOneDrive::SetRequestMountImpl(
    base::OnceCallback<void(ash::file_system_provider::RequestMountCallback)>
        callback) {
  request_mount_impl_ = std::move(callback);
}

FakeExtensionProviderOneDrive::FakeExtensionProviderOneDrive(
    const extensions::ExtensionId& extension_id,
    const ash::file_system_provider::Capabilities& capabilities)
    : FakeExtensionProvider(extension_id, capabilities) {}

FakeExtensionProviderOneDrive::~FakeExtensionProviderOneDrive() = default;

FakeProvidedFileSystemOneDrive::~FakeProvidedFileSystemOneDrive() = default;

ash::file_system_provider::ProvidedFileSystemInterface* MountProvidedFileSystem(
    Profile* profile,
    const extensions::ExtensionId& extension_id,
    ash::file_system_provider::MountOptions options,
    std::unique_ptr<ash::file_system_provider::ProviderInterface> provider) {
  // Create a fake provided file system.
  ash::file_system_provider::Service* service =
      ash::file_system_provider::Service::Get(profile);
  service->RegisterProvider(std::move(provider));
  ash::file_system_provider::ProviderId provider_id =
      ash::file_system_provider::ProviderId::CreateFromExtensionId(
          extension_id);
  EXPECT_EQ(base::File::FILE_OK,
            service->MountFileSystem(provider_id, options));

  // Get a pointer to the provided file system.
  std::vector<ash::file_system_provider::ProvidedFileSystemInfo> file_systems =
      service->GetProvidedFileSystemInfoList(provider_id);
  return service->GetProvidedFileSystem(provider_id, options.file_system_id);
}

ash::file_system_provider::ProviderInterface* GetProvider(
    Profile* profile,
    const extensions::ExtensionId& extension_id) {
  ash::file_system_provider::Service* service =
      ash::file_system_provider::Service::Get(profile);
  ash::file_system_provider::ProviderId provider_id =
      ash::file_system_provider::ProviderId::CreateFromExtensionId(
          extension_id);
  return service->GetProvider(provider_id);
}

FakeProvidedFileSystemOneDrive* MountFakeProvidedFileSystemOneDrive(
    Profile* profile) {
  ash::file_system_provider::MountOptions options(/*file_system_id=*/"odfs",
                                                  /*display_name=*/"ODFS");
  std::unique_ptr<ash::file_system_provider::ProviderInterface> provider =
      FakeExtensionProviderOneDrive::Create(extension_misc::kODFSExtensionId);
  return static_cast<test::FakeProvidedFileSystemOneDrive*>(
      MountProvidedFileSystem(profile, extension_misc::kODFSExtensionId,
                              options, std::move(provider)));
}

FakeExtensionProviderOneDrive* GetFakeProviderOneDrive(Profile* profile) {
  return static_cast<FakeExtensionProviderOneDrive*>(
      GetProvider(profile, extension_misc::kODFSExtensionId));
}

}  // namespace test
}  // namespace file_manager
