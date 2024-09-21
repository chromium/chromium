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
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"

namespace ash {

class AppLaunchSplashScreen;
class CrashRecoveryLauncher;
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

  void StartSession(const KioskAppId& app,
                    bool is_auto_launch,
                    LoginDisplayHost* host,
                    AppLaunchSplashScreen* splash_screen) override;
  void StartSessionAfterCrash(const KioskAppId& app, Profile* profile) override;

  bool IsSessionStarting() const override;
  void CancelSessionStart() override;

  void AddProfileLoadFailedObserver(
      KioskProfileLoadFailedObserver* observer) override;
  void RemoveProfileLoadFailedObserver(
      KioskProfileLoadFailedObserver* observer) override;

  bool HandleAccelerator(LoginAcceleratorAction action) override;

  void OnGuestAdded(content::WebContents* guest_web_contents) override;

  KioskSystemSession* GetKioskSystemSession() override;

  kiosk_vision::TelemetryProcessor* GetKioskVisionTelemetryProcessor() override;

  kiosk_vision::InternalsPageProcessor* GetKioskVisionInternalsPageProcessor()
      override;

 private:
  // `user_manager::UserManager::Observer` implementation:
  void OnUserLoggedIn(const user_manager::User& user) override;

  void OnAppLaunched(const KioskAppId& kiosk_app_id,
                     Profile* profile,
                     const std::optional<std::string>& app_name);
  void OnLaunchComplete(KioskAppLaunchError::Error error);
  void OnLaunchCompleteAfterCrash(const KioskAppId& app,
                                  Profile* profile,
                                  bool success,
                                  const std::optional<std::string>& app_name);
  void InitializeKioskSystemSession(const KioskAppId& kiosk_app_id,
                                    Profile* profile,
                                    const std::optional<std::string>& app_name);

  void DeleteLaunchControllerAsync();
  void DeleteLaunchController();

  void AppendWebApps(std::vector<KioskApp>& apps) const;
  void AppendChromeApps(std::vector<KioskApp>& apps) const;
  void AppendIsolatedWebApps(std::vector<KioskApp>& apps) const;

  SEQUENCE_CHECKER(sequence_checker_);

  KioskIwaManager GUARDED_BY_CONTEXT(sequence_checker_) iwa_manager_;
  WebKioskAppManager GUARDED_BY_CONTEXT(sequence_checker_) web_app_manager_;
  KioskChromeAppManager GUARDED_BY_CONTEXT(sequence_checker_)
      chrome_app_manager_;

  // Created once the Kiosk session launch starts. Only not null during the
  // kiosk launch.
  std::unique_ptr<KioskLaunchController> GUARDED_BY_CONTEXT(sequence_checker_)
      launch_controller_;
  std::unique_ptr<CrashRecoveryLauncher> GUARDED_BY_CONTEXT(sequence_checker_)
      crash_recovery_launcher_;

  // Created once the Kiosk session is launched successfully. `nullopt` before
  // Kiosk launch and generally when outside Kiosk.
  std::optional<KioskSystemSession> GUARDED_BY_CONTEXT(sequence_checker_)
      system_session_;

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      user_manager_observation_{this};

  base::WeakPtrFactory<KioskControllerImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_CONTROLLER_IMPL_H_
