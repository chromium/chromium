// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_data.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_manager.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_policy_util.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data_base.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/image/image_skia.h"
#include "url/origin.h"

namespace ash {

// static
std::unique_ptr<KioskIwaData> KioskIwaData::Create(
    const std::string& user_id,
    const policy::IsolatedWebAppKioskBasicInfo& policy_info,
    KioskAppDataDelegate& delegate,
    PrefService& local_state) {
  ASSIGN_OR_RETURN(
      web_package::SignedWebBundleId web_bundle_id,
      web_package::SignedWebBundleId::Create(policy_info.web_bundle_id()),
      [policy_info](const std::string& error) -> std::unique_ptr<KioskIwaData> {
        LOG(ERROR) << "Cannot create kiosk IWA. Invalid bundle id: "
                   << policy_info.web_bundle_id() << ": " << error;
        return nullptr;
      });

  const GURL update_manifest_url(policy_info.update_manifest_url());
  if (!update_manifest_url.is_valid()) {
    LOG(ERROR) << "Cannot create kiosk IWA. Invalid url: "
               << update_manifest_url;
    return nullptr;
  }

  ASSIGN_OR_RETURN(web_app::UpdateChannel update_channel,
                   GetUpdateChannel(policy_info.update_channel()),
                   [policy_info](auto) -> std::unique_ptr<KioskIwaData> {
                     LOG(ERROR)
                         << "Cannot create kiosk IWA. Invalid update channel: "
                         << policy_info.update_channel();
                     return nullptr;
                   });

  ASSIGN_OR_RETURN(PinnedVersion pinned_version,
                   GetPinnedVersion(policy_info.pinned_version()),
                   [policy_info](auto) -> std::unique_ptr<KioskIwaData> {
                     LOG(ERROR)
                         << "Cannot create kiosk IWA. Invalid pinned version: "
                         << policy_info.pinned_version();
                     return nullptr;
                   });

  const bool allow_downgrades = policy_info.allow_downgrades();
  if (allow_downgrades && !pinned_version.has_value()) {
    LOG(ERROR) << "Cannot create kiosk IWA. Enabling downgrades requires "
                  "pinned version.";
    return nullptr;
  }

  const auto iwa_url_info =
      web_app::IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          web_bundle_id);

  return std::make_unique<ash::KioskIwaData>(
      user_id, iwa_url_info, update_manifest_url, update_channel,
      pinned_version, allow_downgrades, delegate, local_state);
}

KioskIwaData::KioskIwaData(const std::string& user_id,
                           const web_app::IsolatedWebAppUrlInfo& iwa_info,
                           GURL update_manifest_url,
                           web_app::UpdateChannel update_channel,
                           PinnedVersion pinned_version,
                           bool allow_downgrades,
                           KioskAppDataDelegate& delegate,
                           PrefService& local_state)
    : KioskAppDataBase(&local_state,
                       KioskIwaManager::kIwaKioskDictionaryName,
                       iwa_info.app_id(),
                       AccountId::FromUserEmail(user_id)),
      iwa_info_(iwa_info),
      update_manifest_url_(std::move(update_manifest_url)),
      update_channel_(std::move(update_channel)),
      pinned_version_(std::move(pinned_version)),
      allow_downgrades_(allow_downgrades),
      delegate_(delegate) {
  name_ = update_manifest_url_.GetWithoutFilename().GetContent();
}

KioskIwaData::~KioskIwaData() = default;

bool operator==(const KioskIwaData& lhs, const KioskIwaData& rhs) {
  return lhs.account_id() == rhs.account_id() &&
         lhs.web_bundle_id() == rhs.web_bundle_id() &&
         lhs.update_manifest_url() == rhs.update_manifest_url() &&
         lhs.update_channel() == rhs.update_channel() &&
         lhs.pinned_version() == rhs.pinned_version() &&
         lhs.allow_downgrades() == rhs.allow_downgrades();
}

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

  ScopedDictPrefUpdate dict_update(&local_state_.get(), dictionary_name());
  SaveToDictionary(dict_update);

  delegate_->OnKioskAppDataChanged(app_id());
}

bool KioskIwaData::LoadFromCache() {
  const base::Value::Dict& dict = local_state_->GetDict(dictionary_name());
  if (!LoadFromDictionary(dict)) {
    return false;
  }

  DecodeIcon(base::BindOnce(&KioskIwaData::OnIconLoadDone,
                            weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void KioskIwaData::OnIconLoadDone(std::optional<gfx::ImageSkia> icon) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!icon.has_value()) {
    LOG(ERROR) << "Kiosk IWA icon load failure";
    return;
  }

  icon_ = icon.value();
  delegate_->OnKioskAppDataChanged(app_id());
}

}  // namespace ash
