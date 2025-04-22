// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_installer.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/syslog_logging.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_external_loader_broker.h"
#include "chrome/browser/chromeos/app_mode/startup_app_launcher_update_checker.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pending_extension_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"

namespace chromeos {

namespace {

const std::string_view kChromeKioskExtensionUpdateErrorHistogram =
    "Kiosk.ChromeApp.ExtensionUpdateError";

const std::string_view kChromeKioskExtensionHasUpdateDurationHistogram =
    "Kiosk.ChromeApp.ExtensionUpdateDuration.HasUpdate";

const std::string_view kChromeKioskExtensionNoUpdateDurationHistogram =
    "Kiosk.ChromeApp.ExtensionUpdateDuration.NoUpdate";

// Returns true if the app with `id` is pending an install or update.
bool IsExtensionInstallPending(content::BrowserContext& browser_context,
                               const std::string& id) {
  return extensions::PendingExtensionManager::Get(&browser_context)
      ->IsIdPending(id);
}

// Returns the `Extension` corresponding to `id` installed in `browser_context`,
// or null if the extension is not installed.
const extensions::Extension* FindInstalledExtension(
    content::BrowserContext& browser_context,
    const extensions::ExtensionId& id) {
  return extensions::ExtensionRegistry::Get(&browser_context)
      ->GetInstalledExtension(id);
}

// Returns true if the extension with `id` is installed in `browser_context`.
bool IsExtensionInstalled(content::BrowserContext& browser_context,
                          const extensions::ExtensionId& id) {
  return FindInstalledExtension(browser_context, id) != nullptr;
}

// Returns true if all extensions in `ids` are installed in `browser_context`.
bool AreExtensionsInstalled(content::BrowserContext& browser_context,
                            const std::vector<extensions::ExtensionId>& ids) {
  return std::ranges::all_of(ids.begin(), ids.end(),
                             [&browser_context](const auto& id) {
                               return IsExtensionInstalled(browser_context, id);
                             });
}

// Returns true if the secondary apps of `app` contain the given `id`.
bool SecondaryAppsContain(const extensions::Extension& app,
                          const extensions::ExtensionId& id) {
  if (auto* info = extensions::KioskModeInfo::Get(&app); info != nullptr) {
    auto it = std::ranges::find(info->secondary_apps, id,
                                [](const auto& app) { return app.id; });
    return it != info->secondary_apps.end();
  }

  return false;
}

// Returns the IDs of the secondary apps of `app`.
std::vector<extensions::ExtensionId> SecondaryAppIdsOf(
    const extensions::Extension& app) {
  std::vector<extensions::ExtensionId> result;
  if (auto* info = extensions::KioskModeInfo::Get(&app); info != nullptr) {
    std::ranges::transform(info->secondary_apps, std::back_inserter(result),
                           [](const auto& app) { return app.id; });
  }
  return result;
}

// Returns the subset of `app_ids` that is pending install or update.
std::vector<extensions::ExtensionId> CopyIdsPendingInstall(
    content::BrowserContext& browser_context,
    const std::vector<extensions::ExtensionId>& app_ids) {
  std::vector<extensions::ExtensionId> result;
  std::ranges::copy_if(app_ids, std::back_inserter(result),
                       [&browser_context](const auto& id) {
                         return IsExtensionInstallPending(browser_context, id);
                       });
  return result;
}

// Inserts the shared modules `extension` imports into the set of `ids` that are
// pending install.
void InsertPendingSharedModules(content::BrowserContext& browser_context,
                                base::flat_set<extensions::ExtensionId>& ids,
                                const extensions::Extension& extension) {
  const auto& imports = extensions::SharedModuleInfo::GetImports(&extension);
  for (const auto& import_info : imports) {
    if (IsExtensionInstallPending(browser_context, import_info.extension_id)) {
      ids.insert(import_info.extension_id);
    }
  }
}

}  // namespace

ChromeKioskAppInstaller::ChromeKioskAppInstaller(
    Profile* profile,
    const AppInstallParams& install_data)
    : profile_(CHECK_DEREF(profile)), primary_app_install_data_(install_data) {}

ChromeKioskAppInstaller::~ChromeKioskAppInstaller() = default;

void ChromeKioskAppInstaller::BeginInstall(InstallCallback callback) {
  DCHECK(!install_complete_);

  SYSLOG(INFO) << "BeginInstall primary app id: " << primary_app_id();

  on_ready_callback_ = std::move(callback);

  extensions::file_util::SetUseSafeInstallation(true);

  const auto* primary_app =
      FindInstalledExtension(profile_.get(), primary_app_id());

  if (primary_app_install_data_.crx_file_location.empty() &&
      primary_app == nullptr) {
    ReportInstallFailure(InstallResult::kPrimaryAppNotCached);
    return;
  }

  ChromeKioskExternalLoaderBroker::Get()->TriggerPrimaryAppInstall(
      primary_app_install_data_);
  if (IsExtensionInstallPending(profile_.get(), primary_app_id())) {
    ObserveInstallations({primary_app_id()});
    return;
  }

  if (primary_app == nullptr) {
    // The extension is skipped for installation due to some error.
    ReportInstallFailure(InstallResult::kPrimaryAppInstallFailed);
    return;
  }

  if (!extensions::KioskModeInfo::IsKioskEnabled(primary_app)) {
    // The installed primary app is not kiosk enabled.
    ReportInstallFailure(InstallResult::kPrimaryAppNotKioskEnabled);
    return;
  }

  // Install secondary apps.
  MaybeInstallSecondaryApps(*primary_app);
}

void ChromeKioskAppInstaller::MaybeInstallSecondaryApps(
    const extensions::Extension& primary_app) {
  if (install_complete_) {
    return;
  }

  secondary_apps_installing_ = true;

  auto secondary_app_ids = SecondaryAppIdsOf(primary_app);

  ChromeKioskExternalLoaderBroker::Get()->UpdateSecondaryAppList(
      secondary_app_ids);

  if (!secondary_app_ids.empty()) {
    auto pending_ids = CopyIdsPendingInstall(profile_.get(), secondary_app_ids);
    if (!pending_ids.empty()) {
      ObserveInstallations(pending_ids);
      return;
    }
  }

  if (!AreExtensionsInstalled(profile_.get(), secondary_app_ids)) {
    ReportInstallFailure(InstallResult::kSecondaryAppInstallFailed);
    return;
  }

  // Check extension update before launching the primary kiosk app.
  MaybeCheckExtensionUpdate();
}

void ChromeKioskAppInstaller::MaybeCheckExtensionUpdate() {
  DCHECK(!install_complete_);

  SYSLOG(INFO) << "MaybeCheckExtensionUpdate";

  // Record update start time to calculate time consumed by update check. When
  // `OnExtensionUpdateCheckFinished` is called the update is already finished
  // because `extensions::ExtensionUpdater::CheckParams::install_immediately` is
  // set to true.
  extension_update_start_time_ = base::Time::Now();

  // Observe installation failures.
  install_stage_observation_.Observe(
      extensions::InstallStageTracker::Get(&profile_.get()));

  // Enforce an immediate version update check for all extensions before
  // launching the primary app. After the chromeos is updated, the shared
  // module (e.g. ARC runtime) may need to be updated to a newer version
  // compatible with the new chromeos. See crbug.com/555083.
  update_checker_ =
      std::make_unique<StartupAppLauncherUpdateChecker>(&profile_.get());
  if (!update_checker_->Run(base::BindOnce(
          &ChromeKioskAppInstaller::OnExtensionUpdateCheckFinished,
          weak_ptr_factory_.GetWeakPtr()))) {
    update_checker_.reset();
    install_stage_observation_.Reset();
    SYSLOG(WARNING) << "Could not check extension updates";
    FinalizeAppInstall();
    return;
  }

  SYSLOG(INFO) << "Checking extension updates";
}

void ChromeKioskAppInstaller::OnExtensionUpdateCheckFinished(
    bool update_found) {
  DCHECK(!install_complete_);

  SYSLOG(INFO) << "OnExtensionUpdateCheckFinished";
  update_checker_.reset();
  install_stage_observation_.Reset();
  if (update_found) {
    SYSLOG(INFO) << "Reloading extension with id " << primary_app_id();

    // Reload the primary app to make sure any reference to the previous version
    // of the shared module, extension, etc will be cleaned up and the new
    // version will be loaded.
    extensions::ExtensionRegistrar::Get(&profile_.get())
        ->ReloadExtension(primary_app_id());

    SYSLOG(INFO) << "Reloaded extension with id " << primary_app_id();
  }

  base::UmaHistogramMediumTimes(
      update_found ? kChromeKioskExtensionHasUpdateDurationHistogram
                   : kChromeKioskExtensionNoUpdateDurationHistogram,
      base::Time::Now() - extension_update_start_time_);

  FinalizeAppInstall();
}

void ChromeKioskAppInstaller::FinalizeAppInstall() {
  DCHECK(!install_complete_);

  install_complete_ = true;

  if (primary_app_update_failed_) {
    ReportInstallFailure(
        ChromeKioskAppInstaller::InstallResult::kPrimaryAppUpdateFailed);
  } else if (secondary_app_update_failed_) {
    ReportInstallFailure(
        ChromeKioskAppInstaller::InstallResult::kSecondaryAppUpdateFailed);
  } else {
    ReportInstallSuccess();
  }
}

void ChromeKioskAppInstaller::OnFinishCrxInstall(
    content::BrowserContext* context,
    const base::FilePath& source_file,
    const std::string& extension_id,
    const extensions::Extension* extension,
    bool success) {
  DCHECK(!install_complete_);

  SYSLOG(INFO) << (success ? "OnFinishCrxInstall succeeded for id: "
                           : "OnFinishCrxInstall failed for id: ")
               << extension_id;

  // Exit early if this is not one of the IDs we care about.
  if (!waiting_ids_.contains(extension_id)) {
    return;
  }
  waiting_ids_.erase(extension_id);

  // Also wait for updates on any shared modules the extension imports.
  if (extension != nullptr) {
    InsertPendingSharedModules(profile_.get(), waiting_ids_, *extension);
  }

  const auto* primary_app =
      FindInstalledExtension(profile_.get(), primary_app_id());

  if (!success) {
    // Primary or secondary app install failed. Abort and report the failure.
    if (primary_app == nullptr && extension_id == primary_app_id()) {
      install_observation_.Reset();
      ReportInstallFailure(InstallResult::kPrimaryAppInstallFailed);
      return;
    }
    if (primary_app != nullptr &&
        SecondaryAppsContain(*primary_app, extension_id) &&
        !IsExtensionInstalled(profile_.get(), extension_id)) {
      install_observation_.Reset();
      ReportInstallFailure(InstallResult::kSecondaryAppInstallFailed);
      return;
    }
    // Primary or secondary app update failed, but there is an installed version
    // in the `profile_`. Proceed for now and report the update failure later.
    if (primary_app != nullptr && extension_id == primary_app_id()) {
      primary_app_update_failed_ = true;
    }
    if (primary_app != nullptr &&
        SecondaryAppsContain(*primary_app, extension_id) &&
        IsExtensionInstalled(profile_.get(), extension_id)) {
      secondary_app_update_failed_ = true;
    }
  }

  // Wait for `OnFinishCrxInstall` to be called for remaining `waiting_ids_`.
  if (!waiting_ids_.empty()) {
    return;
  }

  install_observation_.Reset();

  if (primary_app == nullptr) {
    ReportInstallFailure(InstallResult::kPrimaryAppInstallFailed);
    return;
  }

  if (!extensions::KioskModeInfo::IsKioskEnabled(primary_app)) {
    ReportInstallFailure(InstallResult::kPrimaryAppNotKioskEnabled);
    return;
  }

  if (!secondary_apps_installing_) {
    MaybeInstallSecondaryApps(*primary_app);
  } else {
    MaybeCheckExtensionUpdate();
  }
}

void ChromeKioskAppInstaller::OnExtensionInstallationFailed(
    const extensions::ExtensionId& id,
    extensions::InstallStageTracker::FailureReason reason) {
  base::UmaHistogramEnumeration(kChromeKioskExtensionUpdateErrorHistogram,
                                reason);
}

void ChromeKioskAppInstaller::ReportInstallSuccess() {
  DCHECK(install_complete_);
  SYSLOG(INFO) << "Kiosk app install succeeded";

  std::move(on_ready_callback_)
      .Run(ChromeKioskAppInstaller::InstallResult::kSuccess);
}

void ChromeKioskAppInstaller::ReportInstallFailure(
    ChromeKioskAppInstaller::InstallResult error) {
  SYSLOG(ERROR) << "App install failed, error: " << static_cast<int>(error);
  DCHECK_NE(ChromeKioskAppInstaller::InstallResult::kSuccess, error);

  std::move(on_ready_callback_).Run(error);
}

void ChromeKioskAppInstaller::ObserveInstallations(
    const std::vector<extensions::ExtensionId>& ids) {
  waiting_ids_.insert(ids.begin(), ids.end());
  install_observation_.Observe(
      extensions::InstallTrackerFactory::GetForBrowserContext(&profile_.get()));
}

}  // namespace chromeos
