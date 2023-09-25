// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_dlc_installer.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "components/services/screen_ai/public/cpp/utilities.h"

namespace {

constexpr char kScreenAIDlcName[] = "screen-ai";

// Retry delay will exponentially increase.
constexpr int kBaseRetryDelayInSeconds = 3;
constexpr int kMaxRetryDelayInSeconds = 180;
constexpr int kMaxInstallRetries = 5;

struct InstallMetadata {
  bool dlc_available_from_before_this_session = false;
  int install_retries = 0;
  int retry_delay_in_seconds = kBaseRetryDelayInSeconds;
};

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

  // Record metric only for new installs.
  if (!metadata.dlc_available_from_before_this_session) {
    screen_ai::ScreenAIInstallState::RecordComponentInstallationResult(
        /*install=*/true,
        /*successful=*/install_result.error == dlcservice::kErrorNone);
  }

  if (install_result.error != dlcservice::kErrorNone) {
    VLOG(0) << "ScreenAI installation failed: " << install_result.error;
    screen_ai::ScreenAIInstallState::GetInstance()->SetState(
        screen_ai::ScreenAIInstallState::State::kFailed);
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

void OnUninstallCompleted(const std::string& err) {
  screen_ai::ScreenAIInstallState::RecordComponentInstallationResult(
      /*install=*/false,
      /*successful=*/err == dlcservice::kErrorNone);

  if (err != dlcservice::kErrorNone) {
    VLOG(0) << "Unistall failed: " << err;
  }
}

void OnInstallProgress(double progress) {
  screen_ai::ScreenAIInstallState::GetInstance()->SetDownloadProgress(progress);
}

void Uninstall() {
  ash::DlcserviceClient::Get()->Uninstall(
      kScreenAIDlcName, base::BindOnce(&OnUninstallCompleted));
}

// This function can be called only on a thread that can be blocked.
bool CheckIfDlcExistsOnNonUIThread() {
  return !screen_ai::GetLatestComponentBinaryPath().empty();
}

void InstallInternal(InstallMetadata metadata) {
  dlcservice::InstallRequest install_request;
  install_request.set_id(kScreenAIDlcName);
  ash::DlcserviceClient::Get()->Install(
      install_request, base::BindOnce(&OnInstallCompleted, metadata),
      base::BindRepeating(&OnInstallProgress));
}

}  // namespace

namespace screen_ai::dlc_installer {

void Install() {
  screen_ai::ScreenAIInstallState::GetInstance()->SetState(
      screen_ai::ScreenAIInstallState::State::kDownloading);

  // Need to know installation state for metrics.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&CheckIfDlcExistsOnNonUIThread),
      base::BindOnce([](bool dlc_exists) {
        InstallMetadata metadata;
        metadata.dlc_available_from_before_this_session = dlc_exists;
        InstallInternal(metadata);
      }));
}

void ManageInstallation(PrefService* local_state) {
  if (screen_ai::ScreenAIInstallState::ShouldInstall(local_state)) {
    Install();
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&CheckIfDlcExistsOnNonUIThread),
      base::BindOnce([](bool dlc_exists) {
        if (dlc_exists) {
          Uninstall();
        }
      }));
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
