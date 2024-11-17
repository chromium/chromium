// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_terminal_provider.h"

#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_types.mojom-shared.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/extensions/api/terminal/startup_status.h"
#include "chrome/browser/ui/views/crostini/crostini_recovery_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace crostini {
using mojom::InstallerState;

namespace {
// Counts from chrome/browser/ash/crostini/crostini_types.mojom.
const int kMaxStage = static_cast<int>(InstallerState::kConfigureContainer) + 1;
}  // namespace

// Displays startup status to the crostini terminal.
class CrostiniStartupStatus : public crostini::CrostiniManager::RestartObserver,
                              public extensions::StartupStatus {
 public:
  explicit CrostiniStartupStatus(
      std::unique_ptr<extensions::StartupStatusPrinter> printer)
      : StartupStatus(std::move(printer), kMaxStage) {}
  ~CrostiniStartupStatus() override = default;

  // crostini::CrostiniManager::RestartObserver
  void OnStageStarted(crostini::mojom::InstallerState stage) override {
    int stage_index = static_cast<int>(stage);
    static base::NoDestructor<base::flat_map<InstallerState, std::string>>
        kStartStrings({
            {InstallerState::kStart,
             l10n_util::GetStringUTF8(IDS_CROSTINI_TERMINAL_STATUS_START)},
            {InstallerState::kInstallImageLoader,
             l10n_util::GetStringUTF8(
                 IDS_CROSTINI_TERMINAL_STATUS_INSTALL_IMAGE_LOADER)},
            {InstallerState::kCreateDiskImage,
             l10n_util::GetStringUTF8(
                 IDS_CROSTINI_TERMINAL_STATUS_CREATE_DISK_IMAGE)},
            {InstallerState::kStartTerminaVm,
             l10n_util::GetStringUTF8(
                 IDS_CROSTINI_TERMINAL_STATUS_START_TERMINA_VM)},
            {InstallerState::kStartLxd,
             l10n_util::GetStringUTF8(IDS_CROSTINI_TERMINAL_STATUS_START_LXD)},
            {InstallerState::kCreateContainer,
             l10n_util::GetStringUTF8(
                 IDS_CROSTINI_TERMINAL_STATUS_CREATE_CONTAINER)},
            {InstallerState::kSetupContainer,
             l10n_util::GetStringUTF8(
                 IDS_CROSTINI_TERMINAL_STATUS_SETUP_CONTAINER)},
            {InstallerState::kStartContainer,
             l10n_util::GetStringUTF8(
                 IDS_CROSTINI_TERMINAL_STATUS_START_CONTAINER)},
            {InstallerState::kConfigureContainer,
             l10n_util::GetStringUTF8(
                 IDS_CROSTINI_TERMINAL_STATUS_CONFIGURE_CONTAINER)},
        });
    const std::string& stage_string = (*kStartStrings)[stage];
    // Ensure we have a valid string for each stage.
    DCHECK(!stage_string.empty());
    printer()->PrintStage(stage_index, stage_string);
  }
};

CrostiniTerminalProvider::CrostiniTerminalProvider(
    Profile* profile,
    guest_os::GuestId container_id)
    : profile_(profile), container_id_(container_id) {}
CrostiniTerminalProvider::~CrostiniTerminalProvider() = default;

std::string CrostiniTerminalProvider::Label() {
  return crostini::FormatForUi(container_id_);
}

guest_os::GuestId CrostiniTerminalProvider::GuestId() {
  return container_id_;
}

bool CrostiniTerminalProvider::RecoveryRequired(int64_t display_id) {
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile_);
  if (crostini_manager->IsUncleanStartup()) {
    ShowCrostiniRecoveryView(profile_, crostini::CrostiniUISurface::kAppList,
                             guest_os::kTerminalSystemAppId, display_id, {},
                             base::DoNothing());
    return true;
  }
  return false;
}

bool CrostiniTerminalProvider::AllowedByPolicy() {
  return CrostiniFeatures::Get()->IsAllowedNow(profile_);
}

std::string CrostiniTerminalProvider::PrepareCwd(storage::FileSystemURL url) {
  std::string cwd;
  CrostiniManager::RestartOptions options;
  auto* share_path = guest_os::GuestOsSharePathFactory::GetForProfile(profile_);
  base::FilePath path;
  if (file_manager::util::ConvertFileSystemURLToPathInsideCrostini(
          profile_, url, &path)) {
    cwd = path.value();
    const auto& fs_id = url.mount_filesystem_id();
    auto mount_name =
        // TODO(b/217469540): Currently the default Crostini container gets
        // mounted in a different location to other Guest OS mounts, as we get
        // consistent file sharing across Guest OSs we can remove this special
        // case.
        (container_id_ == DefaultContainerId())
            ? file_manager::util::GetCrostiniMountPointName(profile_)
            : file_manager::util::GetGuestOsMountPointName(profile_,
                                                           container_id_);
    if (fs_id != mount_name &&
        !share_path->IsPathShared(container_id_.vm_name, url.path())) {
      // Path isn't already shared, so share it.
      options.share_paths.push_back(url.path());
    }
  } else {
    LOG(WARNING) << "Failed to parse: " << path << ". Not setting terminal cwd";
    return "";
  }
  // This completes async, but we don't wait for it since the terminal itself
  // also calls RestartCrostini and that'll get serialised, ensuring that this
  // call has completed before the share gets used.
  CrostiniManager::GetForProfile(profile_)->RestartCrostiniWithOptions(
      container_id_, std::move(options), base::DoNothing());
  return cwd;
}

std::unique_ptr<extensions::StartupStatus>
CrostiniTerminalProvider::CreateStartupStatus(
    std::unique_ptr<extensions::StartupStatusPrinter> printer) {
  return std::make_unique<CrostiniStartupStatus>(std::move(printer));
}

void CrostiniTerminalProvider::EnsureRunning(
    extensions::StartupStatus* startup_status,
    base::OnceCallback<void(bool success, std::string failure_reason)>
        callback) {
  CrostiniManager::GetForProfile(profile_)->RestartCrostini(
      container_id_,
      base::BindOnce(
          [](base::OnceCallback<void(bool, std::string)> callback,
             CrostiniResult result) {
            if (result == CrostiniResult::SUCCESS) {
              std::move(callback).Run(true, "");
            } else {
              crostini::RecordAppLaunchResultHistogram(
                  crostini::CrostiniAppLaunchAppType::kTerminal, result);
              std::move(callback).Run(
                  false,
                  base::StringPrintf(
                      "Error starting crostini for terminal: %d (%s)",
                      static_cast<int>(result), CrostiniResultString(result)));
            }
          },
          std::move(callback)),
      // Downcast is safe because terminal_private_api.cc will only call us with
      // the startup status we created in CreateStartupStatus.
      static_cast<CrostiniStartupStatus*>(startup_status));
}

}  // namespace crostini
