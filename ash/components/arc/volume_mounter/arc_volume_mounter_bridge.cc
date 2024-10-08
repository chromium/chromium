// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/volume_mounter/arc_volume_mounter_bridge.h"

#include <string>
#include <vector>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "chromeos/ash/components/disks/disk.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/components/disks/disks_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace arc {

namespace {

using ::ash::disks::DiskMountManager;

// TODO(crbug.com/929031): Move MyFiles constants to a common place.
constexpr char kMyFilesPath[] = "/home/chronos/user/MyFiles";
// Prefix of the removable media mount paths.
// TODO(crbug.com/1274481): Move ash-wide FileManager constants to a common
// place.
constexpr char kRemovableMediaMountPathPrefix[] = "/media/removable/";
// Dummy UUID for MyFiles volume.
constexpr char kMyFilesUuid[] = "0000000000000000000000000000CAFEF00D2019";
// Dummy UUID for testing.
constexpr char kDummyUuid[] = "00000000000000000000000000000000DEADBEEF";

// The minimum and maximum values of app UID in Android. Defined in Android's
// system/core/libcutils/include/private/android_filesystem_config.h.
constexpr uint32_t kAndroidAppUidStart = 10000;
constexpr uint32_t kAndroidAppUidEnd = 19999;

// Singleton factory for ArcVolumeMounterBridge.
class ArcVolumeMounterBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcVolumeMounterBridge,
          ArcVolumeMounterBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcVolumeMounterBridgeFactory";

  static ArcVolumeMounterBridgeFactory* GetInstance() {
    return base::Singleton<ArcVolumeMounterBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcVolumeMounterBridgeFactory>;
  ArcVolumeMounterBridgeFactory() = default;
  ~ArcVolumeMounterBridgeFactory() override = default;
};

std::string GetChromeOsUserId() {
  auto* arc_service_manager = ArcServiceManager::Get();
  DCHECK(arc_service_manager);
  // Return the string representation of AccountId.
  return cryptohome::CreateAccountIdentifierFromAccountId(
             arc_service_manager->account_id())
      .account_id();
}

}  // namespace

// static
ArcVolumeMounterBridge* ArcVolumeMounterBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcVolumeMounterBridgeFactory::GetForBrowserContext(context);
}

// static
KeyedServiceBaseFactory* ArcVolumeMounterBridge::GetFactory() {
  return ArcVolumeMounterBridgeFactory::GetInstance();
}

ArcVolumeMounterBridge::ArcVolumeMounterBridge(content::BrowserContext* context,
                                               ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      pref_service_(user_prefs::UserPrefs::Get(context)) {
  DCHECK(pref_service_);
  arc_bridge_service_->volume_mounter()->AddObserver(this);
  arc_bridge_service_->volume_mounter()->SetHost(this);
  DCHECK(DiskMountManager::GetInstance());
  DiskMountManager::GetInstance()->AddObserver(this);
  DiskMountManager::GetInstance()->RegisterArcDelegate(this);

  change_registerar_.Init(pref_service_);
  // Start monitoring |kArcVisibleExternalStorages| changes. Note that the
  // registerar automatically stops monitoring the pref in its dtor.
  change_registerar_.Add(
      prefs::kArcVisibleExternalStorages,
      base::BindRepeating(&ArcVolumeMounterBridge::OnVisibleStoragesChanged,
                          weak_ptr_factory_.GetWeakPtr()));
}

ArcVolumeMounterBridge::~ArcVolumeMounterBridge() {
  DCHECK(DiskMountManager::GetInstance());
  DiskMountManager::GetInstance()->UnregisterArcDelegate();
  DiskMountManager::GetInstance()->RemoveObserver(this);
  arc_bridge_service_->volume_mounter()->SetHost(nullptr);
  arc_bridge_service_->volume_mounter()->RemoveObserver(this);
}

void ArcVolumeMounterBridge::Initialize(Delegate* delegate) {
  delegate_ = delegate;
  DCHECK(delegate_);
}

// Sends MountEvents of all existing MountPoints in cros-disks.
void ArcVolumeMounterBridge::SendAllMountEvents() {
  if (!IsReadyToSendMountingEvents()) {
    DVLOG(1) << "Skipped SendAllMountEvents because it is not ready to send "
             << "mounting events to Android";
    return;
  }

  SendMountEventForMyFiles();

  for (const auto& mount_point :
       DiskMountManager::GetInstance()->mount_points()) {
    OnMountEvent(DiskMountManager::MountEvent::MOUNTING,
                 ash::MountError::kSuccess, mount_point);
  }
}

// Notifies ARC of MyFiles volume by sending a mount event.
void ArcVolumeMounterBridge::SendMountEventForMyFiles() {
  mojom::VolumeMounterInstance* volume_mounter_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->volume_mounter(),
                                  OnMountEvent);

  if (!volume_mounter_instance) {
    return;
  }

  std::string device_label =
      l10n_util::GetStringUTF8(IDS_FILE_BROWSER_MY_FILES_ROOT_LABEL);

  // TODO(niwa): Add a new DeviceType enum value for MyFiles.
  ash::DeviceType device_type = ash::DeviceType::kSD;

  // Conditionally set MyFiles to be visible for P and invisible for R. In R, we
  // use IsVisibleRead so this is not needed.
  const bool is_p = arc::GetArcAndroidSdkVersionAsInt() == arc::kArcVersionP;
  volume_mounter_instance->OnMountEvent(mojom::MountPointInfo::New(
      DiskMountManager::MOUNTING, kMyFilesPath, kMyFilesPath, kMyFilesUuid,
      device_label, device_type, is_p));
}

bool ArcVolumeMounterBridge::IsVisibleToAndroidApps(
    const std::string& uuid) const {
  const base::Value::List& uuid_list =
      pref_service_->GetList(prefs::kArcVisibleExternalStorages);
  for (auto& value : uuid_list) {
    if (value.is_string() && value.GetString() == uuid) {
      return true;
    }
  }
  return false;
}

void ArcVolumeMounterBridge::OnVisibleStoragesChanged() {
  // Remount all external mount points when the list of visible storage changes.
  for (const auto& mount_point :
       DiskMountManager::GetInstance()->mount_points()) {
    OnMountEvent(DiskMountManager::MountEvent::UNMOUNTING,
                 ash::MountError::kSuccess, mount_point);
  }
  for (const auto& mount_point :
       DiskMountManager::GetInstance()->mount_points()) {
    OnMountEvent(DiskMountManager::MountEvent::MOUNTING,
                 ash::MountError::kSuccess, mount_point);
  }
}

void ArcVolumeMounterBridge::OnMountEvent(
    DiskMountManager::MountEvent event,
    ash::MountError error_code,
    const DiskMountManager::MountPoint& mount_info) {
  DCHECK(delegate_);

  // Skip mount events for volumes that are not shared with ARC (e.g., those
  // mounted on /media/archive) by allowlisting the removable media mount paths.
  if (!base::StartsWith(mount_info.mount_path, kRemovableMediaMountPathPrefix,
                        base::CompareCase::SENSITIVE)) {
    DVLOG(1) << "Ignored mount event for mount path '" << mount_info.mount_path
             << "'";
    return;
  }
  if (error_code != ash::MountError::kSuccess) {
    DVLOG(1) << "Error " << error_code << " occurred during MountEvent "
             << event;
    return;
  }

  // Skip mount events if removable media is forbidden by the policy.
  if (event == DiskMountManager::MountEvent::MOUNTING &&
      pref_service_->GetBoolean(disks::prefs::kExternalStorageDisabled)) {
    DVLOG(1) << "Ignored mount event since policy disallows removable media";
    return;
  }

  // Skip mount events if removable media access is disabled by a feature.
  if (event == DiskMountManager::MountEvent::MOUNTING &&
      !base::FeatureList::IsEnabled(kExternalStorageAccess)) {
    DVLOG(1)
        << "Ignored mount event since removable media is disabled by feature";
    return;
  }

  if (event == DiskMountManager::MountEvent::MOUNTING &&
      !IsReadyToSendMountingEvents()) {
    DVLOG(1) << "Skipped OnMountEvent because it is not ready to send mounting "
                "events to Android";
    return;
  }

  // Get disks information that are needed by Android MountService.
  const ash::disks::Disk* disk =
      DiskMountManager::GetInstance()->FindDiskBySourcePath(
          mount_info.source_path);
  std::string fs_uuid, device_label;
  ash::DeviceType device_type = ash::DeviceType::kUnknown;
  // There are several cases where disk can be null:
  // 1. The disk is removed physically before being ejected/unmounted.
  // 2. The disk is inserted, but then immediately removed physically. The
  //    disk removal will race with mount event in this case.
  if (disk) {
    fs_uuid = disk->fs_uuid();
    device_label = disk->device_label();
    device_type = disk->device_type();
  } else {
    // This is needed by ChromeOS tast test (arc.RemovableMedia) because it
    // creates a diskless volume (hence, no uuid) and Android expects the volume
    // to have a uuid.
    fs_uuid = kDummyUuid;
    DVLOG(1) << "Disk at " << mount_info.source_path
             << " is null during MountEvent " << event;
  }

  if (device_label.empty()) {
    // To make volume labels consistent with Files app, we follow how Files
    // generates a volume label when the volume doesn't have specific label.
    // That is, we use the base name of mount path instead in such cases.
    // TODO: b/255485048 - Share the implementation with Files app and Settings.
    device_label =
        base::FilePath(mount_info.mount_path).BaseName().AsUTF8Unsafe();
  }

  const bool visible = IsVisibleToAndroidApps(fs_uuid);
  switch (event) {
    case DiskMountManager::MountEvent::MOUNTING:
      // Attach watcher to the directories. This is the best place to add the
      // watcher, because if the watcher is attached after Android mounts (and
      // performs full scan) the removable media, there might be a small time
      // interval that has undetectable changes.
      delegate_->StartWatchingRemovableMedia(
          fs_uuid, mount_info.mount_path,
          base::BindOnce(
              &ArcVolumeMounterBridge::SendMountEventForRemovableMedia,
              weak_ptr_factory_.GetWeakPtr(), event, mount_info.source_path,
              mount_info.mount_path, fs_uuid, device_label, device_type,
              visible));
      break;
    case DiskMountManager::MountEvent::UNMOUNTING:

      // The actual ordering for the unmount event is not very important because
      // during unmount, we don't care about accidentally ignoring changes.
      // Hence, no synchronization is needed as we only care about cleaning up
      // memory usage for watchers which is ok to be done at any time as long as
      // it is done.
      SendMountEventForRemovableMedia(event, mount_info.source_path,
                                      mount_info.mount_path, fs_uuid,
                                      device_label, device_type, visible);
      delegate_->StopWatchingRemovableMedia(mount_info.mount_path);
      break;
  }
}

void ArcVolumeMounterBridge::PrepareForRemovableMediaUnmount(
    const base::FilePath& mount_path,
    const base::TimeDelta& timeout,
    DiskMountManager::ArcDelegate::PreparationCallback callback) {
  CHECK(
      ash::CrosDisksClient::GetRemovableDiskMountPoint().IsParent(mount_path));

  if (prepare_removable_media_unmount_callback_) {
    // TODO: crbug.com/317944073 - Support the case where this method is called
    // again before the previous callback hasn't run yet.
    std::move(callback).Run(false);
    return;
  }
  mojom::VolumeMounterInstance* volume_mounter_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->volume_mounter(),
                                  PrepareForRemovableMediaUnmount);
  if (!volume_mounter_instance) {
    std::move(callback).Run(false);
    return;
  }

  // This will run when the mojo method callback runs or the `timeout` has
  // elapsed, whichever that happens first.
  prepare_removable_media_unmount_callback_ = std::move(callback);

  volume_mounter_instance->PrepareForRemovableMediaUnmount(
      mount_path,
      base::BindOnce(
          &ArcVolumeMounterBridge::OnArcPreparedForRemovableMediaUnmount,
          weak_ptr_factory_.GetWeakPtr()));

  prepare_removable_media_unmount_timer_.Start(
      FROM_HERE, timeout,
      base::BindOnce(
          &ArcVolumeMounterBridge::OnArcPreparedForRemovableMediaUnmount,
          weak_ptr_factory_.GetWeakPtr(), false /* success */));
}

void ArcVolumeMounterBridge::OnArcPreparedForRemovableMediaUnmount(
    bool success) {
  if (prepare_removable_media_unmount_callback_) {
    std::move(prepare_removable_media_unmount_callback_).Run(success);
  }
}

void ArcVolumeMounterBridge::SendMountEventForRemovableMedia(
    DiskMountManager::MountEvent event,
    const std::string& source_path,
    const std::string& mount_path,
    const std::string& fs_uuid,
    const std::string& device_label,
    ash::DeviceType device_type,
    bool visible) {
  mojom::VolumeMounterInstance* volume_mounter_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->volume_mounter(),
                                  OnMountEvent);

  if (!volume_mounter_instance) {
    return;
  }
  volume_mounter_instance->OnMountEvent(
      mojom::MountPointInfo::New(event, source_path, mount_path, fs_uuid,
                                 device_label, device_type, visible));
}

void ArcVolumeMounterBridge::OnConnectionClosed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  external_storage_mount_points_are_ready_ = false;
}

void ArcVolumeMounterBridge::RequestAllMountPoints() {
  // Deferring the SendAllMountEvents as a task to current thread to not
  // block the mojo request since SendAllMountEvents might take non trivial
  // amount of time.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ArcVolumeMounterBridge::SendAllMountEvents,
                                weak_ptr_factory_.GetWeakPtr()));
}

bool ArcVolumeMounterBridge::IsReadyToSendMountingEvents() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  // Check whether external storage mount points are set up and file system
  // watchers are watching file system changes. In ARC P, we can assume that the
  // mount points are set up in an earlier boot stage, whereas in ARC R+ they
  // need to be set up by SetUpExternalStorageMountPoints().
  return (GetArcAndroidSdkVersionAsInt() < arc::kArcVersionR ||
          external_storage_mount_points_are_ready_) &&
         delegate_->IsWatchingFileSystemChanges();
}

void ArcVolumeMounterBridge::SetUpExternalStorageMountPoints(
    uint32_t media_provider_uid,
    SetUpExternalStorageMountPointsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetArcAndroidSdkVersionAsInt() >= arc::kArcVersionR);
  if (media_provider_uid < kAndroidAppUidStart ||
      media_provider_uid > kAndroidAppUidEnd) {
    LOG(ERROR) << "Invalid MediaProvider UID: " << media_provider_uid;
    std::move(callback).Run(false);
    return;
  }

  if (external_storage_mount_points_are_ready_) {
    std::move(callback).Run(true);
    return;
  }

  DVLOG(1) << "MediaProvider UID is " << media_provider_uid;

  const bool is_arcvm = IsArcVmEnabled();
  const std::string job_name = is_arcvm ? kArcVmMediaSharingServicesJobName
                                        : kArcppMediaSharingServicesJobName;
  const std::string chromeos_user = GetChromeOsUserId();
  DCHECK(!chromeos_user.empty());
  std::vector<std::string> environment{
      "CHROMEOS_USER=" + chromeos_user,
      base::StringPrintf("MEDIA_PROVIDER_UID=%u", media_provider_uid)};
  if (!is_arcvm) {
    // We need to explicitly tell R container to use MediaProvider UID.
    environment.push_back("IS_ANDROID_CONTAINER_RVC=true");
  }

  // Post OnSetUpExternalStorageMountPoints() as a task on the current thread
  // because it eventually calls ArcFileSystemWatcherService's methods to attach
  // watchers that need to be called on the UI thread.
  ash::UpstartClient::Get()->StartJobWithErrorDetails(
      job_name, std::move(environment),
      base::BindPostTask(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::BindOnce(
              &ArcVolumeMounterBridge::OnSetUpExternalStorageMountPoints,
              weak_ptr_factory_.GetWeakPtr(), job_name, std::move(callback))));
}

void ArcVolumeMounterBridge::OnSetUpExternalStorageMountPoints(
    const std::string& job_name,
    SetUpExternalStorageMountPointsCallback callback,
    bool result,
    std::optional<std::string> error_name,
    std::optional<std::string> error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!external_storage_mount_points_are_ready_);
  if (!result) {
    // Check if the job has already been running, in which case we treat the
    // result as a success. It can happen when Android's system services are
    // restarted without rebooting.
    if (error_name.has_value() &&
        error_name.value() == ash::UpstartClient::kAlreadyStartedError) {
      DVLOG(1) << job_name << " is already running";
    } else {
      LOG(ERROR) << "Failed to start " << job_name
                 << (error_name.has_value()
                         ? base::StrCat({": ", error_name.value()})
                         : "")
                 << (error_message.has_value()
                         ? base::StrCat({": ", error_message.value()})
                         : "");
      std::move(callback).Run(false);
      return;
    }
  }

  external_storage_mount_points_are_ready_ = true;
  std::move(callback).Run(true);
}

// static
void ArcVolumeMounterBridge::EnsureFactoryBuilt() {
  ArcVolumeMounterBridgeFactory::GetInstance();
}

}  // namespace arc
