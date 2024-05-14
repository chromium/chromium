// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"

#include <map>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data_base.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ash/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"

namespace ash {

namespace {
// Sub directory under DIR_USER_DATA to store cached icon files.
const char kIconCacheDir[] = "kiosk/icon";
}  // namespace

KioskAppManagerBase::KioskAppManagerBase() {
  local_accounts_subscription_ = CrosSettings::Get()->AddSettingsObserver(
      kAccountsPrefDeviceLocalAccounts,
      base::BindRepeating(&KioskAppManagerBase::UpdateAppsFromPolicy,
                          weak_ptr_factory_.GetWeakPtr()));
  local_account_auto_login_id_subscription_ =
      CrosSettings::Get()->AddSettingsObserver(
          kAccountsPrefDeviceLocalAccountAutoLoginId,
          base::BindRepeating(&KioskAppManagerBase::UpdateAppsFromPolicy,
                              weak_ptr_factory_.GetWeakPtr()));
}

KioskAppManagerBase::~KioskAppManagerBase() = default;

KioskAppManagerBase::App::App(const KioskAppDataBase& app)
    : app_id(app.app_id()),
      account_id(app.account_id()),
      name(app.name()),
      icon(app.icon()) {}

KioskAppManagerBase::App::App() : account_id(EmptyAccountId()) {}

KioskAppManagerBase::App::App(const App&) = default;

KioskAppManagerBase::App::~App() = default;

void KioskAppManagerBase::GetKioskAppIconCacheDir(base::FilePath* cache_dir) {
  base::FilePath user_data_dir;
  bool has_dir = base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(has_dir);
  *cache_dir = user_data_dir.AppendASCII(kIconCacheDir);
}

void KioskAppManagerBase::OnKioskAppDataChanged(const std::string& app_id) {
  for (auto& observer : observers_) {
    observer.OnKioskAppDataChanged(app_id);
  }
}

void KioskAppManagerBase::OnKioskAppDataLoadFailure(const std::string& app_id) {
  for (auto& observer : observers_) {
    observer.OnKioskAppDataLoadFailure(app_id);
  }
}

void KioskAppManagerBase::OnExternalCacheDamaged(const std::string& app_id) {
  // Should be implemented only in those kiosks that use ExternalCache.
  NOTREACHED_IN_MIGRATION();
}

bool KioskAppManagerBase::GetDisableBailoutShortcut() const {
  bool enable;
  if (CrosSettings::Get()->GetBoolean(
          kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled, &enable)) {
    return !enable;
  }

  return false;
}

void KioskAppManagerBase::NotifyKioskAppsChanged() const {
  for (auto& observer : observers_) {
    observer.OnKioskAppsSettingsChanged();
  }
}

void KioskAppManagerBase::NotifySessionInitialized() const {
  for (auto& observer : observers_) {
    observer.OnKioskSessionInitialized();
  }
}

void KioskAppManagerBase::AddObserver(KioskAppManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void KioskAppManagerBase::RemoveObserver(KioskAppManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void KioskAppManagerBase::ClearRemovedApps(
    const std::vector<KioskAppDataBase*>& old_apps) {
  std::vector<AccountId> account_ids_to_remove;
  account_ids_to_remove.reserve(old_apps.size());
  for (KioskAppDataBase* entry : old_apps) {
    entry->ClearCache();
    account_ids_to_remove.push_back(entry->account_id());
  }
  KioskCryptohomeRemover::RemoveCryptohomesAndExitIfNeeded(
      account_ids_to_remove);
}

}  // namespace ash
