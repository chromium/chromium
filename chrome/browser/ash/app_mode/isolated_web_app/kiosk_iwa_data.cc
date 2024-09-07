// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_data.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data_base.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "components/account_id/account_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "url/origin.h"

namespace ash {

std::unique_ptr<KioskIwaData> KioskIwaData::Create(
    const std::string& user_id,
    const std::string& web_bundle_id,
    const GURL& update_manifest_url) {
  auto parsed_id = web_package::SignedWebBundleId::Create(web_bundle_id);
  if (!parsed_id.has_value()) {
    LOG(ERROR) << "Cannot create kiosk iwa data for id " << web_bundle_id
               << ": " << parsed_id.error();
    return nullptr;
  }

  auto iwa_url_info =
      web_app::IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          parsed_id.value());

  return std::make_unique<ash::KioskIwaData>(user_id, iwa_url_info,
                                             update_manifest_url);
}

KioskIwaData::KioskIwaData(const std::string& user_id,
                           const web_app::IsolatedWebAppUrlInfo& iwa_info,
                           const GURL& update_manifest_url)
    : KioskAppDataBase(KioskIwaManager::kIwaKioskDictionaryName,
                       iwa_info.app_id(),
                       AccountId::FromUserEmail(user_id)),
      iwa_info_(iwa_info),
      update_manifest_url_(update_manifest_url) {
  name_ = iwa_info_.origin().Serialize();
}

KioskIwaData::~KioskIwaData() = default;
}  // namespace ash
