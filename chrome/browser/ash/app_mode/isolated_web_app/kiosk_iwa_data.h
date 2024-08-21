// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_DATA_H_
#define CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_DATA_H_

#include <memory>
#include <string>

#include "chrome/browser/ash/app_mode/kiosk_app_data_base.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash {

class KioskIwaData : public KioskAppDataBase {
 public:
  static std::unique_ptr<KioskIwaData> Create(const std::string& user_id,
                                              const std::string& web_bundle_id,
                                              const GURL& update_manifest_url);

  KioskIwaData(const std::string& user_id,
               const web_app::IsolatedWebAppUrlInfo& iwa_info,
               const GURL& update_manifest_url);
  KioskIwaData(const KioskIwaData&) = delete;
  KioskIwaData& operator=(const KioskIwaData&) = delete;
  ~KioskIwaData() override;

  [[nodiscard]] const std::string& web_bundle_id() const {
    return iwa_info_.web_bundle_id().id();
  }

  [[nodiscard]] const GURL& update_manifest_url() const {
    return update_manifest_url_;
  }

 private:
  const web_app::IsolatedWebAppUrlInfo iwa_info_;
  const GURL update_manifest_url_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_DATA_H_
