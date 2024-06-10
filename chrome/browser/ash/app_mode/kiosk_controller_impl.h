// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_CONTROLLER_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/public/cpp/login_accelerators.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {

class KioskLaunchController;

class KioskControllerImpl : public KioskController,
                            public user_manager::UserManager::Observer {
 public:
  explicit KioskControllerImpl(user_manager::UserManager* user_manager);
  KioskControllerImpl(const KioskControllerImpl&) = delete;
  KioskControllerImpl& operator=(const KioskControllerImpl&) = delete;
  ~KioskControllerImpl() override;

  // `KioskController` implementation:
  std::vector<KioskApp> GetApps() const override;
  std::optional<KioskApp> GetAppById(const KioskAppId& app_id) const override;
  std::optional<KioskApp> GetAutoLaunchApp() const override;

  // Launches a kiosk session running the given app.
  void StartSession(const KioskAppId& app,
                    bool is_auto_launch,
                    LoginDisplayHost* host) override;

  bool IsSessionStarting() const override;
  void CancelSessionStart() override;

  void AddProfileLoadFailedObserver(
      KioskProfileLoadFailedObserver* observer) override;
  void RemoveProfileLoadFailedObserver(
      KioskProfileLoadFailedObserver* observer) override;

  bool HandleAccelerator(LoginAcceleratorAction action) override;

  void InitializeKioskSystemSession(
      Profile* profile,
      const KioskAppId& kiosk_app_id,
      const std::optional<std::string>& app_name) override;

  KioskSystemSession* GetKioskSystemSession() override;

  kiosk_vision::TelemetryProcessor* GetKioskVisionTelemetryProcessor() override;

 private:
  // `user_manager::UserManager::Observer` implementation:
  void OnUserLoggedIn(const user_manager::User& user) override;

  void OnLaunchComplete(std::optional<KioskAppLaunchError::Error> error);

  void DeleteLaunchControllerAsync();
  void DeleteLaunchController();

  WebKioskAppManager web_app_manager_;
  KioskChromeAppManager chrome_app_manager_;

  // Created once the Kiosk session launch starts. Only not null during the
  // kiosk launch.
  std::unique_ptr<KioskLaunchController> launch_controller_;

  // Created once the Kiosk session is launched successfully. `nullopt` before
  // Kiosk launch and generally when outside Kiosk.
  std::optional<KioskSystemSession> system_session_;

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      user_manager_observation_{this};

  base::WeakPtrFactory<KioskControllerImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_CONTROLLER_IMPL_H_
