// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/arcvm_app/kiosk_arcvm_app_data.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data_base.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

KioskArcvmAppData::KioskArcvmAppData(PrefService* local_state,
                                     std::string app_id,
                                     std::string package_name,
                                     std::string activity,
                                     std::string intent,
                                     AccountId account_id,
                                     std::string name)
    : KioskAppDataBase(kArcvmKioskDictionaryName,
                       std::move(app_id),
                       std::move(account_id)),
      local_state_(local_state),
      package_name_(std::move(package_name)),
      activity_(std::move(activity)),
      intent_(std::move(intent)) {
  CHECK(!package_name_.empty());
  CHECK(activity.empty() || intent.empty());
  name_ = std::move(name);
}

KioskArcvmAppData::~KioskArcvmAppData() = default;

bool KioskArcvmAppData::CompareByAppID(const std::string& other_app_id) const {
  return app_id() == other_app_id;
}

// Loads the locally cached data. Return false if there is none.
// Asynchronously populate icon_ value once DecodeIcon operation completes via
// OnIconLoadDone.
// TODO(crbug.com/418936700) : Handle asynchronous icon update.
bool KioskArcvmAppData::LoadFromCache() {
  const base::Value::Dict& dict = local_state_->GetDict(dictionary_name());

  if (!LoadFromDictionary(dict)) {
    return false;
  }

  DecodeIcon(base::BindOnce(&KioskArcvmAppData::OnIconLoadDone,
                            weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void KioskArcvmAppData::SetCache(const std::string& name,
                                 const gfx::ImageSkia& icon,
                                 const base::FilePath& cache_dir) {
  CHECK(!name.empty());
  CHECK(!icon.isNull());
  name_ = name;
  icon_ = icon;

  SaveIcon(*icon_.bitmap(), cache_dir);

  ScopedDictPrefUpdate dict_update(local_state_, dictionary_name());

  SaveToDictionary(dict_update);
}

void KioskArcvmAppData::OnIconLoadDone(std::optional<gfx::ImageSkia> icon) {
  kiosk_app_icon_loader_.reset();

  if (!icon.has_value()) {
    LOG(ERROR) << "Icon Load Failure";
    return;
  }

  icon_ = icon.value();
}

}  // namespace ash
