// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_external_updater.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/version.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/notifications/kiosk_external_update_notification.h"
#include "chrome/grit/generated_resources.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/common/extension.h"
#include "extensions/common/verifier_formats.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {

namespace {

constexpr base::FilePath::CharType kExternalUpdateManifest[] =
    "external_update.json";
constexpr char kExternalCrx[] = "external_crx";
constexpr char kExternalVersion[] = "external_version";

std::pair<base::Value, KioskExternalUpdater::ErrorCode>
ParseExternalUpdateManifest(const base::FilePath& external_update_dir) {
  base::FilePath manifest = external_update_dir.Append(kExternalUpdateManifest);
  if (!base::PathExists(manifest)) {
    return std::make_pair(base::Value(),
                          KioskExternalUpdater::ErrorCode::kNoManifest);
  }

  JSONFileValueDeserializer deserializer(manifest);
  std::unique_ptr<base::Value> extensions =
      deserializer.Deserialize(nullptr, nullptr);
  if (!extensions) {
    return std::make_pair(base::Value(),
                          KioskExternalUpdater::ErrorCode::kInvalidManifest);
  }

  return std::make_pair(base::Value::FromUniquePtrValue(std::move(extensions)),
                        KioskExternalUpdater::ErrorCode::kNone);
}

// Copies `external_crx_file` to `temp_crx_file`, and removes `temp_dir`
// created for unpacking `external_crx_file`.
bool CopyExternalCrxAndDeleteTempDir(const base::FilePath& external_crx_file,
                                     const base::FilePath& temp_crx_file,
                                     const base::FilePath& temp_dir) {
  base::DeletePathRecursively(temp_dir);
  return base::CopyFile(external_crx_file, temp_crx_file);
}

// Returns true if `version_1` < `version_2`, and
// if `update_for_same_version` is true and `version_1` = `version_2`.
bool ShouldUpdateForHigherVersion(const std::string& version_1,
                                  const std::string& version_2,
                                  bool update_for_same_version) {
  const base::Version v1(version_1);
  const base::Version v2(version_2);
  if (!v1.IsValid() || !v2.IsValid()) {
    return false;
  }
  int compare_result = v1.CompareTo(v2);
  if (compare_result < 0) {
    return true;
  }
  return update_for_same_version && compare_result == 0;
}

}  // namespace

KioskExternalUpdater::ExternalUpdate::ExternalUpdate() = default;

KioskExternalUpdater::ExternalUpdate::ExternalUpdate(
    const ExternalUpdate& other) = default;

KioskExternalUpdater::ExternalUpdate::~ExternalUpdate() = default;

KioskExternalUpdater::KioskExternalUpdater(
    const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner,
    const base::FilePath& crx_cache_dir,
    const base::FilePath& crx_unpack_dir)
    : backend_task_runner_(backend_task_runner),
      crx_cache_dir_(crx_cache_dir),
      crx_unpack_dir_(crx_unpack_dir) {
  // Subscribe to DiskMountManager.
  DCHECK(disks::DiskMountManager::GetInstance());
  disks::DiskMountManager::GetInstance()->AddObserver(this);
}

KioskExternalUpdater::~KioskExternalUpdater() {
  if (disks::DiskMountManager::GetInstance()) {
    disks::DiskMountManager::GetInstance()->RemoveObserver(this);
  }
}

void KioskExternalUpdater::OnMountEvent(
    disks::DiskMountManager::MountEvent event,
    MountError error_code,
    const disks::DiskMountManager::MountPoint& mount_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (mount_info.mount_type != MountType::kDevice ||
      error_code != MountError::kSuccess) {
    return;
  }

  if (event == disks::DiskMountManager::MOUNTING) {
    // If multiple disks have been mounted, skip the rest of them if kiosk
    // update has already been found.
    if (!external_update_path_.empty()) {
      LOG(WARNING) << "External update path already found, skip "
                   << mount_info.mount_path;
      return;
    }

    backend_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&ParseExternalUpdateManifest,
                       base::FilePath(mount_info.mount_path)),
        base::BindOnce(&KioskExternalUpdater::ProcessParsedManifest,
                       weak_factory_.GetWeakPtr(),
                       base::FilePath(mount_info.mount_path)));
    return;
  }

  // unmounting a removable device case.
  if (external_update_path_.value().empty()) {
    // Clear any previously displayed message.
    DismissKioskUpdateNotification();
  } else if (external_update_path_.value() == mount_info.mount_path) {
    DismissKioskUpdateNotification();
    if (IsExternalUpdatePending()) {
      LOG(ERROR) << "External kiosk update is not completed when the usb "
                 << "stick is unmoutned.";
    }
    external_updates_.clear();
    external_update_path_.clear();
  }
}

void KioskExternalUpdater::OnExternalUpdateUnpackSuccess(
    const std::string& app_id,
    const std::string& version,
    const std::string& min_browser_version,
    const base::FilePath& temp_dir) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // User might pull out the usb stick before updating is completed.
  if (CheckExternalUpdateInterrupted()) {
    return;
  }

  if (!ShouldDoExternalUpdate(app_id, version, min_browser_version)) {
    external_updates_[app_id].update_status = UpdateStatus::kFailed;
    MaybeValidateNextExternalUpdate();
    return;
  }

  // User might pull out the usb stick before updating is completed.
  if (CheckExternalUpdateInterrupted()) {
    return;
  }

  base::FilePath external_crx_path =
      external_updates_[app_id].external_crx.path;
  base::FilePath temp_crx_path =
      crx_unpack_dir_.Append(external_crx_path.BaseName());
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CopyExternalCrxAndDeleteTempDir, external_crx_path,
                     temp_crx_path, temp_dir),
      base::BindOnce(&KioskExternalUpdater::PutValidatedExtension,
                     weak_factory_.GetWeakPtr(), app_id, temp_crx_path,
                     version));
}

void KioskExternalUpdater::OnExternalUpdateUnpackFailure(
    const std::string& app_id) {
  // User might pull out the usb stick before updating is completed.
  if (CheckExternalUpdateInterrupted()) {
    return;
  }

  external_updates_[app_id].update_status = UpdateStatus::kFailed;
  external_updates_[app_id].error =
      ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
          IDS_KIOSK_EXTERNAL_UPDATE_BAD_CRX);
  MaybeValidateNextExternalUpdate();
}

void KioskExternalUpdater::ProcessParsedManifest(
    const base::FilePath& external_update_dir,
    const ParseManifestResult& result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value& parsed_manifest = result.first;
  ErrorCode parsing_error = result.second;
  if (parsing_error == ErrorCode::kNoManifest) {
    KioskChromeAppManager::Get()->OnKioskAppExternalUpdateComplete(false);
    return;
  }
  if (parsing_error == ErrorCode::kInvalidManifest) {
    NotifyKioskUpdateProgress(
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_KIOSK_EXTERNAL_UPDATE_INVALID_MANIFEST));
    KioskChromeAppManager::Get()->OnKioskAppExternalUpdateComplete(false);
    return;
  }

  NotifyKioskUpdateProgress(
      ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
          IDS_KIOSK_EXTERNAL_UPDATE_IN_PROGRESS));

  external_update_path_ = external_update_dir;
  for (auto manifest : parsed_manifest.GetDict()) {
    std::string app_id = manifest.first;
    std::string cached_version_str;
    base::FilePath cached_crx;
    if (!KioskChromeAppManager::Get()->GetCachedCrx(app_id, &cached_crx,
                                                    &cached_version_str)) {
      LOG(WARNING) << "Can't find app in existing cache " << app_id;
      continue;
    }

    if (!manifest.second.is_dict()) {
      LOG(ERROR) << "Found bad entry in manifest type "
                 << manifest.second.type();
      continue;
    }
    const base::Value::Dict& extension = manifest.second.GetDict();

    const std::string* external_crx_str = extension.FindString(kExternalCrx);
    if (!external_crx_str) {
      LOG(ERROR) << "Can't find external crx in manifest " << app_id;
      continue;
    }

    const std::string* external_version_str =
        extension.FindString(kExternalVersion);
    if (external_version_str) {
      if (!ShouldUpdateForHigherVersion(cached_version_str,
                                        *external_version_str, false)) {
        LOG(WARNING) << "External app " << app_id
                     << " is at the same or lower version comparing to "
                     << " the existing one.";
        continue;
      }
    }

    ExternalUpdate update;
    KioskChromeAppManager::App app;
    if (KioskChromeAppManager::Get()->GetApp(app_id, &app)) {
      update.app_name = app.name;
    } else {
      NOTREACHED_IN_MIGRATION();
    }
    update.external_crx = extensions::CRXFileInfo(
        external_update_path_.AppendASCII(*external_crx_str),
        extensions::GetExternalVerifierFormat());
    update.external_crx.extension_id = app_id;
    update.update_status = UpdateStatus::kPending;
    external_updates_[app_id] = update;
  }

  if (external_updates_.empty()) {
    NotifyKioskUpdateProgress(
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_KIOSK_EXTERNAL_UPDATE_NO_UPDATES));
    KioskChromeAppManager::Get()->OnKioskAppExternalUpdateComplete(false);
    return;
  }

  ValidateExternalUpdates();
}

bool KioskExternalUpdater::CheckExternalUpdateInterrupted() {
  if (external_updates_.empty()) {
    // This could happen if user pulls out the usb stick before the updating
    // operation is completed.
    LOG(ERROR) << "external_updates_ has been cleared before external "
               << "updating completes.";
    return true;
  }

  return false;
}

void KioskExternalUpdater::ValidateExternalUpdates() {
  for (const auto& it : external_updates_) {
    const ExternalUpdate& update = it.second;
    if (update.update_status == UpdateStatus::kPending) {
      auto crx_validator = base::MakeRefCounted<KioskExternalUpdateValidator>(
          backend_task_runner_, update.external_crx, crx_unpack_dir_,
          weak_factory_.GetWeakPtr());
      crx_validator->Start();
      break;
    }
  }
}

bool KioskExternalUpdater::IsExternalUpdatePending() const {
  for (const auto& it : external_updates_) {
    if (it.second.update_status == UpdateStatus::kPending) {
      return true;
    }
  }
  return false;
}

bool KioskExternalUpdater::IsAllExternalUpdatesSucceeded() const {
  for (const auto& it : external_updates_) {
    if (it.second.update_status != UpdateStatus::kSuccess) {
      return false;
    }
  }
  return true;
}

bool KioskExternalUpdater::ShouldDoExternalUpdate(
    const std::string& app_id,
    const std::string& version,
    const std::string& min_browser_version) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string existing_version_str;
  base::FilePath existing_path;
  bool cached = KioskChromeAppManager::Get()->GetCachedCrx(
      app_id, &existing_path, &existing_version_str);
  DCHECK(cached);

  // Compare app version.
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  if (!ShouldUpdateForHigherVersion(existing_version_str, version, false)) {
    external_updates_[app_id].error = rb->GetLocalizedString(
        IDS_KIOSK_EXTERNAL_UPDATE_SAME_OR_LOWER_APP_VERSION);
    return false;
  }

  // Check minimum browser version.
  if (!min_browser_version.empty() &&
      !ShouldUpdateForHigherVersion(
          min_browser_version, std::string(version_info::GetVersionNumber()),
          true)) {
    external_updates_[app_id].error = l10n_util::GetStringFUTF16(
        IDS_KIOSK_EXTERNAL_UPDATE_REQUIRE_HIGHER_BROWSER_VERSION,
        base::UTF8ToUTF16(min_browser_version));
    return false;
  }

  return true;
}

void KioskExternalUpdater::PutValidatedExtension(const std::string& app_id,
                                                 const base::FilePath& crx_file,
                                                 const std::string& version,
                                                 bool crx_copied) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (CheckExternalUpdateInterrupted()) {
    return;
  }

  if (!crx_copied) {
    LOG(ERROR) << "Cannot copy external crx file to " << crx_file.value();
    external_updates_[app_id].update_status = UpdateStatus::kFailed;
    external_updates_[app_id].error = l10n_util::GetStringFUTF16(
        IDS_KIOSK_EXTERNAL_UPDATE_FAILED_COPY_CRX_TO_TEMP,
        base::UTF8ToUTF16(crx_file.value()));
    MaybeValidateNextExternalUpdate();
    return;
  }

  KioskChromeAppManager::Get()->PutValidatedExternalExtension(
      app_id, crx_file, version,
      base::BindOnce(&KioskExternalUpdater::OnPutValidatedExtension,
                     weak_factory_.GetWeakPtr()));
}

void KioskExternalUpdater::OnPutValidatedExtension(const std::string& app_id,
                                                   bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (CheckExternalUpdateInterrupted()) {
    return;
  }

  if (!success) {
    external_updates_[app_id].update_status = UpdateStatus::kFailed;
    external_updates_[app_id].error = l10n_util::GetStringFUTF16(
        IDS_KIOSK_EXTERNAL_UPDATE_CANNOT_INSTALL_IN_LOCAL_CACHE,
        base::UTF8ToUTF16(external_updates_[app_id].external_crx.path.value()));
  } else {
    external_updates_[app_id].update_status = UpdateStatus::kSuccess;
  }

  // Validate the next pending external update.
  MaybeValidateNextExternalUpdate();
}

void KioskExternalUpdater::MaybeValidateNextExternalUpdate() {
  if (IsExternalUpdatePending()) {
    ValidateExternalUpdates();
  } else {
    MayBeNotifyKioskAppUpdate();
  }
}

void KioskExternalUpdater::MayBeNotifyKioskAppUpdate() {
  if (IsExternalUpdatePending()) {
    return;
  }

  NotifyKioskUpdateProgress(GetUpdateReportMessage());
  NotifyKioskAppUpdateAvailable();
  KioskChromeAppManager::Get()->OnKioskAppExternalUpdateComplete(
      IsAllExternalUpdatesSucceeded());
}

void KioskExternalUpdater::NotifyKioskAppUpdateAvailable() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (const auto& it : external_updates_) {
    if (it.second.update_status == UpdateStatus::kSuccess) {
      KioskChromeAppManager::Get()->OnKioskAppCacheUpdated(it.first);
    }
  }
}

void KioskExternalUpdater::NotifyKioskUpdateProgress(
    const std::u16string& message) {
  if (!notification_) {
    notification_ = std::make_unique<KioskExternalUpdateNotification>(message);
  } else {
    notification_->ShowMessage(message);
  }
}

void KioskExternalUpdater::DismissKioskUpdateNotification() {
  if (notification_.get()) {
    notification_.reset();
  }
}

std::u16string KioskExternalUpdater::GetUpdateReportMessage() const {
  DCHECK(!IsExternalUpdatePending());
  int updated = 0;
  int failed = 0;
  std::u16string updated_apps;
  std::u16string failed_apps;
  for (const auto& it : external_updates_) {
    const ExternalUpdate& update = it.second;
    std::u16string app_name = base::UTF8ToUTF16(update.app_name);
    if (update.update_status == UpdateStatus::kSuccess) {
      ++updated;
      if (updated_apps.empty()) {
        updated_apps = app_name;
      } else {
        updated_apps += u", " + app_name;
      }
    } else {  // UpdateStatus::kFailed
      ++failed;
      if (failed_apps.empty()) {
        failed_apps = app_name + u": " + update.error;
      } else {
        failed_apps += u"\n" + app_name + u": " + update.error;
      }
    }
  }

  std::u16string message =
      ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
          IDS_KIOSK_EXTERNAL_UPDATE_COMPLETE);
  if (updated) {
    std::u16string success_app_msg = l10n_util::GetStringFUTF16(
        IDS_KIOSK_EXTERNAL_UPDATE_SUCCESSFUL_UPDATED_APPS, updated_apps);
    message += u"\n" + success_app_msg;
  }

  if (failed) {
    std::u16string failed_app_msg =
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_KIOSK_EXTERNAL_UPDATE_FAILED_UPDATED_APPS) +
        u"\n" + failed_apps;
    message += u"\n" + failed_app_msg;
  }
  return message;
}

}  // namespace ash
