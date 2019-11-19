// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_data.h"

#include <utility>

#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/common/chrome_paths.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

ArcKioskAppData::ArcKioskAppData(const std::string& app_id,
                                 const std::string& package_name,
                                 const std::string& activity,
                                 const std::string& intent,
                                 const AccountId& account_id,
                                 const std::string& name)
    : KioskAppDataBase(ArcKioskAppManager::kArcKioskDictionaryName,
                       app_id,
                       account_id),
      package_name_(package_name),
      activity_(activity),
      intent_(intent) {
  DCHECK(!package_name_.empty());
  DCHECK(activity.empty() || intent.empty());
  name_ = name;
}

ArcKioskAppData::~ArcKioskAppData() = default;

bool ArcKioskAppData::operator==(const std::string& other_app_id) const {
  return app_id() == other_app_id;
}

bool ArcKioskAppData::LoadFromCache() {
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* dict =
      local_state->GetDictionary(dictionary_name());

  return LoadFromDictionary(*dict);
}

void ArcKioskAppData::SetCache(const std::string& name,
                               const gfx::ImageSkia& icon) {
  DCHECK(!name.empty());
  DCHECK(!icon.isNull());
  name_ = name;
  icon_ = icon;

  base::FilePath cache_dir;
  ArcKioskAppManager::Get()->GetKioskAppIconCacheDir(&cache_dir);

  SaveIcon(*icon_.bitmap(), cache_dir);

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate dict_update(local_state, dictionary_name());

  SaveToDictionary(dict_update);
}

void ArcKioskAppData::OnIconLoadSuccess(const gfx::ImageSkia& icon) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  kiosk_app_icon_loader_.reset();
  icon_ = icon;
}

void ArcKioskAppData::OnIconLoadFailure() {
  kiosk_app_icon_loader_.reset();
  LOG(ERROR) << "Icon Load Failure";
  // Do nothing
}

}  // namespace chromeos
