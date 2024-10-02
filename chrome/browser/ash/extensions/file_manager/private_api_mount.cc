// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/private_api_mount.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/extensions/file_manager/event_router.h"
#include "chrome/browser/ash/extensions/file_manager/private_api_util.h"
#include "chrome/browser/ash/file_manager/file_tasks_notifier.h"
#include "chrome/browser/ash/file_manager/file_tasks_notifier_factory.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/ash/fusebox/fusebox_server.h"
#include "chrome/browser/ash/smb_client/smb_service.h"
#include "chrome/browser/ash/smb_client/smb_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "components/drive/event_logger.h"
#include "components/services/unzip/content/unzip_service.h"
#include "components/services/unzip/public/cpp/unzip.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/common/task_util.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace extensions {
namespace {

std::string Redact(std::string_view path) {
  return LOG_IS_ON(INFO) ? base::StrCat({"'", path, "'"}) : "(redacted)";
}

}  // namespace

using ::ash::disks::DiskMountManager;
using content::BrowserThread;
namespace file_manager_private = extensions::api::file_manager_private;

FileManagerPrivateAddMountFunction::FileManagerPrivateAddMountFunction() =
    default;

FileManagerPrivateAddMountFunction::~FileManagerPrivateAddMountFunction() =
    default;

ExtensionFunction::ResponseAction FileManagerPrivateAddMountFunction::Run() {
  using file_manager_private::AddMount::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  if (drive::EventLogger* logger = file_manager::util::GetLogger(profile)) {
    logger->Log(logging::LOGGING_INFO, "%s[%s] called. (source: '%s')", name(),
                request_uuid().AsLowercaseString().c_str(),
                params->file_url.empty() ? "(none)" : params->file_url.c_str());
  }
  set_log_on_completion(true);

  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());
  const storage::FileSystemURL fs_url(
      file_system_context->CrackURLInFirstPartyContext(GURL(params->file_url)));
  path_ = ash::FileSystemBackend::CanHandleURL(fs_url)
              ? (fs_url.TypeImpliesPathIsReal()
                     ? fs_url.path()
                     : fusebox::Server::SubstituteFuseboxFilePath(fs_url))
              : base::FilePath();

  if (auto* notifier =
          file_manager::file_tasks::FileTasksNotifierFactory::GetForProfile(
              profile)) {
    std::vector<storage::FileSystemURL> urls;
    urls.push_back(std::move(fs_url));
    notifier->NotifyFileTasks(urls);
  }

  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (params->password) {
    options_.push_back("password=" + *params->password);
  }

  extension_ = base::ToLowerASCII(path_.Extension());

  // Detect the file path encoding of ZIP archives.
  if (extension_ == ".zip") {
    unzip::DetectEncoding(
        unzip::LaunchUnzipper(), path_,
        base::BindOnce(&FileManagerPrivateAddMountFunction::OnEncodingDetected,
                       this));
  } else {
    FinishMounting();
  }

  // Pass back the actual source path of the mount point.
  return RespondNow(WithArguments(path_.AsUTF8Unsafe()));
}

void FileManagerPrivateAddMountFunction::OnEncodingDetected(
    const Encoding encoding) {
  // Pass the detected ZIP encoding as a mount option.
  std::string& option = options_.emplace_back("encoding=");

  if (IsShiftJisOrVariant(encoding) || encoding == RUSSIAN_CP866) {
    option += MimeEncodingName(encoding);
  } else {
    option += "libzip";
  }

  FinishMounting();
}

void FileManagerPrivateAddMountFunction::FinishMounting() {
  DiskMountManager* const disk_mount_manager = DiskMountManager::GetInstance();
  DCHECK(disk_mount_manager);
  disk_mount_manager->MountPath(path_.AsUTF8Unsafe(), std::move(extension_),
                                path_.BaseName().AsUTF8Unsafe(),
                                std::move(options_), ash::MountType::kArchive,
                                ash::MountAccessMode::kReadWrite,
                                base::DoNothing());
}

FileManagerPrivateCancelMountingFunction::
    FileManagerPrivateCancelMountingFunction() = default;

FileManagerPrivateCancelMountingFunction::
    ~FileManagerPrivateCancelMountingFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateCancelMountingFunction::Run() {
  using file_manager_private::CancelMounting::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* const profile = Profile::FromBrowserContext(browser_context());

  if (drive::EventLogger* logger = file_manager::util::GetLogger(profile)) {
    logger->Log(logging::LOGGING_INFO, "%s[%s] called. (source: '%s')", name(),
                request_uuid().AsLowercaseString().c_str(),
                params->file_url.empty() ? "(none)" : params->file_url.c_str());
  }
  set_log_on_completion(true);

  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());
  const storage::FileSystemURL fs_url(
      file_system_context->CrackURLInFirstPartyContext(GURL(params->file_url)));
  base::FilePath path =
      ash::FileSystemBackend::CanHandleURL(fs_url)
          ? (fs_url.TypeImpliesPathIsReal()
                 ? fs_url.path()
                 : fusebox::Server::SubstituteFuseboxFilePath(fs_url))
          : base::FilePath();

  DiskMountManager* const disk_mount_manager = DiskMountManager::GetInstance();
  DCHECK(disk_mount_manager);
  disk_mount_manager->UnmountPath(
      path.AsUTF8Unsafe(),
      base::BindOnce(&FileManagerPrivateCancelMountingFunction::OnCancelled,
                     this));

  return RespondLater();
}

void FileManagerPrivateCancelMountingFunction::OnCancelled(
    ash::MountError error) {
  if (error == ash::MountError::kSuccess) {
    Respond(NoArguments());
  } else {
    Respond(Error(file_manager_private::ToString(
        file_manager::MountErrorToMountCompletedStatus(error))));
  }
}

ExtensionFunction::ResponseAction FileManagerPrivateRemoveMountFunction::Run() {
  using file_manager_private::RemoveMount::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  if (drive::EventLogger* logger = file_manager::util::GetLogger(profile)) {
    logger->Log(logging::LOGGING_INFO, "%s[%s] called. (volume_id: '%s')",
                name(), request_uuid().AsLowercaseString().c_str(),
                params->volume_id.c_str());
  }
  set_log_on_completion(true);

  using file_manager::Volume;
  using file_manager::VolumeManager;
  VolumeManager* const volume_manager = VolumeManager::Get(profile);
  DCHECK(volume_manager);

  std::string volume_id = params->volume_id;
  volume_manager->ConvertFuseBoxFSPVolumeIdToFSPIfNeeded(&volume_id);

  const base::WeakPtr<Volume> volume =
      volume_manager->FindVolumeById(volume_id);
  if (!volume) {
    LOG(ERROR) << "Cannot find volume " << Redact(volume_id);
    return RespondNow(Error(file_manager_private::ToString(
        api::file_manager_private::MountError::kPathNotMounted)));
  }

  switch (volume->type()) {
    case file_manager::VOLUME_TYPE_REMOVABLE_DISK_PARTITION:
    case file_manager::VOLUME_TYPE_MOUNTED_ARCHIVE_FILE:
      DiskMountManager::GetInstance()->UnmountPath(
          volume->mount_path().value(),
          base::BindOnce(
              &FileManagerPrivateRemoveMountFunction::OnDiskUnmounted, this));
      return RespondLater();

    case file_manager::VOLUME_TYPE_PROVIDED: {
      auto* service =
          ash::file_system_provider::Service::Get(browser_context());
      DCHECK(service);
      if (!service->RequestUnmount(volume->provider_id(),
                                   volume->file_system_id())) {
        return RespondNow(Error("Unmount failed"));
      }
      return RespondNow(NoArguments());
    }

    case file_manager::VOLUME_TYPE_CROSTINI:
      file_manager::VolumeManager::Get(profile)->RemoveSshfsCrostiniVolume(
          volume->mount_path(),
          base::BindOnce(
              &FileManagerPrivateRemoveMountFunction::OnSshFsUnmounted, this));
      return RespondLater();

    case file_manager::VOLUME_TYPE_SMB:
      ash::smb_client::SmbServiceFactory::Get(profile)->UnmountSmbFs(
          volume->mount_path());
      return RespondNow(NoArguments());

    case file_manager::VOLUME_TYPE_TESTING:
      file_manager::VolumeManager::Get(profile)
          ->RemoveVolumeForTesting(  // IN-TEST
              volume->mount_path(), volume->type(), volume->device_type(),
              volume->is_read_only(), volume->storage_device_path(),
              volume->drive_label(), volume->file_system_type());

      return RespondNow(NoArguments());

    case file_manager::VOLUME_TYPE_GUEST_OS:
      // TODO(crbug/1293229): Figure out if we need to support unmounting. I'm
      // not actually sure if it's possible to reach here.
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];

    default:
      // Requested unmounting a device which is not unmountable.
      return RespondNow(Error("Invalid volume type"));
  }
}

void FileManagerPrivateRemoveMountFunction::OnSshFsUnmounted(bool ok) {
  if (ok) {
    Respond(NoArguments());
  } else {
    Respond(Error(file_manager_private::ToString(
        api::file_manager_private::MountError::kUnknownError)));
  }
}

void FileManagerPrivateRemoveMountFunction::OnDiskUnmounted(
    ash::MountError error) {
  if (error == ash::MountError::kSuccess) {
    Respond(NoArguments());
  } else {
    Respond(Error(file_manager_private::ToString(
        file_manager::MountErrorToMountCompletedStatus(error))));
  }
}

ExtensionFunction::ResponseAction
FileManagerPrivateGetVolumeMetadataListFunction::Run() {
  if (!args().empty()) {
    return RespondNow(Error("Invalid arguments"));
  }

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  const std::vector<base::WeakPtr<file_manager::Volume>>& volume_list =
      file_manager::VolumeManager::Get(profile)->GetVolumeList();

  std::string log_string;
  std::vector<file_manager_private::VolumeMetadata> result;
  for (const auto& volume : volume_list) {
    file_manager_private::VolumeMetadata volume_metadata;
    file_manager::util::VolumeToVolumeMetadata(profile, *volume,
                                               &volume_metadata);
    result.push_back(std::move(volume_metadata));
    if (!log_string.empty()) {
      log_string += ", ";
    }
    log_string += volume->mount_path().AsUTF8Unsafe();
  }

  if (drive::EventLogger* logger = file_manager::util::GetLogger(profile)) {
    logger->Log(logging::LOGGING_INFO,
                "%s[%s] succeeded. (results: '[%s]', %" PRIuS " mount points)",
                name(), request_uuid().AsLowercaseString().c_str(),
                log_string.c_str(), result.size());
  }

  return RespondNow(ArgumentList(
      file_manager_private::GetVolumeMetadataList::Results::Create(result)));
}

}  // namespace extensions
