// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_CONTROLLER_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_CONTROLLER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

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

  // Initializes `kiosk_system_session_`. Should be called during Kiosk launch.
  void InitializeKioskSystemSession(
      Profile* profile,
      const KioskAppId& kiosk_app_id,
      const std::optional<std::string>& app_name = std::nullopt);

  // Returns the `KioskSystemSession`. Can be `nullptr` if called outside a
  // Kiosk session, or before `InitializeSystemSession`.
  KioskSystemSession* GetKioskSystemSession();

  // user_manager::UserManager::Observer:
  void OnUserLoggedIn(const user_manager::User& user) override;

 private:
  WebKioskAppManager web_app_manager_;
  KioskChromeAppManager chrome_app_manager_;
  ArcKioskAppManager arc_app_manager_;

  //  Created once the Kiosk session is launched successfully. `nullopt` before
  //  Kiosk launch and generally when outside Kiosk,
  std::optional<KioskSystemSession> kiosk_system_session_;

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      user_manager_observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_CONTROLLER_H_
