// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_task.h"
#include <string>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/borealis/borealis_context.h"
#include "chrome/browser/chromeos/borealis/borealis_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace borealis {

MountDlc::MountDlc() = default;
MountDlc::~MountDlc() = default;

void MountDlc::Run(BorealisContext* context,
                   CompletionStatusCallback callback) {
  chromeos::DlcserviceClient::Get()->Install(
      kBorealisDlcName,
      base::BindOnce(&MountDlc::OnMountDlc, weak_factory_.GetWeakPtr(), context,
                     std::move(callback)),
      base::DoNothing());
}

void MountDlc::OnMountDlc(
    BorealisContext* context,
    CompletionStatusCallback callback,
    const chromeos::DlcserviceClient::InstallResult& install_result) {
  if (install_result.error != dlcservice::kErrorNone) {
    std::move(callback).Run(
        BorealisContextManager::kMountFailed,
        "Mounting the DLC for Borealis failed: " + install_result.error);
  } else {
    context->set_root_path(install_result.root_path);
    std::move(callback).Run(BorealisContextManager::kSuccess, "");
  }
}

CreateDiskImage::CreateDiskImage() = default;
CreateDiskImage::~CreateDiskImage() = default;

void CreateDiskImage::Run(BorealisContext* context,
                          CompletionStatusCallback callback) {
  // We use a hard-coded name. When multi-instance becomes a feature we'll
  // need to determine the name instead.
  context->set_vm_name("borealis");
  vm_tools::concierge::CreateDiskImageRequest request;
  request.set_disk_path(context->vm_name());
  request.set_cryptohome_id(
      chromeos::ProfileHelper::GetUserIdHashFromProfile(context->profile()));
  request.set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);
  request.set_disk_size(0);

  chromeos::DBusThreadManager::Get()->GetConciergeClient()->CreateDiskImage(
      std::move(request),
      base::BindOnce(&CreateDiskImage::OnCreateDiskImage,
                     weak_factory_.GetWeakPtr(), context, std::move(callback)));
}

void CreateDiskImage::OnCreateDiskImage(
    BorealisContext* context,
    CompletionStatusCallback callback,
    base::Optional<vm_tools::concierge::CreateDiskImageResponse> response) {
  if (!response) {
    context->set_disk_path(base::FilePath());
    std::move(callback).Run(
        BorealisContextManager::kDiskImageFailed,
        "Failed to create disk image for Borealis: Empty response.");
    return;
  }

  if (response->status() != vm_tools::concierge::DISK_STATUS_EXISTS &&
      response->status() != vm_tools::concierge::DISK_STATUS_CREATED) {
    context->set_disk_path(base::FilePath());
    std::move(callback).Run(BorealisContextManager::kDiskImageFailed,
                            "Failed to create disk image for Borealis: " +
                                response->failure_reason());
    return;
  }
  context->set_disk_path(base::FilePath(response->disk_path()));
  std::move(callback).Run(BorealisContextManager::kSuccess, "");
}

StartBorealisVm::StartBorealisVm() = default;
StartBorealisVm::~StartBorealisVm() = default;

void StartBorealisVm::Run(BorealisContext* context,
                          CompletionStatusCallback callback) {
  vm_tools::concierge::StartVmRequest request;
  vm_tools::concierge::VirtualMachineSpec* vm = request.mutable_vm();
  vm->set_kernel(context->root_path() + "/vm_kernel");
  vm->set_rootfs(context->root_path() + "/vm_rootfs.img");
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
      std::move(request),
      base::BindOnce(&StartBorealisVm::OnStartBorealisVm,
                     weak_factory_.GetWeakPtr(), context, std::move(callback)));
}

void StartBorealisVm::OnStartBorealisVm(
    BorealisContext* context,
    CompletionStatusCallback callback,
    base::Optional<vm_tools::concierge::StartVmResponse> response) {
  if (!response) {
    std::move(callback).Run(BorealisContextManager::kStartVmFailed,
                            "Failed to start Borealis VM: Empty response.");
    return;
  }

  if (response->status() == vm_tools::concierge::VM_STATUS_RUNNING ||
      response->status() == vm_tools::concierge::VM_STATUS_STARTING) {
    std::move(callback).Run(BorealisContextManager::kSuccess, "");
    return;
  }

  std::move(callback).Run(
      BorealisContextManager::kStartVmFailed,
      "Failed to start Borealis VM: " + response->failure_reason() + " (code " +
          base::NumberToString(response->status()) + ")");
}

void AwaitBorealisStartup::Run(BorealisContext* context,
                               CompletionStatusCallback callback) {
  // TODO(b/170696557): Refactor to use the LaunchWatcher which is not finished
  // yet. In our case the name is hard-coded, so we can use that.
  context->set_container_name("penguin");
  std::move(callback).Run(BorealisContextManager::kSuccess, "");
}

}  // namespace borealis
