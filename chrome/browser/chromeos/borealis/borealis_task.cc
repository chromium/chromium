// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_task.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/borealis/borealis_context.h"
#include "chrome/browser/chromeos/borealis/borealis_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
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
    LOG(ERROR) << "Mounting the DLC for Borealis failed: "
               << install_result.error;
    std::move(callback).Run(false);
  } else {
    context->set_root_path(install_result.root_path);
    std::move(callback).Run(true);
  }
}

CreateDiskImage::CreateDiskImage() = default;
CreateDiskImage::~CreateDiskImage() = default;

void CreateDiskImage::Run(BorealisContext* context,
                          CompletionStatusCallback callback) {
  vm_tools::concierge::CreateDiskImageRequest request;
  request.set_disk_path(
      base::FilePath(context->container_name()).AsUTF8Unsafe());
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
    LOG(ERROR) << "Failed to create disk image for Borealis. Empty response.";
    context->set_disk_path(base::FilePath());
    std::move(callback).Run(false);
    return;
  }

  if (response->status() != vm_tools::concierge::DISK_STATUS_EXISTS &&
      response->status() != vm_tools::concierge::DISK_STATUS_CREATED) {
    LOG(ERROR) << "Failed to create disk image for Borealis: "
               << response->failure_reason();
    context->set_disk_path(base::FilePath());
    std::move(callback).Run(false);
    return;
  }
  context->set_disk_path(base::FilePath(response->disk_path()));
  std::move(callback).Run(true);
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
  request.set_name(context->container_name());

  vm_tools::concierge::DiskImage* disk_image = request.add_disks();
  disk_image->set_path(context->disk_path().AsUTF8Unsafe());
  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  disk_image->set_writable(true);
  disk_image->set_do_mount(false);

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
    LOG(ERROR) << "Failed to start Borealis VM. Empty response.";
    std::move(callback).Run(false);
    return;
  }

  if (response->status() == vm_tools::concierge::VM_STATUS_RUNNING) {
    std::move(callback).Run(true);
    return;
  }

  if (response->status() == vm_tools::concierge::VM_STATUS_FAILURE ||
      response->status() == vm_tools::concierge::VM_STATUS_UNKNOWN) {
    LOG(ERROR) << "Failed to start Borealis VM: " << response->failure_reason();
    std::move(callback).Run(false);
    return;
  }

  DCHECK_EQ(response->status(), vm_tools::concierge::VM_STATUS_STARTING);
  std::move(callback).Run(true);
}
}  // namespace borealis
