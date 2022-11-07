// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_launcher.h"

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_service.pb.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace bruschetta {

namespace {

// TODO(b/233289313): Once we have an installer and multiple Bruschettas this
// needs to be dynamic, but for now we hardcode the same path that the go/brua
// instructions have people using for the alpha, and the same disk name that
// people following the instructions will have (base64 encoded "bru").
const char kDiskName[] = "YnJ1.img";

base::File OpenBios(base::FilePath bios_path) {
  base::File file(base::FilePath(bios_path),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  return file;
}

}  // namespace

BruschettaLauncher::BruschettaLauncher(std::string vm_name, Profile* profile)
    : vm_name_(vm_name), profile_(profile) {}
BruschettaLauncher::~BruschettaLauncher() = default;

void BruschettaLauncher::EnsureRunning(
    base::OnceCallback<void(BruschettaResult)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool launch_in_progress = false;
  if (!callbacks_.empty()) {
    launch_in_progress = true;
  }
  callbacks_.AddUnsafe(std::move(callback));
  if (!launch_in_progress) {
    EnsureDlcInstalled();
    // If we're not complete after 4 minutes time out the entire launch.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&BruschettaLauncher::OnTimeout,
                       weak_factory_.GetWeakPtr()),
        base::Seconds(240));
  }
}

void BruschettaLauncher::EnsureDlcInstalled() {
  dlcservice::InstallRequest request;
  request.set_id(crostini::kCrostiniDlcName);
  ash::DlcserviceClient::Get()->Install(
      request,
      base::BindOnce(&BruschettaLauncher::OnMountDlc,
                     weak_factory_.GetWeakPtr()),
      base::DoNothing());
}

void BruschettaLauncher::OnMountDlc(
    const ash::DlcserviceClient::InstallResult& install_result) {
  if (install_result.error != dlcservice::kErrorNone) {
    LOG(ERROR) << "Error installing DLC: " << install_result.error;
    callbacks_.Notify(BruschettaResult::kDlcInstallError);
    return;
  }

  // TODO(b/233289313): Same comment as on kDiskName. Hardcode this for now to
  // match the alpha instructions at go/brua, but once we have an installer this
  // needs to move to somewhere that's not user-accessible.
  base::FilePath bios_path = profile_->GetPath().Append("Downloads/bios");
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::MayBlock(),
      base::BindOnce(&OpenBios, std::move(bios_path)),
      base::BindOnce(&BruschettaLauncher::StartVm, weak_factory_.GetWeakPtr()));
}

void BruschettaLauncher::StartVm(base::File bios) {
  if (!bios.IsValid()) {
    LOG(ERROR) << "Error opening BIOS: " << bios.error_details();
    callbacks_.Notify(BruschettaResult::kBiosNotAccessible);
    return;
  }

  auto* client = chromeos::ConciergeClient::Get();
  if (!client) {
    LOG(ERROR) << "Error connecting to concierge. Client is NULL.";
    callbacks_.Notify(BruschettaResult::kStartVmFailed);
    return;
  }

  std::string user_hash =
      ash::ProfileHelper::GetUserIdHashFromProfile(profile_);
  vm_tools::concierge::StartVmRequest request;
  request.set_start_termina(false);
  request.set_name(vm_name_);
  *request.mutable_vm()->mutable_tools_dlc_id() = "termina-dlc";
  *request.mutable_owner_id() = user_hash;
  request.set_start_termina(false);
  request.set_timeout(240);
  base::ScopedFD fd(bios.TakePlatformFile());
  request.add_fds(vm_tools::concierge::StartVmRequest::BIOS);

  auto* disk = request.mutable_disks()->Add();
  *disk->mutable_path() =
      base::StrCat({"/run/daemon-store/crosvm/", user_hash, "/", kDiskName});
  disk->set_writable(true);
  disk->set_do_mount(false);

  client->StartVmWithFd(std::move(fd), request,
                        base::BindOnce(&BruschettaLauncher::OnStartVm,
                                       weak_factory_.GetWeakPtr()));
}

void BruschettaLauncher::OnStartVm(
    absl::optional<vm_tools::concierge::StartVmResponse> response) {
  if (!response) {
    LOG(ERROR) << "Error starting VM: no response from Concierge";
    callbacks_.Notify(BruschettaResult::kStartVmFailed);
    return;
  }

  if (response->status() != vm_tools::concierge::VM_STATUS_RUNNING &&
      response->status() != vm_tools::concierge::VM_STATUS_STARTING) {
    LOG(ERROR) << "Error starting VM, got status: " << response->status()
               << " and reason " << response->failure_reason();
    callbacks_.Notify(BruschettaResult::kStartVmFailed);
    return;
  }

  auto* tracker = guest_os::GuestOsSessionTracker::GetForProfile(profile_);
  subscription_ = tracker->RunOnceContainerStarted(
      guest_os::GuestId{guest_os::VmType::BRUSCHETTA, vm_name_, "penguin"},
      base::BindOnce(&BruschettaLauncher::OnContainerRunning,
                     weak_factory_.GetWeakPtr()));
}

void BruschettaLauncher::OnContainerRunning(guest_os::GuestInfo info) {
  callbacks_.Notify(BruschettaResult::kSuccess);
}

void BruschettaLauncher::OnTimeout() {
  // These are no-ops if empty so safe to always call.
  subscription_.reset();
  callbacks_.Notify(BruschettaResult::kTimeout);

  // We don't actually abort or cancel the launch, let it keep going in the
  // background in case it's really slow for some reason then the next time they
  // try it might succeed.
}

base::WeakPtr<BruschettaLauncher> BruschettaLauncher::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace bruschetta
