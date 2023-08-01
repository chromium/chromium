// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"

#include <sys/types.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/file_utils.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/policy/dlp/dlp_extract_io_task_observer.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_event_storage.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/enterprise/data_controls/component.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/recursive_operation_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace policy {
namespace {

// Timeout defining when two events having the same properties are considered
// duplicates.
// TODO(crbug.com/1368982): determine the value to use.
constexpr base::TimeDelta kCooldownTimeout = base::Seconds(5);

// The maximum number of entries that can be kept in the
// DlpFilesEventStorage.
// TODO(crbug.com/1366299): determine the value to use.
constexpr size_t kEntriesLimit = 100;

// FileSystemContext instance set for testing.
storage::FileSystemContext* g_file_system_context_for_testing = nullptr;

// Returns true if `file_path` is in My Files directory.
bool IsInLocalFileSystem(const base::FilePath& file_path) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto my_files_folder =
      file_manager::util::GetMyFilesFolderForProfile(profile);
  if (my_files_folder == file_path || my_files_folder.IsParent(file_path)) {
    return true;
  }
  return false;
}

// Returns a `DlpFileDestination` with a source URL or component, based on
// |app_update|. If neither URL nor component can be found, returns nullopt.
absl::optional<DlpFileDestination> GetFileDestinationForApp(
    const apps::AppUpdate& app_update) {
  switch (app_update.AppType()) {
    case apps::AppType::kStandaloneBrowserChromeApp:
    case apps::AppType::kExtension:
    case apps::AppType::kStandaloneBrowserExtension:
    case apps::AppType::kChromeApp:
      return DlpFileDestination(GURL(base::StrCat(
          {extensions::kExtensionScheme, "://", app_update.AppId()})));
    case apps::AppType::kArc:
      return DlpFileDestination(data_controls::Component::kArc);
    case apps::AppType::kCrostini:
      return DlpFileDestination(data_controls::Component::kCrostini);
    case apps::AppType::kPluginVm:
      return DlpFileDestination(data_controls::Component::kPluginVm);
    case apps::AppType::kWeb:
      // Expecting `PublisherId()` to return an URL. For web apps this should be
      // the start URL.
      return DlpFileDestination(GURL(app_update.PublisherId()));
    case apps::AppType::kUnknown:
    case apps::AppType::kBuiltIn:
    case apps::AppType::kMacOs:
    case apps::AppType::kStandaloneBrowser:
    case apps::AppType::kRemote:
    case apps::AppType::kBorealis:
    case apps::AppType::kBruschetta:
    case apps::AppType::kSystemWeb:
      return absl::nullopt;
  }
  return absl::nullopt;
}

// Returns |g_file_system_context_for_testing| if set, otherwise
// it returns FileSystemContext* for the primary profile.
storage::FileSystemContext* GetFileSystemContext() {
  if (g_file_system_context_for_testing) {
    return g_file_system_context_for_testing;
  }

  auto* primary_profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(primary_profile);
  return file_manager::util::GetFileManagerFileSystemContext(primary_profile);
}

// Gets all files inside |root| recursively and runs |callback_| with the
// files list.
class FolderRecursionDelegate : public storage::RecursiveOperationDelegate {
 public:
  using FileURLsCallback =
      base::OnceCallback<void(std::vector<storage::FileSystemURL>)>;

  FolderRecursionDelegate(storage::FileSystemContext* file_system_context,
                          const storage::FileSystemURL& root,
                          FileURLsCallback callback)
      : RecursiveOperationDelegate(file_system_context),
        root_(root),
        callback_(std::move(callback)) {}

  FolderRecursionDelegate(const FolderRecursionDelegate&) = delete;
  FolderRecursionDelegate& operator=(const FolderRecursionDelegate&) = delete;

  ~FolderRecursionDelegate() override = default;

  // RecursiveOperationDelegate:
  void Run() override { NOTREACHED(); }
  void RunRecursively() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    StartRecursiveOperation(*root_,
                            storage::FileSystemOperation::ERROR_BEHAVIOR_SKIP,
                            base::BindOnce(&FolderRecursionDelegate::Completed,
                                           weak_ptr_factory_.GetWeakPtr()));
  }
  void ProcessFile(const storage::FileSystemURL& url,
                   StatusCallback callback) override {
    file_system_context()->operation_runner()->GetMetadata(
        url, storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY,
        base::BindOnce(&FolderRecursionDelegate::OnGetMetadata,
                       weak_ptr_factory_.GetWeakPtr(), url,
                       std::move(callback)));
  }
  void ProcessDirectory(const storage::FileSystemURL& url,
                        StatusCallback callback) override {
    std::move(callback).Run(base::File::FILE_OK);
  }
  void PostProcessDirectory(const storage::FileSystemURL& url,
                            StatusCallback callback) override {
    std::move(callback).Run(base::File::FILE_OK);
  }

 private:
  void OnGetMetadata(const storage::FileSystemURL& url,
                     StatusCallback callback,
                     base::File::Error result,
                     const base::File::Info& file_info) {
    if (result != base::File::FILE_OK) {
      std::move(callback).Run(result);
      return;
    }
    if (file_info.is_directory) {
      std::move(callback).Run(base::File::FILE_ERROR_NOT_A_FILE);
      return;
    }
    files_urls_.push_back(url);
    std::move(callback).Run(base::File::FILE_OK);
  }

  void Completed(base::File::Error result) {
    std::move(callback_).Run(std::move(files_urls_));
  }

  const raw_ref<const storage::FileSystemURL, ExperimentalAsh> root_;
  FileURLsCallback callback_;
  std::vector<storage::FileSystemURL> files_urls_;

  base::WeakPtrFactory<FolderRecursionDelegate> weak_ptr_factory_{this};
};

// Gets all files inside |roots| recursively and runs |callback_| with the
// whole files list. Deletes itself after |callback_| is run.
// TODO(crbug.com/1378202): Extract RootsRecursionDelegate to another file to
// have better testing coverage.
class RootsRecursionDelegate {
 public:
  RootsRecursionDelegate(storage::FileSystemContext* file_system_context,
                         std::vector<storage::FileSystemURL> roots,
                         FolderRecursionDelegate::FileURLsCallback callback)
      : file_system_context_(file_system_context),
        roots_(std::move(roots)),
        callback_(std::move(callback)) {}

  RootsRecursionDelegate(const RootsRecursionDelegate&) = delete;
  RootsRecursionDelegate& operator=(const RootsRecursionDelegate&) = delete;

  ~RootsRecursionDelegate() = default;

  // Starts getting all files inside |roots| recursively.
  void Run() {
    for (const auto& root : roots_) {
      auto recursion_delegate = std::make_unique<FolderRecursionDelegate>(
          file_system_context_, root,
          base::BindOnce(&RootsRecursionDelegate::Completed,
                         weak_ptr_factory_.GetWeakPtr()));
      recursion_delegate->RunRecursively();
      delegates_.push_back(std::move(recursion_delegate));
    }
  }

  // Runs |callback_| when all files are ready.
  void Completed(std::vector<storage::FileSystemURL> files_urls) {
    counter_++;
    files_urls_.insert(std::end(files_urls_), std::begin(files_urls),
                       std::end(files_urls));
    if (counter_ == roots_.size()) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback_), std::move(files_urls_)));
      content::GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE, this);
    }
  }

 private:
  // counts the number of |roots| processed.
  uint counter_ = 0;
  raw_ptr<storage::FileSystemContext, ExperimentalAsh> file_system_context_ =
      nullptr;
  const std::vector<storage::FileSystemURL> roots_;
  FolderRecursionDelegate::FileURLsCallback callback_;
  std::vector<storage::FileSystemURL> files_urls_;
  std::vector<std::unique_ptr<FolderRecursionDelegate>> delegates_;

  base::WeakPtrFactory<RootsRecursionDelegate> weak_ptr_factory_{this};
};

// Converts files paths to file system URLs.
std::vector<storage::FileSystemURL> ConvertFilePathsToFileSystemUrls(
    const std::vector<base::FilePath>& files_paths) {
  std::vector<storage::FileSystemURL> file_system_urls;

  auto* file_system_context = GetFileSystemContext();
  if (!file_system_context) {
    return file_system_urls;
  }

  auto* profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(profile);

  for (const auto& file_path : files_paths) {
    GURL gurl;
    if (file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
            profile, file_path, file_manager::util::GetFileManagerURL(),
            &gurl)) {
      file_system_urls.push_back(
          file_system_context->CrackURLInFirstPartyContext(gurl));
    }
  }

  return file_system_urls;
}

// Converts DataTransferEndpoint object to DlpFileDestination.
DlpFileDestination DTEndpointToFileDestination(
    const ui::DataTransferEndpoint* endpoint) {
  DCHECK(endpoint);

  switch (endpoint->type()) {
    case ui::EndpointType::kUrl:
      DCHECK(endpoint->GetURL());
      return DlpFileDestination(*endpoint->GetURL());

    case ui::EndpointType::kArc:
      return DlpFileDestination(data_controls::Component::kArc);

    case ui::EndpointType::kCrostini:
      return DlpFileDestination(data_controls::Component::kCrostini);

    case ui::EndpointType::kPluginVm:
      return DlpFileDestination(data_controls::Component::kPluginVm);

    case ui::EndpointType::kLacros:
    case ui::EndpointType::kDefault:
    case ui::EndpointType::kClipboardHistory:
    case ui::EndpointType::kBorealis:
    case ui::EndpointType::kUnknownVm:
      return DlpFileDestination(data_controls::Component::kUnknownComponent);
  }
}

// Shows DLP block desktop notification.
void ShowDlpBlockedFiles(
    absl::optional<file_manager::io_task::IOTaskId> task_id,
    std::vector<base::FilePath> blocked_files,
    dlp::FileAction action) {
  auto* profile = ProfileManager::GetPrimaryUserProfile();
  CHECK(profile);

  auto* fpnm =
      FilesPolicyNotificationManagerFactory::GetForBrowserContext(profile);
  if (!fpnm) {
    LOG(ERROR) << "No FilesPolicyNotificationManager instantiated,"
                  "can't show policy block UI";
    return;
  }

  fpnm->ShowDlpBlockedFiles(std::move(task_id), std::move(blocked_files),
                            action);
}

file_manager::VolumeManager* GetVolumeManager() {
  auto* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile) {
    // May not be available in some tests.
    CHECK_IS_TEST();
    return nullptr;
  }

  file_manager::VolumeManager* const volume_manager =
      file_manager::VolumeManager::Get(profile);
  if (!volume_manager) {
    return nullptr;
  }
  return volume_manager;
}

}  // namespace

// static
DlpFilesControllerAsh* DlpFilesControllerAsh::GetForPrimaryProfile() {
  DlpRulesManager* rules_manager =
      DlpRulesManagerFactory::GetForPrimaryProfile();
  if (!rules_manager) {
    return nullptr;
  }
  return static_cast<DlpFilesControllerAsh*>(
      rules_manager ? rules_manager->GetDlpFilesController() : nullptr);
}

DlpFilesControllerAsh::DlpFileMetadata::DlpFileMetadata(
    const std::string& source_url,
    const std::string& referrer_url,
    bool is_dlp_restricted,
    bool is_restricted_for_destination)
    : source_url(source_url),
      referrer_url(referrer_url),
      is_dlp_restricted(is_dlp_restricted),
      is_restricted_for_destination(is_restricted_for_destination) {}

DlpFilesControllerAsh::DlpFileRestrictionDetails::DlpFileRestrictionDetails() =
    default;

DlpFilesControllerAsh::DlpFileRestrictionDetails::DlpFileRestrictionDetails(
    DlpFileRestrictionDetails&&) = default;
DlpFilesControllerAsh::DlpFileRestrictionDetails&
DlpFilesControllerAsh::DlpFileRestrictionDetails::operator=(
    DlpFilesControllerAsh::DlpFileRestrictionDetails&&) = default;

DlpFilesControllerAsh::DlpFileRestrictionDetails::~DlpFileRestrictionDetails() =
    default;

DlpFilesControllerAsh::DlpFilesControllerAsh(
    const DlpRulesManager& rules_manager)
    : DlpFilesController(rules_manager),
      event_storage_(std::make_unique<DlpFilesEventStorage>(kCooldownTimeout,
                                                            kEntriesLimit)) {
  auto* volume_manager = GetVolumeManager();
  if (!volume_manager) {
    LOG(ERROR)
        << "DlpFilesControllerAsh failed to find file_manager::VolumeManager";
    return;
  }

  volume_manager->AddObserver(this);

  auto* io_task_controller = volume_manager->io_task_controller();
  if (!io_task_controller) {
    LOG(ERROR) << "DlpFilesControllerAsh failed to find "
                  "file_manager::io_task::IOTaskController";
    return;
  }
  extract_io_task_observer_ =
      std::make_unique<DlpExtractIOTaskObserver>(*io_task_controller);
}

DlpFilesControllerAsh::~DlpFilesControllerAsh() {
  if (extract_io_task_observer_) {
    // If `extract_io_task_observer_` is still alive, it means we are deleting
    // FilesController before VolumeManager, otherwise we would have been
    // notified in `OnShutdownStart`.
    auto* volume_manager = GetVolumeManager();
    CHECK(volume_manager);
    volume_manager->RemoveObserver(this);
  }
}

void DlpFilesControllerAsh::CheckIfTransferAllowed(
    absl::optional<file_manager::io_task::IOTaskId> task_id,
    const std::vector<storage::FileSystemURL>& transferred_files,
    storage::FileSystemURL destination,
    bool is_move,
    CheckIfTransferAllowedCallback result_callback) {
  auto* file_system_context = GetFileSystemContext();
  if (!file_system_context) {
    std::move(result_callback).Run(std::vector<storage::FileSystemURL>());
    return;
  }

  // If the destination file path is in My Files, all files transfers should be
  // allowed.
  if (IsInLocalFileSystem(destination.path())) {
    std::move(result_callback).Run(std::vector<storage::FileSystemURL>());
    return;
  }

  std::vector<storage::FileSystemURL> filtered_files;
  // If the copied file isn't in the local file system, or the file is in the
  // same file system as the destination, no restrictions should be applied.
  for (const auto& file : transferred_files) {
    if (!IsInLocalFileSystem(file.path()) ||
        file.IsInSameFileSystem(destination)) {
      continue;
    }
    filtered_files.push_back(file);
  }
  if (filtered_files.empty()) {
    std::move(result_callback).Run(std::vector<storage::FileSystemURL>());
    return;
  }

  auto* roots_recursion_delegate = new RootsRecursionDelegate(
      file_system_context, std::move(filtered_files),
      base::BindOnce(&DlpFilesControllerAsh::ContinueCheckIfTransferAllowed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(task_id),
                     std::move(destination), is_move,
                     std::move(result_callback)));
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&RootsRecursionDelegate::Run,
                     // base::Unretained() is safe since |recursion_delegate|
                     // will delete itself after all the files list if ready.
                     base::Unretained(roots_recursion_delegate)));
}

void DlpFilesControllerAsh::GetDlpMetadata(
    const std::vector<storage::FileSystemURL>& files,
    absl::optional<DlpFileDestination> destination,
    GetDlpMetadataCallback result_callback) {
  if (!chromeos::DlpClient::Get() || !chromeos::DlpClient::Get()->IsAlive()) {
    std::move(result_callback).Run(std::vector<DlpFileMetadata>());
    return;
  }

  ::dlp::GetFilesSourcesRequest request;
  for (const auto& file : files) {
    if (IsInLocalFileSystem(file.path())) {
      request.add_files_paths(file.path().value());
    }
  }
  chromeos::DlpClient::Get()->GetFilesSources(
      request, base::BindOnce(&DlpFilesControllerAsh::ReturnDlpMetadata,
                              weak_ptr_factory_.GetWeakPtr(), std::move(files),
                              destination, std::move(result_callback)));
}

void DlpFilesControllerAsh::FilterDisallowedUploads(
    std::vector<ui::SelectedFileInfo> selected_files,
    const DlpFileDestination& destination,
    FilterDisallowedUploadsCallback result_callback) {
  if (selected_files.empty()) {
    std::move(result_callback).Run(std::move(selected_files));
    return;
  }

  std::vector<base::FilePath> files_paths;
  for (const auto& file : selected_files) {
    files_paths.push_back(file.local_path.empty() ? file.file_path
                                                  : file.local_path);
  }

  std::vector<storage::FileSystemURL> file_system_urls =
      ConvertFilePathsToFileSystemUrls(files_paths);

  if (file_system_urls.empty()) {
    std::move(result_callback).Run(std::move(selected_files));
    return;
  }

  auto* file_system_context = GetFileSystemContext();
  if (!file_system_context) {
    std::move(result_callback).Run(std::move(selected_files));
    return;
  }

  auto* roots_recursion_delegate = new RootsRecursionDelegate(
      file_system_context, std::move(file_system_urls),
      base::BindOnce(&DlpFilesControllerAsh::ContinueFilterDisallowedUploads,
                     weak_ptr_factory_.GetWeakPtr(), std::move(selected_files),
                     std::move(destination), std::move(result_callback)));
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&RootsRecursionDelegate::Run,
                     // base::Unretained() is safe since |recursion_delegate|
                     // will delete itself after all the files list if ready.
                     base::Unretained(roots_recursion_delegate)));
}

void DlpFilesControllerAsh::CheckIfDownloadAllowed(
    const DlpFileDestination& download_src,
    const base::FilePath& file_path,
    CheckIfDlpAllowedCallback result_callback) {
  auto* profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(profile);

  auto dst_component =
      MapFilePathtoPolicyComponent(profile, base::FilePath(file_path));
  if (!dst_component.has_value()) {
    // We may block downloads only if saved to external component, otherwise
    // downloads should be allowed.
    std::move(result_callback).Run(true);
    return;
  }

  if (!download_src.url().has_value()) {
    // Currently we only support urls as sources.
    std::move(result_callback).Run(true);
    return;
  }

  // TODO(b/290200170): Check whether referrer_url could be set too.
  FileDaemonInfo file_info({}, {}, file_path, download_src.url()->spec(),
                           /*referrer_url=*/"");

  absl::optional<data_controls::Component> component =
      MapFilePathtoPolicyComponent(profile, file_path);
  DlpFileDestination dlp_destination =
      component ? DlpFileDestination(*component) : DlpFileDestination();
  IsFilesTransferRestricted(
      absl::nullopt, {std::move(file_info)}, dlp_destination,
      dlp::FileAction::kDownload,
      base::BindOnce(
          [](CheckIfDlpAllowedCallback result_callback,
             const std::vector<std::pair<
                 FileDaemonInfo, ::dlp::RestrictionLevel>>& files_levels) {
            bool is_allowed = true;
            base::FilePath file_path;
            for (const auto& [file, level] : files_levels) {
              if (level == ::dlp::RestrictionLevel::LEVEL_BLOCK ||
                  level == ::dlp::RestrictionLevel::LEVEL_WARN_CANCEL) {
                is_allowed = false;
                file_path = file.path;
                break;
              }
            }
            if (!is_allowed) {
              ShowDlpBlockedFiles(/*task_id=*/absl::nullopt, {file_path},
                                  dlp::FileAction::kDownload);
            }
            std::move(result_callback).Run(is_allowed);
          },
          std::move(result_callback)));
}

bool DlpFilesControllerAsh::ShouldPromptBeforeDownload(
    const DlpFileDestination& download_src,
    const base::FilePath& file_path) {
  if (download_src.IsFileSystem()) {
    return false;
  }
  auto* profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(profile);
  auto dst_component =
      MapFilePathtoPolicyComponent(profile, base::FilePath(file_path));
  if (!dst_component.has_value()) {
    // We may block downloads only if saved to external component, otherwise
    // downloads should be allowed.
    return false;
  }

  DlpRulesManager::Level level = rules_manager_->IsRestrictedComponent(
      download_src.url().value(), dst_component.value(),
      DlpRulesManager::Restriction::kFiles, /*out_source_pattern=*/nullptr,
      /*out_rule_metadata=*/nullptr);
  return level == DlpRulesManager::Level::kBlock ||
         level == DlpRulesManager::Level::kWarn;
}

void DlpFilesControllerAsh::CheckIfLaunchAllowed(
    const apps::AppUpdate& app_update,
    apps::IntentPtr intent,
    CheckIfDlpAllowedCallback result_callback) {
  if (intent->files.empty()) {
    std::move(result_callback).Run(/*is_allowed=*/true);
    return;
  }
  auto* profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(profile);
  ::dlp::CheckFilesTransferRequest request;
  for (const auto& file : intent->files) {
    auto file_url = apps::GetFileSystemURL(profile, file->url);
    request.add_files_paths(file_url.path().value());
  }

  request.set_file_action(intent->IsShareIntent() ? ::dlp::FileAction::SHARE
                                                  : ::dlp::FileAction::OPEN);

  absl::optional<DlpFileDestination> destination =
      GetFileDestinationForApp(app_update);
  if (destination.has_value()) {
    if (destination->url().has_value()) {
      request.set_destination_url(destination->url()->spec());
    } else if (destination->component().has_value()) {
      request.set_destination_component(
          dlp::MapPolicyComponentToProto(destination->component().value()));
    }
  }

  chromeos::DlpClient::Get()->CheckFilesTransfer(
      request,
      base::BindOnce(&DlpFilesControllerAsh::ReturnIfActionAllowed,
                     weak_ptr_factory_.GetWeakPtr(), dlp::FileAction::kOpen,
                     std::move(result_callback)));
}

bool DlpFilesControllerAsh::IsLaunchBlocked(const apps::AppUpdate& app_update,
                                            const apps::IntentPtr& intent) {
  if (intent->files.empty()) {
    return false;
  }

  absl::optional<DlpFileDestination> destination =
      GetFileDestinationForApp(app_update);

  if (!destination.has_value()) {
    return false;
  }

  for (const auto& file : intent->files) {
    if (!file->dlp_source_url.has_value()) {
      continue;
    }
    if (destination->url().has_value()) {
      DlpRulesManager::Level level = rules_manager_->IsRestrictedDestination(
          GURL(file->dlp_source_url.value()), *destination->url(),
          DlpRulesManager::Restriction::kFiles, /*out_source_pattern=*/nullptr,
          /*out_destination_pattern=*/nullptr, /*out_rule_metadata=*/nullptr);
      if (level == DlpRulesManager::Level::kBlock) {
        return true;
      }
    } else if (destination->component().has_value()) {
      DlpRulesManager::Level level = rules_manager_->IsRestrictedComponent(
          GURL(file->dlp_source_url.value()), destination->component().value(),
          DlpRulesManager::Restriction::kFiles, /*out_source_pattern=*/nullptr,
          /*out_rule_metadata=*/nullptr);
      if (level == DlpRulesManager::Level::kBlock) {
        return true;
      }
    }
  }

  return false;
}

void DlpFilesControllerAsh::IsFilesTransferRestricted(
    absl::optional<file_manager::io_task::IOTaskId> task_id,
    const std::vector<FileDaemonInfo>& transferred_files,
    const DlpFileDestination& destination,
    dlp::FileAction files_action,
    IsFilesTransferRestrictedCallback result_callback) {
  auto* profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(profile);

  DlpFileDestination actual_dst = destination;

  std::vector<std::pair<FileDaemonInfo, ::dlp::RestrictionLevel>> files_levels;
  std::vector<FileDaemonInfo> warned_files;
  absl::optional<std::string> destination_pattern;
  std::vector<std::string> warned_source_patterns;
  std::vector<DlpRulesManager::RuleMetadata> warned_rules_metadata;
  for (const auto& file : transferred_files) {
    DlpRulesManager::Level level;
    std::string source_pattern;
    DlpRulesManager::RuleMetadata rule_metadata;
    if (destination.component()) {
      data_controls::Component dst_component = *destination.component();
      level = rules_manager_->IsRestrictedComponent(
          GURL(file.source_url), dst_component,
          DlpRulesManager::Restriction::kFiles, &source_pattern,
          &rule_metadata);
      actual_dst = DlpFileDestination(dst_component);
      MaybeReportEvent(file.inode, file.crtime, file.path, source_pattern,
                       actual_dst, absl::nullopt, rule_metadata, level);
    } else if (destination.IsFileSystem()) {
      level = DlpRulesManager::Level::kAllow;
    } else {
      DCHECK(destination.url().has_value());
      destination_pattern = std::string();
      level = rules_manager_->IsRestrictedDestination(
          GURL(file.source_url), GURL(*destination.url()),
          DlpRulesManager::Restriction::kFiles, &source_pattern,
          &destination_pattern.value(), &rule_metadata);
      MaybeReportEvent(file.inode, file.crtime, file.path, source_pattern,
                       actual_dst, destination_pattern, rule_metadata, level);
    }

    switch (level) {
      case DlpRulesManager::Level::kBlock: {
        files_levels.emplace_back(file, ::dlp::RestrictionLevel::LEVEL_BLOCK);
        DlpHistogramEnumeration(dlp::kFileActionBlockedUMA, files_action);
        break;
      }
      case DlpRulesManager::Level::kNotSet:
      case DlpRulesManager::Level::kAllow: {
        files_levels.emplace_back(file, ::dlp::RestrictionLevel::LEVEL_ALLOW);
        break;
      }
      case DlpRulesManager::Level::kReport: {
        files_levels.emplace_back(file, ::dlp::RestrictionLevel::LEVEL_REPORT);
        break;
      }
      case DlpRulesManager::Level::kWarn: {
        warned_files.push_back(file);
        warned_source_patterns.emplace_back(source_pattern);
        warned_rules_metadata.emplace_back(rule_metadata);
        DlpHistogramEnumeration(dlp::kFileActionWarnedUMA, files_action);
        break;
      }
    }
  }

  if (warned_files.empty()) {
    std::move(result_callback).Run(std::move(files_levels));
    return;
  }

  auto* fpnm =
      FilesPolicyNotificationManagerFactory::GetForBrowserContext(profile);
  if (!fpnm) {
    LOG(ERROR) << "No FilesPolicyNotificationManager instantiated,"
                  "can't show policy warning UI";
    return;
  }
  std::vector<base::FilePath> warning_files_paths;
  base::ranges::for_each(warned_files, [&](auto& warned_file) {
    warning_files_paths.push_back(warned_file.path);
  });
  fpnm->ShowDlpWarning(
      base::BindOnce(&DlpFilesControllerAsh::OnDlpWarnDialogReply,
                     weak_ptr_factory_.GetWeakPtr(), std::move(files_levels),
                     std::move(warned_files), std::move(warned_source_patterns),
                     std::move(warned_rules_metadata), actual_dst,
                     destination_pattern, files_action,
                     std::move(result_callback)),
      std::move(task_id), std::move(warning_files_paths), actual_dst,
      files_action);
}

std::vector<DlpFilesControllerAsh::DlpFileRestrictionDetails>
DlpFilesControllerAsh::GetDlpRestrictionDetails(const std::string& source_url) {
  const GURL source(source_url);
  const DlpRulesManager::AggregatedDestinations aggregated_destinations =
      rules_manager_->GetAggregatedDestinations(
          source, DlpRulesManager::Restriction::kFiles);
  const DlpRulesManager::AggregatedComponents aggregated_components =
      rules_manager_->GetAggregatedComponents(
          source, DlpRulesManager::Restriction::kFiles);

  std::vector<DlpFilesControllerAsh::DlpFileRestrictionDetails> result;
  // Add levels for which urls are set.
  for (const auto& [level, urls] : aggregated_destinations) {
    DlpFileRestrictionDetails details;
    details.level = level;
    base::ranges::move(urls.begin(), urls.end(),
                       std::back_inserter(details.urls));
    // Add the components for this level, if any.
    const auto it = aggregated_components.find(level);
    if (it != aggregated_components.end()) {
      base::ranges::move(it->second.begin(), it->second.end(),
                         std::back_inserter(details.components));
    }
    result.emplace_back(std::move(details));
  }

  // There might be levels for which only components are set, so we need to add
  // those separately.
  for (const auto& [level, components] : aggregated_components) {
    if (aggregated_destinations.find(level) != aggregated_destinations.end()) {
      // Already added in the previous loop.
      continue;
    }
    DlpFileRestrictionDetails details;
    details.level = level;
    base::ranges::move(components.begin(), components.end(),
                       std::back_inserter(details.components));
    result.emplace_back(std::move(details));
  }

  return result;
}

std::vector<data_controls::Component>
DlpFilesControllerAsh::GetBlockedComponents(const std::string& source_url) {
  const GURL source(source_url);
  const DlpRulesManager::AggregatedComponents aggregated_components =
      rules_manager_->GetAggregatedComponents(
          source, DlpRulesManager::Restriction::kFiles);

  std::vector<data_controls::Component> result;
  const auto it = aggregated_components.find(DlpRulesManager::Level::kBlock);
  if (it != aggregated_components.end()) {
    base::ranges::move(it->second.begin(), it->second.end(),
                       std::back_inserter(result));
  }
  return result;
}

bool DlpFilesControllerAsh::IsDlpPolicyMatched(const FileDaemonInfo& file) {
  bool restricted = false;

  std::string src_pattern;
  DlpRulesManager::RuleMetadata rule_metadata;
  policy::DlpRulesManager::Level level = rules_manager_->IsRestrictedByAnyRule(
      GURL(file.source_url.spec()),
      policy::DlpRulesManager::Restriction::kFiles, &src_pattern,
      &rule_metadata);

  switch (level) {
    case policy::DlpRulesManager::Level::kBlock:
      restricted = true;
      DlpHistogramEnumeration(dlp::kFileActionBlockedUMA,
                              dlp::FileAction::kUnknown);
      break;
    case policy::DlpRulesManager::Level::kWarn:
      DlpHistogramEnumeration(dlp::kFileActionWarnedUMA,
                              dlp::FileAction::kUnknown);
      // TODO(crbug.com/1172959): Implement Warning mode for Files restriction
      break;
    default:
      break;
  }

  MaybeReportEvent(
      file.inode, file.crtime, file.path, src_pattern,
      DlpFileDestination(data_controls::Component::kUnknownComponent),
      absl::nullopt, rule_metadata, level);

  return restricted;
}

void DlpFilesControllerAsh::CheckIfDropAllowed(
    const std::vector<ui::FileInfo>& dropped_files,
    const ui::DataTransferEndpoint* data_dst,
    CheckIfDlpAllowedCallback result_callback) {
  std::vector<base::FilePath> files_paths;
  for (const auto& file : dropped_files) {
    if (!IsInLocalFileSystem(file.path)) {
      continue;
    }
    files_paths.push_back(file.path);
  }

  std::vector<storage::FileSystemURL> files_urls =
      ConvertFilePathsToFileSystemUrls(files_paths);
  if (files_urls.empty()) {
    std::move(result_callback).Run(/*is_allowed=*/true);
    return;
  }

  DlpFileDestination destination = DTEndpointToFileDestination(data_dst);

  auto* file_system_context = GetFileSystemContext();
  if (!file_system_context) {
    std::move(result_callback).Run(/*is_allowed=*/true);
    return;
  }

  auto* roots_recursion_delegate = new RootsRecursionDelegate(
      file_system_context, std::move(files_urls),
      base::BindOnce(&DlpFilesControllerAsh::ContinueCheckIfDropAllowed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(destination),
                     std::move(result_callback)));
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&RootsRecursionDelegate::Run,
                     // base::Unretained() is safe since |recursion_delegate|
                     // will delete itself after all the files list if ready.
                     base::Unretained(roots_recursion_delegate)));
}

void DlpFilesControllerAsh::OnShutdownStart(
    file_manager::VolumeManager* volume_manager) {
  volume_manager->RemoveObserver(this);
  // IOTaskController is destroyed at VolumeManager deletion. Delete the
  // observer since it depends on IOTaskController.
  extract_io_task_observer_.reset();
}

DlpFilesEventStorage* DlpFilesControllerAsh::GetEventStorageForTesting() {
  return event_storage_.get();
}

void DlpFilesControllerAsh::SetFileSystemContextForTesting(
    storage::FileSystemContext* file_system_context) {
  g_file_system_context_for_testing = file_system_context;
}

absl::optional<data_controls::Component>
DlpFilesControllerAsh::MapFilePathtoPolicyComponent(
    Profile* profile,
    const base::FilePath& file_path) {
  if (base::FilePath(file_manager::util::GetAndroidFilesPath())
          .IsParent(file_path)) {
    return data_controls::Component::kArc;
  }

  if (base::FilePath(file_manager::util::kRemovableMediaPath)
          .IsParent(file_path)) {
    return data_controls::Component::kUsb;
  }

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (integration_service && integration_service->is_enabled() &&
      integration_service->GetMountPointPath().IsParent(file_path)) {
    return data_controls::Component::kDrive;
  }

  if (ash::cloud_upload::CloudUploadDialog::IsODFSMounted(profile)) {
    auto* service = ash::file_system_provider::Service::Get(profile);
    auto provider_id =
        ash::file_system_provider::ProviderId::CreateFromExtensionId(
            extension_misc::kODFSExtensionId);
    auto one_drive_file_systems =
        service->GetProvidedFileSystemInfoList(provider_id);
    CHECK(one_drive_file_systems.size() == 1);

    if (one_drive_file_systems[0].mount_path().IsParent(file_path)) {
      return data_controls::Component::kOneDrive;
    }
  }

  base::FilePath linux_files =
      file_manager::util::GetCrostiniMountDirectory(profile);
  if (linux_files == file_path || linux_files.IsParent(file_path)) {
    return data_controls::Component::kCrostini;
  }

  return {};
}

void DlpFilesControllerAsh::OnDlpWarnDialogReply(
    std::vector<std::pair<FileDaemonInfo, ::dlp::RestrictionLevel>>
        files_levels,
    std::vector<FileDaemonInfo> warned_files,
    std::vector<std::string> warned_src_patterns,
    std::vector<DlpRulesManager::RuleMetadata> warned_rules_metadata,
    const DlpFileDestination& dst,
    const absl::optional<std::string>& dst_pattern,
    dlp::FileAction files_action,
    IsFilesTransferRestrictedCallback callback,
    bool should_proceed) {
  DCHECK(warned_files.size() == warned_src_patterns.size());
  DCHECK(warned_files.size() == warned_rules_metadata.size());
  for (size_t i = 0; i < warned_files.size(); ++i) {
    if (should_proceed) {
      DlpHistogramEnumeration(dlp::kFileActionWarnProceededUMA, files_action);
      MaybeReportEvent(warned_files[i].inode, warned_files[i].crtime,
                       warned_files[i].path, warned_src_patterns[i], dst,
                       dst_pattern, warned_rules_metadata[i], absl::nullopt);
    }
    files_levels.emplace_back(warned_files[i],
                              should_proceed
                                  ? ::dlp::RestrictionLevel::LEVEL_WARN_PROCEED
                                  : ::dlp::RestrictionLevel::LEVEL_WARN_CANCEL);
  }
  std::move(callback).Run(std::move(files_levels));
}

void DlpFilesControllerAsh::ReturnDisallowedFiles(
    absl::optional<file_manager::io_task::IOTaskId> task_id,
    base::flat_map<std::string, storage::FileSystemURL> files_map,
    dlp::FileAction file_action,
    CheckIfTransferAllowedCallback result_callback,
    ::dlp::CheckFilesTransferResponse response) {
  std::vector<storage::FileSystemURL> restricted_files_urls;
  std::vector<base::FilePath> restricted_files_paths;
  if (response.has_error_message()) {
    LOG(ERROR) << "Failed to get check files transfer, error: "
               << response.error_message();
    for (const auto& [file_path, file_system_url] : files_map) {
      restricted_files_urls.push_back(file_system_url);
    }
    std::move(result_callback).Run(std::move(restricted_files_urls));
    return;
  }

  for (const auto& file : response.files_paths()) {
    DCHECK(files_map.find(file) != files_map.end());
    restricted_files_urls.push_back(files_map.at(file));
    restricted_files_paths.emplace_back(file);
  }
  if (!restricted_files_paths.empty() && kNewFilesPolicyUXEnabled &&
      task_id.has_value()) {
    ShowDlpBlockedFiles(std::move(task_id), std::move(restricted_files_paths),
                        file_action);
  }
  std::move(result_callback).Run(std::move(restricted_files_urls));
}

void DlpFilesControllerAsh::ReturnAllowedUploads(
    std::vector<ui::SelectedFileInfo> selected_files,
    FilterDisallowedUploadsCallback result_callback,
    ::dlp::CheckFilesTransferResponse response) {
  if (response.has_error_message()) {
    LOG(ERROR) << "Failed to get check files transfer, error: "
               << response.error_message();
    std::move(result_callback).Run(std::move(selected_files));
    return;
  }

  if (!response.files_paths().empty()) {
    std::vector<base::FilePath> restricted_files(response.files_paths().begin(),
                                                 response.files_paths().end());
    // If any of the selected files/folders is restricted or contains a
    // restricted file, it'll be removed.
    base::EraseIf(
        selected_files,
        [&restricted_files](const ui::SelectedFileInfo& selected_file) -> bool {
          return base::ranges::any_of(
              restricted_files, [&](const base::FilePath& restricted_file) {
                return selected_file.file_path == restricted_file ||
                       selected_file.file_path.IsParent(restricted_file);
              });
        });

    ShowDlpBlockedFiles(/*task_id=*/absl::nullopt, std::move(restricted_files),
                        dlp::FileAction::kUpload);
  }

  std::move(result_callback).Run(std::move(selected_files));
}

void DlpFilesControllerAsh::ReturnDlpMetadata(
    const std::vector<storage::FileSystemURL>& files,
    absl::optional<DlpFileDestination> destination,
    GetDlpMetadataCallback result_callback,
    const ::dlp::GetFilesSourcesResponse response) {
  if (response.has_error_message()) {
    LOG(ERROR) << "Failed to get files sources, error: "
               << response.error_message();
  }

  base::flat_map<std::string, DlpFileMetadata> metadata_map;
  for (const auto& metadata : response.files_metadata()) {
    DlpRulesManager::Level level = rules_manager_->IsRestrictedByAnyRule(
        GURL(metadata.source_url()), DlpRulesManager::Restriction::kFiles,
        nullptr, nullptr);
    bool is_dlp_restricted = level != DlpRulesManager::Level::kNotSet &&
                             level != DlpRulesManager::Level::kAllow;
    bool is_restricted_for_destination = false;
    // Only if it's restricted by any rule and the destination is passed, check
    // if this combination is also blocked or not.
    if (level == DlpRulesManager::Level::kBlock && destination.has_value()) {
      auto* profile = ProfileManager::GetPrimaryUserProfile();
      DCHECK(profile);
      absl::optional<data_controls::Component> dst_component =
          destination->component();
      if (dst_component.has_value()) {
        DlpRulesManager::Level dst_level =
            rules_manager_->IsRestrictedComponent(
                GURL(metadata.source_url()), dst_component.value(),
                DlpRulesManager::Restriction::kFiles, nullptr, nullptr);
        is_restricted_for_destination =
            dst_level == DlpRulesManager::Level::kBlock;
      } else {
        DCHECK(destination->url());
        DlpRulesManager::Level dst_level =
            rules_manager_->IsRestrictedDestination(
                GURL(metadata.source_url()), *destination->url(),
                DlpRulesManager::Restriction::kFiles, nullptr, nullptr,
                nullptr);
        is_restricted_for_destination =
            dst_level == DlpRulesManager::Level::kBlock;
      }
    }

    metadata_map.emplace(
        metadata.path(),
        DlpFileMetadata(metadata.source_url(), metadata.referrer_url(),
                        is_dlp_restricted, is_restricted_for_destination));
  }

  std::vector<DlpFileMetadata> result;
  for (const auto& file : files) {
    auto metadata_itr = metadata_map.find(file.path().value());
    if (metadata_itr == metadata_map.end()) {
      result.emplace_back("", "", false, false);
    } else {
      result.emplace_back(metadata_itr->second);
    }
  }

  std::move(result_callback).Run(std::move(result));
}

void DlpFilesControllerAsh::ReturnIfActionAllowed(
    dlp::FileAction action,
    CheckIfDlpAllowedCallback result_callback,
    ::dlp::CheckFilesTransferResponse response) {
  if (response.has_error_message()) {
    LOG(ERROR) << "Failed to get check files transfer, error: "
               << response.error_message();
    std::move(result_callback).Run(/*is_allowed=*/true);
    return;
  }

  if (response.files_paths().empty()) {
    std::move(result_callback).Run(/*is_allowed=*/true);
    return;
  }

  std::vector<base::FilePath> blocked_files(response.files_paths().begin(),
                                            response.files_paths().end());
  ShowDlpBlockedFiles(/*task_id=*/absl::nullopt, std::move(blocked_files),
                      action);
  std::move(result_callback).Run(/*is_allowed=*/false);
}

void DlpFilesControllerAsh::MaybeReportEvent(
    ino64_t inode,
    time_t crtime,
    const base::FilePath& path,
    const std::string& source_pattern,
    const DlpFileDestination& dst,
    const absl::optional<std::string>& dst_pattern,
    const DlpRulesManager::RuleMetadata& rule_metadata,
    absl::optional<DlpRulesManager::Level> level) {
  const bool is_warning_proceeded_event = !level.has_value();

  if (!is_warning_proceeded_event &&
      (level.value() == DlpRulesManager::Level::kAllow ||
       level.value() == DlpRulesManager::Level::kNotSet)) {
    return;
  }

  DlpReportingManager* reporting_manager =
      rules_manager_->GetReportingManager();
  if (!reporting_manager) {
    return;
  }

  // Warning proceeded events are always user-initiated since they are triggered
  // only when the user interacts with the warning dialog.
  if (!is_warning_proceeded_event &&
      !event_storage_->StoreEventAndCheckIfItShouldBeReported({inode, crtime},
                                                              dst)) {
    return;
  }

  std::unique_ptr<DlpPolicyEventBuilder> event_builder =
      is_warning_proceeded_event
          ? DlpPolicyEventBuilder::WarningProceededEvent(
                source_pattern, rule_metadata.name, rule_metadata.obfuscated_id,
                DlpRulesManager::Restriction::kFiles)
          : DlpPolicyEventBuilder::Event(
                source_pattern, rule_metadata.name, rule_metadata.obfuscated_id,
                DlpRulesManager::Restriction::kFiles, level.value());

  event_builder->SetContentName(path.BaseName().value());

  if (dst_pattern.has_value()) {
    DCHECK(!dst.component().has_value());
    event_builder->SetDestinationPattern(dst_pattern.value());
  } else {
    DCHECK(dst.component().has_value());
    event_builder->SetDestinationComponent(dst.component().value());
  }
  reporting_manager->ReportEvent(event_builder->Create());
}

void DlpFilesControllerAsh::ContinueCheckIfTransferAllowed(
    absl::optional<file_manager::io_task::IOTaskId> task_id,
    storage::FileSystemURL destination,
    bool is_move,
    CheckIfTransferAllowedCallback result_callback,
    std::vector<storage::FileSystemURL> transferred_files) {
  if (!chromeos::DlpClient::Get() || !chromeos::DlpClient::Get()->IsAlive()) {
    std::move(result_callback).Run(std::vector<storage::FileSystemURL>());
    return;
  }

  ::dlp::CheckFilesTransferRequest request;
  base::flat_map<std::string, storage::FileSystemURL> transferred_files_map;
  for (const auto& file : transferred_files) {
    auto file_path = file.path().value();
    transferred_files_map[file_path] = file;
    request.add_files_paths(file_path);
  }
  if (transferred_files_map.empty()) {
    std::move(result_callback).Run(std::vector<storage::FileSystemURL>());
    return;
  }

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  absl::optional<data_controls::Component> component =
      MapFilePathtoPolicyComponent(profile, destination.path());
  ::dlp::DlpComponent proto;
  if (component) {
    proto = dlp::MapPolicyComponentToProto(*component);
  } else {
    proto = ::dlp::DlpComponent::SYSTEM;
  }
  request.set_destination_component(proto);
  request.set_file_action(is_move ? ::dlp::FileAction::MOVE
                                  : ::dlp::FileAction::COPY);
  if (task_id) {
    request.set_io_task_id(task_id.value());
  }

  auto return_transfers_callback =
      base::BindOnce(&DlpFilesControllerAsh::ReturnDisallowedFiles,
                     weak_ptr_factory_.GetWeakPtr(), std::move(task_id),
                     std::move(transferred_files_map),
                     is_move ? dlp::FileAction::kMove : dlp::FileAction::kCopy,
                     std::move(result_callback));
  chromeos::DlpClient::Get()->CheckFilesTransfer(
      request, std::move(return_transfers_callback));
}

void DlpFilesControllerAsh::ContinueFilterDisallowedUploads(
    std::vector<ui::SelectedFileInfo> selected_files,
    const DlpFileDestination& destination,
    FilterDisallowedUploadsCallback result_callback,
    std::vector<storage::FileSystemURL> uploaded_files) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!chromeos::DlpClient::Get() || !chromeos::DlpClient::Get()->IsAlive()) {
    std::move(result_callback).Run(std::move(selected_files));
    return;
  }

  ::dlp::CheckFilesTransferRequest request;
  for (const auto& file : uploaded_files) {
    request.add_files_paths(file.path().value());
  }
  if (destination.component().has_value()) {
    request.set_destination_component(
        dlp::MapPolicyComponentToProto(destination.component().value()));
  } else {
    DCHECK(destination.url());
    request.set_destination_url(destination.url()->spec());
  }
  request.set_file_action(::dlp::FileAction::UPLOAD);

  auto return_uploads_callback =
      base::BindOnce(&DlpFilesControllerAsh::ReturnAllowedUploads,
                     weak_ptr_factory_.GetWeakPtr(), std::move(selected_files),
                     std::move(result_callback));
  chromeos::DlpClient::Get()->CheckFilesTransfer(
      request, std::move(return_uploads_callback));
}

void DlpFilesControllerAsh::ContinueCheckIfDropAllowed(
    const DlpFileDestination& destination,
    CheckIfDlpAllowedCallback result_callback,
    std::vector<storage::FileSystemURL> dropped_files) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!chromeos::DlpClient::Get() || !chromeos::DlpClient::Get()->IsAlive()) {
    std::move(result_callback).Run(/*is_allowed=*/true);
    return;
  }

  ::dlp::CheckFilesTransferRequest request;
  for (const auto& file : dropped_files) {
    request.add_files_paths(file.path().value());
  }
  if (destination.component().has_value()) {
    request.set_destination_component(
        dlp::MapPolicyComponentToProto(destination.component().value()));
  } else {
    DCHECK(destination.url());
    request.set_destination_url(destination.url()->spec());
  }
  request.set_file_action(::dlp::FileAction::MOVE);

  auto return_drop_allowed_cb =
      base::BindOnce(&DlpFilesControllerAsh::ReturnIfActionAllowed,
                     weak_ptr_factory_.GetWeakPtr(), dlp::FileAction::kMove,
                     std::move(result_callback));
  chromeos::DlpClient::Get()->CheckFilesTransfer(
      request, std::move(return_drop_allowed_cb));
}

}  // namespace policy
