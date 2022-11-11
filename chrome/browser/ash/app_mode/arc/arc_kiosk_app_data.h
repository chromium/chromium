// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_ARC_ARC_KIOSK_APP_DATA_H_
#define CHROME_BROWSER_ASH_APP_MODE_ARC_ARC_KIOSK_APP_DATA_H_

#include <string>

#include "chrome/browser/ash/app_mode/kiosk_app_data_base.h"
#include "components/account_id/account_id.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// Fetches from Android side and caches ARC kiosk app data such as name and
// icon.
class ArcKioskAppData : public KioskAppDataBase {
 public:
  ArcKioskAppData(const std::string& app_id,
                  const std::string& package_name,
                  const std::string& activity,
                  const std::string& intent,
                  const AccountId& account_id,
                  const std::string& name);
  ArcKioskAppData(const ArcKioskAppData&) = delete;
  ArcKioskAppData& operator=(const ArcKioskAppData&) = delete;
  ~ArcKioskAppData() override;

  const std::string& package_name() const { return package_name_; }
  const std::string& activity() const { return activity_; }
  const std::string& intent() const { return intent_; }

  bool operator==(const std::string& other_app_id) const;

  // Loads the locally cached data. Return false if there is none.
  bool LoadFromCache();

  // Sets the cached data.
  void SetCache(const std::string& name, const gfx::ImageSkia& icon);

  // Callbacks for KioskAppIconLoader.
  void OnIconLoadSuccess(const gfx::ImageSkia& icon) override;
  void OnIconLoadFailure() override;

 private:
  // Not cached, always provided in ctor.
  const std::string package_name_;
  const std::string activity_;
  const std::string intent_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_ARC_ARC_KIOSK_APP_DATA_H_
