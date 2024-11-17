// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_task.h"

#include <optional>
#include <sstream>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_launch_options.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/public/guest_os_wayland_server.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "chromeos/ash/components/dbus/vm_launch/launch.pb.h"

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
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), status, std::move(message)));
}

CheckAllowed::CheckAllowed() : BorealisTask("CheckAllowed") {}
CheckAllowed::~CheckAllowed() = default;

void CheckAllowed::RunInternal(BorealisContext* context) {
  BorealisServiceFactory::GetForProfile(context->profile())
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

GetLaunchOptions::GetLaunchOptions() : BorealisTask("GetLaunchOptions") {}
GetLaunchOptions::~GetLaunchOptions() = default;

void GetLaunchOptions::RunInternal(BorealisContext* context) {
  BorealisServiceFactory::GetForProfile(context->profile())
      ->LaunchOptions()
      .Build(base::BindOnce(&GetLaunchOptions::HandleOptions,
                            weak_factory_.GetWeakPtr(), context));
}

void GetLaunchOptions::HandleOptions(BorealisContext* context,
                                     BorealisLaunchOptions::Options options) {
  context->set_launch_options(options);
  Complete(BorealisStartupResult::kSuccess, "");
}

MountDlc::MountDlc() : BorealisTask("MountDlc") {}
MountDlc::~MountDlc() = default;

void MountDlc::RunInternal(BorealisContext* context) {
  // TODO(b/172279567): Ensure the DLC is present before trying to install,
  // otherwise we will silently download borealis here.
  installation_ = std::make_unique<guest_os::GuestOsDlcInstallation>(
      kBorealisDlcName,
      base::BindOnce(&MountDlc::OnMountDlc, weak_factory_.GetWeakPtr(),
                     context),
      base::DoNothing());
}

void MountDlc::OnMountDlc(
    BorealisContext* context,
    guest_os::GuestOsDlcInstallation::Result install_result) {
  if (!install_result.has_value()) {
    std::stringstream ss;
    ss << "Mounting the DLC for Borealis failed: " << install_result.error();
    switch (install_result.error()) {
      case guest_os::GuestOsDlcInstallation::Error::Cancelled:
        Complete(BorealisStartupResult::kDlcCancelled, ss.str());
        return;
      case guest_os::GuestOsDlcInstallation::Error::Offline:
        Complete(BorealisStartupResult::kDlcOffline, ss.str());
        return;
      case guest_os::GuestOsDlcInstallation::Error::NeedUpdate:
        Complete(BorealisStartupResult::kDlcNeedUpdateError, ss.str());
        return;
      case guest_os::GuestOsDlcInstallation::Error::NeedReboot:
        Complete(BorealisStartupResult::kDlcNeedRebootError, ss.str());
        return;
      case guest_os::GuestOsDlcInstallation::Error::DiskFull:
        Complete(BorealisStartupResult::kDlcNeedSpaceError, ss.str());
        return;
      case guest_os::GuestOsDlcInstallation::Error::Busy:
        Complete(BorealisStartupResult::kDlcBusyError, ss.str());
        return;
      case guest_os::GuestOsDlcInstallation::Error::Internal:
        Complete(BorealisStartupResult::kDlcInternalError, ss.str());
        return;
      case guest_os::GuestOsDlcInstallation::Error::Invalid:
        Complete(BorealisStartupResult::kDlcUnsupportedError, ss.str());
        return;
      case guest_os::GuestOsDlcInstallation::Error::UnknownFailure:
        Complete(BorealisStartupResult::kDlcUnknownError, ss.str());
        return;
    }
  } else {
    Complete(BorealisStartupResult::kSuccess, "");
  }
}

CreateDiskImage::CreateDiskImage() : BorealisTask("CreateDiskImage") {}
CreateDiskImage::~CreateDiskImage() = default;

void CreateDiskImage::RunInternal(BorealisContext* context) {
  ash::ConciergeClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&CreateDiskImage::OnConciergeAvailable,
                     weak_factory_.GetWeakPtr(), context));
}

void CreateDiskImage::OnConciergeAvailable(BorealisContext* context,
                                           bool is_available) {
  if (!is_available) {
    context->set_disk_path({});
    Complete(BorealisStartupResult::kConciergeUnavailable,
             "Concierge service is not available");
    return;
  }

  vm_tools::concierge::CreateDiskImageRequest request;
  request.set_vm_name(context->vm_name());
  request.set_cryptohome_id(
      ash::ProfileHelper::GetUserIdHashFromProfile(context->profile()));
  request.set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);
  request.set_disk_size(0);
  request.set_filesystem_type(vm_tools::concierge::EXT4);
  request.set_storage_ballooning(true);

  ash::ConciergeClient::Get()->CreateDiskImage(
      std::move(request), base::BindOnce(&CreateDiskImage::OnCreateDiskImage,
                                         weak_factory_.GetWeakPtr(), context));
}

void CreateDiskImage::OnCreateDiskImage(
    BorealisContext* context,
    std::optional<vm_tools::concierge::CreateDiskImageResponse> response) {
  if (!response) {
    context->set_disk_path(base::FilePath());
    Complete(BorealisStartupResult::kEmptyDiskResponse,
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

namespace {

std::optional<base::File> MaybeOpenFile(
    std::optional<base::FilePath> file_path) {
  if (!file_path) {
    return std::nullopt;
  }

  base::File file(file_path.value(), base::File::FLAG_OPEN |
                                         base::File::FLAG_READ |
                                         base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    LOG(WARNING) << "Failed to open " << file_path.value();
    return std::nullopt;
  }
  return file;
}

}  // namespace

StartBorealisVm::StartBorealisVm() : BorealisTask("StartBorealisVm") {}
StartBorealisVm::~StartBorealisVm() = default;

void StartBorealisVm::RunInternal(BorealisContext* context) {
  ash::ConciergeClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&StartBorealisVm::OnConciergeAvailable,
                     weak_factory_.GetWeakPtr(), context));
}

void StartBorealisVm::OnConciergeAvailable(BorealisContext* context,
                                           bool service_is_available) {
  if (!service_is_available) {
    Complete(BorealisStartupResult::kConciergeUnavailable,
             "Concierge service is not available");
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::MayBlock(),
      base::BindOnce(&MaybeOpenFile, context->launch_options().extra_disk),
      base::BindOnce(&StartBorealisVm::StartBorealisWithExternalDisk,
                     weak_factory_.GetWeakPtr(), context));
}

void StartBorealisVm::StartBorealisWithExternalDisk(
    BorealisContext* context,
    std::optional<base::File> external_disk) {
  vm_tools::concierge::StartVmRequest request;
  request.mutable_vm()->set_dlc_id(kBorealisDlcName);
  request.set_start_termina(false);
  request.set_owner_id(
      ash::ProfileHelper::GetUserIdHashFromProfile(context->profile()));
  request.set_enable_gpu(true);
  request.set_enable_audio_capture(false);
  if (base::FeatureList::IsEnabled(ash::features::kBorealisBigGl)) {
    request.set_enable_big_gl(true);
  }
  request.set_name(context->vm_name());
  request.set_storage_ballooning(true);
  if (base::FeatureList::IsEnabled(ash::features::kBorealisDGPU)) {
    request.set_enable_dgpu_passthrough(true);
  }

  vm_tools::concierge::DiskImage* disk_image = request.add_disks();
  disk_image->set_path(context->disk_path().AsUTF8Unsafe());
  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  disk_image->set_writable(true);
  disk_image->set_do_mount(false);

  if (external_disk) {
    base::ScopedFD fd(external_disk->TakePlatformFile());
    request.add_fds(vm_tools::concierge::StartVmRequest::STORAGE);
    ash::ConciergeClient::Get()->StartVmWithFd(
        std::move(fd), std::move(request),
        base::BindOnce(&StartBorealisVm::OnStartBorealisVm,
                       weak_factory_.GetWeakPtr(), context));
    return;
  }
  ash::ConciergeClient::Get()->StartVm(
      std::move(request), base::BindOnce(&StartBorealisVm::OnStartBorealisVm,
                                         weak_factory_.GetWeakPtr(), context));
}

void StartBorealisVm::OnStartBorealisVm(
    BorealisContext* context,
    std::optional<vm_tools::concierge::StartVmResponse> response) {
  if (!response) {
    Complete(BorealisStartupResult::kStartVmEmptyResponse,
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

AwaitBorealisStartup::AwaitBorealisStartup()
    : BorealisTask("AwaitBorealisStartup") {}
AwaitBorealisStartup::~AwaitBorealisStartup() = default;

void AwaitBorealisStartup::RunInternal(BorealisContext* context) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AwaitBorealisStartup::OnTimeout,
                     weak_factory_.GetWeakPtr()),
      base::Seconds(30));
  // TODO(b/292020283): make hard-coded "penguin"s into a constant.
  guest_os::GuestId id(guest_os::VmType::BOREALIS, context->vm_name(),
                       "penguin");
  // It is safe to BindOnce() the context* since it is guaranteed to be alive
  // until this task calls Complete().
  subscription_ =
      guest_os::GuestOsSessionTrackerFactory::GetForProfile(context->profile())
          ->RunOnceContainerStarted(
              id, base::BindOnce(&AwaitBorealisStartup::OnContainerStarted,
                                 weak_factory_.GetWeakPtr(), context));
}

void AwaitBorealisStartup::OnContainerStarted(BorealisContext* context,
                                              guest_os::GuestInfo info) {
  context->set_container_name(info.guest_id.container_name);
  Complete(BorealisStartupResult::kSuccess, "");
}

void AwaitBorealisStartup::OnTimeout() {
  Complete(BorealisStartupResult::kAwaitBorealisStartupFailed,
           "Awaiting for Borealis launch failed: timed out");
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

// Helper for converting |feature| and |param| of enum type into
// feature_name=param_value arg for the given |out_command|.
template <typename Enum>
void PushParamEnum(const base::Feature& feature,
                   const base::FeatureParam<Enum>& param,
                   std::vector<std::string>& out_command) {
  out_command.emplace_back(std::string(feature.name) + "=" +
                           (base::FeatureList::IsEnabled(feature)
                                ? param.GetName(param.Get())
                                : "false"));
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
  PushFlag(ash::features::kBorealisLinuxMode, command);
  PushFlag(ash::features::kBorealisForceBetaClient, command);
  PushFlag(ash::features::kBorealisForceDoubleScale, command);
  PushFlag(ash::features::kBorealisScaleClientByDPI, command);

  PushParamEnum(ash::features::kBorealisZinkGlDriver,
                ash::features::kBorealisZinkGlDriverParam, command);

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

}  // namespace borealis
