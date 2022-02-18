// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_task.h"

#include <sstream>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_disk_manager.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_launch_options.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/borealis_wayland_interface.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"
#include "chromeos/dbus/dlcservice/dlcservice.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace borealis {

BorealisTask::BorealisTask(std::string name) : name_(std::move(name)) {}

BorealisTask::~BorealisTask() = default;

void BorealisTask::Run(BorealisContext* context,
                       CompletionResultCallback callback) {
  callback_ = std::move(callback);
  start_time_ = base::Time::Now();
  RunInternal(context);
}

void BorealisTask::Complete(BorealisStartupResult status, std::string message) {
  // TODO(b/198698779): Remove these logs before going live.
  LOG(WARNING) << "Task " << name_ << " completed in "
               << (base::Time::Now() - start_time_);
  // Task completion is self-mutually-exclusive, because tasks are deleted once
  // complete.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), status, std::move(message)));
}

CheckAllowed::CheckAllowed() : BorealisTask("CheckAllowed") {}
CheckAllowed::~CheckAllowed() = default;

void CheckAllowed::RunInternal(BorealisContext* context) {
  BorealisService::GetForProfile(context->profile())
      ->Features()
      .IsAllowed(base::BindOnce(&CheckAllowed::OnAllowednessChecked,
                                weak_factory_.GetWeakPtr(), context));
}

void CheckAllowed::OnAllowednessChecked(
    BorealisContext* context,
    BorealisFeatures::AllowStatus allow_status) {
  if (allow_status == BorealisFeatures::AllowStatus::kAllowed) {
    Complete(BorealisStartupResult::kSuccess, "");
    return;
  }
  std::stringstream ss;
  ss << "Borealis is disallowed: " << allow_status;
  Complete(BorealisStartupResult::kDisallowed, ss.str());
}

MountDlc::MountDlc() : BorealisTask("MountDlc") {}
MountDlc::~MountDlc() = default;

void MountDlc::RunInternal(BorealisContext* context) {
  // TODO(b/172279567): Ensure the DLC is present before trying to install,
  // otherwise we will silently download borealis here.
  dlcservice::InstallRequest install_request;
  install_request.set_id(kBorealisDlcName);
  chromeos::DlcserviceClient::Get()->Install(
      install_request,
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

CreateDiskImage::CreateDiskImage() : BorealisTask("CreateDiskImage") {}
CreateDiskImage::~CreateDiskImage() = default;

void CreateDiskImage::RunInternal(BorealisContext* context) {
  vm_tools::concierge::CreateDiskImageRequest request;
  request.set_vm_name(context->vm_name());
  request.set_cryptohome_id(
      ash::ProfileHelper::GetUserIdHashFromProfile(context->profile()));
  request.set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);
  request.set_disk_size(0);

  chromeos::ConciergeClient::Get()->CreateDiskImage(
      std::move(request), base::BindOnce(&CreateDiskImage::OnCreateDiskImage,
                                         weak_factory_.GetWeakPtr(), context));
}

void CreateDiskImage::OnCreateDiskImage(
    BorealisContext* context,
    absl::optional<vm_tools::concierge::CreateDiskImageResponse> response) {
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

RequestWaylandServer::RequestWaylandServer()
    : BorealisTask("RequestWaylandServer") {}
RequestWaylandServer::~RequestWaylandServer() = default;

void RequestWaylandServer::RunInternal(BorealisContext* context) {
  borealis::BorealisService::GetForProfile(context->profile())
      ->WaylandInterface()
      .GetWaylandServer(base::BindOnce(&RequestWaylandServer::OnServerRequested,
                                       weak_factory_.GetWeakPtr(), context));
}

void RequestWaylandServer::OnServerRequested(
    BorealisContext* context,
    BorealisCapabilities* capabilities,
    const base::FilePath& server_path) {
  if (!capabilities) {
    Complete(BorealisStartupResult::kRequestWaylandFailed,
             "Failed to create a wayland server");
    return;
  }
  context->set_wayland_path(server_path);
  Complete(BorealisStartupResult::kSuccess, "");
}

namespace {

bool GetDeveloperMode() {
  std::string output;
  if (!base::GetAppOutput({"/usr/bin/crossystem", "cros_debug"}, &output)) {
    return false;
  }
  return output == "1";
}

absl::optional<base::File> GetExtraDiskIfInDeveloperMode(
    absl::optional<base::FilePath> file_path) {
  if (!file_path)
    return absl::nullopt;

  if (!GetDeveloperMode())
    return absl::nullopt;

  base::File file(file_path.value(), base::File::FLAG_OPEN |
                                         base::File::FLAG_READ |
                                         base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    LOG(WARNING) << "Failed to open " << file_path.value();
    return absl::nullopt;
  }
  return file;
}

}  // namespace

StartBorealisVm::StartBorealisVm() : BorealisTask("StartBorealisVm") {}
StartBorealisVm::~StartBorealisVm() = default;

void StartBorealisVm::RunInternal(BorealisContext* context) {
  absl::optional<base::FilePath> external_disk =
      borealis::BorealisService::GetForProfile(context->profile())
          ->LaunchOptions()
          .GetExtraDisk();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::MayBlock(),
      base::BindOnce(&GetExtraDiskIfInDeveloperMode, std::move(external_disk)),
      base::BindOnce(&StartBorealisVm::StartBorealisWithExternalDisk,
                     weak_factory_.GetWeakPtr(), context));
}
void StartBorealisVm::StartBorealisWithExternalDisk(
    BorealisContext* context,
    absl::optional<base::File> external_disk) {
  vm_tools::concierge::StartVmRequest request;
  request.mutable_vm()->set_dlc_id(kBorealisDlcName);
  request.mutable_vm()->set_wayland_server(
      context->wayland_path().AsUTF8Unsafe());
  request.set_start_termina(false);
  request.set_owner_id(
      ash::ProfileHelper::GetUserIdHashFromProfile(context->profile()));
  request.set_enable_gpu(true);
  request.set_software_tpm(false);
  request.set_enable_audio_capture(false);
  request.set_enable_vulkan(true);
  if (base::FeatureList::IsEnabled(chromeos::features::kBorealisBigGl)) {
    request.set_enable_big_gl(true);
  }
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

  if (external_disk) {
    base::ScopedFD fd(external_disk->TakePlatformFile());
    request.add_fds(vm_tools::concierge::StartVmRequest::STORAGE);
    chromeos::ConciergeClient::Get()->StartTerminaVmWithFd(
        std::move(fd), std::move(request),
        base::BindOnce(&StartBorealisVm::OnStartBorealisVm,
                       weak_factory_.GetWeakPtr(), context));
    return;
  }
  chromeos::ConciergeClient::Get()->StartTerminaVm(
      std::move(request), base::BindOnce(&StartBorealisVm::OnStartBorealisVm,
                                         weak_factory_.GetWeakPtr(), context));
}

void StartBorealisVm::OnStartBorealisVm(
    BorealisContext* context,
    absl::optional<vm_tools::concierge::StartVmResponse> response) {
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
    : BorealisTask("AwaitBorealisStartup"), watcher_(profile, vm_name) {}
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
    absl::optional<std::string> container) {
  if (!container) {
    Complete(BorealisStartupResult::kAwaitBorealisStartupFailed,
             "Awaiting for Borealis launch failed: timed out");
    return;
  }
  context->set_container_name(container.value());
  Complete(BorealisStartupResult::kSuccess, "");
}

namespace {

// Helper for converting |feature| flags into name=bool args for the given
// |out_command|.
void PushFlag(const base::Feature& feature,
              std::vector<std::string>& out_command) {
  out_command.emplace_back(
      std::string(feature.name) + "=" +
      (base::FeatureList::IsEnabled(feature) ? "true" : "false"));
}

// Runs the update_flags script on the vm with the given |vm_name| and
// |owner_id|, where the |flags| are <name, value> pairs. Returns "" on success,
// otherwise returns an error message.
//
// TODO(b/207792847): avoid vsh and add a higher-level command to garcon.
std::string SendFlagsToVm(const std::string& owner_id,
                          const std::string& vm_name) {
  std::vector<std::string> command{"/usr/bin/vsh", "--owner_id=" + owner_id,
                                   "--vm_name=" + vm_name, "--",
                                   "update_chrome_flags"};
  PushFlag(chromeos::features::kBorealisLinuxMode, command);
  PushFlag(chromeos::features::kBorealisForceBetaClient, command);

  std::string output;
  if (!base::GetAppOutput(command, &output)) {
    return output;
  }
  return "";
}

}  // namespace

UpdateChromeFlags::UpdateChromeFlags(Profile* profile)
    : BorealisTask("UpdateChromeFlags"), profile_(profile) {}
UpdateChromeFlags::~UpdateChromeFlags() = default;

void UpdateChromeFlags::RunInternal(BorealisContext* context) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::MayBlock(),
      base::BindOnce(&SendFlagsToVm,
                     ash::ProfileHelper::GetUserIdHashFromProfile(profile_),
                     context->vm_name()),
      base::BindOnce(&UpdateChromeFlags::OnFlagsUpdated,
                     weak_factory_.GetWeakPtr(), context));
}

void UpdateChromeFlags::OnFlagsUpdated(BorealisContext* context,
                                       std::string error) {
  // This step should not block startup, so just log the error and declare
  // success.
  if (!error.empty()) {
    LOG(ERROR) << "Failed to update chrome's flags in Borealis: " << error;
  }
  Complete(BorealisStartupResult::kSuccess, "");
}

SyncBorealisDisk::SyncBorealisDisk() : BorealisTask("SyncBorealisDisk") {}
SyncBorealisDisk::~SyncBorealisDisk() = default;

void SyncBorealisDisk::RunInternal(BorealisContext* context) {
  context->get_disk_manager().SyncDiskSize(
      base::BindOnce(&SyncBorealisDisk::OnSyncBorealisDisk,
                     weak_factory_.GetWeakPtr(), context));
}

void SyncBorealisDisk::OnSyncBorealisDisk(
    BorealisContext* context,
    Expected<BorealisSyncDiskSizeResult, Described<BorealisSyncDiskSizeResult>>
        result) {
  if (!result) {
    Complete(BorealisStartupResult::kSyncDiskFailed,
             "Failed to sync disk: " + result.Error().description());
    return;
  }
  Complete(BorealisStartupResult::kSuccess, "");
}

}  // namespace borealis
