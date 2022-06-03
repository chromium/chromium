// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/event_router.h"

#include <stddef.h>

#include <cmath>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/disks/disk.h"
#include "ash/components/drivefs/drivefs_host.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/webui/file_manager/file_manager_ui.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_system_provider_metrics_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/extensions/api/file_system/chrome_file_system_delegate.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/components/disks/disks_prefs.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "storage/browser/file_system/external_mount_points.h"
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

// Frequency of sending onFileTransferUpdated.
const int64_t kProgressEventFrequencyInMilliseconds = 1000;

// Whether Files SWA has any open windows.
bool DoFilesSwaWindowsExist(Profile* profile) {
  return ash::file_manager::FileManagerUI::GetNumInstances() != 0;
}

// Checks if the Recovery Tool is running. This is a temporary solution.
// TODO(mtomasz): Replace with crbug.com/341902 solution.
bool IsRecoveryToolRunning(Profile* profile) {
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(profile);
  if (!extension_prefs)
    return false;

  const std::string kRecoveryToolIds[] = {
      "kkebgepbbgbcmghedmmdfcbdcodlkngh",  // Recovery tool staging
      "jndclpdbaamdhonoechobihbbiimdgai"   // Recovery tool prod
  };

  for (size_t i = 0; i < std::size(kRecoveryToolIds); ++i) {
    const std::string extension_id = kRecoveryToolIds[i];
    if (extension_prefs->IsExtensionRunning(extension_id))
      return true;
  }

  return false;
}

// Sends an event named |event_name| with arguments |event_args| to extensions.
void BroadcastEvent(Profile* profile,
                    extensions::events::HistogramValue histogram_value,
                    const std::string& event_name,
                    std::vector<base::Value> event_args) {
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
    std::vector<base::Value> event_args) {
  extensions::EventRouter::Get(profile)->DispatchEventToExtension(
      extension_id, std::make_unique<extensions::Event>(
                        histogram_value, event_name, std::move(event_args)));
}

file_manager_private::CopyOrMoveProgressStatusType
CopyOrMoveProgressTypeToCopyOrMoveProgressStatusType(
    FileManagerCopyOrMoveHookDelegate::ProgressType type) {
  switch (type) {
    case FileManagerCopyOrMoveHookDelegate::ProgressType::kBegin:
      return file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_BEGIN;
    case FileManagerCopyOrMoveHookDelegate::ProgressType::kProgress:
      return file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_PROGRESS;
    case FileManagerCopyOrMoveHookDelegate::ProgressType::kEndCopy:
      return file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_END_COPY;
    case FileManagerCopyOrMoveHookDelegate::ProgressType::kEndMove:
      return file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_END_MOVE;
    case FileManagerCopyOrMoveHookDelegate::ProgressType::kEndRemoveSource:
      return file_manager_private::
          COPY_OR_MOVE_PROGRESS_STATUS_TYPE_END_REMOVE_SOURCE;
    case FileManagerCopyOrMoveHookDelegate::ProgressType::kError:
      return file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_ERROR;
  }
  NOTREACHED();
  return file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_NONE;
}

// Convert the IO Task State enum to the Private API enum.
file_manager_private::IOTaskState GetIOTaskState(
    file_manager::io_task::State state) {
  switch (state) {
    case file_manager::io_task::State::kQueued:
      return file_manager_private::IO_TASK_STATE_QUEUED;
    case file_manager::io_task::State::kInProgress:
      return file_manager_private::IO_TASK_STATE_IN_PROGRESS;
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
    case file_manager::io_task::OperationType::kExtract:
      return file_manager_private::IO_TASK_TYPE_EXTRACT;
    case file_manager::io_task::OperationType::kMove:
      return file_manager_private::IO_TASK_TYPE_MOVE;
    case file_manager::io_task::OperationType::kTrash:
      return file_manager_private::IO_TASK_TYPE_TRASH;
    case file_manager::io_task::OperationType::kZip:
      return file_manager_private::IO_TASK_TYPE_ZIP;
    default:
      NOTREACHED();
      return file_manager_private::IO_TASK_TYPE_COPY;
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

// Checks if we should send a progress event or not according to the
// |last_time| of sending an event. If |always| is true, the function always
// returns true. If the function returns true, the function also updates
// |last_time|.
bool ShouldSendProgressEvent(bool always, base::Time* last_time) {
  const base::Time now = base::Time::Now();
  const int64_t delta = (now - *last_time).InMilliseconds();
  // delta < 0 may rarely happen if system clock is synced and rewinded.
  // To be conservative, we don't skip in that case.
  if (!always && 0 <= delta && delta < kProgressEventFrequencyInMilliseconds) {
    return false;
  } else {
    *last_time = now;
    return true;
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

  if (device_event_router.is_resuming() || device_event_router.is_starting_up())
    return false;

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
  if (IsRecoveryToolRunning(profile))
    return false;

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
  Profile* const profile_;
};

class DriveFsEventRouterImpl : public DriveFsEventRouter {
 public:
  DriveFsEventRouterImpl(const DriveFsEventRouterImpl&) = delete;
  DriveFsEventRouterImpl(
      SystemNotificationManager* notification_manager,
      Profile* profile,
      const std::map<base::FilePath, std::unique_ptr<FileWatcher>>*
          file_watchers)
      : DriveFsEventRouter(notification_manager),
        profile_(profile),
        file_watchers_(file_watchers) {}

  DriveFsEventRouterImpl& operator=(const DriveFsEventRouterImpl&) = delete;

 private:
  std::set<GURL> GetEventListenerURLs(const std::string& event_name) override {
    const extensions::EventListenerMap::ListenerList& listeners =
        extensions::EventRouter::Get(profile_)
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
    // In SWA, there may not be a window open to listen to the event, so always
    // add the File Manager URL so events can be sent to the
    // SystemNotificationManager.
    if (ash::features::IsFileManagerSwaEnabled()) {
      urls.insert(file_manager::util::GetFileManagerURL());
    }
    return urls;
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
                      std::vector<base::Value> event_args) override {
    std::unique_ptr<extensions::Event> event =
        std::make_unique<extensions::Event>(histogram_value, event_name,
                                            std::move(event_args));
    system_notification_manager()->HandleEvent(*event.get());
    extensions::EventRouter::Get(profile_)->BroadcastEvent(std::move(event));
  }

  Profile* const profile_;
  const std::map<base::FilePath, std::unique_ptr<FileWatcher>>* const
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
    if (sample != fsp_sample_map.end())
      UMA_HISTOGRAM_ENUMERATION(kFileSystemProviderMountedMetricName,
                                sample->second);
    else
      UMA_HISTOGRAM_ENUMERATION(kFileSystemProviderMountedMetricName,
                                FileSystemProviderMountedType::UNKNOWN);
  }
}

}  // namespace

file_manager_private::MountCompletedStatus MountErrorToMountCompletedStatus(
    chromeos::MountError error) {
  switch (error) {
    case chromeos::MOUNT_ERROR_NONE:
      return file_manager_private::MOUNT_COMPLETED_STATUS_SUCCESS;
    case chromeos::MOUNT_ERROR_UNKNOWN:
      return file_manager_private::MOUNT_COMPLETED_STATUS_ERROR_UNKNOWN;
    case chromeos::MOUNT_ERROR_INTERNAL:
      return file_manager_private::MOUNT_COMPLETED_STATUS_ERROR_INTERNAL;
    case chromeos::MOUNT_ERROR_INVALID_ARGUMENT:
      return file_manager_private::
          MOUNT_COMPLETED_STATUS_ERROR_INVALID_ARGUMENT;
    case chromeos::MOUNT_ERROR_INVALID_PATH:
      return file_manager_private::MOUNT_COMPLETED_STATUS_ERROR_INVALID_PATH;
    case chromeos::MOUNT_ERROR_PATH_ALREADY_MOUNTED:
      return file_manager_private::
          MOUNT_COMPLETED_STATUS_ERROR_PATH_ALREADY_MOUNTED;
    case chromeos::MOUNT_ERROR_PATH_NOT_MOUNTED:
      return file_manager_private::
          MOUNT_COMPLETED_STATUS_ERROR_PATH_NOT_MOUNTED;
    case chromeos::MOUNT_ERROR_DIRECTORY_CREATION_FAILED:
      return file_manager_private::
          MOUNT_COMPLETED_STATUS_ERROR_DIRECTORY_CREATION_FAILED;
    case chromeos::MOUNT_ERROR_INVALID_MOUNT_OPTIONS:
      return file_manager_private::
          MOUNT_COMPLETED_STATUS_ERROR_INVALID_MOUNT_OPTIONS;
    case chromeos::MOUNT_ERROR_INVALID_UNMOUNT_OPTIONS:
      return file_manager_private::
          MOUNT_COMPLETED_STATUS_ERROR_INVALID_UNMOUNT_OPTIONS;
    case chromeos::MOUNT_ERROR_INSUFFICIENT_PERMISSIONS:
      return file_manager_private::
          MOUNT_COMPLETED_STATUS_ERROR_INSUFFICIENT_PERMISSIONS;
    case chromeos::MOUNT_ERROR_MOUNT_PROGRAM_NOT_FOUND:
      return file_manager_private::
          MOUNT_COMPLETED_STATUS_ERROR_MOUNT_PROGRAM_NOT_FOUND;
    case chromeos::MOUNT_ERROR_MOUNT_PROGRAM_FAILED:
      return file_manager_private::
          MOUNT_COMPLETED_STATUS_ERROR_MOUNT_PROGRAM_FAILED;
    case chromeos::MOUNT_ERROR_INVALID_DEVICE_PATH:
      return file_manager_private::
          MOUNT_COMPLETED_STATUS_ERROR_INVALID_DEVICE_PATH;
    case chromeos::MOUNT_ERROR_UNKNOWN_FILESYSTEM:
      return file_manager_private::
          MOUNT_COMPLETED_STATUS_ERROR_UNKNOWN_FILESYSTEM;
    case chromeos::MOUNT_ERROR_UNSUPPORTED_FILESYSTEM:
      return file_manager_private::
          MOUNT_COMPLETED_STATUS_ERROR_UNSUPPORTED_FILESYSTEM;
    case chromeos::MOUNT_ERROR_INVALID_ARCHIVE:
      return file_manager_private::MOUNT_COMPLETED_STATUS_ERROR_INVALID_ARCHIVE;
    case chromeos::MOUNT_ERROR_NEED_PASSWORD:
      return file_manager_private::MOUNT_COMPLETED_STATUS_ERROR_NEED_PASSWORD;
    case chromeos::MOUNT_ERROR_IN_PROGRESS:
      return file_manager_private::MOUNT_COMPLETED_STATUS_ERROR_IN_PROGRESS;
    case chromeos::MOUNT_ERROR_CANCELLED:
      return file_manager_private::MOUNT_COMPLETED_STATUS_ERROR_CANCELLED;
    // Not a real error.
    case chromeos::MOUNT_ERROR_COUNT:
      NOTREACHED();
  }
  NOTREACHED();
  return file_manager_private::MOUNT_COMPLETED_STATUS_NONE;
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
  if (tablet_mode)
    tablet_mode->RemoveObserver(this);

  auto* intent_helper =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  if (intent_helper)
    intent_helper->RemoveObserver(this);

  ash::system::TimezoneSettings::GetInstance()->RemoveObserver(this);

  DLOG_IF(WARNING, !file_watchers_.empty())
      << "Not all file watchers are "
      << "removed. This can happen when the Files app is open during shutdown.";
  file_watchers_.clear();
  DCHECK(profile_);

  pref_change_registrar_->RemoveAll();

  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);

  extensions::ExtensionRegistry::Get(profile_)->RemoveObserver(this);

  DriveIntegrationService* const integration_service =
      DriveIntegrationServiceFactory::FindForProfile(profile_);
  if (integration_service) {
    integration_service->RemoveObserver(this);
    integration_service->GetDriveFsHost()->RemoveObserver(
        drivefs_event_router_.get());
    integration_service->GetDriveFsHost()->set_dialog_handler({});
  }

  VolumeManager* const volume_manager = VolumeManager::Get(profile_);
  if (volume_manager) {
    volume_manager->RemoveObserver(this);
    volume_manager->RemoveObserver(device_event_router_.get());
    auto* io_task_controller = volume_manager->io_task_controller();
    if (io_task_controller)
      io_task_controller->RemoveObserver(this);
  }

  chromeos::PowerManagerClient* const power_manager_client =
      chromeos::PowerManagerClient::Get();
  power_manager_client->RemoveObserver(device_event_router_.get());

  if (base::FeatureList::IsEnabled(chromeos::features::kGuestOsFiles)) {
    auto* registry = guest_os::GuestOsService::GetForProfile(profile_)
                         ->MountProviderRegistry();
    registry->RemoveObserver(this);
  }

  profile_ = nullptr;
}

void EventRouter::ObserveEvents() {
  DCHECK(profile_);

  if (!chromeos::LoginState::IsInitialized() ||
      !chromeos::LoginState::Get()->IsUserLoggedIn()) {
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
    integration_service->GetDriveFsHost()->AddObserver(
        drivefs_event_router_.get());
    integration_service->GetDriveFsHost()->set_dialog_handler(
        base::BindRepeating(&EventRouter::DisplayDriveConfirmDialog,
                            weak_factory_.GetWeakPtr()));
  }

  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);

  extensions::ExtensionRegistry::Get(profile_)->AddObserver(this);

  pref_change_registrar_->Init(profile_->GetPrefs());
  auto callback = base::BindRepeating(&EventRouter::OnFileManagerPrefsChanged,
                                      weak_factory_.GetWeakPtr());
  pref_change_registrar_->Add(drive::prefs::kDisableDriveOverCellular,
                              callback);
  pref_change_registrar_->Add(drive::prefs::kDisableDrive, callback);
  pref_change_registrar_->Add(prefs::kSearchSuggestEnabled, callback);
  pref_change_registrar_->Add(prefs::kUse24HourClock, callback);
  pref_change_registrar_->Add(
      crostini::prefs::kCrostiniEnabled,
      base::BindRepeating(
          &EventRouter::OnCrostiniChanged, weak_factory_.GetWeakPtr(),
          crostini::kCrostiniDefaultVmName, crostini::prefs::kCrostiniEnabled,
          file_manager_private::CROSTINI_EVENT_TYPE_ENABLE,
          file_manager_private::CROSTINI_EVENT_TYPE_DISABLE));
  pref_change_registrar_->Add(arc::prefs::kArcEnabled, callback);
  pref_change_registrar_->Add(arc::prefs::kArcHasAccessToRemovableMedia,
                              callback);
  pref_change_registrar_->Add(ash::prefs::kFilesAppFolderShortcuts, callback);

  auto plugin_vm_callback = base::BindRepeating(&EventRouter::OnPluginVmChanged,
                                                weak_factory_.GetWeakPtr());
  plugin_vm_subscription_ =
      std::make_unique<plugin_vm::PluginVmPolicySubscription>(
          profile_, base::BindRepeating([](base::RepeatingClosure closure,
                                           bool is_allowed) { closure.Run(); },
                                        plugin_vm_callback));
  pref_change_registrar_->Add(plugin_vm::prefs::kPluginVmImageExists,
                              plugin_vm_callback);

  ash::system::TimezoneSettings::GetInstance()->AddObserver(this);

  auto* intent_helper =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  if (intent_helper)
    intent_helper->AddObserver(this);

  auto* guest_os_share_path =
      guest_os::GuestOsSharePath::GetForProfile(profile_);
  if (guest_os_share_path)
    guest_os_share_path->AddObserver(this);

  ash::TabletMode* tablet_mode = ash::TabletMode::Get();
  if (tablet_mode)
    tablet_mode->AddObserver(this);

  if (base::FeatureList::IsEnabled(chromeos::features::kGuestOsFiles)) {
    auto* registry = guest_os::GuestOsService::GetForProfile(profile_)
                         ->MountProviderRegistry();
    registry->AddObserver(this);
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
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
  }
}

void EventRouter::RemoveFileWatch(const base::FilePath& local_path,
                                  const url::Origin& listener_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto iter = file_watchers_.find(local_path);
  if (iter == file_watchers_.end())
    return;
  // Remove the watcher if |local_path| is no longer watched by any extensions.
  iter->second->RemoveListener(listener_origin);
  if (iter->second->GetListeners().empty())
    file_watchers_.erase(iter);
}

void EventRouter::OnCopyStarted(int copy_id,
                                const GURL& source_url,
                                const GURL& destination_url,
                                int64_t space_needed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  file_manager_private::CopyOrMoveProgressStatus status;
  // Send started event.
  status.type = file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_BEGIN;
  status.source_url = std::make_unique<std::string>(source_url.spec());
  status.destination_url =
      std::make_unique<std::string>(destination_url.spec());
  // Use the bytes copied member to store space needed for this event.
  status.size = std::make_unique<double>(space_needed);

  notification_manager_->HandleCopyStart(copy_id, status);
}

void EventRouter::OnCopyCompleted(int copy_id,
                                  const GURL& source_url,
                                  const GURL& destination_url,
                                  base::File::Error error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  file_manager_private::CopyOrMoveProgressStatus status;
  if (error == base::File::FILE_OK) {
    // Send success event.
    status.type =
        file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_SUCCESS;
    status.source_url = std::make_unique<std::string>(source_url.spec());
    status.destination_url =
        std::make_unique<std::string>(destination_url.spec());
  } else {
    // Send error event.
    status.type = file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_ERROR;
    status.error = std::make_unique<std::string>(FileErrorToErrorName(error));
  }

  notification_manager_->HandleCopyEvent(copy_id, status);
  BroadcastEvent(profile_,
                 extensions::events::FILE_MANAGER_PRIVATE_ON_COPY_PROGRESS,
                 file_manager_private::OnCopyProgress::kEventName,
                 file_manager_private::OnCopyProgress::Create(copy_id, status));
}

void EventRouter::OnCopyProgress(
    int copy_id,
    FileManagerCopyOrMoveHookDelegate::ProgressType type,
    const GURL& source_url,
    const GURL& destination_url,
    int64_t size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  file_manager_private::CopyOrMoveProgressStatus status;
  status.type = CopyOrMoveProgressTypeToCopyOrMoveProgressStatusType(type);
  status.source_url = std::make_unique<std::string>(source_url.spec());
  if (type == FileManagerCopyOrMoveHookDelegate::ProgressType::kError) {
    // For cross-filesystems moves, no destination_url is provided when an error
    // occurs. This translates into to a non-valid destination GURL.
    // status.destination_url should never be used in this case.
    status.destination_url =
        std::make_unique<std::string>(destination_url.possibly_invalid_spec());
  } else if (type != FileManagerCopyOrMoveHookDelegate::ProgressType::
                         kEndRemoveSource) {
    status.destination_url =
        std::make_unique<std::string>(destination_url.spec());
  }

  if (type == FileManagerCopyOrMoveHookDelegate::ProgressType::kError)
    status.error = std::make_unique<std::string>(
        FileErrorToErrorName(base::File::FILE_ERROR_FAILED));
  if (type == FileManagerCopyOrMoveHookDelegate::ProgressType::kProgress)
    status.size = std::make_unique<double>(size);

  // Discard error progress since current JS code cannot handle this properly.
  // TODO(yawano): Remove this after JS side is implemented correctly.
  if (type == FileManagerCopyOrMoveHookDelegate::ProgressType::kError)
    return;

  // Should not skip events other than TYPE_PROGRESS.
  const bool always =
      status.type !=
      file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_PROGRESS;
  if (!ShouldSendProgressEvent(always, &last_copy_progress_event_))
    return;

  notification_manager_->HandleCopyEvent(copy_id, status);
  BroadcastEvent(profile_,
                 extensions::events::FILE_MANAGER_PRIVATE_ON_COPY_PROGRESS,
                 file_manager_private::OnCopyProgress::kEventName,
                 file_manager_private::OnCopyProgress::Create(copy_id, status));
}

void EventRouter::OnWatcherManagerNotification(
    const storage::FileSystemURL& file_system_url,
    const url::Origin& listener_origin,
    storage::WatcherManager::ChangeType /* change_type */) {
  std::vector<url::Origin> listeners = {listener_origin};

  DispatchDirectoryChangeEvent(file_system_url.virtual_path(),
                               false /* error */, listeners);
}

void EventRouter::OnConnectionChanged(network::mojom::ConnectionType type) {
  NotifyDriveConnectionStatusChanged();
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

  event.entry.additional_properties.SetStringKey(
      "fileSystemName", entry_definition.file_system_name);
  event.entry.additional_properties.SetStringKey(
      "fileSystemRoot", entry_definition.file_system_root_url);
  event.entry.additional_properties.SetStringKey(
      "fileFullPath", "/" + entry_definition.full_path.value());
  event.entry.additional_properties.SetBoolKey("fileIsDirectory",
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

void EventRouter::OnVolumeMounted(chromeos::MountError error_code,
                                  const Volume& volume) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // profile_ is NULL if ShutdownOnUIThread() is called earlier. This can
  // happen at shutdown. This should be removed after removing Drive mounting
  // code in addMount. (addMount -> OnFileSystemMounted -> OnVolumeMounted is
  // the only path to come here after Shutdown is called).
  if (!profile_)
    return;

  DispatchMountCompletedEvent(
      file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT, error_code,
      volume);

  // If the Files SWA is enabled, record the UMA metrics for mounted FSPs.
  if (ash::features::IsFileManagerSwaEnabled()) {
    RecordFileSystemProviderMountMetrics(volume);
  }

  // TODO(mtomasz): Move VolumeManager and part of the event router outside of
  // file_manager, so there is no dependency between File System API and the
  // file_manager code.
  extensions::file_system_api::DispatchVolumeListChangeEvent(profile_);
}

void EventRouter::OnVolumeUnmounted(chromeos::MountError error_code,
                                    const Volume& volume) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DispatchMountCompletedEvent(
      file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_UNMOUNT, error_code,
      volume);
}

void EventRouter::DispatchMountCompletedEvent(
    file_manager_private::MountCompletedEventType event_type,
    chromeos::MountError error,
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

// Send crostini share, unshare event.
void EventRouter::SendCrostiniEvent(
    file_manager_private::CrostiniEventType event_type,
    const std::string& vm_name,
    const base::FilePath& path) {
  std::string mount_name;
  std::string file_system_name;
  std::string full_path;
  if (!util::ExtractMountNameFileSystemNameFullPath(
          path, &mount_name, &file_system_name, &full_path))
    return;

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
  file_manager_private::CrostiniEvent::EntriesType entry;
  entry.additional_properties.SetStringKey(
      "fileSystemRoot",
      storage::GetExternalFileSystemRootURIString(origin.GetURL(), mount_name));
  entry.additional_properties.SetStringKey("fileSystemName", file_system_name);
  entry.additional_properties.SetStringKey("fileFullPath", full_path);
  entry.additional_properties.SetBoolKey("fileIsDirectory", true);
  event.entries.emplace_back(std::move(entry));
}

void EventRouter::OnShare(const std::string& vm_name,
                          const base::FilePath& path,
                          bool persist) {
  if (persist) {
    SendCrostiniEvent(file_manager_private::CROSTINI_EVENT_TYPE_SHARE, vm_name,
                      path);
  }
}

void EventRouter::OnUnshare(const std::string& vm_name,
                            const base::FilePath& path) {
  SendCrostiniEvent(file_manager_private::CROSTINI_EVENT_TYPE_UNSHARE, vm_name,
                    path);
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

void EventRouter::OnCrostiniChanged(
    const std::string& vm_name,
    const std::string& pref_name,
    extensions::api::file_manager_private::CrostiniEventType pref_true,
    extensions::api::file_manager_private::CrostiniEventType pref_false) {
  file_manager_private::CrostiniEvent event;
  event.vm_name = vm_name;
  event.event_type =
      profile_->GetPrefs()->GetBoolean(pref_name) ? pref_true : pref_false;
  BroadcastEvent(profile_,
                 extensions::events::FILE_MANAGER_PRIVATE_ON_CROSTINI_CHANGED,
                 file_manager_private::OnCrostiniChanged::kEventName,
                 file_manager_private::OnCrostiniChanged::Create(event));
}

void EventRouter::OnPluginVmChanged() {
  file_manager_private::CrostiniEvent event;
  event.vm_name = plugin_vm::kPluginVmName;
  event.event_type = plugin_vm::PluginVmFeatures::Get()->IsEnabled(profile_)
                         ? file_manager_private::CROSTINI_EVENT_TYPE_ENABLE
                         : file_manager_private::CROSTINI_EVENT_TYPE_DISABLE;
  BroadcastEvent(profile_,
                 extensions::events::FILE_MANAGER_PRIVATE_ON_CROSTINI_CHANGED,
                 file_manager_private::OnCrostiniChanged::kEventName,
                 file_manager_private::OnCrostiniChanged::Create(event));
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

base::WeakPtr<EventRouter> EventRouter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void EventRouter::OnIOTaskStatus(const io_task::ProgressStatus& status) {
  // If any Files app window exists we send the progress to all of them.
  if (DoFilesSwaWindowsExist(profile_)) {
    file_manager_private::ProgressStatus event_status;
    event_status.task_id = status.task_id;
    event_status.type = GetIOTaskType(status.type);
    event_status.state = GetIOTaskState(status.state);

    // Speedometer can produce infinite result which can't be serialized to JSON
    // when sending the status via private API.
    if (std::isfinite(status.remaining_seconds)) {
      event_status.remaining_seconds = status.remaining_seconds;
    }

    if (status.destination_folder.is_valid()) {
      event_status.destination_name =
          util::GetDisplayablePath(profile_, status.destination_folder)
              .value_or(base::FilePath())
              .BaseName()
              .value();
    }

    size_t processed = 0;
    for (const auto& file_status : status.outputs) {
      if (file_status.error)
        processed++;
    }
    event_status.num_remaining_items = status.sources.size() - processed;
    event_status.item_count = status.sources.size();

    // Get the last error occurrence in the `sources`.
    for (const io_task::EntryStatus& source : base::Reversed(status.sources)) {
      if (source.error && source.error.value() != base::File::FILE_OK) {
        event_status.error_name = FileErrorToErrorName(source.error.value());
      }
    }

    if (status.sources.size() > 0) {
      event_status.source_name =
          util::GetDisplayablePath(profile_, status.sources.front().url)
              .value_or(base::FilePath())
              .BaseName()
              .value();
    }
    event_status.bytes_transferred = status.bytes_transferred;
    event_status.total_bytes = status.total_bytes;

    BroadcastEvent(
        profile_,
        extensions::events::FILE_MANAGER_PRIVATE_ON_IO_TASK_PROGRESS_STATUS,
        file_manager_private::OnIOTaskProgressStatus::kEventName,
        file_manager_private::OnIOTaskProgressStatus::Create(event_status));

    // Send file watch notifications on I/O task completion. inotify is flaky on
    // some filesystems, so send these notifications so that at least operations
    // made from Files App are always reflected in the UI.
    if (status.IsCompleted()) {
      std::set<base::FilePath> updated_paths;
      if (status.destination_folder.is_valid()) {
        updated_paths.insert(status.destination_folder.path());
      }
      for (const auto& source : status.sources) {
        updated_paths.insert(source.url.path().DirName());
      }
      for (const auto& output : status.outputs) {
        updated_paths.insert(output.url.path().DirName());
      }

      for (const auto& path : updated_paths) {
        HandleFileWatchNotification(path, false);
      }
    }

    // Send the progress report to the system notification regardless of whether
    // Files app window exists as we may need to remove an existing
    // notification.
  }

  notification_manager_->HandleIOTaskProgress(status);
}
void EventRouter::OnRegistered(guest_os::GuestOsMountProviderRegistry::Id id,
                               guest_os::GuestOsMountProvider* provider) {
  OnMountableGuestsChanged();
}

void EventRouter::OnUnregistered(
    guest_os::GuestOsMountProviderRegistry::Id id) {
  OnMountableGuestsChanged();
}

void EventRouter::OnMountableGuestsChanged() {
  auto* registry = guest_os::GuestOsService::GetForProfile(profile_)
                       ->MountProviderRegistry();
  std::vector<file_manager_private::MountableGuest> guests;
  for (const auto id : registry->List()) {
    file_manager_private::MountableGuest guest;
    auto* provider = registry->Get(id);
    guest.id = id;
    guest.display_name = provider->DisplayName();
    guests.push_back(std::move(guest));
  }
  BroadcastEvent(
      profile_,
      extensions::events::FILE_MANAGER_PRIVATE_ON_IO_TASK_PROGRESS_STATUS,
      file_manager_private::OnMountableGuestsChanged::kEventName,
      file_manager_private::OnMountableGuestsChanged::Create(guests));
}

}  // namespace file_manager
