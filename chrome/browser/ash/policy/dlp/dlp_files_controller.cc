// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"

#include <sys/types.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/new_window_delegate.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/file_utils.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/extensions/file_manager/system_notification_manager.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_event_storage.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_warn_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_warn_notifier.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_copy.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/recursive_operation_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/views/widget/widget.h"
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

constexpr char kUploadBlockedNotificationId[] = "upload_dlp_blocked";
constexpr char kDownloadBlockedNotificationId[] = "download_dlp_blocked";
constexpr char kOpenBlockedNotificationId[] = "open_dlp_blocked";

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

// Returns inode value for local files.
absl::optional<ino64_t> GetInodeValue(const base::FilePath& path) {
  if (!IsInLocalFileSystem(path)) {
    return absl::nullopt;
  }

  struct stat file_stats;
  if (stat(path.value().c_str(), &file_stats) != 0) {
    return absl::nullopt;
  }
  return file_stats.st_ino;
}

// Returns a `DlpFileDestination` with a source URL or component, based on
// |app_update|.
DlpFilesController::DlpFileDestination GetFileDestinationForApp(
    const apps::AppUpdate& app_update) {
  DlpFilesController::DlpFileDestination destination;
  switch (app_update.AppType()) {
    case apps::AppType::kStandaloneBrowserChromeApp:
    case apps::AppType::kExtension:
    case apps::AppType::kStandaloneBrowserExtension:
    case apps::AppType::kChromeApp:
      destination.url_or_path = base::StrCat(
          {extensions::kExtensionScheme, "://", app_update.AppId()});
      break;
    case apps::AppType::kArc:
      destination.component = DlpRulesManager::Component::kArc;
      break;
    case apps::AppType::kCrostini:
      destination.component = DlpRulesManager::Component::kCrostini;
      break;
    case apps::AppType::kPluginVm:
      destination.component = DlpRulesManager::Component::kPluginVm;
      break;
    case apps::AppType::kWeb:
      destination.url_or_path = app_update.PublisherId();
      break;
    case apps::AppType::kUnknown:
    case apps::AppType::kBuiltIn:
    case apps::AppType::kMacOs:
    case apps::AppType::kStandaloneBrowser:
    case apps::AppType::kRemote:
    case apps::AppType::kBorealis:
    case apps::AppType::kBruschetta:
    case apps::AppType::kSystemWeb:
      break;
  }
  return destination;
}

std::vector<absl::optional<ino64_t>> GetFilesInodes(
    const std::vector<storage::FileSystemURL>& files) {
  std::vector<absl::optional<ino64_t>> inodes;
  for (const auto& file : files) {
    inodes.push_back(GetInodeValue(file.path()));
  }
  return inodes;
}

// Maps |file_path| to DlpRulesManager::Component if possible.
absl::optional<DlpRulesManager::Component> MapFilePathtoPolicyComponent(
    Profile* profile,
    const base::FilePath file_path) {
  if (base::FilePath(file_manager::util::GetAndroidFilesPath())
          .IsParent(file_path)) {
    return DlpRulesManager::Component::kArc;
  }

  if (base::FilePath(file_manager::util::kRemovableMediaPath)
          .IsParent(file_path)) {
    return DlpRulesManager::Component::kUsb;
  }

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (integration_service && integration_service->is_enabled() &&
      integration_service->GetMountPointPath().IsParent(file_path)) {
    return DlpRulesManager::Component::kDrive;
  }

  base::FilePath linux_files =
      file_manager::util::GetCrostiniMountDirectory(profile);
  if (linux_files == file_path || linux_files.IsParent(file_path)) {
    return DlpRulesManager::Component::kCrostini;
  }

  return {};
}

// Gets the component out of |destination| if possible.
absl::optional<DlpRulesManager::Component> MaybeGetComponent(
    Profile* profile,
    const DlpFilesController::DlpFileDestination& destination) {
  if (destination.component.has_value()) {
    return destination.component;
  }
  DCHECK(destination.url_or_path.has_value());
  return MapFilePathtoPolicyComponent(profile,
                                      base::FilePath(*destination.url_or_path));
}

// Maps |component| to DlpRulesManager::Component.
DlpRulesManager::Component MapProtoToPolicyComponent(
    ::dlp::DlpComponent component) {
  switch (component) {
    case ::dlp::DlpComponent::ARC:
      return DlpRulesManager::Component::kArc;
    case ::dlp::DlpComponent::CROSTINI:
      return DlpRulesManager::Component::kCrostini;
    case ::dlp::DlpComponent::PLUGIN_VM:
      return DlpRulesManager::Component::kPluginVm;
    case ::dlp::DlpComponent::USB:
      return DlpRulesManager::Component::kUsb;
    case ::dlp::DlpComponent::GOOGLE_DRIVE:
      return DlpRulesManager::Component::kDrive;
    case ::dlp::DlpComponent::UNKNOWN_COMPONENT:
    case ::dlp::DlpComponent::SYSTEM:
      return DlpRulesManager::Component::kUnknownComponent;
  }
}

// Maps |component| to ::dlp::DlpComponent.
::dlp::DlpComponent MapPolicyComponentToProto(
    DlpRulesManager::Component component) {
  switch (component) {
    case DlpRulesManager::Component::kUnknownComponent:
      return ::dlp::DlpComponent::UNKNOWN_COMPONENT;
    case DlpRulesManager::Component::kArc:
      return ::dlp::DlpComponent::ARC;
    case DlpRulesManager::Component::kCrostini:
      return ::dlp::DlpComponent::CROSTINI;
    case DlpRulesManager::Component::kPluginVm:
      return ::dlp::DlpComponent::PLUGIN_VM;
    case DlpRulesManager::Component::kUsb:
      return ::dlp::DlpComponent::USB;
    case DlpRulesManager::Component::kDrive:
      return ::dlp::DlpComponent::GOOGLE_DRIVE;
  }
}

// Returns |g_file_system_context_for_testing| if set, otherwise
// it returns FileSystemContext* for the primary profile.
storage::FileSystemContext* GetFileSystemContext() {
  if (g_file_system_context_for_testing)
    return g_file_system_context_for_testing;

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
    StartRecursiveOperation(root_,
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

  const storage::FileSystemURL& root_;
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
  storage::FileSystemContext* file_system_context_ = nullptr;
  const std::vector<storage::FileSystemURL> roots_;
  FolderRecursionDelegate::FileURLsCallback callback_;
  std::vector<storage::FileSystemURL> files_urls_;
  std::vector<std::unique_ptr<FolderRecursionDelegate>> delegates_;

  base::WeakPtrFactory<RootsRecursionDelegate> weak_ptr_factory_{this};
};

// This callback is used when we copy a file within the internal filesystem
// (Downloads / MyFiles). It is called after the source URL of the source file
// is retrieved. It creates a callback `delayed_add_file` and requests the
// ScopedFileAccess for the copy operation. To this access token the
// `delayed_add_file` callback is added so it is called after the copy operation
// finishes.
void GotFilesSourcesOfCopy(
    storage::FileSystemURL destination,
    ::dlp::RequestFileAccessRequest file_access_request,
    base::OnceCallback<void(std::unique_ptr<file_access::ScopedFileAccess>)>
        result_callback,
    std::vector<DlpFilesController::DlpFileMetadata> metadata) {
  if (metadata.size() == 0) {
    std::move(result_callback)
        .Run(std::make_unique<file_access::ScopedFileAccess>(
            file_access::ScopedFileAccess::Allowed()));
    return;
  }
  DCHECK(metadata.size() == 1);
  if (!chromeos::DlpClient::Get() || !chromeos::DlpClient::Get()->IsAlive()) {
    std::move(result_callback)
        .Run(std::make_unique<file_access::ScopedFileAccess>(
            file_access::ScopedFileAccess::Allowed()));
    return;
  }

  if (metadata[0].source_url.empty()) {
    std::move(result_callback)
        .Run(std::make_unique<file_access::ScopedFileAccess>(
            file_access::ScopedFileAccess::Allowed()));
    return;
  }

  ::dlp::AddFileRequest add_request;
  add_request.set_file_path(destination.path().value());
  add_request.set_source_url(metadata[0].source_url);

  // The callback will be invoked with the destruction of the
  // ScopedFileAccessCopy object
  base::OnceCallback<void()> delayed_add_file = base::BindPostTask(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::BindOnce(
          [](::dlp::AddFileRequest&& add_request) {
            // TODO(https://crbug.com/1368497): we might want to use the
            // callback for error handling.
            chromeos::DlpClient::Get()->AddFile(add_request, base::DoNothing());
          },
          std::move(add_request)));

  chromeos::DlpClient::RequestFileAccessCallback add_file_callback =
      base::BindOnce(
          [](base::OnceCallback<void(
                 std::unique_ptr<file_access::ScopedFileAccess>)>
                 result_callback,
             base::OnceCallback<void()> delayed_add_file,
             const ::dlp::RequestFileAccessResponse response,
             base::ScopedFD fd) {
            std::move(result_callback)
                .Run(std::make_unique<file_access::ScopedFileAccessCopy>(
                    response.allowed(), base::ScopedFD(),
                    std::move(delayed_add_file)));
          },
          std::move(result_callback), std::move(delayed_add_file));

  chromeos::DlpClient::Get()->RequestFileAccess(file_access_request,
                                                std::move(add_file_callback));
}

::dlp::DlpComponent MapPolicyToProtoComponent(
    DlpRulesManager::Component component) {
  switch (component) {
    case DlpRulesManager::Component::kUnknownComponent:
      return ::dlp::DlpComponent::UNKNOWN_COMPONENT;
    case DlpRulesManager::Component::kArc:
      return ::dlp::DlpComponent::ARC;
    case DlpRulesManager::Component::kCrostini:
      return ::dlp::DlpComponent::CROSTINI;
    case DlpRulesManager::Component::kPluginVm:
      return ::dlp::DlpComponent::PLUGIN_VM;
    case DlpRulesManager::Component::kUsb:
      return ::dlp::DlpComponent::USB;
    case DlpRulesManager::Component::kDrive:
      return ::dlp::DlpComponent::GOOGLE_DRIVE;
  }
}

// Returns an instance of NotificationDisplayService for the primary profile.
NotificationDisplayService* GetNotificationDisplayService() {
  auto* profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(profile);
  auto* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile);
  DCHECK(display_service);
  return display_service;
}

// Opens DLP Learn more link and closes the notification having
// `notification_id`.
void OnLearnMoreButtonClicked(const std::string& notification_id,
                              absl::optional<int> button_index) {
  if (!button_index || button_index.value() != 0)
    return;

  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(dlp::kDlpLearnMoreUrl),
      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);

  GetNotificationDisplayService()->Close(NotificationHandler::Type::TRANSIENT,
                                         notification_id);
}

// Shows a system notification having `notification_id`, `title`, and `message`.
void ShowNotification(const std::string& notification_id,
                      const std::u16string& title,
                      const std::u16string& message) {
  auto notification = file_manager::CreateSystemNotification(
      notification_id, std::move(title), std::move(message),
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&OnLearnMoreButtonClicked, notification_id)));
  notification->set_buttons(
      {message_center::ButtonInfo(l10n_util::GetStringUTF16(IDS_LEARN_MORE))});

  GetNotificationDisplayService()->Display(NotificationHandler::Type::TRANSIENT,
                                           *notification,
                                           /*metadata=*/nullptr);
}

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
DlpFilesController::DlpFileDestination DTEndpointToFileDestination(
    const ui::DataTransferEndpoint* endpoint) {
  DCHECK(endpoint);

  switch (endpoint->type()) {
    case ui::EndpointType::kUrl:
      DCHECK(endpoint->GetURL());
      return DlpFilesController::DlpFileDestination(endpoint->GetURL()->spec());

    case ui::EndpointType::kArc:
      return DlpFilesController::DlpFileDestination(
          DlpRulesManager::Component::kArc);

    case ui::EndpointType::kCrostini:
      return DlpFilesController::DlpFileDestination(
          DlpRulesManager::Component::kCrostini);

    case ui::EndpointType::kPluginVm:
      return DlpFilesController::DlpFileDestination(
          DlpRulesManager::Component::kPluginVm);

    case ui::EndpointType::kLacros:
    case ui::EndpointType::kDefault:
    case ui::EndpointType::kClipboardHistory:
    case ui::EndpointType::kBorealis:
    case ui::EndpointType::kUnknownVm:
      return DlpFilesController::DlpFileDestination(
          DlpRulesManager::Component::kUnknownComponent);
  }
}

}  // namespace

DlpFilesController::DlpFileMetadata::DlpFileMetadata(
    const std::string& source_url,
    bool is_dlp_restricted,
    bool is_restricted_for_destination)
    : source_url(source_url),
      is_dlp_restricted(is_dlp_restricted),
      is_restricted_for_destination(is_restricted_for_destination) {}

DlpFilesController::DlpFileRestrictionDetails::DlpFileRestrictionDetails() =
    default;

DlpFilesController::DlpFileRestrictionDetails::DlpFileRestrictionDetails(
    DlpFileRestrictionDetails&&) = default;
DlpFilesController::DlpFileRestrictionDetails&
DlpFilesController::DlpFileRestrictionDetails::operator=(
    DlpFilesController::DlpFileRestrictionDetails&&) = default;

DlpFilesController::DlpFileRestrictionDetails::~DlpFileRestrictionDetails() =
    default;

DlpFilesController::FileDaemonInfo::FileDaemonInfo(
    ino64_t inode,
    const base::FilePath& path,
    const std::string& source_url)
    : inode(inode), path(path), source_url(source_url) {}
DlpFilesController::DlpFileDestination::DlpFileDestination() = default;
DlpFilesController::DlpFileDestination::DlpFileDestination(
    const std::string& url)
    : url_or_path(url) {}
DlpFilesController::DlpFileDestination::DlpFileDestination(
    const ::dlp::DlpComponent component)
    : component(MapProtoToPolicyComponent(component)) {}
DlpFilesController::DlpFileDestination::DlpFileDestination(
    const DlpRulesManager::Component component)
    : component(component) {}

DlpFilesController::DlpFileDestination::DlpFileDestination(
    const DlpFileDestination&) = default;
DlpFilesController::DlpFileDestination&
DlpFilesController::DlpFileDestination::operator=(const DlpFileDestination&) =
    default;
DlpFilesController::DlpFileDestination::DlpFileDestination(
    DlpFileDestination&&) = default;
DlpFilesController::DlpFileDestination&
DlpFilesController::DlpFileDestination::operator=(DlpFileDestination&&) =
    default;
bool DlpFilesController::DlpFileDestination::operator==(
    const DlpFileDestination& other) const {
  return component == other.component && url_or_path == other.url_or_path;
}
bool DlpFilesController::DlpFileDestination::operator!=(
    const DlpFileDestination& other) const {
  return !(*this == other);
}
bool DlpFilesController::DlpFileDestination::operator<(
    const DlpFileDestination& other) const {
  if (component.has_value() && other.component.has_value()) {
    return static_cast<int>(component.value()) <
           static_cast<int>(other.component.value());
  }
  if (component.has_value()) {
    return true;
  }
  if (other.component.has_value()) {
    return false;
  }
  DCHECK(url_or_path.has_value() && other.url_or_path.has_value());
  return url_or_path.value() < other.url_or_path.value();
}
bool DlpFilesController::DlpFileDestination::operator<=(
    const DlpFileDestination& other) const {
  return *this == other || *this < other;
}
bool DlpFilesController::DlpFileDestination::operator>(
    const DlpFileDestination& other) const {
  return !(*this <= other);
}
bool DlpFilesController::DlpFileDestination::operator>=(
    const DlpFileDestination& other) const {
  return !(*this < other);
}

DlpFilesController::DlpFileDestination::~DlpFileDestination() = default;

DlpFilesController::DlpFilesController(const DlpRulesManager& rules_manager)
    : rules_manager_(rules_manager),
      warn_notifier_(std::make_unique<DlpWarnNotifier>()),
      event_storage_(std::make_unique<DlpFilesEventStorage>(kCooldownTimeout,
                                                            kEntriesLimit)) {}

DlpFilesController::~DlpFilesController() = default;

void DlpFilesController::GetDisallowedTransfers(
    const std::vector<storage::FileSystemURL>& transferred_files,
    storage::FileSystemURL destination,
    bool is_move,
    GetDisallowedTransfersCallback result_callback) {
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
      base::BindOnce(&DlpFilesController::ContinueGetDisallowedTransfers,
                     weak_ptr_factory_.GetWeakPtr(), std::move(destination),
                     is_move, std::move(result_callback)));
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&RootsRecursionDelegate::Run,
                     // base::Unretained() is safe since |recursion_delegate|
                     // will delete itself after all the files list if ready.
                     base::Unretained(roots_recursion_delegate)));
}

void DlpFilesController::RequestCopyAccess(
    const storage::FileSystemURL& source_file,
    const storage::FileSystemURL& destination,
    base::OnceCallback<void(std::unique_ptr<file_access::ScopedFileAccess>)>
        result_callback) {
  if (!chromeos::DlpClient::Get() || !chromeos::DlpClient::Get()->IsAlive()) {
    std::move(result_callback)
        .Run(std::make_unique<file_access::ScopedFileAccess>(
            file_access::ScopedFileAccess::Allowed()));
    return;
  }
  Profile* profile = ProfileManager::GetPrimaryUserProfile();

  absl::optional<DlpRulesManager::Component> dst_component =
      MapFilePathtoPolicyComponent(profile, destination.path());
  absl::optional<DlpRulesManager::Component> src_component =
      MapFilePathtoPolicyComponent(profile, source_file.path());
  ::dlp::DlpComponent component_proto;
  if (!src_component.has_value()) {
    src_component = DlpRulesManager::Component::kUnknownComponent;
  }
  if (dst_component.has_value()) {
    component_proto = MapPolicyToProtoComponent(dst_component.value());
  } else {
    // Treat non external as system. We want to allow the operation and system
    // is allowed always
    component_proto = ::dlp::SYSTEM;
  }

  // Copy from external is not limited by DLP.
  if (src_component != DlpRulesManager::Component::kUnknownComponent) {
    std::move(result_callback)
        .Run(std::make_unique<file_access::ScopedFileAccess>(
            file_access::ScopedFileAccess::Allowed()));
    return;
  }

  ::dlp::RequestFileAccessRequest file_access_request;
  file_access_request.add_files_paths(source_file.path().value());
  file_access_request.set_destination_component(component_proto);

  if (component_proto == ::dlp::SYSTEM) {
    // We allow internal copy, we still have to get the scopedFS
    // and we might need to copy the source URL information.
    GetDlpMetadata(
        {source_file}, /*destination=*/absl::nullopt,
        base::BindOnce(&GotFilesSourcesOfCopy, destination, file_access_request,
                       std::move(result_callback)));
    return;
  }

  // TODO(http://b/262223235) check for the actual component.
  file_access_request.set_destination_component(::dlp::SYSTEM);

  chromeos::DlpClient::Get()->RequestFileAccess(
      file_access_request,
      base::BindOnce(
          [](base::OnceCallback<void(
                 std::unique_ptr<file_access::ScopedFileAccess>)> callback,
             ::dlp::RequestFileAccessResponse res, base::ScopedFD fd) {
            std::move(callback).Run(
                std::make_unique<file_access::ScopedFileAccess>(res.allowed(),
                                                                std::move(fd)));
          },
          std::move(result_callback)));
}

void DlpFilesController::GetDlpMetadata(
    const std::vector<storage::FileSystemURL>& files,
    absl::optional<DlpFileDestination> destination,
    GetDlpMetadataCallback result_callback) {
  if (!chromeos::DlpClient::Get() || !chromeos::DlpClient::Get()->IsAlive()) {
    std::move(result_callback).Run(std::vector<DlpFileMetadata>());
    return;
  }

  std::vector<absl::optional<ino64_t>> inodes = GetFilesInodes(files);
  ::dlp::GetFilesSourcesRequest request;
  for (const auto& inode : inodes) {
    if (inode.has_value()) {
      request.add_files_inodes(inode.value());
    }
  }
  chromeos::DlpClient::Get()->GetFilesSources(
      request, base::BindOnce(&DlpFilesController::ReturnDlpMetadata,
                              weak_ptr_factory_.GetWeakPtr(), std::move(inodes),
                              destination, std::move(result_callback)));
}

void DlpFilesController::FilterDisallowedUploads(
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
      base::BindOnce(&DlpFilesController::ContinueFilterDisallowedUploads,
                     weak_ptr_factory_.GetWeakPtr(), std::move(selected_files),
                     std::move(destination), std::move(result_callback)));
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&RootsRecursionDelegate::Run,
                     // base::Unretained() is safe since |recursion_delegate|
                     // will delete itself after all the files list if ready.
                     base::Unretained(roots_recursion_delegate)));
}

void DlpFilesController::CheckIfDownloadAllowed(
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

  if (!download_src.url_or_path.has_value()) {
    // Currently we only support urls as sources.
    std::move(result_callback).Run(true);
    return;
  }

  FileDaemonInfo file_info({}, file_path, download_src.url_or_path.value());
  IsFilesTransferRestricted(
      {std::move(file_info)}, DlpFileDestination(file_path.value()),
      FileAction::kDownload,
      base::BindOnce(  // TODO(b/270015718): Unify to ReturnIfActionAllowed.
          [](CheckIfDlpAllowedCallback result_callback,
             const std::vector<std::pair<
                 FileDaemonInfo, ::dlp::RestrictionLevel>>& files_levels) {
            bool is_allowed = true;
            for (const auto& [file, level] : files_levels) {
              if (level == ::dlp::RestrictionLevel::LEVEL_BLOCK ||
                  level == ::dlp::RestrictionLevel::LEVEL_WARN_CANCEL) {
                is_allowed = false;
                break;
              }
            }
            if (!is_allowed) {
              ShowNotification(
                  kDownloadBlockedNotificationId,
                  l10n_util::GetStringUTF16(
                      IDS_POLICY_DLP_FILES_DOWNLOAD_BLOCK_TITLE),
                  l10n_util::GetStringUTF16(
                      IDS_POLICY_DLP_FILES_DOWNLOAD_BLOCK_MESSAGE));
            }
            std::move(result_callback).Run(is_allowed);
          },
          std::move(result_callback)));
}

bool DlpFilesController::ShouldPromptBeforeDownload(
    const DlpFileDestination& download_src,
    const base::FilePath& file_path) {
  if (!download_src.url_or_path.has_value()) {
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

  DlpRulesManager::Level level = rules_manager_.IsRestrictedComponent(
      GURL(download_src.url_or_path.value()), dst_component.value(),
      DlpRulesManager::Restriction::kFiles, /*out_source_pattern=*/nullptr,
      /*out_rule_metadata=*/nullptr);
  return level == DlpRulesManager::Level::kBlock ||
         level == DlpRulesManager::Level::kWarn;
}

void DlpFilesController::CheckIfLaunchAllowed(
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

  DlpFileDestination destination = GetFileDestinationForApp(app_update);
  if (destination.url_or_path.has_value()) {
    request.set_destination_url(destination.url_or_path.value());
  } else if (destination.component.has_value()) {
    request.set_destination_component(
        MapPolicyComponentToProto(destination.component.value()));
  }

  chromeos::DlpClient::Get()->CheckFilesTransfer(
      request, base::BindOnce(&DlpFilesController::LaunchIfAllowed,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::move(result_callback)));
}

bool DlpFilesController::IsLaunchBlocked(const apps::AppUpdate& app_update,
                                         const apps::IntentPtr& intent) {
  if (intent->files.empty()) {
    return false;
  }

  DlpFileDestination destination = GetFileDestinationForApp(app_update);
  for (const auto& file : intent->files) {
    if (!file->dlp_source_url.has_value()) {
      continue;
    }
    if (destination.url_or_path.has_value()) {
      DlpRulesManager::Level level = rules_manager_.IsRestrictedDestination(
          GURL(file->dlp_source_url.value()),
          GURL(destination.url_or_path.value()),
          DlpRulesManager::Restriction::kFiles, /*out_source_pattern=*/nullptr,
          /*out_destination_pattern=*/nullptr, /*out_rule_metadata=*/nullptr);
      if (level == DlpRulesManager::Level::kBlock) {
        return true;
      }
    } else if (destination.component.has_value()) {
      DlpRulesManager::Level level = rules_manager_.IsRestrictedComponent(
          GURL(file->dlp_source_url.value()), destination.component.value(),
          DlpRulesManager::Restriction::kFiles, /*out_source_pattern=*/nullptr,
          /*out_rule_metadata=*/nullptr);
      if (level == DlpRulesManager::Level::kBlock) {
        return true;
      }
    }
  }

  return false;
}

void DlpFilesController::IsFilesTransferRestricted(
    const std::vector<FileDaemonInfo>& transferred_files,
    const DlpFileDestination& destination,
    FileAction files_action,
    IsFilesTransferRestrictedCallback result_callback) {
  auto* profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(profile);
  absl::optional<DlpRulesManager::Component> dst_component =
      MaybeGetComponent(profile, destination);

  DlpFileDestination deduplication_dst;

  std::vector<std::pair<FileDaemonInfo, ::dlp::RestrictionLevel>> files_levels;
  std::vector<FileDaemonInfo> warned_files;
  std::vector<DlpConfidentialFile> dialog_files;
  absl::optional<std::string> destination_pattern;
  std::vector<std::string> warned_source_patterns;
  std::vector<DlpRulesManager::RuleMetadata> warned_rules_metadata;
  for (const auto& file : transferred_files) {
    DlpRulesManager::Level level;
    std::string source_pattern;
    DlpRulesManager::RuleMetadata rule_metadata;
    if (dst_component.has_value()) {
      level = rules_manager_.IsRestrictedComponent(
          GURL(file.source_url), dst_component.value(),
          DlpRulesManager::Restriction::kFiles, &source_pattern,
          &rule_metadata);
      deduplication_dst = DlpFileDestination(dst_component.value());
      MaybeReportEvent(file.inode, file.path, source_pattern, deduplication_dst,
                       absl::nullopt, rule_metadata, level);
    } else {
      // TODO(crbug.com/1286366): Revisit whether passing files paths here
      // make sense.
      DCHECK(destination.url_or_path.has_value());
      destination_pattern = std::string();
      level = rules_manager_.IsRestrictedDestination(
          GURL(file.source_url), GURL(*destination.url_or_path),
          DlpRulesManager::Restriction::kFiles, &source_pattern,
          &destination_pattern.value(), &rule_metadata);
      deduplication_dst = destination;
      MaybeReportEvent(file.inode, file.path, source_pattern, deduplication_dst,
                       destination_pattern, rule_metadata, level);
    }

    switch (level) {
      case DlpRulesManager::Level::kBlock: {
        files_levels.push_back({file, ::dlp::RestrictionLevel::LEVEL_BLOCK});
        DlpHistogramEnumeration(dlp::kFileActionBlockedUMA, files_action);
        break;
      }
      case DlpRulesManager::Level::kNotSet:
      case DlpRulesManager::Level::kAllow: {
        files_levels.push_back({file, ::dlp::RestrictionLevel::LEVEL_ALLOW});
        break;
      }
      case DlpRulesManager::Level::kReport: {
        files_levels.push_back({file, ::dlp::RestrictionLevel::LEVEL_REPORT});
        break;
      }
      case DlpRulesManager::Level::kWarn: {
        warned_files.push_back(file);
        warned_source_patterns.emplace_back(source_pattern);
        warned_rules_metadata.emplace_back(rule_metadata);
        if (files_action != FileAction::kDownload) {
          dialog_files.emplace_back(file.path);
        }
        DlpHistogramEnumeration(dlp::kFileActionWarnedUMA, files_action);
        break;
      }
    }
  }

  if (warned_files.empty()) {
    std::move(result_callback).Run(std::move(files_levels));
    return;
  }

  if (warn_dialog_widget_ && !warn_dialog_widget_->IsClosed()) {
    warn_dialog_widget_->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
  }

  warn_dialog_widget_ = warn_notifier_->ShowDlpFilesWarningDialog(
      base::BindOnce(&DlpFilesController::OnDlpWarnDialogReply,
                     weak_ptr_factory_.GetWeakPtr(), std::move(files_levels),
                     std::move(warned_files), std::move(warned_source_patterns),
                     std::move(warned_rules_metadata),
                     std::move(deduplication_dst), destination_pattern,
                     files_action, std::move(result_callback)),
      std::move(dialog_files), dst_component, destination_pattern,
      files_action);
}

std::vector<DlpFilesController::DlpFileRestrictionDetails>
DlpFilesController::GetDlpRestrictionDetails(const std::string& source_url) {
  const GURL source(source_url);
  const DlpRulesManager::AggregatedDestinations aggregated_destinations =
      rules_manager_.GetAggregatedDestinations(
          source, DlpRulesManager::Restriction::kFiles);
  const DlpRulesManager::AggregatedComponents aggregated_components =
      rules_manager_.GetAggregatedComponents(
          source, DlpRulesManager::Restriction::kFiles);

  std::vector<DlpFilesController::DlpFileRestrictionDetails> result;
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

std::vector<DlpRulesManager::Component>
DlpFilesController::GetBlockedComponents(const std::string& source_url) {
  const GURL source(source_url);
  const DlpRulesManager::AggregatedComponents aggregated_components =
      rules_manager_.GetAggregatedComponents(
          source, DlpRulesManager::Restriction::kFiles);

  std::vector<DlpRulesManager::Component> result;
  const auto it = aggregated_components.find(DlpRulesManager::Level::kBlock);
  if (it != aggregated_components.end()) {
    base::ranges::move(it->second.begin(), it->second.end(),
                       std::back_inserter(result));
  }
  return result;
}

bool DlpFilesController::IsDlpPolicyMatched(const FileDaemonInfo& file) {
  bool restricted = false;

  std::string src_pattern;
  DlpRulesManager::RuleMetadata rule_metadata;
  policy::DlpRulesManager::Level level = rules_manager_.IsRestrictedByAnyRule(
      GURL(file.source_url.spec()),
      policy::DlpRulesManager::Restriction::kFiles, &src_pattern,
      &rule_metadata);

  switch (level) {
    case policy::DlpRulesManager::Level::kBlock:
      restricted = true;
      DlpHistogramEnumeration(dlp::kFileActionBlockedUMA, FileAction::kUnknown);
      break;
    case policy::DlpRulesManager::Level::kWarn:
      DlpHistogramEnumeration(dlp::kFileActionWarnedUMA, FileAction::kUnknown);
      // TODO(crbug.com/1172959): Implement Warning mode for Files restriction
      break;
    default:
      break;
  }

  MaybeReportEvent(
      file.inode, file.path, src_pattern,
      DlpFileDestination(DlpRulesManager::Component::kUnknownComponent),
      absl::nullopt, rule_metadata, level);

  return restricted;
}

void DlpFilesController::CheckIfDropAllowed(
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
      base::BindOnce(&DlpFilesController::ContinueCheckIfDropAllowed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(destination),
                     std::move(result_callback)));
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&RootsRecursionDelegate::Run,
                     // base::Unretained() is safe since |recursion_delegate|
                     // will delete itself after all the files list if ready.
                     base::Unretained(roots_recursion_delegate)));
}

void DlpFilesController::SetWarnNotifierForTesting(
    std::unique_ptr<DlpWarnNotifier> warn_notifier) {
  DCHECK(warn_notifier);
  warn_notifier_ = std::move(warn_notifier);
}

DlpFilesEventStorage* DlpFilesController::GetEventStorageForTesting() {
  return event_storage_.get();
}

void DlpFilesController::SetFileSystemContextForTesting(
    storage::FileSystemContext* file_system_context) {
  g_file_system_context_for_testing = file_system_context;
}

base::WeakPtr<views::Widget> DlpFilesController::GetWarnDialogForTesting() {
  return warn_dialog_widget_;
}

void DlpFilesController::OnDlpWarnDialogReply(
    std::vector<std::pair<FileDaemonInfo, ::dlp::RestrictionLevel>>
        files_levels,
    std::vector<FileDaemonInfo> warned_files,
    std::vector<std::string> warned_src_patterns,
    std::vector<DlpRulesManager::RuleMetadata> warned_rules_metadata,
    const DlpFileDestination& dst,
    const absl::optional<std::string>& dst_pattern,
    FileAction files_action,
    IsFilesTransferRestrictedCallback callback,
    bool should_proceed) {
  DCHECK(warned_files.size() == warned_src_patterns.size());
  DCHECK(warned_files.size() == warned_rules_metadata.size());
  for (size_t i = 0; i < warned_files.size(); ++i) {
    if (should_proceed) {
      DlpHistogramEnumeration(dlp::kFileActionWarnProceededUMA, files_action);
      MaybeReportEvent(warned_files[i].inode, warned_files[i].path,
                       warned_src_patterns[i], dst, dst_pattern,
                       warned_rules_metadata[i], absl::nullopt);
    }
    files_levels.emplace_back(warned_files[i],
                              should_proceed
                                  ? ::dlp::RestrictionLevel::LEVEL_WARN_PROCEED
                                  : ::dlp::RestrictionLevel::LEVEL_WARN_CANCEL);
  }
  std::move(callback).Run(std::move(files_levels));
}

void DlpFilesController::ReturnDisallowedTransfers(
    base::flat_map<std::string, storage::FileSystemURL> files_map,
    GetDisallowedTransfersCallback result_callback,
    ::dlp::CheckFilesTransferResponse response) {
  MaybeCloseDialog(response);

  std::vector<storage::FileSystemURL> restricted_files;
  if (response.has_error_message()) {
    LOG(ERROR) << "Failed to get check files transfer, error: "
               << response.error_message();
    for (const auto& [file_path, file_system_url] : files_map)
      restricted_files.push_back(file_system_url);
    std::move(result_callback).Run(std::move(restricted_files));
    return;
  }
  for (const auto& file : response.files_paths()) {
    DCHECK(files_map.find(file) != files_map.end());
    restricted_files.push_back(files_map.at(file));
  }
  std::move(result_callback).Run(std::move(restricted_files));
}

void DlpFilesController::ReturnAllowedUploads(
    std::vector<ui::SelectedFileInfo> selected_files,
    FilterDisallowedUploadsCallback result_callback,
    ::dlp::CheckFilesTransferResponse response) {
  MaybeCloseDialog(response);

  if (response.has_error_message()) {
    LOG(ERROR) << "Failed to get check files transfer, error: "
               << response.error_message();
    std::move(result_callback).Run(std::move(selected_files));
    return;
  }
  std::set<base::FilePath> restricted_files(response.files_paths().begin(),
                                            response.files_paths().end());
  if (!restricted_files.empty()) {
    ShowNotification(
        kUploadBlockedNotificationId,
        l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_UPLOAD_BLOCK_TITLE),
        l10n_util::GetPluralStringFUTF16(
            IDS_POLICY_DLP_FILES_UPLOAD_BLOCK_MESSAGE,
            // TODO(b/261575072): What's the correct number to show if multiple
            // folders are uploaded?
            restricted_files.size()));
  }

  // If any of the selected files/folders is restricted or contains a restricted
  // file, it'll be removed.
  base::EraseIf(
      selected_files,
      [&restricted_files](const ui::SelectedFileInfo& selected_file) -> bool {
        return base::ranges::any_of(
            restricted_files, [&](const base::FilePath& restricted_file) {
              return selected_file.file_path == restricted_file ||
                     selected_file.file_path.IsParent(restricted_file);
            });
      });

  std::move(result_callback).Run(std::move(selected_files));
}

void DlpFilesController::ReturnDlpMetadata(
    std::vector<absl::optional<ino64_t>> inodes,
    absl::optional<DlpFileDestination> destination,
    GetDlpMetadataCallback result_callback,
    const ::dlp::GetFilesSourcesResponse response) {
  if (response.has_error_message()) {
    LOG(ERROR) << "Failed to get files sources, error: "
               << response.error_message();
  }

  base::flat_map<ino64_t, DlpFileMetadata> metadata_map;
  for (const auto& metadata : response.files_metadata()) {
    DlpRulesManager::Level level = rules_manager_.IsRestrictedByAnyRule(
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
      absl::optional<DlpRulesManager::Component> dst_component =
          MaybeGetComponent(profile, destination.value());
      if (dst_component.has_value()) {
        DlpRulesManager::Level dst_level = rules_manager_.IsRestrictedComponent(
            GURL(metadata.source_url()), dst_component.value(),
            DlpRulesManager::Restriction::kFiles, nullptr, nullptr);
        is_restricted_for_destination =
            dst_level == DlpRulesManager::Level::kBlock;
      } else {
        DCHECK(destination->url_or_path.has_value());
        DlpRulesManager::Level dst_level =
            rules_manager_.IsRestrictedDestination(
                GURL(metadata.source_url()),
                GURL(destination->url_or_path.value()),
                DlpRulesManager::Restriction::kFiles, nullptr, nullptr,
                nullptr);
        is_restricted_for_destination =
            dst_level == DlpRulesManager::Level::kBlock;
      }
    }

    metadata_map.emplace(
        metadata.inode(),
        DlpFileMetadata(metadata.source_url(), is_dlp_restricted,
                        is_restricted_for_destination));
  }

  std::vector<DlpFileMetadata> result;
  for (const auto& inode : inodes) {
    if (!inode.has_value()) {
      result.emplace_back("", false, false);
      continue;
    }
    auto metadata_itr = metadata_map.find(inode.value());
    if (metadata_itr == metadata_map.end()) {
      result.emplace_back("", false, false);
    } else {
      result.emplace_back(metadata_itr->second);
    }
  }

  std::move(result_callback).Run(std::move(result));
}

// TODO(b/270015718): Unify to ReturnIfActionAllowed.
void DlpFilesController::LaunchIfAllowed(
    CheckIfDlpAllowedCallback result_callback,
    ::dlp::CheckFilesTransferResponse response) {
  if (response.has_error_message()) {
    LOG(ERROR) << "Failed to get check files transfer, error: "
               << response.error_message();
    std::move(result_callback).Run(/*is_allowed=*/true);
    return;
  }

  if (!response.files_paths().empty()) {
    ShowNotification(
        kOpenBlockedNotificationId,
        l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_OPEN_BLOCK_TITLE),
        l10n_util::GetPluralStringFUTF16(
            IDS_POLICY_DLP_FILES_OPEN_BLOCK_MESSAGE,
            response.files_paths().size()));
    std::move(result_callback).Run(/*is_allowed=*/false);
    return;
  }
  std::move(result_callback).Run(/*is_allowed=*/true);
}

// TODO(b/270015718): Unify to ReturnIfActionAllowed.
void DlpFilesController::ReturnIfDropAllowed(
    CheckIfDlpAllowedCallback result_callback,
    ::dlp::CheckFilesTransferResponse response) {
  if (response.has_error_message()) {
    LOG(ERROR) << "Failed to get check files transfer, error: "
               << response.error_message();
    std::move(result_callback).Run(/*is_allowed=*/true);
    return;
  }

  if (!response.files_paths().empty()) {
    // TODO(b/269609831): Show correct notification here.
    std::move(result_callback).Run(/*is_allowed=*/false);
    return;
  }
  std::move(result_callback).Run(/*is_allowed=*/true);
}

void DlpFilesController::MaybeReportEvent(
    ino64_t inode,
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

  DlpReportingManager* reporting_manager = rules_manager_.GetReportingManager();
  if (!reporting_manager) {
    return;
  }

  // Warning proceeded events are always user-initiated since they are triggered
  // only when the user interacts with the warning dialog.
  if (!is_warning_proceeded_event &&
      !event_storage_->StoreEventAndCheckIfItShouldBeReported(inode, dst)) {
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
    DCHECK(!dst.component.has_value());
    event_builder->SetDestinationPattern(dst_pattern.value());
  } else {
    DCHECK(dst.component.has_value());
    event_builder->SetDestinationComponent(dst.component.value());
  }
  reporting_manager->ReportEvent(event_builder->Create());
}

void DlpFilesController::MaybeCloseDialog(
    ::dlp::CheckFilesTransferResponse response) {
  if (response.has_error_message() && warn_dialog_widget_ &&
      !warn_dialog_widget_->IsClosed()) {
    warn_dialog_widget_->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
  }
}

void DlpFilesController::ContinueGetDisallowedTransfers(
    storage::FileSystemURL destination,
    bool is_move,
    GetDisallowedTransfersCallback result_callback,
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

  request.set_destination_url(destination.path().value());
  request.set_file_action(is_move ? ::dlp::FileAction::MOVE
                                  : ::dlp::FileAction::COPY);

  auto return_transfers_callback = base::BindOnce(
      &DlpFilesController::ReturnDisallowedTransfers,
      weak_ptr_factory_.GetWeakPtr(), std::move(transferred_files_map),
      std::move(result_callback));
  chromeos::DlpClient::Get()->CheckFilesTransfer(
      request, std::move(return_transfers_callback));
}

void DlpFilesController::ContinueFilterDisallowedUploads(
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
  if (destination.component.has_value()) {
    request.set_destination_component(
        MapPolicyComponentToProto(destination.component.value()));
  } else {
    DCHECK(destination.url_or_path.has_value());
    request.set_destination_url(destination.url_or_path.value());
  }
  request.set_file_action(::dlp::FileAction::UPLOAD);

  auto return_uploads_callback = base::BindOnce(
      &DlpFilesController::ReturnAllowedUploads, weak_ptr_factory_.GetWeakPtr(),
      std::move(selected_files), std::move(result_callback));
  chromeos::DlpClient::Get()->CheckFilesTransfer(
      request, std::move(return_uploads_callback));
}

void DlpFilesController::ContinueCheckIfDropAllowed(
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
  if (destination.component.has_value()) {
    request.set_destination_component(
        MapPolicyComponentToProto(destination.component.value()));
  } else {
    DCHECK(destination.url_or_path.has_value());
    request.set_destination_url(destination.url_or_path.value());
  }
  request.set_file_action(::dlp::FileAction::MOVE);

  auto return_drop_allowed_cb = base::BindOnce(
      &DlpFilesController::ReturnIfDropAllowed, weak_ptr_factory_.GetWeakPtr(),
      std::move(result_callback));
  chromeos::DlpClient::Get()->CheckFilesTransfer(
      request, std::move(return_drop_allowed_cb));
}

}  // namespace policy
