// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_data.h"

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data_base.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/account_id/account_id.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/image/image_skia.h"
#include "url/origin.h"

namespace ash {

std::unique_ptr<KioskIwaData> KioskIwaData::Create(
    const std::string& user_id,
    const std::string& web_bundle_id,
    const GURL& update_manifest_url,
    KioskAppDataDelegate& delegate) {
  auto parsed_id = web_package::SignedWebBundleId::Create(web_bundle_id);
  if (!parsed_id.has_value()) {
    LOG(ERROR) << "Cannot create kiosk IWA data. Invalid bundle id: "
               << web_bundle_id << ": " << parsed_id.error();
    return nullptr;
  }

  if (!update_manifest_url.is_valid()) {
    LOG(ERROR) << "Cannot create kiosk IWA data. Invalid url: "
               << update_manifest_url;
    return nullptr;
  }

  auto iwa_url_info =
      web_app::IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          parsed_id.value());

  return std::make_unique<ash::KioskIwaData>(user_id, iwa_url_info,
                                             update_manifest_url, delegate);
}

KioskIwaData::KioskIwaData(const std::string& user_id,
                           const web_app::IsolatedWebAppUrlInfo& iwa_info,
                           const GURL& update_manifest_url,
                           KioskAppDataDelegate& delegate)
    : KioskAppDataBase(KioskIwaManager::kIwaKioskDictionaryName,
                       iwa_info.app_id(),
                       AccountId::FromUserEmail(user_id)),
      iwa_info_(iwa_info),
      update_manifest_url_(update_manifest_url),
      delegate_(delegate) {
  name_ = iwa_info_.origin().Serialize();
}

KioskIwaData::~KioskIwaData() = default;

void KioskIwaData::Update(const std::string& title,
                          const web_app::IconBitmaps& icon_bitmaps) {
  name_ = title;

  base::FilePath cache_dir = delegate_->GetKioskAppIconCacheDir();

  auto iter = icon_bitmaps.any.find(kIconSize);
  if (iter != icon_bitmaps.any.end() && !cache_dir.empty()) {
    const SkBitmap& bitmap = iter->second;
    icon_ = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
    icon_.MakeThreadSafe();
    SaveIcon(bitmap, cache_dir);
  }

  PrefService* local_state = g_browser_process->local_state();
  ScopedDictPrefUpdate dict_update(local_state, dictionary_name());
  SaveToDictionary(dict_update);

  delegate_->OnKioskAppDataChanged(app_id());
}

bool KioskIwaData::LoadFromCache() {
  PrefService* local_state = g_browser_process->local_state();
  const base::Value::Dict& dict = local_state->GetDict(dictionary_name());

  if (!LoadFromDictionary(dict)) {
    return false;
  }

  DecodeIcon(base::BindOnce(&KioskIwaData::OnIconLoadDone,
                            weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void KioskIwaData::OnIconLoadDone(std::optional<gfx::ImageSkia> icon) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  kiosk_app_icon_loader_.reset();

  if (!icon.has_value()) {
    LOG(ERROR) << "Kiosk IWA icon load failure";
    return;
  }

  icon_ = icon.value();
  delegate_->OnKioskAppDataChanged(app_id());
}

}  // namespace ash
