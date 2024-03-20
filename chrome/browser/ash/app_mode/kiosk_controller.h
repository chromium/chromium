// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_CONTROLLER_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_CONTROLLER_H_

#include <optional>
#include <vector>

#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "components/user_manager/user_manager.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// Public interface for Kiosk.
class KioskController : public user_manager::UserManager::Observer {
 public:
  static KioskController& Get();

  explicit KioskController(user_manager::UserManager* user_manager);
  KioskController(const KioskController&) = delete;
  KioskController& operator=(const KioskController&) = delete;
  ~KioskController() override;

  std::vector<KioskApp> GetApps() const;
  std::optional<KioskApp> GetAppById(const KioskAppId& app_id) const;
  std::optional<KioskApp> GetAutoLaunchApp() const;

  // user_manager::UserManager::Observer:
  void OnUserLoggedIn(const user_manager::User& user) override;

 private:
  WebKioskAppManager web_app_manager_;
  KioskChromeAppManager chrome_app_manager_;
  ArcKioskAppManager arc_app_manager_;

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      user_manager_observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_CONTROLLER_H_
