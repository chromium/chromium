// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/event_router.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/open_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/extensions/api/file_system/chrome_file_system_delegate.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/components/drivefs/drivefs_host.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/disks/disk.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/arc/arc_prefs.h"
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
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"

using chromeos::disks::Disk;
using chromeos::disks::DiskMountManager;
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

  for (size_t i = 0; i < base::size(kRecoveryToolIds); ++i) {
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
                    std::unique_ptr<base::ListValue> event_args) {
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
    std::unique_ptr<base::ListValue> event_args) {
  extensions::EventRouter::Get(profile)->DispatchEventToExtension(
      extension_id, std::make_unique<extensions::Event>(
                        histogram_value, event_name, std::move(event_args)));
}

file_manager_private::MountCompletedStatus
MountErrorToMountCompletedStatus(chromeos::MountError error) {
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
      return file_manager_private
          ::MOUNT_COMPLETED_STATUS_ERROR_DIRECTORY_CREATION_FAILED;
    case chromeos::MOUNT_ERROR_INVALID_MOUNT_OPTIONS:
      return file_manager_private
          ::MOUNT_COMPLETED_STATUS_ERROR_INVALID_MOUNT_OPTIONS;
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
    // Not a real error.
    case chromeos::MOUNT_ERROR_COUNT:
      NOTREACHED();
  }
  NOTREACHED();
  return file_manager_private::MOUNT_COMPLETED_STATUS_NONE;
}

file_manager_private::CopyProgressStatusType
CopyProgressTypeToCopyProgressStatusType(
    storage::FileSystemOperation::CopyProgressType type) {
  switch (type) {
    case storage::FileSystemOperation::BEGIN_COPY_ENTRY:
      return file_manager_private::COPY_PROGRESS_STATUS_TYPE_BEGIN_COPY_ENTRY;
    case storage::FileSystemOperation::END_COPY_ENTRY:
      return file_manager_private::COPY_PROGRESS_STATUS_TYPE_END_COPY_ENTRY;
    case storage::FileSystemOperation::PROGRESS:
      return file_manager_private::COPY_PROGRESS_STATUS_TYPE_PROGRESS;
    case storage::FileSystemOperation::ERROR_COPY_ENTRY:
      return file_manager_private::COPY_PROGRESS_STATUS_TYPE_ERROR;
  }
  NOTREACHED();
  return file_manager_private::COPY_PROGRESS_STATUS_TYPE_NONE;
}

std::string FileErrorToErrorName(base::File::Error error_code) {
  namespace js = extensions::api::file_manager_private;
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
  if (chromeos::LoginDisplayHost::default_host() ||
      chromeos::ScreenLocker::default_screen_locker() ||
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

std::set<std::string> GetEventListenerExtensionIds(
    Profile* profile,
    const std::string& event_name) {
  const extensions::EventListenerMap::ListenerList& listeners =
      extensions::EventRouter::Get(profile)
          ->listeners()
          .GetEventListenersByName(event_name);
  std::set<std::string> extension_ids;
  for (const auto& listener : listeners) {
    extension_ids.insert(listener->extension_id());
  }
  return extension_ids;
}

// Sub-part of the event router for handling device events.
class DeviceEventRouterImpl : public DeviceEventRouter {
 public:
  explicit DeviceEventRouterImpl(Profile* profile) : profile_(profile) {}

  // DeviceEventRouter overrides.
  void OnDeviceEvent(file_manager_private::DeviceEventType type,
                     const std::string& device_path) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    file_manager_private::DeviceEvent event;
    event.type = type;
    event.device_path = device_path;

    BroadcastEvent(profile_,
                   extensions::events::FILE_MANAGER_PRIVATE_ON_DEVICE_CHANGED,
                   file_manager_private::OnDeviceChanged::kEventName,
                   file_manager_private::OnDeviceChanged::Create(event));
  }

  // DeviceEventRouter overrides.
  bool IsExternalStorageDisabled() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return profile_->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled);
  }

 private:
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(DeviceEventRouterImpl);
};

class DriveFsEventRouterImpl : public DriveFsEventRouter {
 public:
  DriveFsEventRouterImpl(
      Profile* profile,
      const std::map<base::FilePath, std::unique_ptr<FileWatcher>>*
          file_watchers)
      : profile_(profile), file_watchers_(file_watchers) {}

 private:
  std::set<std::string> GetEventListenerExtensionIds(
      const std::string& event_name) override {
    return ::file_manager::GetEventListenerExtensionIds(profile_, event_name);
  }

  GURL ConvertDrivePathToFileSystemUrl(
      const base::FilePath& file_path,
      const std::string& extension_id) override {
    GURL url;
    file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
        profile_,
        base::FilePath(DriveIntegrationServiceFactory::FindForProfile(profile_)
                           ->GetMountPointPath()
                           .value() +
                       file_path.value()),
        extension_id, &url);
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

  void DispatchEventToExtension(
      const std::string& extension_id,
      extensions::events::HistogramValue histogram_value,
      const std::string& event_name,
      std::unique_ptr<base::ListValue> event_args) override {
    extensions::EventRouter::Get(profile_)->DispatchEventToExtension(
        extension_id, std::make_unique<extensions::Event>(
                          histogram_value, event_name, std::move(event_args)));
  }

  Profile* const profile_;
  const std::map<base::FilePath, std::unique_ptr<FileWatcher>>* const
      file_watchers_;

  DISALLOW_COPY_AND_ASSIGN(DriveFsEventRouterImpl);
};

}  // namespace

EventRouter::EventRouter(Profile* profile)
    : pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()),
      profile_(profile),
      device_event_router_(std::make_unique<DeviceEventRouterImpl>(profile)),
      drivefs_event_router_(
          std::make_unique<DriveFsEventRouterImpl>(profile, &file_watchers_)),
      dispatch_directory_change_event_impl_(
          base::Bind(&EventRouter::DispatchDirectoryChangeEventImpl,
                     base::Unretained(this))) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ObserveEvents();
}

EventRouter::~EventRouter() = default;

void EventRouter::OnIntentFiltersUpdated(
    const base::Optional<std::string>& package_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BroadcastEvent(profile_,
                 extensions::events::FILE_MANAGER_PRIVATE_ON_APPS_UPDATED,
                 file_manager_private::OnAppsUpdated::kEventName,
                 file_manager_private::OnAppsUpdated::Create());
}

void EventRouter::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* intent_helper =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  if (intent_helper)
    intent_helper->RemoveObserver(this);

  chromeos::system::TimezoneSettings::GetInstance()->RemoveObserver(this);

  DLOG_IF(WARNING, !file_watchers_.empty())
      << "Not all file watchers are "
      << "removed. This can happen when the Files app is open during shutdown.";
  file_watchers_.clear();
  DCHECK(profile_);

  pref_change_registrar_->RemoveAll();

  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);

  DriveIntegrationService* const integration_service =
      DriveIntegrationServiceFactory::FindForProfile(profile_);
  if (integration_service) {
    integration_service->RemoveObserver(this);
    integration_service->GetDriveFsHost()->RemoveObserver(
        drivefs_event_router_.get());
  }

  VolumeManager* const volume_manager = VolumeManager::Get(profile_);
  if (volume_manager) {
    volume_manager->RemoveObserver(this);
    volume_manager->RemoveObserver(device_event_router_.get());
  }

  chromeos::PowerManagerClient* const power_manager_client =
      chromeos::PowerManagerClient::Get();
  power_manager_client->RemoveObserver(device_event_router_.get());

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
  }

  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);

  pref_change_registrar_->Init(profile_->GetPrefs());
  base::Closure callback =
      base::Bind(&EventRouter::OnFileManagerPrefsChanged,
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

  chromeos::system::TimezoneSettings::GetInstance()->AddObserver(this);

  auto* intent_helper =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  if (intent_helper)
    intent_helper->AddObserver(this);

  auto* guest_os_share_path =
      guest_os::GuestOsSharePath::GetForProfile(profile_);
  if (guest_os_share_path)
    guest_os_share_path->AddObserver(this);
}

// File watch setup routines.
void EventRouter::AddFileWatch(const base::FilePath& local_path,
                               const base::FilePath& virtual_path,
                               const std::string& extension_id,
                               BoolCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  auto iter = file_watchers_.find(local_path);
  if (iter == file_watchers_.end()) {
    std::unique_ptr<FileWatcher> watcher(new FileWatcher(virtual_path));
    watcher->AddExtension(extension_id);
    watcher->WatchLocalFile(
        local_path,
        base::Bind(&EventRouter::HandleFileWatchNotification,
                   weak_factory_.GetWeakPtr()),
        std::move(callback));

    file_watchers_[local_path] = std::move(watcher);
  } else {
    iter->second->AddExtension(extension_id);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
  }
}

void EventRouter::RemoveFileWatch(const base::FilePath& local_path,
                                  const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto iter = file_watchers_.find(local_path);
  if (iter == file_watchers_.end())
    return;
  // Remove the watcher if |local_path| is no longer watched by any extensions.
  iter->second->RemoveExtension(extension_id);
  if (iter->second->GetExtensionIds().empty())
    file_watchers_.erase(iter);
}

void EventRouter::OnCopyCompleted(int copy_id,
                                  const GURL& source_url,
                                  const GURL& destination_url,
                                  base::File::Error error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  file_manager_private::CopyProgressStatus status;
  if (error == base::File::FILE_OK) {
    // Send success event.
    status.type = file_manager_private::COPY_PROGRESS_STATUS_TYPE_SUCCESS;
    status.source_url = std::make_unique<std::string>(source_url.spec());
    status.destination_url =
        std::make_unique<std::string>(destination_url.spec());
  } else {
    // Send error event.
    status.type = file_manager_private::COPY_PROGRESS_STATUS_TYPE_ERROR;
    status.error = std::make_unique<std::string>(FileErrorToErrorName(error));
  }

  BroadcastEvent(profile_,
                 extensions::events::FILE_MANAGER_PRIVATE_ON_COPY_PROGRESS,
                 file_manager_private::OnCopyProgress::kEventName,
                 file_manager_private::OnCopyProgress::Create(copy_id, status));
}

void EventRouter::OnCopyProgress(
    int copy_id,
    storage::FileSystemOperation::CopyProgressType type,
    const GURL& source_url,
    const GURL& destination_url,
    int64_t size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  file_manager_private::CopyProgressStatus status;
  status.type = CopyProgressTypeToCopyProgressStatusType(type);
  status.source_url = std::make_unique<std::string>(source_url.spec());
  if (type == storage::FileSystemOperation::END_COPY_ENTRY ||
      type == storage::FileSystemOperation::ERROR_COPY_ENTRY)
    status.destination_url =
        std::make_unique<std::string>(destination_url.spec());
  if (type == storage::FileSystemOperation::ERROR_COPY_ENTRY)
    status.error = std::make_unique<std::string>(
        FileErrorToErrorName(base::File::FILE_ERROR_FAILED));
  if (type == storage::FileSystemOperation::PROGRESS)
    status.size = std::make_unique<double>(size);

  // Discard error progress since current JS code cannot handle this properly.
  // TODO(yawano): Remove this after JS side is implemented correctly.
  if (type == storage::FileSystemOperation::ERROR_COPY_ENTRY)
    return;

  // Should not skip events other than TYPE_PROGRESS.
  const bool always =
      status.type != file_manager_private::COPY_PROGRESS_STATUS_TYPE_PROGRESS;
  if (!ShouldSendProgressEvent(always, &last_copy_progress_event_))
    return;

  BroadcastEvent(profile_,
                 extensions::events::FILE_MANAGER_PRIVATE_ON_COPY_PROGRESS,
                 file_manager_private::OnCopyProgress::kEventName,
                 file_manager_private::OnCopyProgress::Create(copy_id, status));
}

void EventRouter::OnWatcherManagerNotification(
    const storage::FileSystemURL& file_system_url,
    const std::string& extension_id,
    storage::WatcherManager::ChangeType /* change_type */) {
  std::vector<std::string> extension_ids;
  extension_ids.push_back(extension_id);

  DispatchDirectoryChangeEvent(file_system_url.virtual_path(),
                               false /* error */, extension_ids);
}

void EventRouter::OnConnectionChanged(network::mojom::ConnectionType type) {
  DCHECK(profile_);
  DCHECK(extensions::EventRouter::Get(profile_));

  BroadcastEvent(
      profile_, extensions::events::
                    FILE_MANAGER_PRIVATE_ON_DRIVE_CONNECTION_STATUS_CHANGED,
      file_manager_private::OnDriveConnectionStatusChanged::kEventName,
      file_manager_private::OnDriveConnectionStatusChanged::Create());
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

  DispatchDirectoryChangeEvent(iter->second->virtual_path(),
                               got_error,
                               iter->second->GetExtensionIds());
}

void EventRouter::DispatchDirectoryChangeEvent(
    const base::FilePath& virtual_path,
    bool got_error,
    const std::vector<std::string>& extension_ids) {
  dispatch_directory_change_event_impl_.Run(virtual_path, got_error,
                                            extension_ids);
}

void EventRouter::DispatchDirectoryChangeEventImpl(
    const base::FilePath& virtual_path,
    bool got_error,
    const std::vector<std::string>& extension_ids) {
  DCHECK(profile_);

  for (size_t i = 0; i < extension_ids.size(); ++i) {
    std::string* extension_id = new std::string(extension_ids[i]);

    FileDefinition file_definition;
    file_definition.virtual_path = virtual_path;
    // TODO(mtomasz): Add support for watching files in File System Provider
    // API.
    file_definition.is_directory = true;

    file_manager::util::ConvertFileDefinitionToEntryDefinition(
        profile_, *extension_id, file_definition,
        base::BindOnce(
            &EventRouter::DispatchDirectoryChangeEventWithEntryDefinition,
            weak_factory_.GetWeakPtr(), base::Owned(extension_id), got_error));
  }
}

void EventRouter::DispatchDirectoryChangeEventWithEntryDefinition(
    const std::string* extension_id,
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

  event.entry.additional_properties.SetString(
      "fileSystemName", entry_definition.file_system_name);
  event.entry.additional_properties.SetString(
      "fileSystemRoot", entry_definition.file_system_root_url);
  event.entry.additional_properties.SetString(
      "fileFullPath", "/" + entry_definition.full_path.value());
  event.entry.additional_properties.SetBoolean("fileIsDirectory",
                                               entry_definition.is_directory);

  DispatchEventToExtension(
      profile_, *extension_id,
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
  BroadcastEvent(profile_,
                 extensions::events::FILE_MANAGER_PRIVATE_ON_MOUNT_COMPLETED,
                 file_manager_private::OnMountCompleted::kEventName,
                 file_manager_private::OnMountCompleted::Create(event));
}

void EventRouter::OnFormatStarted(const std::string& device_path,
                                  bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Do nothing.
}

void EventRouter::OnFormatCompleted(const std::string& device_path,
                                    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Do nothing.
}

void EventRouter::OnRenameStarted(const std::string& device_path,
                                  bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Do nothing.
}

void EventRouter::OnRenameCompleted(const std::string& device_path,
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

void EventRouter::PopulateCrostiniUnshareEvent(
    file_manager_private::CrostiniEvent& event,
    const std::string& vm_name,
    const std::string& extension_id,
    const std::string& mount_name,
    const std::string& file_system_name,
    const std::string& full_path) {
  event.event_type = file_manager_private::CROSTINI_EVENT_TYPE_UNSHARE;
  event.vm_name = vm_name;
  file_manager_private::CrostiniEvent::EntriesType entry;
  entry.additional_properties.SetString(
      "fileSystemRoot",
      storage::GetExternalFileSystemRootURIString(
          extensions::Extension::GetBaseURLFromExtensionId(extension_id),
          mount_name));
  entry.additional_properties.SetString("fileSystemName", file_system_name);
  entry.additional_properties.SetString("fileFullPath", full_path);
  entry.additional_properties.SetBoolean("fileIsDirectory", true);
  event.entries.emplace_back(std::move(entry));
}

void EventRouter::OnUnshare(const std::string& vm_name,
                            const base::FilePath& path) {
  std::string mount_name;
  std::string file_system_name;
  std::string full_path;
  if (!util::ExtractMountNameFileSystemNameFullPath(
          path, &mount_name, &file_system_name, &full_path))
    return;

  for (const auto& extension_id : GetEventListenerExtensionIds(
           profile_, file_manager_private::OnCrostiniChanged::kEventName)) {
    file_manager_private::CrostiniEvent event;
    PopulateCrostiniUnshareEvent(event, vm_name, extension_id, mount_name,
                                 file_system_name, full_path);
    DispatchEventToExtension(
        profile_, extension_id,
        extensions::events::FILE_MANAGER_PRIVATE_ON_CROSTINI_CHANGED,
        file_manager_private::OnCrostiniChanged::kEventName,
        file_manager_private::OnCrostiniChanged::Create(event));
  }
}

void EventRouter::OnCrostiniChanged(
    const std::string& vm_name,
    const std::string& pref_name,
    extensions::api::file_manager_private::CrostiniEventType pref_true,
    extensions::api::file_manager_private::CrostiniEventType pref_false) {
  for (const auto& extension_id : GetEventListenerExtensionIds(
           profile_, file_manager_private::OnCrostiniChanged::kEventName)) {
    file_manager_private::CrostiniEvent event;
    event.vm_name = vm_name;
    event.event_type =
        profile_->GetPrefs()->GetBoolean(pref_name) ? pref_true : pref_false;
    DispatchEventToExtension(
        profile_, extension_id,
        extensions::events::FILE_MANAGER_PRIVATE_ON_CROSTINI_CHANGED,
        file_manager_private::OnCrostiniChanged::kEventName,
        file_manager_private::OnCrostiniChanged::Create(event));
  }
}

base::WeakPtr<EventRouter> EventRouter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace file_manager
