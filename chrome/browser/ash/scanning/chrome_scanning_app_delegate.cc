// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/chrome_scanning_app_delegate.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/web_app_id_constants.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/scanning/scan_service.h"
#include "chrome/browser/ash/scanning/scan_service_factory.h"
#include "chrome/browser/ash/scanning/scanning_file_path_helper.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/window_open_disposition.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace ash {

namespace {

// The name of the sticky settings pref.
constexpr char kScanningStickySettingsPref[] =
    "scanning.scanning_sticky_settings";

}  // namespace

ChromeScanningAppDelegate::ChromeScanningAppDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  const base::FilePath my_files_path =
      file_manager::util::GetMyFilesFolderForProfile(
          Profile::FromWebUI(web_ui));

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(
          Profile::FromWebUI(web_ui_));
  const bool drive_available = integration_service &&
                               integration_service->is_enabled() &&
                               integration_service->IsMounted();
  const base::FilePath google_drive_path =
      drive_available ? integration_service->GetMountPointPath()
                      : base::FilePath();
  SetValidPaths(google_drive_path, my_files_path);
}

ChromeScanningAppDelegate::~ChromeScanningAppDelegate() = default;

std::unique_ptr<ui::SelectFilePolicy>
ChromeScanningAppDelegate::CreateChromeSelectFilePolicy() {
  return std::make_unique<ChromeSelectFilePolicy>(web_ui_->GetWebContents());
}

std::string ChromeScanningAppDelegate::GetBaseNameFromPath(
    const base::FilePath& path) {
  return file_path_helper_.GetBaseNameFromPath(path);
}

base::FilePath ChromeScanningAppDelegate::GetMyFilesPath() {
  return file_path_helper_.GetMyFilesPath();
}

std::string ChromeScanningAppDelegate::GetScanSettingsFromPrefs() {
  return GetPrefs()->GetString(kScanningStickySettingsPref);
}

// Determines if |path_to_file| is a supported file path for the Files app. Only
// files under the |drive_path_| and |my_files_path_| paths are allowed to be
// opened to from the Scan app. Paths with references (i.e. "../path") are not
// supported.
bool ChromeScanningAppDelegate::IsFilePathSupported(
    const base::FilePath& path_to_file) {
  return file_path_helper_.IsFilePathSupported(path_to_file);
}

void ChromeScanningAppDelegate::OpenFilesInMediaApp(
    const std::vector<base::FilePath>& file_paths) {
  DCHECK(!file_paths.empty());

  ash::SystemAppLaunchParams params;
  params.launch_paths = file_paths;
  params.launch_source = apps::LaunchSource::kFromOtherApp;
  ash::LaunchSystemWebAppAsync(Profile::FromWebUI(web_ui_),
                               ash::SystemWebAppType::MEDIA, params);
}

void ChromeScanningAppDelegate::SaveScanSettingsToPrefs(
    const std::string& scan_settings) {
  GetPrefs()->SetString(kScanningStickySettingsPref, scan_settings);
}

void ChromeScanningAppDelegate::ShowFileInFilesApp(
    const base::FilePath& path_to_file,
    base::OnceCallback<void(bool)> callback) {
  if (!IsFilePathSupported(path_to_file)) {
    std::move(callback).Run(false);
    return;
  }

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::PathExists, path_to_file),
      base::BindOnce(&ChromeScanningAppDelegate::OnPathExists,
                     weak_ptr_factory_.GetWeakPtr(), path_to_file,
                     std::move(callback)));
}

ChromeScanningAppDelegate::BindScanServiceCallback
ChromeScanningAppDelegate::GetBindScanServiceCallback(content::WebUI* web_ui) {
  return base::BindRepeating(
      [](Profile* profile,
         mojo::PendingReceiver<ash::scanning::mojom::ScanService>
             pending_receiver) {
        ash::ScanService* service =
            ash::ScanServiceFactory::GetForBrowserContext(profile);
        if (service) {
          service->BindInterface(std::move(pending_receiver));
        }
      },
      Profile::FromWebUI(web_ui));
}

void ChromeScanningAppDelegate::OnPathExists(
    const base::FilePath& path_to_file,
    base::OnceCallback<void(bool)> callback,
    bool file_path_exists) {
  if (!file_path_exists) {
    std::move(callback).Run(false);
    return;
  }

  platform_util::ShowItemInFolder(Profile::FromWebUI(web_ui_), path_to_file);
  std::move(callback).Run(true);
}

// static
void ChromeScanningAppDelegate::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(kScanningStickySettingsPref, std::string());
}

void ChromeScanningAppDelegate::SetValidPaths(
    const base::FilePath& google_drive_path,
    const base::FilePath& my_files_path) {
  file_path_helper_ = ScanningFilePathHelper(google_drive_path, my_files_path);
}

PrefService* ChromeScanningAppDelegate::GetPrefs() const {
  return Profile::FromWebUI(web_ui_)->GetPrefs();
}

void ChromeScanningAppDelegate::SetRemoveableMediaPathForTesting(
    const base::FilePath& path) {
  file_path_helper_.SetRemoveableMediaPathForTesting(path);
}

}  // namespace ash
