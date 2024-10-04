// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_manager.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_data.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/ash/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/prefs/pref_registry_simple.h"
#include "url/origin.h"

namespace ash {

namespace {
KioskIwaManager* g_kiosk_iwa_manager_instance = nullptr;
}  // namespace

// static
const char KioskIwaManager::kIwaKioskDictionaryName[] = "iwa-kiosk";

// static
void KioskIwaManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kIwaKioskDictionaryName);
}

// static
KioskIwaManager* KioskIwaManager::Get() {
  CHECK(g_kiosk_iwa_manager_instance);
  return g_kiosk_iwa_manager_instance;
}

KioskIwaManager::KioskIwaManager() {
  CHECK(!g_kiosk_iwa_manager_instance);  // Only one instance is allowed.
  g_kiosk_iwa_manager_instance = this;
  UpdateAppsFromPolicy();
}

KioskIwaManager::~KioskIwaManager() {
  g_kiosk_iwa_manager_instance = nullptr;
}

KioskAppManagerBase::AppList KioskIwaManager::GetApps() const {
  AppList apps;
  for (const auto& iwa_app_data : isolated_web_apps_) {
    // TODO(crbug.com/361017701): fill in the install url
    apps.emplace_back(*iwa_app_data);
  }
  return apps;
}

const KioskIwaData* KioskIwaManager::GetApp(const AccountId& account_id) const {
  auto iter =
      std::find_if(isolated_web_apps_.begin(), isolated_web_apps_.end(),
                   [&account_id](const std::unique_ptr<KioskIwaData>& app) {
                     return app->account_id() == account_id;
                   });

  if (iter == isolated_web_apps_.end()) {
    return nullptr;
  }
  return iter->get();
}

void KioskIwaManager::UpdateAppsFromPolicy() {
  // TODO(crbug.com/361017701): remove old apps as other app managers do.
  isolated_web_apps_.clear();

  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(CrosSettings::Get());

  for (const policy::DeviceLocalAccount& account : device_local_accounts) {
    if (account.type != policy::DeviceLocalAccountType::kKioskIsolatedWebApp) {
      continue;
    }

    const std::string web_bundle_id(account.kiosk_iwa_info.web_bundle_id());
    const GURL update_manifest_url(
        account.kiosk_iwa_info.update_manifest_url());

    auto iwa_data = KioskIwaData::Create(account.user_id, web_bundle_id,
                                         update_manifest_url);

    if (!iwa_data) {
      LOG(WARNING) << "Could not add a Kiosk IWA for id " << web_bundle_id;
      continue;
    }

    KioskCryptohomeRemover::CancelDelayedCryptohomeRemoval(
        iwa_data->account_id());
    isolated_web_apps_.push_back(std::move(iwa_data));
  }

  NotifyKioskAppsChanged();
}

}  // namespace ash
