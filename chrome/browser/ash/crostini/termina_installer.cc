// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/termina_installer.h"

#include <algorithm>
#include <memory>

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace crostini {

TerminaInstaller::TerminaInstaller() = default;
TerminaInstaller::~TerminaInstaller() = default;

void TerminaInstaller::CancelInstall() {
  is_cancelled_ = true;
}

void TerminaInstaller::Install(base::OnceCallback<void(InstallResult)> callback,
                               bool is_initial_install) {
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

  InstallDlc(std::move(callback), is_initial_install);
}

void TerminaInstaller::InstallDlc(
    base::OnceCallback<void(InstallResult)> callback,
    bool is_initial_install) {
  dlcservice::InstallRequest install_request;
  install_request.set_id(kCrostiniDlcName);
  ash::DlcserviceClient::Get()->Install(
      install_request,
      base::BindOnce(&TerminaInstaller::OnInstallDlc,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     is_initial_install),
      base::DoNothing());
}

void TerminaInstaller::OnInstallDlc(
    base::OnceCallback<void(InstallResult)> callback,
    bool is_initial_install,
    const ash::DlcserviceClient::InstallResult& result) {
  CHECK(result.dlc_id == kCrostiniDlcName);
  InstallResult response;
  if (is_cancelled_) {
    response = InstallResult::Cancelled;
  } else if (result.error == dlcservice::kErrorNone) {
    response = InstallResult::Success;
    dlc_id_ = kCrostiniDlcName;
    termina_location_ = base::FilePath(result.root_path);
  } else if (is_initial_install && result.error == dlcservice::kErrorBusy) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TerminaInstaller::RetryInstallDlc,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       is_initial_install),
        base::Seconds(5));
    return;
  } else if (result.error == dlcservice::kErrorNeedReboot ||
             result.error == dlcservice::kErrorNoImageFound) {
    LOG(ERROR)
        << "Failed to install termina-dlc because the OS must be updated";
    response = InstallResult::NeedUpdate;
  } else {
    if (content::GetNetworkConnectionTracker()->IsOffline()) {
      LOG(ERROR) << "Failed to install termina-dlc while offline, assuming "
                    "network issue: "
                 << result.error;
      response = InstallResult::Offline;
    } else {
      LOG(ERROR) << "Failed to install termina-dlc: " << result.error;
      response = InstallResult::Failure;
    }
  }

  is_cancelled_ = false;
  std::move(callback).Run(response);
}

void TerminaInstaller::RetryInstallDlc(
    base::OnceCallback<void(InstallResult)> callback,
    bool is_initial_install) {
  if (is_cancelled_) {
    is_cancelled_ = false;
    std::move(callback).Run(InstallResult::Cancelled);
    return;
  }
  InstallDlc(std::move(callback), is_initial_install);
}

void TerminaInstaller::Uninstall(base::OnceCallback<void(bool)> callback) {
  // Unset |termina_location_| now since it will become invalid at some point
  // soon.
  termina_location_ = absl::nullopt;

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
  scoped_refptr<component_updater::CrOSComponentManager> component_manager =
      g_browser_process->platform_part()->cros_component_manager();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](scoped_refptr<component_updater::CrOSComponentManager>
                 component_manager) {
            return component_manager->IsRegisteredMayBlock(
                imageloader::kTerminaComponentName);
          },
          std::move(component_manager)),
      base::BindOnce(
          [](base::OnceCallback<void()> callback, UninstallResult* result,
             bool is_present) {
            scoped_refptr<component_updater::CrOSComponentManager>
                component_manager = g_browser_process->platform_part()
                                        ->cros_component_manager();
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
         const std::string& err,
         const dlcservice::DlcsWithContent& dlcs_with_content) {
        if (!weak_this)
          return;

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
      kCrostiniDlcName,
      base::BindOnce(
          [](base::OnceCallback<void()> callback, UninstallResult* result,
             const std::string& err) {
            if (err == dlcservice::kErrorNone) {
              VLOG(1) << "Removed DLC";
              *result = true;
            } else {
              LOG(ERROR) << "Failed to remove termina-dlc: " << err;
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

absl::optional<std::string> TerminaInstaller::GetDlcId() {
  CHECK(termina_location_) << "GetDlcId() called while termina not installed";
  return dlc_id_;
}

}  // namespace crostini
