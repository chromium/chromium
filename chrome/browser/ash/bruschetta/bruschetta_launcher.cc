// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_launcher.h"

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/guest_os/guest_os_dlc_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "content/public/browser/browser_thread.h"

namespace bruschetta {

namespace {

// TODO(b/233289313): Once we have an installer and multiple Bruschettas this
// needs to be dynamic, but for now we hardcode the same path that the go/brua
// instructions have people using for the alpha, and the same disk name that
// people following the instructions will have (base64 encoded "bru").
const char kDiskName[] = "YnJ1.img";

}  // namespace

// Wrapper class for dispatching `OnTimeout` calls so we can cancel the timeout
// by destroying the wrapper instance.
class BruschettaLauncher::Timeout {
 public:
  explicit Timeout(BruschettaLauncher* launcher) : launcher_(launcher) {}
  void OnTimeout() { launcher_->OnTimeout(); }

  // BruschettaLauncher owns us so it will always outlive us, hence raw_ptr is
  // fine.
  raw_ptr<BruschettaLauncher> launcher_;

  // Must be last.
  base::WeakPtrFactory<Timeout> weak_factory_{this};
};

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
    EnsureToolsDlcInstalled();
    timeout_ = std::make_unique<Timeout>(this);
    // If we're not complete after 4 minutes time out the entire launch.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&BruschettaLauncher::Timeout::OnTimeout,
                       timeout_->weak_factory_.GetWeakPtr()),
        base::Seconds(240));
  }
}

void BruschettaLauncher::EnsureToolsDlcInstalled() {
  in_progress_dlc_ = std::make_unique<guest_os::GuestOsDlcInstallation>(
      kToolsDlc,
      base::BindOnce(&BruschettaLauncher::OnMountToolsDlc,
                     weak_factory_.GetWeakPtr()),
      base::DoNothing());
}

void BruschettaLauncher::OnMountToolsDlc(
    guest_os::GuestOsDlcInstallation::Result install_result) {
  in_progress_dlc_.reset();
  if (!install_result.has_value()) {
    LOG(ERROR) << "Error installing tools DLC: " << install_result.error();
    Finish(BruschettaResult::kDlcInstallError);
    return;
  }

  EnsureFirmwareDlcInstalled();
}

void BruschettaLauncher::EnsureFirmwareDlcInstalled() {
  in_progress_dlc_ = std::make_unique<guest_os::GuestOsDlcInstallation>(
      kUefiDlc,
      base::BindOnce(&BruschettaLauncher::OnMountFirmwareDlc,
                     weak_factory_.GetWeakPtr()),
      base::DoNothing());
}

void BruschettaLauncher::OnMountFirmwareDlc(
    guest_os::GuestOsDlcInstallation::Result install_result) {
  if (!install_result.has_value()) {
    LOG(ERROR) << "Error installing firmware DLC: " << install_result.error();
    Finish(BruschettaResult::kDlcInstallError);
    return;
  }

  EnsureConciergeAvailable();
}

void BruschettaLauncher::EnsureConciergeAvailable() {
  auto* client = ash::ConciergeClient::Get();
  if (!client) {
    LOG(ERROR) << "Error connecting to concierge. Client is NULL.";
    Finish(BruschettaResult::kConciergeUnavailable);
    return;
  }

  client->WaitForServiceToBeAvailable(base::BindOnce(
      &BruschettaLauncher::OnConciergeAvailable, weak_factory_.GetWeakPtr()));
}

void BruschettaLauncher::OnConciergeAvailable(bool service_is_available) {
  if (!service_is_available) {
    LOG(ERROR) << "Error connecting to concierge. Service is not available.";
    Finish(BruschettaResult::kConciergeUnavailable);
    return;
  }

  StartVm();
}

void BruschettaLauncher::StartVm() {
  auto* client = ash::ConciergeClient::Get();
  if (!client) {
    LOG(ERROR) << "Error connecting to concierge. Client is NULL.";
    Finish(BruschettaResult::kStartVmFailed);
    return;
  }

  const std::string config_id =
      GetContainerPrefValue(profile_, MakeBruschettaId(vm_name_),
                            guest_os::prefs::kBruschettaConfigId)
          ->GetString();
  RunningVmPolicy launch_policy;
  auto opt = GetLaunchPolicyForConfig(profile_, config_id);
  if (!opt.has_value()) {
    // Policy prohibits starting the VM, so don't.
    LOG(ERROR) << "Starting VM prohibited by policy";
    Finish(BruschettaResult::kForbiddenByPolicy);
    return;
  } else {
    launch_policy = *opt;
  }

  std::string user_hash =
      ash::ProfileHelper::GetUserIdHashFromProfile(profile_);
  std::string vm_username = GetVmUsername(profile_);
  vm_tools::concierge::StartVmRequest request;
  request.set_start_termina(false);
  request.set_name(vm_name_);
  request.mutable_vm()->set_tools_dlc_id(kToolsDlc);
  request.mutable_vm()->set_bios_dlc_id(kUefiDlc);
  request.set_owner_id(user_hash);
  request.set_vm_username(vm_username);
  request.set_start_termina(false);
  request.set_timeout(240);
  request.set_vtpm_proxy(launch_policy.vtpm_enabled);

  auto* disk = request.mutable_disks()->Add();
  *disk->mutable_path() =
      base::StrCat({"/run/daemon-store/crosvm/", user_hash, "/", kDiskName});
  disk->set_writable(true);
  disk->set_do_mount(false);

  client->StartVm(request,
                  base::BindOnce(&BruschettaLauncher::OnStartVm,
                                 weak_factory_.GetWeakPtr(), launch_policy));
}

void BruschettaLauncher::OnStartVm(
    RunningVmPolicy launch_policy,
    std::optional<vm_tools::concierge::StartVmResponse> response) {
  if (!response || !response->success()) {
    if (response) {
      LOG(ERROR) << "Error starting VM, got status: " << response->status()
                 << " and reason " << response->failure_reason();
    } else {
      LOG(ERROR) << "Error starting VM: no response from Concierge";
    }
    Finish(BruschettaResult::kStartVmFailed);
    return;
  }

  BruschettaServiceFactory::GetForProfile(profile_)->RegisterVmLaunch(
      vm_name_, launch_policy);

  auto* tracker =
      guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile_);
  subscription_ = tracker->RunOnceContainerStarted(
      guest_os::GuestId{guest_os::VmType::BRUSCHETTA, vm_name_, "penguin"},
      base::BindOnce(&BruschettaLauncher::OnContainerRunning,
                     weak_factory_.GetWeakPtr()));
}

void BruschettaLauncher::OnContainerRunning(guest_os::GuestInfo info) {
  Finish(BruschettaResult::kSuccess);
}

void BruschettaLauncher::OnTimeout() {
  subscription_.reset();
  Finish(BruschettaResult::kTimeout);

  // We don't actually abort or cancel the launch, let it keep going in the
  // background in case it's really slow for some reason then the next time they
  // try it might succeed.
}

void BruschettaLauncher::Finish(BruschettaResult result) {
  base::UmaHistogramEnumeration("Bruschetta.LaunchResult", result);
  callbacks_.Notify(result);
  timeout_.reset();
}

base::WeakPtr<BruschettaLauncher> BruschettaLauncher::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace bruschetta
