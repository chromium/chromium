// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/termina_installer.h"

#include <algorithm>
#include <memory>
#include <string_view>

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_dlc_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "components/component_updater/ash/component_manager_ash.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace crostini {

TerminaInstaller::TerminaInstaller() = default;
TerminaInstaller::~TerminaInstaller() = default;

void TerminaInstaller::CancelInstall() {
  // TODO(b/277835995): Tests demand concurrent installations despite that they
  // need to be mass cancelled here (which is probably unintended). Consider
  // switching to CachedCallback or similar.
  for (auto& installation : installations_) {
    installation->CancelGracefully();
  }
}

void TerminaInstaller::Install(
    base::OnceCallback<void(InstallResult)> callback) {
  // The Remove*IfPresent methods require an unowned UninstallResult pointer to
  // record their success/failure state. This has to be unowned so that in
  // Uninstall it can be accessed further down the callback chain, but here we
  // don't care about it, so we assign ownership of the pointer to a
  // base::DoNothing that will delete it once called.
  auto ptr = std::make_unique<UninstallResult>();
  auto* uninstall_result_ptr = ptr.get();
  auto remove_callback = base::BindOnce(
      [](std::unique_ptr<UninstallResult> ptr) {}, std::move(ptr));
  RemoveComponentIfPresent(std::move(remove_callback), uninstall_result_ptr);

  installations_.push_back(std::make_unique<guest_os::GuestOsDlcInstallation>(
      kCrostiniDlcName,
      base::BindOnce(&TerminaInstaller::OnInstallDlc,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      base::DoNothing()));
}

void TerminaInstaller::OnInstallDlc(
    base::OnceCallback<void(InstallResult)> callback,
    guest_os::GuestOsDlcInstallation::Result result) {
  if (result.has_value()) {
    dlc_id_ = kCrostiniDlcName;
    termina_location_ = result.value();
  }
  InstallResult response =
      result
          .transform_error([](guest_os::GuestOsDlcInstallation::Error err) {
            switch (err) {
              case guest_os::GuestOsDlcInstallation::Error::Cancelled:
                return InstallResult::Cancelled;
              case guest_os::GuestOsDlcInstallation::Error::Offline:
                LOG(ERROR)
                    << "Failed to install termina-dlc while offline, assuming "
                       "network issue.";
                return InstallResult::Offline;
              case guest_os::GuestOsDlcInstallation::Error::NeedUpdate:
              case guest_os::GuestOsDlcInstallation::Error::NeedReboot:
                LOG(ERROR) << "Failed to install termina-dlc because the OS "
                              "must be updated";
                return InstallResult::NeedUpdate;
              case guest_os::GuestOsDlcInstallation::Error::DiskFull:
              case guest_os::GuestOsDlcInstallation::Error::Busy:
              case guest_os::GuestOsDlcInstallation::Error::Internal:
              case guest_os::GuestOsDlcInstallation::Error::Invalid:
              case guest_os::GuestOsDlcInstallation::Error::UnknownFailure:
                LOG(ERROR) << "Failed to install termina-dlc: " << err;
                return InstallResult::Failure;
            }
          })
          .error_or(InstallResult::Success);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

void TerminaInstaller::Uninstall(base::OnceCallback<void(bool)> callback) {
  // Unset |termina_location_| now since it will become invalid at some point
  // soon.
  termina_location_ = std::nullopt;

  // This is really a vector of bool, but std::vector<bool> has weird properties
  // that stop us from using it in this way.
  std::vector<UninstallResult> partial_results{0, 0};
  UninstallResult* component_result = &partial_results[0];
  UninstallResult* dlc_result = &partial_results[1];

  // We want to get the results from both uninstall calls and combine them, and
  // the asynchronous nature of this process means we can't use return values,
  // so we need to pass a pointer into those calls to store their results and
  // pass ownership of that memory into the result callback.
  auto b_closure = BarrierClosure(
      2, base::BindOnce(&TerminaInstaller::OnUninstallFinished,
                        weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                        std::move(partial_results)));

  RemoveComponentIfPresent(b_closure, component_result);
  RemoveDlcIfPresent(b_closure, dlc_result);
}

void TerminaInstaller::RemoveComponentIfPresent(
    base::OnceCallback<void()> callback,
    UninstallResult* result) {
  VLOG(1) << "Removing component";
  scoped_refptr<component_updater::ComponentManagerAsh> component_manager =
      g_browser_process->platform_part()->component_manager_ash();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](scoped_refptr<component_updater::ComponentManagerAsh>
                 component_manager) {
            return component_manager->IsRegisteredMayBlock(
                imageloader::kTerminaComponentName);
          },
          std::move(component_manager)),
      base::BindOnce(
          [](base::OnceCallback<void()> callback, UninstallResult* result,
             bool is_present) {
            scoped_refptr<component_updater::ComponentManagerAsh>
                component_manager =
                    g_browser_process->platform_part()->component_manager_ash();
            if (is_present) {
              VLOG(1) << "Component present, unloading";
              *result =
                  component_manager->Unload(imageloader::kTerminaComponentName);
              if (!*result) {
                LOG(ERROR) << "Failed to remove cros-termina component";
              }
            } else {
              VLOG(1) << "No component present, skipping";
              *result = true;
            }
            std::move(callback).Run();
          },
          std::move(callback), result));
}

void TerminaInstaller::RemoveDlcIfPresent(base::OnceCallback<void()> callback,
                                          UninstallResult* result) {
  ash::DlcserviceClient::Get()->GetExistingDlcs(base::BindOnce(
      [](base::WeakPtr<TerminaInstaller> weak_this,
         base::OnceCallback<void()> callback, UninstallResult* result,
         std::string_view err,
         const dlcservice::DlcsWithContent& dlcs_with_content) {
        if (!weak_this) {
          return;
        }

        if (err != dlcservice::kErrorNone) {
          LOG(ERROR) << "Failed to list installed DLCs: " << err;
          *result = false;
          std::move(callback).Run();
          return;
        }
        for (const auto& dlc : dlcs_with_content.dlc_infos()) {
          if (dlc.id() == kCrostiniDlcName) {
            VLOG(1) << "DLC present, removing";
            weak_this->RemoveDlc(std::move(callback), result);
            return;
          }
        }
        VLOG(1) << "No DLC present, skipping";
        *result = true;
        std::move(callback).Run();
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), result));
}

void TerminaInstaller::RemoveDlc(base::OnceCallback<void()> callback,
                                 UninstallResult* result) {
  ash::DlcserviceClient::Get()->Uninstall(
      kCrostiniDlcName, base::BindOnce(
                            [](base::OnceCallback<void()> callback,
                               UninstallResult* result, std::string_view err) {
                              if (err == dlcservice::kErrorNone) {
                                VLOG(1) << "Removed DLC";
                                *result = true;
                              } else {
                                LOG(ERROR)
                                    << "Failed to remove termina-dlc: " << err;
                                *result = false;
                              }
                              std::move(callback).Run();
                            },
                            std::move(callback), result));
}

void TerminaInstaller::OnUninstallFinished(
    base::OnceCallback<void(bool)> callback,
    std::vector<UninstallResult> partial_results) {
  std::move(callback).Run(!base::Contains(partial_results, 0));
}

base::FilePath TerminaInstaller::GetInstallLocation() {
  CHECK(termina_location_)
      << "GetInstallLocation() called while termina not installed";
  return *termina_location_;
}

std::optional<std::string> TerminaInstaller::GetDlcId() {
  CHECK(termina_location_) << "GetDlcId() called while termina not installed";
  return dlc_id_;
}

}  // namespace crostini
