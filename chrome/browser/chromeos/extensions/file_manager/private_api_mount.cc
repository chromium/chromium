// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_mount.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/smb_client/smb_service.h"
#include "chrome/browser/chromeos/smb_client/smb_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/drive/event_logger.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/drive/task_util.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace extensions {

using chromeos::disks::DiskMountManager;
using content::BrowserThread;
namespace file_manager_private = extensions::api::file_manager_private;

FileManagerPrivateAddMountFunction::FileManagerPrivateAddMountFunction() =
    default;

ExtensionFunction::ResponseAction FileManagerPrivateAddMountFunction::Run() {
  using file_manager_private::AddMount::Params;
  const std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  drive::EventLogger* logger = file_manager::util::GetLogger(profile);
  if (logger) {
    logger->Log(logging::LOG_INFO, "%s[%d] called. (source: '%s')", name(),
                request_id(),
                params->source.empty() ? "(none)" : params->source.c_str());
  }
  set_log_on_completion(true);

  const base::FilePath path = file_manager::util::GetLocalPathFromURL(
      render_frame_host(), profile, GURL(params->source));

  if (path.empty())
    return RespondNow(Error("Invalid path"));

  if (auto* notifier =
          file_manager::file_tasks::FileTasksNotifier::GetForProfile(profile)) {
    const scoped_refptr<storage::FileSystemContext> file_system_context =
        file_manager::util::GetFileSystemContextForRenderFrameHost(
            profile, render_frame_host());

    std::vector<storage::FileSystemURL> urls;
    const storage::FileSystemURL url =
        file_system_context->CrackURL(GURL(params->source));
    urls.push_back(url);

    notifier->NotifyFileTasks(urls);
  }

  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<std::string> options;
  if (params->password)
    options.push_back("password=" + *params->password);

  // MountPath() takes a std::string.
  DiskMountManager* disk_mount_manager = DiskMountManager::GetInstance();
  disk_mount_manager->MountPath(
      path.AsUTF8Unsafe(), base::ToLowerASCII(path.Extension()),
      path.BaseName().AsUTF8Unsafe(), options, chromeos::MOUNT_TYPE_ARCHIVE,
      chromeos::MOUNT_ACCESS_MODE_READ_WRITE);

  // Pass back the actual source path of the mount point.
  return RespondNow(OneArgument(base::Value(path.AsUTF8Unsafe())));
}

ExtensionFunction::ResponseAction FileManagerPrivateRemoveMountFunction::Run() {
  using file_manager_private::RemoveMount::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  drive::EventLogger* logger = file_manager::util::GetLogger(profile);
  if (logger) {
    logger->Log(logging::LOG_INFO, "%s[%d] called. (volume_id: '%s')", name(),
                request_id(), params->volume_id.c_str());
  }
  set_log_on_completion(true);

  using file_manager::Volume;
  using file_manager::VolumeManager;
  VolumeManager* const volume_manager = VolumeManager::Get(profile);
  DCHECK(volume_manager);

  base::WeakPtr<Volume> volume =
      volume_manager->FindVolumeById(params->volume_id);
  if (!volume.get())
    return RespondNow(Error("Volume not available"));

  // TODO(tbarzic): Send response when callback is received, it would make more
  // sense than remembering issued unmount requests in file manager and showing
  // errors for them when MountCompleted event is received.
  switch (volume->type()) {
    case file_manager::VOLUME_TYPE_REMOVABLE_DISK_PARTITION:
    case file_manager::VOLUME_TYPE_MOUNTED_ARCHIVE_FILE: {
      DiskMountManager::GetInstance()->UnmountPath(
          volume->mount_path().value(),
          DiskMountManager::UnmountPathCallback());
      break;
    }
    case file_manager::VOLUME_TYPE_PROVIDED: {
      chromeos::file_system_provider::Service* service =
          chromeos::file_system_provider::Service::Get(browser_context());
      DCHECK(service);
      // TODO(mtomasz): Pass a more detailed error than just a bool.
      if (!service->RequestUnmount(volume->provider_id(),
                                   volume->file_system_id())) {
        return RespondNow(Error("Unmount failed"));
      }
      break;
    }
    case file_manager::VOLUME_TYPE_CROSTINI:
      file_manager::VolumeManager::Get(profile)->RemoveSshfsCrostiniVolume(
          volume->mount_path(), base::DoNothing());
      break;
    case file_manager::VOLUME_TYPE_SMB:
      chromeos::smb_client::SmbServiceFactory::Get(profile)->UnmountSmbFs(
          volume->mount_path());
      break;
    default:
      // Requested unmounting a device which is not unmountable.
      return RespondNow(Error("Invalid volume type"));
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateGetVolumeMetadataListFunction::Run() {
  if (args_->GetSize())
    return RespondNow(Error("Invalid arguments"));

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
    if (!log_string.empty())
      log_string += ", ";
    log_string += volume->mount_path().AsUTF8Unsafe();
  }

  drive::EventLogger* logger = file_manager::util::GetLogger(profile);
  if (logger) {
    logger->Log(logging::LOG_INFO,
                "%s[%d] succeeded. (results: '[%s]', %" PRIuS " mount points)",
                name(), request_id(), log_string.c_str(), result.size());
  }

  return RespondNow(ArgumentList(
      file_manager_private::GetVolumeMetadataList::Results::Create(result)));
}

}  // namespace extensions
