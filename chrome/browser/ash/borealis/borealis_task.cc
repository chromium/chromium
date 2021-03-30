// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_task.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace borealis {

BorealisTask::BorealisTask() = default;

BorealisTask::~BorealisTask() = default;

void BorealisTask::Run(BorealisContext* context,
                       CompletionResultCallback callback) {
  callback_ = std::move(callback);
  RunInternal(context);
}

void BorealisTask::Complete(BorealisStartupResult status, std::string message) {
  // Task completion is self-mutually-exclusive, because tasks are deleted once
  // complete.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), status, std::move(message)));
}

MountDlc::MountDlc() = default;
MountDlc::~MountDlc() = default;

void MountDlc::RunInternal(BorealisContext* context) {
  // TODO(b/172279567): Ensure the DLC is present before trying to install,
  // otherwise we will silently download borealis here.
  chromeos::DlcserviceClient::Get()->Install(
      kBorealisDlcName,
      base::BindOnce(&MountDlc::OnMountDlc, weak_factory_.GetWeakPtr(),
                     context),
      base::DoNothing());
}

void MountDlc::OnMountDlc(
    BorealisContext* context,
    const chromeos::DlcserviceClient::InstallResult& install_result) {
  if (install_result.error != dlcservice::kErrorNone) {
    Complete(BorealisStartupResult::kMountFailed,
             "Mounting the DLC for Borealis failed: " + install_result.error);
  } else {
    Complete(BorealisStartupResult::kSuccess, "");
  }
}

CreateDiskImage::CreateDiskImage() = default;
CreateDiskImage::~CreateDiskImage() = default;

void CreateDiskImage::RunInternal(BorealisContext* context) {
  vm_tools::concierge::CreateDiskImageRequest request;
  request.set_disk_path(context->vm_name());
  request.set_cryptohome_id(
      chromeos::ProfileHelper::GetUserIdHashFromProfile(context->profile()));
  request.set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);
  request.set_disk_size(0);

  chromeos::DBusThreadManager::Get()->GetConciergeClient()->CreateDiskImage(
      std::move(request), base::BindOnce(&CreateDiskImage::OnCreateDiskImage,
                                         weak_factory_.GetWeakPtr(), context));
}

void CreateDiskImage::OnCreateDiskImage(
    BorealisContext* context,
    base::Optional<vm_tools::concierge::CreateDiskImageResponse> response) {
  if (!response) {
    context->set_disk_path(base::FilePath());
    Complete(BorealisStartupResult::kDiskImageFailed,
             "Failed to create disk image for Borealis: Empty response.");
    return;
  }

  if (response->status() != vm_tools::concierge::DISK_STATUS_EXISTS &&
      response->status() != vm_tools::concierge::DISK_STATUS_CREATED) {
    context->set_disk_path(base::FilePath());
    Complete(BorealisStartupResult::kDiskImageFailed,
             "Failed to create disk image for Borealis: " +
                 response->failure_reason());
    return;
  }
  context->set_disk_path(base::FilePath(response->disk_path()));
  Complete(BorealisStartupResult::kSuccess, "");
}

StartBorealisVm::StartBorealisVm() = default;
StartBorealisVm::~StartBorealisVm() = default;

void StartBorealisVm::RunInternal(BorealisContext* context) {
  vm_tools::concierge::StartVmRequest request;
  request.mutable_vm()->set_dlc_id(kBorealisDlcName);
  request.set_start_termina(false);
  request.set_owner_id(
      chromeos::ProfileHelper::GetUserIdHashFromProfile(context->profile()));
  request.set_enable_gpu(true);
  request.set_software_tpm(false);
  request.set_enable_audio_capture(false);
  request.set_name(context->vm_name());

  vm_tools::concierge::DiskImage* disk_image = request.add_disks();
  disk_image->set_path(context->disk_path().AsUTF8Unsafe());
  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  disk_image->set_writable(true);
  disk_image->set_do_mount(false);

  // TODO(b/161952658): Remove this logging when the exo-pointer-lock is fixed,
  // this is only meant to be temporary.
  LOG(WARNING) << "Starting Borealis with exo-pointer-lock: "
               << (base::FeatureList::IsEnabled(
                       chromeos::features::kExoPointerLock)
                       ? "enabled"
                       : "disabled");
  chromeos::DBusThreadManager::Get()->GetConciergeClient()->StartTerminaVm(
      std::move(request), base::BindOnce(&StartBorealisVm::OnStartBorealisVm,
                                         weak_factory_.GetWeakPtr(), context));
}

void StartBorealisVm::OnStartBorealisVm(
    BorealisContext* context,
    base::Optional<vm_tools::concierge::StartVmResponse> response) {
  if (!response) {
    Complete(BorealisStartupResult::kStartVmFailed,
             "Failed to start Borealis VM: Empty response.");
    return;
  }

  if (response->status() == vm_tools::concierge::VM_STATUS_RUNNING ||
      response->status() == vm_tools::concierge::VM_STATUS_STARTING) {
    Complete(BorealisStartupResult::kSuccess, "");
    return;
  }

  Complete(BorealisStartupResult::kStartVmFailed,
           "Failed to start Borealis VM: " + response->failure_reason() +
               " (code " + base::NumberToString(response->status()) + ")");
}

AwaitBorealisStartup::AwaitBorealisStartup(Profile* profile,
                                           std::string vm_name)
    : watcher_(profile, vm_name) {}
AwaitBorealisStartup::~AwaitBorealisStartup() = default;

void AwaitBorealisStartup::RunInternal(BorealisContext* context) {
  watcher_.AwaitLaunch(
      base::BindOnce(&AwaitBorealisStartup::OnAwaitBorealisStartup,
                     weak_factory_.GetWeakPtr(), context));
}

BorealisLaunchWatcher& AwaitBorealisStartup::GetWatcherForTesting() {
  return watcher_;
}

void AwaitBorealisStartup::OnAwaitBorealisStartup(
    BorealisContext* context,
    base::Optional<std::string> container) {
  if (!container) {
    Complete(BorealisStartupResult::kAwaitBorealisStartupFailed,
             "Awaiting for Borealis launch failed: timed out");
    return;
  }
  context->set_container_name(container.value());
  Complete(BorealisStartupResult::kSuccess, "");
}
}  // namespace borealis
