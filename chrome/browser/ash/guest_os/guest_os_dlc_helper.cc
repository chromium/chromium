// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_dlc_helper.h"

#include <string_view>

#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "content/public/browser/network_service_instance.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

namespace guest_os {

namespace {

// How long to wait between retry attempts.
constexpr base::TimeDelta kBetweenRetryDelay = base::Seconds(5);

// Maximum number of times the installation will retry before giving up.
constexpr int kMaxRetries = 5;

GuestOsDlcInstallation::Error ToError(const std::string& error) {
  if (error == dlcservice::kErrorInternal) {
    return GuestOsDlcInstallation::Error::Internal;
  } else if (error == dlcservice::kErrorBusy) {
    return GuestOsDlcInstallation::Error::Busy;
  } else if (error == dlcservice::kErrorNeedReboot) {
    return GuestOsDlcInstallation::Error::NeedReboot;
  } else if (error == dlcservice::kErrorInvalidDlc) {
    return GuestOsDlcInstallation::Error::Invalid;
  } else if (error == dlcservice::kErrorAllocation) {
    return GuestOsDlcInstallation::Error::DiskFull;
  } else if (error == dlcservice::kErrorNoImageFound) {
    // Actually this isn't always actionable with an update but it often is so
    // we advise it.
    return GuestOsDlcInstallation::Error::NeedUpdate;
  }
  // DLC records success the same way as failure, but this method should never
  // be called on success.
  CHECK(error != dlcservice::kErrorNone);
  LOG(ERROR) << "DLC Installation failed with unrecognized error: " << error;
  return GuestOsDlcInstallation::Error::UnknownFailure;
}

enum class Actionability {
  // Errors which we know will just happen again until the user does something.
  UserIntervention,
  // Errors which may not happen again so we automatically retry them.
  Retry,
  // Errors which can neither be actioned or retried by the user.
  None,
};

Actionability GetActionability(GuestOsDlcInstallation::Error err) {
  switch (err) {
    case GuestOsDlcInstallation::Error::Cancelled:
    case GuestOsDlcInstallation::Error::Offline:
    case GuestOsDlcInstallation::Error::NeedUpdate:
    case GuestOsDlcInstallation::Error::NeedReboot:
    case GuestOsDlcInstallation::Error::DiskFull:
      return Actionability::UserIntervention;
    case GuestOsDlcInstallation::Error::Busy:
    case GuestOsDlcInstallation::Error::Internal:
      return Actionability::Retry;
    case GuestOsDlcInstallation::Error::Invalid:
    case GuestOsDlcInstallation::Error::UnknownFailure:
      return Actionability::None;
  }
}

}  // namespace

GuestOsDlcInstallation::GuestOsDlcInstallation(
    std::string dlc_id,
    base::OnceCallback<void(Result)> completion_callback,
    ProgressCallback progress_callback)
    : dlc_id_(std::move(dlc_id)),
      retries_remaining_(kMaxRetries),
      completion_callback_(std::move(completion_callback)),
      progress_callback_(std::move(progress_callback)) {
  // This object represents the installation so begin that installation in
  // its constructor. First, check if the DLC is installed.
  CheckState();
}

GuestOsDlcInstallation::~GuestOsDlcInstallation() {
  if (completion_callback_) {
    std::move(completion_callback_).Run(base::unexpected(Error::Cancelled));
  }
}

void GuestOsDlcInstallation::CancelGracefully() {
  gracefully_cancelled_ = true;
  retries_remaining_ = 0;
}

void GuestOsDlcInstallation::CheckState() {
  ash::DlcserviceClient::Get()->GetDlcState(
      dlc_id_, base::BindOnce(&GuestOsDlcInstallation::OnGetDlcStateCompleted,
                              weak_factory_.GetWeakPtr()));
}

void GuestOsDlcInstallation::OnGetDlcStateCompleted(
    std::string_view err,
    const dlcservice::DlcState& dlc_state) {
  ash::DlcserviceClient::InstallResult result;
  switch (dlc_state.state()) {
    case dlcservice::DlcState::INSTALLED:
      result.dlc_id = dlc_state.id();
      result.root_path = dlc_state.root_path();
      result.error = dlcservice::kErrorNone;
      OnDlcInstallCompleted(result);
      break;
    case dlcservice::DlcState::NOT_INSTALLED:
      StartInstall();
      break;
    case dlcservice::DlcState::INSTALLING:
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&GuestOsDlcInstallation::CheckState,
                         weak_factory_.GetWeakPtr()),
          kBetweenRetryDelay);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void GuestOsDlcInstallation::StartInstall() {
  // Skip calling install if we've canceled.
  if (gracefully_cancelled_) {
    OnDlcInstallCompleted({});
    return;
  }
  dlcservice::InstallRequest install_request;
  install_request.set_id(dlc_id_);
  ash::DlcserviceClient::Get()->Install(
      install_request,
      base::BindOnce(&GuestOsDlcInstallation::OnDlcInstallCompleted,
                     weak_factory_.GetWeakPtr()),
      progress_callback_);
}

void GuestOsDlcInstallation::OnDlcInstallCompleted(
    const ash::DlcserviceClient::InstallResult& result) {
  if (gracefully_cancelled_) {
    std::move(completion_callback_).Run(base::unexpected(Error::Cancelled));
    return;
  }
  CHECK(result.dlc_id == dlc_id_);
  if (result.error == dlcservice::kErrorNone) {
    std::move(completion_callback_)
        .Run(base::ok(base::FilePath(result.root_path)));
    return;
  }

  Error err = ToError(result.error);

  switch (GetActionability(err)) {
    case Actionability::UserIntervention:
      std::move(completion_callback_).Run(base::unexpected(err));
      return;

    case Actionability::Retry:
      if (retries_remaining_ > 0) {
        --retries_remaining_;
        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&GuestOsDlcInstallation::StartInstall,
                           weak_factory_.GetWeakPtr()),
            kBetweenRetryDelay);
        return;
      }
      // If we're out of retries then we can't action this error ourselves.
      ABSL_FALLTHROUGH_INTENDED;

    case Actionability::None:
      // Unless we know the cause of an error (because it was actionable or we
      // have run out of retries), assume being offline causes errors.
      if (content::GetNetworkConnectionTracker()->IsOffline()) {
        err = Error::Offline;
      }

      std::move(completion_callback_).Run(base::unexpected(err));
      return;
  }
}

}  // namespace guest_os

std::ostream& operator<<(std::ostream& stream,
                         guest_os::GuestOsDlcInstallation::Error err) {
  switch (err) {
    case guest_os::GuestOsDlcInstallation::Error::Cancelled:
      return stream << "cancelled";
    case guest_os::GuestOsDlcInstallation::Error::Offline:
      return stream << "offline";
    case guest_os::GuestOsDlcInstallation::Error::NeedUpdate:
      return stream << "need update";
    case guest_os::GuestOsDlcInstallation::Error::NeedReboot:
      return stream << "need reboot";
    case guest_os::GuestOsDlcInstallation::Error::DiskFull:
      return stream << "disk full";
    case guest_os::GuestOsDlcInstallation::Error::Busy:
      return stream << "busy";
    case guest_os::GuestOsDlcInstallation::Error::Internal:
      return stream << "internal";
    case guest_os::GuestOsDlcInstallation::Error::Invalid:
      return stream << "invalid";
    case guest_os::GuestOsDlcInstallation::Error::UnknownFailure:
      return stream << "unknown";
  }
}
