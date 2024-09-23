// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_dlc_installer.h"

#include <string_view>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "services/screen_ai/public/cpp/utilities.h"

namespace {

using screen_ai::dlc_installer::DlcInstallResult;

constexpr char kScreenAIDlcName[] = "screen-ai";

// Retry delay will exponentially increase.
constexpr int kBaseRetryDelayInSeconds = 3;
constexpr int kMaxRetryDelayInSeconds = 180;
constexpr int kMaxInstallRetries = 5;
constexpr int kUninstallDelayInSeconds = 300;

struct InstallMetadata {
  int install_retries = 0;
  int retry_delay_in_seconds = kBaseRetryDelayInSeconds;
};

void RecordDlcInstallResult(std::string_view result_string) {
  DlcInstallResult result_enum = DlcInstallResult::kSuccess;

  if (result_string == dlcservice::kErrorNone) {
    result_enum = DlcInstallResult::kSuccess;
  } else if (result_string == dlcservice::kErrorInternal) {
    result_enum = DlcInstallResult::kErrorInternal;
  } else if (result_string == dlcservice::kErrorBusy) {
    result_enum = DlcInstallResult::kErrorBusy;
  } else if (result_string == dlcservice::kErrorNeedReboot) {
    result_enum = DlcInstallResult::kErrorNeedReboot;
  } else if (result_string == dlcservice::kErrorInvalidDlc) {
    result_enum = DlcInstallResult::kErrorInvalidDlc;
  } else if (result_string == dlcservice::kErrorAllocation) {
    result_enum = DlcInstallResult::kErrorAllocation;
  } else if (result_string == dlcservice::kErrorNoImageFound) {
    result_enum = DlcInstallResult::kErrorNoImageFound;
  } else {
    NOTREACHED() << "Unexpected error: " << result_string;
  }

  base::UmaHistogramEnumeration("Accessibility.ScreenAI.DlcInstallResult",
                                result_enum);
}

void InstallInternal(InstallMetadata metadata);

int CalculateNextDelayInSeconds(int delay_in_seconds) {
  return std::min(delay_in_seconds * delay_in_seconds, kMaxRetryDelayInSeconds);
}

void OnInstallCompleted(
    InstallMetadata metadata,
    const ash::DlcserviceClient::InstallResult& install_result) {
  if (install_result.error == dlcservice::kErrorBusy &&
      metadata.install_retries < kMaxInstallRetries) {
    VLOG(1) << "ScreenAI installation failed as DLC service is busy, retrying.";
    base::TimeDelta retry_delay =
        base::Seconds(metadata.retry_delay_in_seconds);

    metadata.retry_delay_in_seconds =
        CalculateNextDelayInSeconds(metadata.retry_delay_in_seconds);
    metadata.install_retries++;

    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&InstallInternal, metadata), retry_delay);
    return;
  }

  RecordDlcInstallResult(install_result.error);

  if (install_result.error != dlcservice::kErrorNone) {
    VLOG(0) << "ScreenAI installation failed: " << install_result.error;
    screen_ai::ScreenAIInstallState::GetInstance()->SetState(
        screen_ai::ScreenAIInstallState::State::kDownloadFailed);
    return;
  }

  VLOG(2) << "ScreenAI installation completed in path: "
          << install_result.root_path;
  if (!install_result.root_path.empty()) {
    screen_ai::ScreenAIInstallState::GetInstance()->SetComponentFolder(
        base::FilePath(install_result.root_path));
  }

  base::UmaHistogramCounts100("Accessibility.ScreenAI.Component.InstallRetries",
                              metadata.install_retries);
}

void OnInstallProgress(double progress) {
  screen_ai::ScreenAIInstallState::GetInstance()->SetDownloadProgress(progress);
}

// This function can be called only on a thread that can be blocked.
bool CheckIfDlcExistsOnNonUIThread() {
  return base::PathExists(screen_ai::GetComponentDir());
}

void InstallInternal(InstallMetadata metadata) {
  dlcservice::InstallRequest install_request;
  install_request.set_id(kScreenAIDlcName);
  ash::DlcserviceClient::Get()->Install(
      install_request, base::BindOnce(&OnInstallCompleted, metadata),
      base::BindRepeating(&OnInstallProgress));
}

void UninstallIfNotUsedAndAvailableOnDisk() {
  // If ScreenAIInstallState is not `kNotDownloaded`, it means that a client has
  // asked for this DLC and it should not be uninstalled.
  // Note that "Not downloaded" does not necessarily mean that the DLC is not
  // on disk, but just states that from ScreenAI point of view, no client
  // requested downloading it.
  if (screen_ai::ScreenAIInstallState::GetInstance()->get_state() !=
      screen_ai::ScreenAIInstallState::State::kNotDownloaded) {
    return;
  }

  // Checking if DLC exists on disk should be done on a non-UI thread, but
  // actual uninstall should be done on the same thread that called this
  // function.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&CheckIfDlcExistsOnNonUIThread),
      base::BindOnce([](bool dlc_exists) {
        if (dlc_exists) {
          screen_ai::dlc_installer::Uninstall();
        }
      }));
}

}  // namespace

namespace screen_ai::dlc_installer {

void Install() {
  screen_ai::ScreenAIInstallState::GetInstance()->SetState(
      screen_ai::ScreenAIInstallState::State::kDownloading);
  InstallMetadata metadata;
  InstallInternal(metadata);
}

void Uninstall() {
  ash::DlcserviceClient::Get()->Uninstall(
      kScreenAIDlcName, base::BindOnce([](std::string_view err) {
        if (err != dlcservice::kErrorNone) {
          VLOG(0) << "Unistall failed: " << err;
        }
      }));
}

void ManageInstallation(PrefService* local_state) {
  if (screen_ai::ScreenAIInstallState::ShouldInstall(local_state)) {
    Install();
    return;
  }

  // This function is run on browser startup. The DLC uninstallation will be
  // called after a delay, so that if a feature relies on it and has not yet
  // triggered it, it would have time to do it. This is specifically helpful for
  // tests. Note that uninstall should happen on the same thread that runs this
  // function.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&UninstallIfNotUsedAndAvailableOnDisk),
      base::Seconds(kUninstallDelayInSeconds));
}

int CalculateNextDelayInSecondsForTesting(int delay_in_seconds) {
  return CalculateNextDelayInSeconds(delay_in_seconds);
}

int base_retry_delay_in_seconds_for_testing() {
  return kBaseRetryDelayInSeconds;
}

int max_install_retries_for_testing() {
  return kMaxInstallRetries;
}

}  // namespace screen_ai::dlc_installer
