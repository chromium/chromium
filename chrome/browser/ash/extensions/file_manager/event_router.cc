// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/event_router.h"

#include <stddef.h>

#include <cmath>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/webui/file_manager/file_manager_ui.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/extensions/file_manager/file_system_provider_metrics_util.h"
#include "chrome/browser/ash/extensions/file_manager/private_api_util.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/operation_request_manager.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/extensions/api/file_system/chrome_file_system_delegate_ash.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/disks/disk.h"
#include "chromeos/ash/components/drivefs/drivefs_host.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/components/disks/disks_prefs.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"

using ::ash::disks::Disk;
using ::ash::disks::DiskMountManager;
using content::BrowserThread;
using drive::DriveIntegrationService;
using drive::DriveIntegrationServiceFactory;
using file_manager::util::EntryDefinition;
using file_manager::util::FileDefinition;

namespace file_manager_private = extensions::api::file_manager_private;

namespace file_manager {
namespace {

// Whether Files SWA has any open windows.
bool DoFilesSwaWindowsExist(Profile* profile) {
  return ash::file_manager::FileManagerUI::GetNumInstances() != 0;
}

// Checks if the Recovery Tool is running. This is a temporary solution.
// TODO(mtomasz): Replace with crbug.com/341902 solution.
bool IsRecoveryToolRunning(Profile* profile) {
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(profile);
  if (!extension_prefs) {
    return false;
  }

  const std::string kRecoveryToolIds[] = {
      "kkebgepbbgbcmghedmmdfcbdcodlkngh",  // Recovery tool staging
      "jndclpdbaamdhonoechobihbbiimdgai"   // Recovery tool prod
  };

  for (const auto& extension_id : kRecoveryToolIds) {
    if (extension_prefs->IsExtensionRunning(extension_id)) {
      return true;
    }
  }

  return false;
}

// Sends an event named |event_name| with arguments |event_args| to extensions.
void BroadcastEvent(Profile* profile,
                    extensions::events::HistogramValue histogram_value,
                    const std::string& event_name,
                    base::Value::List event_args) {
  extensions::EventRouter::Get(profile)->BroadcastEvent(
      std::make_unique<extensions::Event>(histogram_value, event_name,
                                          std::move(event_args)));
}

// Sends an event named |event_name| with arguments |event_args| to an extension
// of |extention_id|.
void DispatchEventToExtension(
    Profile* profile,
    const std::string& extension_id,
    extensions::events::HistogramValue histogram_value,
    const std::string& event_name,
    base::Value::List event_args) {
  extensions::EventRouter::Get(profile)->DispatchEventToExtension(
      extension_id, std::make_unique<extensions::Event>(
                        histogram_value, event_name, std::move(event_args)));
}

// Convert the IO Task State enum to the Private API enum.
file_manager_private::IOTaskState GetIOTaskState(
    file_manager::io_task::State state) {
  switch (state) {
    case file_manager::io_task::State::kQueued:
      return file_manager_private::IO_TASK_STATE_QUEUED;
    case file_manager::io_task::State::kScanning:
      return file_manager_private::IO_TASK_STATE_SCANNING;
    case file_manager::io_task::State::kInProgress:
      return file_manager_private::IO_TASK_STATE_IN_PROGRESS;
    case file_manager::io_task::State::kPaused:
      return file_manager_private::IO_TASK_STATE_PAUSED;
    case file_manager::io_task::State::kSuccess:
      return file_manager_private::IO_TASK_STATE_SUCCESS;
    case file_manager::io_task::State::kError:
      return file_manager_private::IO_TASK_STATE_ERROR;
    case file_manager::io_task::State::kNeedPassword:
      return file_manager_private::IO_TASK_STATE_NEED_PASSWORD;
    case file_manager::io_task::State::kCancelled:
      return file_manager_private::IO_TASK_STATE_CANCELLED;
    default:
      NOTREACHED();
      return file_manager_private::IO_TASK_STATE_ERROR;
  }
}

// Convert the IO Task Type enum to the Private API enum.
file_manager_private::IOTaskType GetIOTaskType(
    file_manager::io_task::OperationType type) {
  switch (type) {
    case file_manager::io_task::OperationType::kCopy:
      return file_manager_private::IO_TASK_TYPE_COPY;
    case file_manager::io_task::OperationType::kDelete:
      return file_manager_private::IO_TASK_TYPE_DELETE;
    case file_manager::io_task::OperationType::kEmptyTrash:
      return file_manager_private::IO_TASK_TYPE_EMPTY_TRASH;
    case file_manager::io_task::OperationType::kExtract:
      return file_manager_private::IO_TASK_TYPE_EXTRACT;
    case file_manager::io_task::OperationType::kMove:
      return file_manager_private::IO_TASK_TYPE_MOVE;
    case file_manager::io_task::OperationType::kRestore:
      return file_manager_private::IO_TASK_TYPE_RESTORE;
    case file_manager::io_task::OperationType::kRestoreToDestination:
      return file_manager_private::IO_TASK_TYPE_RESTORE_TO_DESTINATION;
    case file_manager::io_task::OperationType::kTrash:
      return file_manager_private::IO_TASK_TYPE_TRASH;
    case file_manager::io_task::OperationType::kZip:
      return file_manager_private::IO_TASK_TYPE_ZIP;
    default:
      NOTREACHED();
      return file_manager_private::IO_TASK_TYPE_COPY;
  }
}

file_manager_private::PolicyErrorType GetPolicyErrorType(
    absl::optional<file_manager::io_task::PolicyErrorType> type) {
  if (!type.has_value()) {
    return file_manager_private::PolicyErrorType::POLICY_ERROR_TYPE_NONE;
  }
  switch (type.value()) {
    case io_task::PolicyErrorType::kDlp:
      return file_manager_private::POLICY_ERROR_TYPE_DLP;
    case io_task::PolicyErrorType::kEnterpriseConnectors:
      return file_manager_private::POLICY_ERROR_TYPE_ENTERPRISE_CONNECTORS;
    case io_task::PolicyErrorType::kDlpWarningTimeout:
      return file_manager_private::POLICY_ERROR_TYPE_DLP_WARNING_TIMEOUT;
    default:
      NOTREACHED();
      return file_manager_private::POLICY_ERROR_TYPE_NONE;
  }
}

file_manager_private::PolicyErrorType GetPolicyErrorType(
    policy::Policy policy) {
  switch (policy) {
    case policy::Policy::kDlp:
      return file_manager_private::POLICY_ERROR_TYPE_DLP;
    case policy::Policy::kEnterpriseConnectors:
      return file_manager_private::POLICY_ERROR_TYPE_ENTERPRISE_CONNECTORS;
  }
}

std::string FileErrorToErrorName(base::File::Error error_code) {
  switch (error_code) {
    case base::File::FILE_ERROR_NOT_FOUND:
      return "NotFoundError";
    case base::File::FILE_ERROR_INVALID_OPERATION:
    case base::File::FILE_ERROR_EXISTS:
    case base::File::FILE_ERROR_NOT_EMPTY:
      return "InvalidModificationError";
    case base::File::FILE_ERROR_NOT_A_DIRECTORY:
    case base::File::FILE_ERROR_NOT_A_FILE:
      return "TypeMismatchError";
    case base::File::FILE_ERROR_ACCESS_DENIED:
      return "NoModificationAllowedError";
    case base::File::FILE_ERROR_FAILED:
      return "InvalidStateError";
    case base::File::FILE_ERROR_ABORT:
      return "AbortError";
    case base::File::FILE_ERROR_SECURITY:
      return "SecurityError";
    case base::File::FILE_ERROR_NO_SPACE:
      return "QuotaExceededError";
    case base::File::FILE_ERROR_INVALID_URL:
      return "EncodingError";
    default:
      return "InvalidModificationError";
  }
}

// Obtains whether the Files app should handle the volume or not.
bool ShouldShowNotificationForVolume(
    Profile* profile,
    const DeviceEventRouter& device_event_router,
    const Volume& volume) {
  if (volume.type() != VOLUME_TYPE_MTP &&
      volume.type() != VOLUME_TYPE_REMOVABLE_DISK_PARTITION) {
    return false;
  }

  if (device_event_router.is_resuming() ||
      device_event_router.is_starting_up()) {
    return false;
  }

  // Do not attempt to open File Manager while the login is in progress or
  // the screen is locked or running in kiosk app mode and make sure the file
  // manager is opened only for the active user.
  if (ash::LoginDisplayHost::default_host() ||
      ash::ScreenLocker::default_screen_locker() ||
      chrome::IsRunningInForcedAppMode() ||
      profile != ProfileManager::GetActiveUserProfile()) {
    return false;
  }

  // Do not pop-up the File Manager, if the recovery tool is running.
  if (IsRecoveryToolRunning(profile)) {
    return false;
  }

  // If the disable-default-apps flag is on, the Files app is not opened
  // automatically on device mount not to obstruct the manual test.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableDefaultApps)) {
    return false;
  }

  if (volume.type() == VOLUME_TYPE_REMOVABLE_DISK_PARTITION) {
    const Disk* disk = DiskMountManager::GetInstance()->FindDiskBySourcePath(
        volume.source_path().AsUTF8Unsafe());
    if (disk) {
      // We suppress notifications about HP Elite USB-C Dock's internal storage.
      // chrome-os-partner:58309.
      // TODO(fukino): Remove this workaround when the root cause is fixed.
      if (disk->vendor_id() == "0ea0" && disk->product_id() == "2272") {
        return false;
      }
      // Suppress notifications for this disk if it has been mounted before.
      // This is to avoid duplicate notifications for operations that require a
      // remount of the disk (e.g. format or rename).
      if (!disk->is_first_mount()) {
        return false;
      }
    }
  }

  return true;
}

std::set<GURL> GetEventListenerURLs(Profile* profile,
                                    const std::string& event_name) {
  const extensions::EventListenerMap::ListenerList& listeners =
      extensions::EventRouter::Get(profile)
          ->listeners()
          .GetEventListenersByName(event_name);
  std::set<GURL> urls;
  for (const auto& listener : listeners) {
    if (!listener->extension_id().empty()) {
      urls.insert(extensions::Extension::GetBaseURLFromExtensionId(
          listener->extension_id()));
    } else {
      urls.insert(listener->listener_url());
    }
  }
  // In the Files app, there may not be a window open to listen to the event,
  // so always add the File Manager URL so events can be sent to the
  // SystemNotificationManager.
  urls.insert(file_manager::util::GetFileManagerURL());
  return urls;
}

// Sub-part of the event router for handling device events.
class DeviceEventRouterImpl : public DeviceEventRouter {
 public:
  DeviceEventRouterImpl(SystemNotificationManager* notification_manager,
                        Profile* profile)
      : DeviceEventRouter(notification_manager), profile_(profile) {}

  DeviceEventRouterImpl(const DeviceEventRouterImpl&) = delete;
  DeviceEventRouterImpl& operator=(const DeviceEventRouterImpl&) = delete;

  // DeviceEventRouter overrides.
  void OnDeviceEvent(file_manager_private::DeviceEventType type,
                     const std::string& device_path,
                     const std::string& device_label) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    file_manager_private::DeviceEvent event;
    event.type = type;
    event.device_path = device_path;
    event.device_label = device_label;

    BroadcastEvent(profile_,
                   extensions::events::FILE_MANAGER_PRIVATE_ON_DEVICE_CHANGED,
                   file_manager_private::OnDeviceChanged::kEventName,
                   file_manager_private::OnDeviceChanged::Create(event));

    system_notification_manager()->HandleDeviceEvent(event);
  }

  // DeviceEventRouter overrides.
  bool IsExternalStorageDisabled() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return profile_->GetPrefs()->GetBoolean(
        disks::prefs::kExternalStorageDisabled);
  }

 private:
  const raw_ptr<Profile, ExperimentalAsh> profile_;
};

class DriveFsEventRouterImpl : public DriveFsEventRouter {
 public:
  DriveFsEventRouterImpl(const DriveFsEventRouterImpl&) = delete;
  DriveFsEventRouterImpl(
      SystemNotificationManager* notification_manager,
      Profile* profile,
      const std::map<base::FilePath, std::unique_ptr<FileWatcher>>*
          file_watchers)
      : DriveFsEventRouter(profile, notification_manager),
        profile_(profile),
        file_watchers_(file_watchers) {}

  DriveFsEventRouterImpl& operator=(const DriveFsEventRouterImpl&) = delete;

 private:
  std::set<GURL> GetEventListenerURLs(const std::string& event_name) override {
    return ::file_manager::GetEventListenerURLs(profile_, event_name);
  }

  GURL ConvertDrivePathToFileSystemUrl(const base::FilePath& file_path,
                                       const GURL& listener_url) override {
    GURL url;
    file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
        profile_,
        base::FilePath(DriveIntegrationServiceFactory::FindForProfile(profile_)
                           ->GetMountPointPath()
                           .value() +
                       file_path.value()),
        listener_url, &url);
    return url;
  }

  std::vector<GURL> ConvertPathsToFileSystemUrls(
      const std::vector<base::FilePath>& paths,
      const GURL& listener_url) override {
    std::vector<GURL> urls;
    for (const auto& path : paths) {
      GURL url;
      bool did_convert =
          file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
              profile_, path, listener_url, &url);
      LOG_IF(ERROR, !did_convert)
          << "Failed to convert file path to file system URL";
      urls.emplace_back(std::move(url));
    }
    return urls;
  }

  std::string GetDriveFileSystemName() override {
    return DriveIntegrationServiceFactory::FindForProfile(profile_)
        ->GetMountPointPath()
        .BaseName()
        .value();
  }

  bool IsPathWatched(const base::FilePath& path) override {
    base::FilePath absolute_path =
        DriveIntegrationServiceFactory::FindForProfile(profile_)
            ->GetMountPointPath();
    return base::FilePath("/").AppendRelativePath(path, &absolute_path) &&
           base::Contains(*file_watchers_, absolute_path);
  }

  void BroadcastEvent(extensions::events::HistogramValue histogram_value,
                      const std::string& event_name,
                      base::Value::List event_args,
                      bool dispatch_to_system_notification = true) override {
    std::unique_ptr<extensions::Event> event =
        std::make_unique<extensions::Event>(histogram_value, event_name,
                                            std::move(event_args));
    if (dispatch_to_system_notification) {
      system_notification_manager()->HandleEvent(*event.get());
    }
    extensions::EventRouter::Get(profile_)->BroadcastEvent(std::move(event));
  }

  const raw_ptr<Profile, ExperimentalAsh> profile_;
  const raw_ptr<const std::map<base::FilePath, std::unique_ptr<FileWatcher>>,
                ExperimentalAsh>
      file_watchers_;
};

// Records mounted File System Provider type if known otherwise UNKNOWN.
void RecordFileSystemProviderMountMetrics(const Volume& volume) {
  const ash::file_system_provider::ProviderId& provider_id =
      volume.provider_id();
  if (provider_id.GetType() != ash::file_system_provider::ProviderId::INVALID) {
    using FileSystemProviderMountedTypeMap =
        std::unordered_map<std::string, FileSystemProviderMountedType>;

    const std::string fsp_key = provider_id.ToString();
    FileSystemProviderMountedTypeMap fsp_sample_map =
        GetUmaForFileSystemProvider();
    FileSystemProviderMountedTypeMap::iterator sample =
        fsp_sample_map.find(fsp_key);
    if (sample != fsp_sample_map.end()) {
      UMA_HISTOGRAM_ENUMERATION(kFileSystemProviderMountedMetricName,
                                sample->second);
    } else {
      UMA_HISTOGRAM_ENUMERATION(kFileSystemProviderMountedMetricName,
                                FileSystemProviderMountedType::UNKNOWN);
    }
  }
}

// Returns a map from the given `files` to their parent directory.
std::map<base::FilePath, std::vector<base::FilePath>>
MapFilePathsToParentDirectory(const std::vector<base::FilePath> files) {
  std::map<base::FilePath, std::vector<base::FilePath>> dir_files_map;
  for (const auto& file : files) {
    dir_files_map[file.DirName()].push_back(file);
  }
  return dir_files_map;
}

// Creates a file watch event for the given `changed_files` in `directory`
// belonging to a filesystem described by `info`.
extensions::api::file_manager_private::FileWatchEvent CreateFileWatchEvent(
    Profile* profile,
    const GURL& listener_url,
    const std::vector<base::FilePath>& changed_files,
    const storage::FileSystemInfo& info,
    const base::FilePath& directory,
    extensions::api::file_manager_private::ChangeType change_type) {
  extensions::api::file_manager_private::FileWatchEvent event;

  event.event_type =
      extensions::api::file_manager_private::FILE_WATCH_EVENT_TYPE_CHANGED;
  event.entry.additional_properties.Set("fileSystemRoot", info.root_url.spec());
  event.entry.additional_properties.Set("fileSystemName", info.name);
  event.entry.additional_properties.Set("fileFullPath",
                                        "/" + directory.value());
  event.entry.additional_properties.Set("fileIsDirectory", true);

  // Constructs the optional.
  event.changed_files.emplace();

  for (const base::FilePath& file : changed_files) {
    auto& change = event.changed_files->emplace_back();
    GURL url;
    file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
        profile, file, listener_url, &url);
    change.url = url.spec();
    change.changes.push_back(change_type);
  }

  return event;
}

std::unique_ptr<ash::file_system_provider::ScopedUserInteraction>
MaybeStartInteractionWithODFS(const storage::FileSystemURL& url,
                              Profile* profile) {
  ash::file_system_provider::util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    return nullptr;
  }
  if (parser.file_system()->GetFileSystemInfo().provider_id() !=
      ash::file_system_provider::ProviderId::CreateFromExtensionId(
          extension_misc::kODFSExtensionId)) {
    return nullptr;
  }
  return parser.file_system()->StartUserInteraction();
}

}  // namespace

file_manager_private::MountError MountErrorToMountCompletedStatus(
    ash::MountError error) {
  switch (error) {
    case ash::MountError::kSuccess:
      return file_manager_private::MOUNT_ERROR_SUCCESS;
    case ash::MountError::kUnknownError:
      return file_manager_private::MOUNT_ERROR_UNKNOWN_ERROR;
    case ash::MountError::kInternalError:
      return file_manager_private::MOUNT_ERROR_INTERNAL_ERROR;
    case ash::MountError::kInvalidArgument:
      return file_manager_private::MOUNT_ERROR_INVALID_ARGUMENT;
    case ash::MountError::kInvalidPath:
      return file_manager_private::MOUNT_ERROR_INVALID_PATH;
    case ash::MountError::kPathAlreadyMounted:
      return file_manager_private::MOUNT_ERROR_PATH_ALREADY_MOUNTED;
    case ash::MountError::kPathNotMounted:
      return file_manager_private::MOUNT_ERROR_PATH_NOT_MOUNTED;
    case ash::MountError::kDirectoryCreationFailed:
      return file_manager_private::MOUNT_ERROR_DIRECTORY_CREATION_FAILED;
    case ash::MountError::kInvalidMountOptions:
      return file_manager_private::MOUNT_ERROR_INVALID_MOUNT_OPTIONS;
    case ash::MountError::kInsufficientPermissions:
      return file_manager_private::MOUNT_ERROR_INSUFFICIENT_PERMISSIONS;
    case ash::MountError::kMountProgramNotFound:
      return file_manager_private::MOUNT_ERROR_MOUNT_PROGRAM_NOT_FOUND;
    case ash::MountError::kMountProgramFailed:
      return file_manager_private::MOUNT_ERROR_MOUNT_PROGRAM_FAILED;
    case ash::MountError::kInvalidDevicePath:
      return file_manager_private::MOUNT_ERROR_INVALID_DEVICE_PATH;
    case ash::MountError::kUnknownFilesystem:
      return file_manager_private::MOUNT_ERROR_UNKNOWN_FILESYSTEM;
    case ash::MountError::kUnsupportedFilesystem:
      return file_manager_private::MOUNT_ERROR_UNSUPPORTED_FILESYSTEM;
    case ash::MountError::kNeedPassword:
      return file_manager_private::MOUNT_ERROR_NEED_PASSWORD;
    case ash::MountError::kInProgress:
      return file_manager_private::MOUNT_ERROR_IN_PROGRESS;
    case ash::MountError::kCancelled:
      return file_manager_private::MOUNT_ERROR_CANCELLED;
    case ash::MountError::kBusy:
      return file_manager_private::MOUNT_ERROR_BUSY;
    default:
      LOG(ERROR) << "Unexpected mount error: " << error;
      return file_manager_private::MOUNT_ERROR_UNKNOWN_ERROR;
  }
}

EventRouter::EventRouter(Profile* profile)
    : pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()),
      profile_(profile),
      notification_manager_(
          std::make_unique<SystemNotificationManager>(profile)),
      device_event_router_(
          std::make_unique<DeviceEventRouterImpl>(notification_manager_.get(),
                                                  profile)),
      drivefs_event_router_(
          std::make_unique<DriveFsEventRouterImpl>(notification_manager_.get(),
                                                   profile,
                                                   &file_watchers_)),
      dispatch_directory_change_event_impl_(
          base::BindRepeating(&EventRouter::DispatchDirectoryChangeEventImpl,
                              base::Unretained(this))) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Notification manager can call into Drive FS for dialog handling.
  notification_manager_->SetDriveFSEventRouter(drivefs_event_router_.get());
  ObserveEvents();
}

EventRouter::~EventRouter() = default;

void EventRouter::OnIntentFiltersUpdated(
    const absl::optional<std::string>& package_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BroadcastEvent(profile_,
                 extensions::events::FILE_MANAGER_PRIVATE_ON_APPS_UPDATED,
                 file_manager_private::OnAppsUpdated::kEventName,
                 file_manager_private::OnAppsUpdated::Create());
}

void EventRouter::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ash::TabletMode* tablet_mode = ash::TabletMode::Get();
  if (tablet_mode) {
    tablet_mode->RemoveObserver(this);
  }

  auto* intent_helper =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  if (intent_helper) {
    intent_helper->RemoveObserver(this);
  }

  ash::system::TimezoneSettings::GetInstance()->RemoveObserver(this);

  DLOG_IF(WARNING, !file_watchers_.empty())
      << "Not all file watchers are "
      << "removed. This can happen when the Files app is open during shutdown.";
  file_watchers_.clear();
  DCHECK(profile_);

  pref_change_registrar_->RemoveAll();

  extensions::ExtensionRegistry::Get(profile_)->RemoveObserver(this);

  DriveIntegrationService* const integration_service =
      DriveIntegrationServiceFactory::FindForProfile(profile_);
  if (integration_service) {
    integration_service->RemoveObserver(this);
    integration_service->RemoveObserver(drivefs_event_router_.get());
    integration_service->GetDriveFsHost()->RemoveObserver(
        drivefs_event_router_.get());
    integration_service->GetDriveFsHost()->set_dialog_handler({});
  }

  VolumeManager* const volume_manager = VolumeManager::Get(profile_);
  if (volume_manager) {
    volume_manager->RemoveObserver(this);
    volume_manager->RemoveObserver(device_event_router_.get());
    auto* io_task_controller = volume_manager->io_task_controller();
    if (io_task_controller) {
      io_task_controller->RemoveObserver(this);
    }
  }

  chromeos::PowerManagerClient* const power_manager_client =
      chromeos::PowerManagerClient::Get();
  power_manager_client->RemoveObserver(device_event_router_.get());

  auto* guest_os_service = guest_os::GuestOsService::GetForProfile(profile_);
  if (guest_os_service) {
    // GuestOsService doesn't exist for all profiles.
    auto* registry = guest_os_service->MountProviderRegistry();
    registry->RemoveObserver(this);
  }

  auto* guest_os_share_path =
      guest_os::GuestOsSharePath::GetForProfile(profile_);
  if (guest_os_share_path) {
    guest_os_share_path->RemoveObserver(this);
  }

  app_registry_cache_observer_.Reset();

  auto* dlp_client = chromeos::DlpClient::Get();
  if (dlp_client) {
    dlp_client->RemoveObserver(this);
  }

  profile_ = nullptr;
}

void EventRouter::ObserveEvents() {
  DCHECK(profile_);

  if (!ash::LoginState::IsInitialized() ||
      !ash::LoginState::Get()->IsUserLoggedIn()) {
    return;
  }

  // Ignore device events for the first few seconds.
  device_event_router_->Startup();

  // VolumeManager's construction triggers DriveIntegrationService's
  // construction, so it is necessary to call VolumeManager's Get before
  // accessing DriveIntegrationService.
  VolumeManager* const volume_manager = VolumeManager::Get(profile_);
  if (volume_manager) {
    volume_manager->AddObserver(this);
    volume_manager->AddObserver(device_event_router_.get());
    auto* io_task_controller = volume_manager->io_task_controller();
    if (io_task_controller) {
      io_task_controller->AddObserver(this);
      notification_manager_->SetIOTaskController(io_task_controller);
    }
  }

  chromeos::PowerManagerClient* const power_manager_client =
      chromeos::PowerManagerClient::Get();
  power_manager_client->AddObserver(device_event_router_.get());

  DriveIntegrationService* const integration_service =
      DriveIntegrationServiceFactory::FindForProfile(profile_);
  if (integration_service) {
    integration_service->AddObserver(this);
    integration_service->AddObserver(drivefs_event_router_.get());
    integration_service->GetDriveFsHost()->AddObserver(
        drivefs_event_router_.get());
    integration_service->GetDriveFsHost()->set_dialog_handler(
        base::BindRepeating(&EventRouter::DisplayDriveConfirmDialog,
                            weak_factory_.GetWeakPtr()));
  }

  extensions::ExtensionRegistry::Get(profile_)->AddObserver(this);

  pref_change_registrar_->Init(profile_->GetPrefs());

  auto file_manager_prefs_callback = base::BindRepeating(
      &EventRouter::OnFileManagerPrefsChanged, weak_factory_.GetWeakPtr());
  pref_change_registrar_->Add(drive::prefs::kDriveFsBulkPinningEnabled,
                              file_manager_prefs_callback);
  pref_change_registrar_->Add(drive::prefs::kDisableDriveOverCellular,
                              file_manager_prefs_callback);
  pref_change_registrar_->Add(drive::prefs::kDisableDrive,
                              file_manager_prefs_callback);
  pref_change_registrar_->Add(ash::prefs::kFilesAppTrashEnabled,
                              file_manager_prefs_callback);
  pref_change_registrar_->Add(prefs::kSearchSuggestEnabled,
                              file_manager_prefs_callback);
  pref_change_registrar_->Add(prefs::kUse24HourClock,
                              file_manager_prefs_callback);
  pref_change_registrar_->Add(arc::prefs::kArcEnabled,
                              file_manager_prefs_callback);
  pref_change_registrar_->Add(arc::prefs::kArcHasAccessToRemovableMedia,
                              file_manager_prefs_callback);
  pref_change_registrar_->Add(ash::prefs::kFilesAppFolderShortcuts,
                              file_manager_prefs_callback);
  pref_change_registrar_->Add(prefs::kOfficeFileMovedToOneDrive,
                              file_manager_prefs_callback);
  pref_change_registrar_->Add(prefs::kOfficeFileMovedToGoogleDrive,
                              file_manager_prefs_callback);

  auto on_apps_update_callback = base::BindRepeating(
      &EventRouter::BroadcastOnAppsUpdatedEvent, weak_factory_.GetWeakPtr());
  pref_change_registrar_->Add(prefs::kDefaultTasksByMimeType,
                              on_apps_update_callback);
  pref_change_registrar_->Add(prefs::kDefaultTasksBySuffix,
                              on_apps_update_callback);

  ash::system::TimezoneSettings::GetInstance()->AddObserver(this);

  auto* intent_helper =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  if (intent_helper) {
    intent_helper->AddObserver(this);
  }

  auto* guest_os_share_path =
      guest_os::GuestOsSharePath::GetForProfile(profile_);
  if (guest_os_share_path) {
    guest_os_share_path->AddObserver(this);
  }

  ash::TabletMode* tablet_mode = ash::TabletMode::Get();
  if (tablet_mode) {
    tablet_mode->AddObserver(this);
  }

  auto* guest_os_service = guest_os::GuestOsService::GetForProfile(profile_);
  if (guest_os_service) {
    // GuestOsService doesn't exist for all profiles.
    auto* registry = guest_os_service->MountProviderRegistry();
    registry->AddObserver(this);
  }

  if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile_)) {
    apps::AppServiceProxy* proxy =
        apps::AppServiceProxyFactory::GetForProfile(profile_);
    DCHECK(proxy);
    app_registry_cache_observer_.Observe(&proxy->AppRegistryCache());
  }

  auto* dlp_client = chromeos::DlpClient::Get();
  if (dlp_client) {
    dlp_client->AddObserver(this);
  }
}

// File watch setup routines.
void EventRouter::AddFileWatch(const base::FilePath& local_path,
                               const base::FilePath& virtual_path,
                               const url::Origin& listener_origin,
                               BoolCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  auto iter = file_watchers_.find(local_path);
  if (iter == file_watchers_.end()) {
    std::unique_ptr<FileWatcher> watcher(new FileWatcher(virtual_path));
    watcher->AddListener(listener_origin);
    watcher->WatchLocalFile(
        profile_, local_path,
        base::BindRepeating(&EventRouter::HandleFileWatchNotification,
                            weak_factory_.GetWeakPtr()),
        std::move(callback));

    file_watchers_[local_path] = std::move(watcher);
  } else {
    iter->second->AddListener(listener_origin);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
  }
}

void EventRouter::RemoveFileWatch(const base::FilePath& local_path,
                                  const url::Origin& listener_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto iter = file_watchers_.find(local_path);
  if (iter == file_watchers_.end()) {
    return;
  }
  // Remove the watcher if |local_path| is no longer watched by any extensions.
  iter->second->RemoveListener(listener_origin);
  if (iter->second->GetListeners().empty()) {
    file_watchers_.erase(iter);
  }
}

void EventRouter::OnWatcherManagerNotification(
    const storage::FileSystemURL& file_system_url,
    const url::Origin& listener_origin,
    storage::WatcherManager::ChangeType /* change_type */) {
  std::vector<url::Origin> listeners = {listener_origin};

  DispatchDirectoryChangeEvent(file_system_url.virtual_path(),
                               false /* error */, listeners);
}

void EventRouter::OnExtensionLoaded(content::BrowserContext* browser_context,
                                    const extensions::Extension* extension) {
  NotifyDriveConnectionStatusChanged();
}

void EventRouter::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  NotifyDriveConnectionStatusChanged();
}

void EventRouter::TimezoneChanged(const icu::TimeZone& timezone) {
  OnFileManagerPrefsChanged();
}

void EventRouter::OnFileManagerPrefsChanged() {
  DCHECK(profile_);
  DCHECK(extensions::EventRouter::Get(profile_));

  BroadcastEvent(
      profile_, extensions::events::FILE_MANAGER_PRIVATE_ON_PREFERENCES_CHANGED,
      file_manager_private::OnPreferencesChanged::kEventName,
      file_manager_private::OnPreferencesChanged::Create());
}

void EventRouter::HandleFileWatchNotification(const base::FilePath& local_path,
                                              bool got_error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto iter = file_watchers_.find(local_path);
  if (iter == file_watchers_.end()) {
    return;
  }

  DispatchDirectoryChangeEvent(iter->second->virtual_path(), got_error,
                               iter->second->GetListeners());
}

void EventRouter::DispatchDirectoryChangeEvent(
    const base::FilePath& virtual_path,
    bool got_error,
    const std::vector<url::Origin>& listeners) {
  dispatch_directory_change_event_impl_.Run(virtual_path, got_error, listeners);
}

void EventRouter::DispatchDirectoryChangeEventImpl(
    const base::FilePath& virtual_path,
    bool got_error,
    const std::vector<url::Origin>& listeners) {
  DCHECK(profile_);

  for (const url::Origin& origin : listeners) {
    FileDefinition file_definition;
    file_definition.virtual_path = virtual_path;
    // TODO(mtomasz): Add support for watching files in File System Provider
    // API.
    file_definition.is_directory = true;

    file_manager::util::ConvertFileDefinitionToEntryDefinition(
        util::GetFileSystemContextForSourceURL(profile_, origin.GetURL()),
        origin, file_definition,
        base::BindOnce(
            &EventRouter::DispatchDirectoryChangeEventWithEntryDefinition,
            weak_factory_.GetWeakPtr(), got_error));
  }
}

void EventRouter::DispatchDirectoryChangeEventWithEntryDefinition(
    bool watcher_error,
    const EntryDefinition& entry_definition) {
  // TODO(mtomasz): Add support for watching files in File System Provider API.
  if (entry_definition.error != base::File::FILE_OK ||
      !entry_definition.is_directory) {
    DVLOG(1) << "Unable to dispatch event because resolving the directory "
             << "entry definition failed.";
    return;
  }

  file_manager_private::FileWatchEvent event;
  event.event_type = watcher_error
                         ? file_manager_private::FILE_WATCH_EVENT_TYPE_ERROR
                         : file_manager_private::FILE_WATCH_EVENT_TYPE_CHANGED;

  event.entry.additional_properties.Set("fileSystemName",
                                        entry_definition.file_system_name);
  event.entry.additional_properties.Set("fileSystemRoot",
                                        entry_definition.file_system_root_url);
  event.entry.additional_properties.Set(
      "fileFullPath", "/" + entry_definition.full_path.value());
  event.entry.additional_properties.Set("fileIsDirectory",
                                        entry_definition.is_directory);

  BroadcastEvent(profile_,
                 extensions::events::FILE_MANAGER_PRIVATE_ON_DIRECTORY_CHANGED,
                 file_manager_private::OnDirectoryChanged::kEventName,
                 file_manager_private::OnDirectoryChanged::Create(event));
}

void EventRouter::OnDiskAdded(const Disk& disk, bool mounting) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Do nothing.
}

void EventRouter::OnDiskRemoved(const Disk& disk) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Do nothing.
}

void EventRouter::OnDeviceAdded(const std::string& device_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Do nothing.
}

void EventRouter::OnDeviceRemoved(const std::string& device_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Do nothing.
}

void EventRouter::OnVolumeMounted(ash::MountError error_code,
                                  const Volume& volume) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // profile_ is NULL if ShutdownOnUIThread() is called earlier. This can
  // happen at shutdown. This should be removed after removing Drive mounting
  // code in addMount. (addMount -> OnFileSystemMounted -> OnVolumeMounted is
  // the only path to come here after Shutdown is called).
  if (!profile_) {
    return;
  }

  DispatchMountCompletedEvent(
      file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT, error_code,
      volume);

  // Record the UMA metrics for mounted FSPs.
  RecordFileSystemProviderMountMetrics(volume);

  // TODO(mtomasz): Move VolumeManager and part of the event router outside of
  // file_manager, so there is no dependency between File System API and the
  // file_manager code.
  extensions::file_system_api::DispatchVolumeListChangeEventAsh(profile_);
}

void EventRouter::OnVolumeUnmounted(ash::MountError error_code,
                                    const Volume& volume) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DispatchMountCompletedEvent(
      file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_UNMOUNT, error_code,
      volume);

  // TODO(mtomasz): Move VolumeManager and part of the event router outside of
  // file_manager, so there is no dependency between File System API and the
  // file_manager code.
  extensions::file_system_api::DispatchVolumeListChangeEventAsh(profile_);
}

void EventRouter::DispatchMountCompletedEvent(
    file_manager_private::MountCompletedEventType event_type,
    ash::MountError error,
    const Volume& volume) {
  // Build an event object.
  file_manager_private::MountCompletedEvent event;
  event.event_type = event_type;
  event.status = MountErrorToMountCompletedStatus(error);
  util::VolumeToVolumeMetadata(profile_, volume, &event.volume_metadata);
  event.should_notify =
      ShouldShowNotificationForVolume(profile_, *device_event_router_, volume);
  notification_manager_->HandleMountCompletedEvent(event, volume);
  BroadcastEvent(profile_,
                 extensions::events::FILE_MANAGER_PRIVATE_ON_MOUNT_COMPLETED,
                 file_manager_private::OnMountCompleted::kEventName,
                 file_manager_private::OnMountCompleted::Create(event));
}

void EventRouter::OnFormatStarted(const std::string& device_path,
                                  const std::string& device_label,
                                  bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Do nothing.
}

void EventRouter::OnFormatCompleted(const std::string& device_path,
                                    const std::string& device_label,
                                    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Do nothing.
}

void EventRouter::OnPartitionStarted(const std::string& device_path,
                                     const std::string& device_label,
                                     bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Do nothing.
}

void EventRouter::OnPartitionCompleted(const std::string& device_path,
                                       const std::string& device_label,
                                       bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Do nothing.
}

void EventRouter::OnRenameStarted(const std::string& device_path,
                                  const std::string& device_label,
                                  bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Do nothing.
}

void EventRouter::OnRenameCompleted(const std::string& device_path,
                                    const std::string& device_label,
                                    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Do nothing.
}

void EventRouter::SetDispatchDirectoryChangeEventImplForTesting(
    const DispatchDirectoryChangeEventImplCallback& callback) {
  dispatch_directory_change_event_impl_ = callback;
}

void EventRouter::OnFileSystemMountFailed() {
  OnFileManagerPrefsChanged();
}

void EventRouter::OnDriveConnectionStatusChanged(
    drive::util::ConnectionStatusType status) {
  NotifyDriveConnectionStatusChanged();
}

// Send crostini share, unshare event.
void EventRouter::SendCrostiniEvent(
    file_manager_private::CrostiniEventType event_type,
    const std::string& vm_name,
    const base::FilePath& path) {
  std::string mount_name;
  std::string file_system_name;
  std::string full_path;
  if (!util::ExtractMountNameFileSystemNameFullPath(
          path, &mount_name, &file_system_name, &full_path)) {
    return;
  }

  const std::string event_name(
      file_manager_private::OnCrostiniChanged::kEventName);
  const extensions::EventListenerMap::ListenerList& listeners =
      extensions::EventRouter::Get(profile_)
          ->listeners()
          .GetEventListenersByName(event_name);

  // We handle two types of listeners, those with extension IDs and those with
  // listener URL. For listeners with extension IDs we use direct dispatch. For
  // listeners with listener URL we use a broadcast.
  std::set<std::string> extension_ids;
  std::set<url::Origin> origins;
  for (auto const& listener : listeners) {
    if (!listener->extension_id().empty()) {
      extension_ids.insert(listener->extension_id());
    } else if (listener->listener_url().is_valid()) {
      origins.insert(url::Origin::Create(listener->listener_url()));
    }
  }

  for (const std::string& extension_id : extension_ids) {
    url::Origin origin = url::Origin::Create(
        extensions::Extension::GetBaseURLFromExtensionId(extension_id));
    file_manager_private::CrostiniEvent event;
    PopulateCrostiniEvent(event, event_type, vm_name, origin, mount_name,
                          file_system_name, full_path);
    DispatchEventToExtension(
        profile_, extension_id,
        extensions::events::FILE_MANAGER_PRIVATE_ON_CROSTINI_CHANGED,
        event_name, file_manager_private::OnCrostiniChanged::Create(event));
  }
  for (const url::Origin& origin : origins) {
    file_manager_private::CrostiniEvent event;
    PopulateCrostiniEvent(event, event_type, vm_name, origin, mount_name,
                          file_system_name, full_path);
    BroadcastEvent(
        profile_, extensions::events::FILE_MANAGER_PRIVATE_ON_CROSTINI_CHANGED,
        event_name, file_manager_private::OnCrostiniChanged::Create(event));
  }
}

// static
void EventRouter::PopulateCrostiniEvent(
    file_manager_private::CrostiniEvent& event,
    file_manager_private::CrostiniEventType event_type,
    const std::string& vm_name,
    const url::Origin& origin,
    const std::string& mount_name,
    const std::string& file_system_name,
    const std::string& full_path) {
  event.event_type = event_type;
  event.vm_name = vm_name;
  event.container_name = "";  // Unused for the event types handled by this.
  file_manager_private::CrostiniEvent::EntriesType entry;
  entry.additional_properties.Set(
      "fileSystemRoot",
      storage::GetExternalFileSystemRootURIString(origin.GetURL(), mount_name));
  entry.additional_properties.Set("fileSystemName", file_system_name);
  entry.additional_properties.Set("fileFullPath", full_path);
  entry.additional_properties.Set("fileIsDirectory", true);
  event.entries.emplace_back(std::move(entry));
}

void EventRouter::OnPersistedPathRegistered(const std::string& vm_name,
                                            const base::FilePath& path) {
  SendCrostiniEvent(file_manager_private::CROSTINI_EVENT_TYPE_SHARE, vm_name,
                    path);
}

void EventRouter::OnUnshare(const std::string& vm_name,
                            const base::FilePath& path) {
  SendCrostiniEvent(file_manager_private::CROSTINI_EVENT_TYPE_UNSHARE, vm_name,
                    path);
}

void EventRouter::OnGuestRegistered(const guest_os::GuestId& guest) {
  file_manager_private::CrostiniEvent event;
  event.vm_name = guest.vm_name;
  event.container_name = guest.container_name;
  event.event_type =
      extensions::api::file_manager_private::CROSTINI_EVENT_TYPE_ENABLE;
  BroadcastEvent(profile_,
                 extensions::events::FILE_MANAGER_PRIVATE_ON_CROSTINI_CHANGED,
                 file_manager_private::OnCrostiniChanged::kEventName,
                 file_manager_private::OnCrostiniChanged::Create(event));
}

void EventRouter::OnGuestUnregistered(const guest_os::GuestId& guest) {
  file_manager_private::CrostiniEvent event;
  event.vm_name = guest.vm_name;
  event.container_name = guest.container_name;
  event.event_type =
      extensions::api::file_manager_private::CROSTINI_EVENT_TYPE_DISABLE;
  BroadcastEvent(profile_,
                 extensions::events::FILE_MANAGER_PRIVATE_ON_CROSTINI_CHANGED,
                 file_manager_private::OnCrostiniChanged::kEventName,
                 file_manager_private::OnCrostiniChanged::Create(event));
}

void EventRouter::OnTabletModeStarted() {
  BroadcastEvent(
      profile_, extensions::events::FILE_MANAGER_PRIVATE_ON_TABLET_MODE_CHANGED,
      file_manager_private::OnTabletModeChanged::kEventName,
      file_manager_private::OnTabletModeChanged::Create(/*enabled=*/true));
}

void EventRouter::OnTabletModeEnded() {
  BroadcastEvent(
      profile_, extensions::events::FILE_MANAGER_PRIVATE_ON_TABLET_MODE_CHANGED,
      file_manager_private::OnTabletModeChanged::kEventName,
      file_manager_private::OnTabletModeChanged::Create(/*enabled=*/false));
}

void EventRouter::NotifyDriveConnectionStatusChanged() {
  DCHECK(profile_);
  DCHECK(extensions::EventRouter::Get(profile_));

  BroadcastEvent(
      profile_,
      extensions::events::
          FILE_MANAGER_PRIVATE_ON_DRIVE_CONNECTION_STATUS_CHANGED,
      file_manager_private::OnDriveConnectionStatusChanged::kEventName,
      file_manager_private::OnDriveConnectionStatusChanged::Create());
}

void EventRouter::DropFailedPluginVmDirectoryNotShared() {
  file_manager_private::CrostiniEvent event;
  event.vm_name = plugin_vm::kPluginVmName;
  event.event_type = file_manager_private::
      CROSTINI_EVENT_TYPE_DROP_FAILED_PLUGIN_VM_DIRECTORY_NOT_SHARED;
  BroadcastEvent(profile_,
                 extensions::events::FILE_MANAGER_PRIVATE_ON_CROSTINI_CHANGED,
                 file_manager_private::OnCrostiniChanged::kEventName,
                 file_manager_private::OnCrostiniChanged::Create(event));
}

void EventRouter::DisplayDriveConfirmDialog(
    const drivefs::mojom::DialogReason& reason,
    base::OnceCallback<void(drivefs::mojom::DialogResult)> callback) {
  drivefs_event_router_->DisplayConfirmDialog(reason, std::move(callback));
}

void EventRouter::OnDriveDialogResult(drivefs::mojom::DialogResult result) {
  drivefs_event_router_->OnDialogResult(result);
}

void EventRouter::SuppressDriveNotificationsForFilePath(
    const base::FilePath& relative_drive_path) {
  drivefs_event_router_->SuppressNotificationsForFilePath(relative_drive_path);
}

void EventRouter::RestoreDriveNotificationsForFilePath(
    const base::FilePath& relative_drive_path) {
  drivefs_event_router_->RestoreNotificationsForFilePath(relative_drive_path);
}

base::WeakPtr<EventRouter> EventRouter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void EventRouter::OnIOTaskStatus(const io_task::ProgressStatus& status) {
  // Send the progress report to the system notification regardless of whether
  // Files app window exists as we may need to remove an existing
  // notification.
  notification_manager_->HandleIOTaskProgress(status);
  if (!DoFilesSwaWindowsExist(profile_) && !force_broadcasting_for_testing_) {
    return;
  }

  // If copying to/from ODFS, mark the provider's request manager
  // as "interacting with user" to prevent long operation warnings when
  // progress UI is already displayed.
  if (chromeos::features::IsUploadOfficeToCloudEnabled()) {
    if (status.IsCompleted()) {
      odfs_interactions_.erase(status.task_id);
    } else {
      auto it = odfs_interactions_.find(status.task_id);
      if (it == odfs_interactions_.end()) {
        auto interaction = MaybeStartInteractionWithODFS(
            status.GetDestinationFolder(), profile_);
        if (!interaction) {
          for (const io_task::EntryStatus& entry : status.sources) {
            interaction = MaybeStartInteractionWithODFS(entry.url, profile_);
            if (interaction) {
              break;
            }
          }
        }
        if (interaction) {
          odfs_interactions_[status.task_id] = std::move(interaction);
        }
      }
    }
  }

  // Send directory change events on I/O task completion. inotify is flaky on
  // some filesystems, so send these notifications so that at least operations
  // made from Files App are always reflected in the UI. Additionally, this
  // ensures the directory tree will be updated too, as the tree needs
  // notifications for folders outside of those being watched by a file watcher.
  if (status.IsCompleted()) {
    std::set<std::pair<base::FilePath, url::Origin>> updated_paths;
    if (status.GetDestinationFolder().is_valid()) {
      updated_paths.emplace(status.GetDestinationFolder().virtual_path(),
                            status.GetDestinationFolder().origin());
    }
    for (const auto& source : status.sources) {
      updated_paths.emplace(source.url.virtual_path().DirName(),
                            source.url.origin());
    }
    for (const auto& output : status.outputs) {
      updated_paths.emplace(output.url.virtual_path().DirName(),
                            output.url.origin());
    }

    for (const auto& [path, origin] : updated_paths) {
      DispatchDirectoryChangeEvent(path, false, {origin});
    }
  }

  // If any Files app window exists we send the progress to all of them.
  file_manager_private::ProgressStatus event_status;
  event_status.task_id = status.task_id;
  event_status.type = GetIOTaskType(status.type);
  event_status.state = GetIOTaskState(status.state);
  if (status.policy_error.has_value()) {
    event_status.policy_error.emplace();
    event_status.policy_error->type =
        GetPolicyErrorType(status.policy_error->type);
    event_status.policy_error->policy_file_count =
        status.policy_error->blocked_files;
    event_status.policy_error->file_name = status.policy_error->file_name;
  }
  event_status.sources_scanned = status.sources_scanned;
  event_status.destination_volume_id = status.GetDestinationVolumeId();
  event_status.show_notification = status.show_notification;

  // Speedometer can produce infinite result which can't be serialized to JSON
  // when sending the status via private API.
  if (std::isfinite(status.remaining_seconds)) {
    event_status.remaining_seconds = status.remaining_seconds;
  }

  if (status.GetDestinationFolder().is_valid()) {
    event_status.destination_name =
        util::GetDisplayablePath(profile_, status.GetDestinationFolder())
            .value_or(base::FilePath())
            .BaseName()
            .value();
  }

  size_t processed = 0;
  std::vector<storage::FileSystemURL> outputs;
  for (const auto& file_status : status.outputs) {
    if (file_status.error) {
      if (status.type == file_manager::io_task::OperationType::kTrash &&
          file_status.error.value() == base::File::FILE_OK) {
        // These entries are currently used to undo a TrashIOTask so only
        // consider the successfully trashed files.
        outputs.push_back(file_status.url);
      }
      processed++;
    }
  }

  event_status.num_remaining_items = status.sources.size() - processed;
  event_status.item_count = status.sources.size();

  // Get the last error occurrence in the `sources`.
  for (const io_task::EntryStatus& source : base::Reversed(status.sources)) {
    if (source.error && source.error.value() != base::File::FILE_OK) {
      event_status.error_name = FileErrorToErrorName(source.error.value());
    }
  }
  // If we have no error on 'sources', check if an error came from 'outputs'.
  if (status.state == io_task::State::kError &&
      event_status.error_name.empty()) {
    for (const io_task::EntryStatus& dest : base::Reversed(status.outputs)) {
      if (dest.error && dest.error.value() != base::File::FILE_OK) {
        event_status.error_name = FileErrorToErrorName(dest.error.value());
      }
    }
  }

  event_status.source_name = status.GetSourceName(profile_);
  event_status.bytes_transferred = status.bytes_transferred;
  event_status.total_bytes = status.total_bytes;

  // CopyOrMoveIOTask can enter PAUSED state when it needs the user to resolve
  // a file name conflict, or because it needs user to review a policy warning.
  if (GetIOTaskState(status.state) ==
      file_manager_private::IO_TASK_STATE_PAUSED) {
    file_manager_private::PauseParams pause_params;
    if (status.pause_params.conflict_params) {
      pause_params.conflict_params.emplace();
      pause_params.conflict_params->conflict_name =
          status.pause_params.conflict_params->conflict_name;
      pause_params.conflict_params->conflict_multiple =
          status.pause_params.conflict_params->conflict_multiple;
      pause_params.conflict_params->conflict_is_directory =
          status.pause_params.conflict_params->conflict_is_directory;
      pause_params.conflict_params->conflict_target_url =
          status.pause_params.conflict_params->conflict_target_url;
    }
    if (status.pause_params.policy_params) {
      pause_params.policy_params.emplace();
      pause_params.policy_params->type =
          GetPolicyErrorType(status.pause_params.policy_params->type);
      pause_params.policy_params->policy_file_count =
          status.pause_params.policy_params->warning_files_count;
      pause_params.policy_params->file_name =
          status.pause_params.policy_params->file_name;
    }
    event_status.pause_params = std::move(pause_params);
  }

  // The TrashIOTask is the only IOTask that uses the output Entry's, so don't
  // try to resolve the outputs for all other IOTasks.
  if (GetIOTaskType(status.type) != file_manager_private::IO_TASK_TYPE_TRASH ||
      outputs.size() == 0) {
    BroadcastIOTask(std::move(event_status));
    return;
  }

  // All FileSystemURLs in the output come from the same FileSystemContext, so
  // use the first URL to obtain the context.
  auto* file_system_context = util::GetFileSystemContextForSourceURL(
      profile_, outputs[0].origin().GetURL());
  if (file_system_context == nullptr) {
    LOG(ERROR) << "Could not find file system context";
    BroadcastIOTask(std::move(event_status));
    return;
  }

  file_manager::util::FileDefinitionList file_definition_list;
  for (const auto& url : outputs) {
    file_manager::util::FileDefinition file_definition;
    if (file_manager::util::ConvertAbsoluteFilePathToRelativeFileSystemPath(
            profile_, url.origin().GetURL(), url.path(),
            &file_definition.virtual_path)) {
      file_definition_list.push_back(std::move(file_definition));
    }
  }

  file_manager::util::ConvertFileDefinitionListToEntryDefinitionList(
      file_system_context, outputs[0].origin(), std::move(file_definition_list),
      base::BindOnce(
          &EventRouter::OnConvertFileDefinitionListToEntryDefinitionList,
          weak_factory_.GetWeakPtr(), std::move(event_status)));
}

void EventRouter::OnConvertFileDefinitionListToEntryDefinitionList(
    file_manager_private::ProgressStatus event_status,
    std::unique_ptr<file_manager::util::EntryDefinitionList>
        entry_definition_list) {
  if (entry_definition_list == nullptr) {
    BroadcastIOTask(std::move(event_status));
    return;
  }
  std::vector<OutputsType> outputs;
  for (const auto& def : *entry_definition_list) {
    if (def.error != base::File::FILE_OK) {
      LOG(WARNING) << "File entry ignored: " << static_cast<int>(def.error);
      continue;
    }
    OutputsType output_entry;
    output_entry.additional_properties.Set("fileSystemName",
                                           def.file_system_name);
    output_entry.additional_properties.Set("fileSystemRoot",
                                           def.file_system_root_url);
    // The `full_path` comes back as relative to the file system root, but the
    // UI requires it as an absolute path.
    output_entry.additional_properties.Set(
        "fileFullPath", base::FilePath("/").Append(def.full_path).value());
    output_entry.additional_properties.Set("fileIsDirectory", def.is_directory);
    outputs.push_back(std::move(output_entry));
  }
  event_status.outputs = std::move(outputs);
  BroadcastIOTask(std::move(event_status));
}

void EventRouter::OnFilesChanged(
    const std::vector<base::FilePath>& changed_files,
    extensions::api::file_manager_private::ChangeType change_type) {
  std::map<base::FilePath, std::vector<base::FilePath>> files_to_directory_map =
      MapFilePathsToParentDirectory(changed_files);
  for (const auto& listener_url : GetEventListenerURLs(
           profile_, file_manager_private::OnDirectoryChanged::kEventName)) {
    BroadcastDirectoryChangeEvent(files_to_directory_map, listener_url,
                                  change_type);
  }
}

void EventRouter::BroadcastDirectoryChangeEvent(
    const std::map<base::FilePath, std::vector<base::FilePath>>&
        files_to_directory_map,
    const GURL& listener_url,
    extensions::api::file_manager_private::ChangeType change_type) {
  auto* file_system_context =
      util::GetFileSystemContextForSourceURL(profile_, listener_url);
  if (file_system_context == nullptr) {
    LOG(ERROR) << "Could not find file system context";
    return;
  }
  for (const auto& [dir, files] : files_to_directory_map) {
    GURL dir_url;
    file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
        profile_, dir, listener_url, &dir_url);

    const storage::FileSystemURL dir_filesystem_url =
        file_system_context->CrackURLInFirstPartyContext(dir_url);
    if (dir_filesystem_url.path().empty()) {
      LOG(ERROR) << "Invalid URL";
      continue;
    }

    file_system_context->ResolveURL(
        dir_filesystem_url,
        base::BindOnce(
            &EventRouter::BroadcastDirectoryChangeEventOnFilesystemInfoResolved,
            weak_factory_.GetWeakPtr(), listener_url, std::move(files),
            change_type));
  }
}

void EventRouter::BroadcastDirectoryChangeEventOnFilesystemInfoResolved(
    GURL listener_url,
    std::vector<base::FilePath> changed_files,
    extensions::api::file_manager_private::ChangeType change_type,
    base::File::Error result,
    const storage::FileSystemInfo& info,
    const base::FilePath& dir_path,
    storage::FileSystemContext::ResolvedEntryType) {
  extensions::api::file_manager_private::FileWatchEvent event =
      CreateFileWatchEvent(profile_, listener_url, changed_files, info,
                           dir_path, change_type);
  BroadcastEvent(profile_,
                 extensions::events::FILE_MANAGER_PRIVATE_ON_DIRECTORY_CHANGED,
                 file_manager_private::OnDirectoryChanged::kEventName,
                 file_manager_private::OnDirectoryChanged::Create(event));
}

void EventRouter::BroadcastIOTask(
    const file_manager_private::ProgressStatus& event_status) {
  BroadcastEvent(
      profile_,
      extensions::events::FILE_MANAGER_PRIVATE_ON_IO_TASK_PROGRESS_STATUS,
      file_manager_private::OnIOTaskProgressStatus::kEventName,
      file_manager_private::OnIOTaskProgressStatus::Create(event_status));
}

void EventRouter::OnRegistered(guest_os::GuestOsMountProviderRegistry::Id id,
                               guest_os::GuestOsMountProvider* provider) {
  OnMountableGuestsChanged();
}

void EventRouter::OnUnregistered(
    guest_os::GuestOsMountProviderRegistry::Id id) {
  OnMountableGuestsChanged();
}

void EventRouter::BroadcastOnAppsUpdatedEvent() {
  DCHECK(profile_);
  DCHECK(extensions::EventRouter::Get(profile_));

  BroadcastEvent(profile_,
                 extensions::events::FILE_MANAGER_PRIVATE_ON_APPS_UPDATED,
                 file_manager_private::OnAppsUpdated::kEventName,
                 file_manager_private::OnAppsUpdated::Create());
}

void EventRouter::OnMountableGuestsChanged() {
  auto guests = util::CreateMountableGuestList(profile_);
  BroadcastEvent(
      profile_,
      extensions::events::FILE_MANAGER_PRIVATE_ON_IO_TASK_PROGRESS_STATUS,
      file_manager_private::OnMountableGuestsChanged::kEventName,
      file_manager_private::OnMountableGuestsChanged::Create(guests));
}

drivefs::SyncState EventRouter::GetDriveSyncStateForPath(
    const base::FilePath& drive_path) {
  return drivefs_event_router_->GetDriveSyncStateForPath(drive_path);
}

void EventRouter::OnFilesAddedToDlpDaemon(
    const std::vector<base::FilePath>& files) {
  OnFilesChanged(files, extensions::api::file_manager_private::ChangeType::
                            CHANGE_TYPE_ADD_OR_UPDATE);
}

// Observes App Service and notifies Files app when there are any changes in the
// apps which might affect which file tasks are currently available, e.g. when
// an app is installed or uninstalled.
void EventRouter::OnAppUpdate(const apps::AppUpdate& update) {
  BroadcastOnAppsUpdatedEvent();
}

void EventRouter::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

}  // namespace file_manager
