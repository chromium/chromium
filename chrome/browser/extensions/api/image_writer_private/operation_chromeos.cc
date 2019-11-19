// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/operation.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/image_burner_client.h"
#include "chromeos/disks/disk.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {
namespace image_writer {

using chromeos::disks::DiskMountManager;
using chromeos::ImageBurnerClient;
using content::BrowserThread;

namespace {

void ClearImageBurner() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&ClearImageBurner));
    return;
  }

  chromeos::DBusThreadManager::Get()->
      GetImageBurnerClient()->
      ResetEventHandlers();
}

}  // namespace

void Operation::Write(const base::Closure& continuation) {
  DCHECK(IsRunningInCorrectSequence());
  SetStage(image_writer_api::STAGE_WRITE);

  // Note this has to be run on the FILE thread to avoid concurrent access.
  AddCleanUpFunction(base::Bind(&ClearImageBurner));

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&Operation::UnmountVolumes, this, continuation));
}

void Operation::VerifyWrite(const base::Closure& continuation) {
  DCHECK(IsRunningInCorrectSequence());

  // No verification is available in Chrome OS currently.
  continuation.Run();
}

void Operation::UnmountVolumes(const base::Closure& continuation) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DiskMountManager::GetInstance()->UnmountDeviceRecursively(
      device_path_.value(),
      base::Bind(&Operation::UnmountVolumesCallback, this, continuation));
}

void Operation::UnmountVolumesCallback(const base::Closure& continuation,
                                       chromeos::MountError error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (error_code != chromeos::MOUNT_ERROR_NONE) {
    LOG(ERROR) << "Volume unmounting failed with error code " << error_code;
    PostTask(base::Bind(&Operation::Error, this, error::kUnmountVolumesError));
    return;
  }

  const DiskMountManager::DiskMap& disks =
      DiskMountManager::GetInstance()->disks();
  DiskMountManager::DiskMap::const_iterator iter =
      disks.find(device_path_.value());

  if (iter == disks.end()) {
    LOG(ERROR) << "Disk not found in disk list after unmounting volumes.";
    PostTask(base::Bind(&Operation::Error, this, error::kUnmountVolumesError));
    return;
  }

  StartWriteOnUIThread(iter->second->file_path(), continuation);
}

void Operation::StartWriteOnUIThread(const std::string& target_path,
                                     const base::Closure& continuation) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(haven): Image Burner cannot handle multiple burns. crbug.com/373575
  ImageBurnerClient* burner =
      chromeos::DBusThreadManager::Get()->GetImageBurnerClient();

  burner->SetEventHandlers(
      base::Bind(&Operation::OnBurnFinished, this, continuation),
      base::Bind(&Operation::OnBurnProgress, this));

  burner->BurnImage(image_path_.value(),
                    target_path,
                    base::Bind(&Operation::OnBurnError, this));
}

void Operation::OnBurnFinished(const base::Closure& continuation,
                               const std::string& target_path,
                               bool success,
                               const std::string& error) {
  if (success) {
    PostTask(base::BindOnce(&Operation::SetProgress, this, kProgressComplete));
    PostTask(continuation);
  } else {
    DLOG(ERROR) << "Error encountered while burning: " << error;
    PostTask(base::BindOnce(&Operation::Error, this,
                            error::kChromeOSImageBurnerError));
  }
}

void Operation::OnBurnProgress(const std::string& target_path,
                               int64_t num_bytes_burnt,
                               int64_t total_size) {
  int progress = kProgressComplete * num_bytes_burnt / total_size;
  PostTask(base::BindOnce(&Operation::SetProgress, this, progress));
}

void Operation::OnBurnError() {
  PostTask(base::BindOnce(&Operation::Error, this,
                          error::kChromeOSImageBurnerError));
}

}  // namespace image_writer
}  // namespace extensions
