// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_UPDATE_SERVICE_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_UPDATE_SERVICE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ash/system/automatic_reboot_manager_observer.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/update_observer.h"

class Profile;

namespace extensions {
class Extension;
}

namespace ash {

namespace system {
class AutomaticRebootManager;
}

extern const char kKioskPrimaryAppInSessionUpdateHistogram[];

// This class enforces automatic restart on app and Chrome updates in app mode.
class KioskAppUpdateService : public KeyedService,
                              public extensions::UpdateObserver,
                              public system::AutomaticRebootManagerObserver,
                              public KioskAppManagerObserver {
 public:
  KioskAppUpdateService(
      Profile* profile,
      system::AutomaticRebootManager* automatic_reboot_manager);
  KioskAppUpdateService(const KioskAppUpdateService&) = delete;
  KioskAppUpdateService& operator=(const KioskAppUpdateService&) = delete;
  ~KioskAppUpdateService() override;

  void Init(const std::string& app_id);

  std::string get_app_id() const { return app_id_; }

 private:
  friend class KioskAppUpdateServiceTest;

  void StartAppUpdateRestartTimer();
  void ForceAppUpdateRestart();

  // KeyedService overrides:
  void Shutdown() override;

  // extensions::UpdateObserver overrides:
  void OnAppUpdateAvailable(const extensions::Extension* extension) override;
  void OnChromeUpdateAvailable() override {}

  // system::AutomaticRebootManagerObserver overrides:
  void OnRebootRequested(Reason reason) override;
  void WillDestroyAutomaticRebootManager() override;

  // KioskAppManagerObserver overrides:
  void OnKioskAppCacheUpdated(const std::string& app_id) override;

  raw_ptr<Profile> profile_;
  std::string app_id_;

  // After we detect an upgrade we start a one-short timer to force restart.
  base::OneShotTimer restart_timer_;

  raw_ptr<system::AutomaticRebootManager>
      automatic_reboot_manager_;  // Not owned.
};

// Singleton that owns all KioskAppUpdateServices and associates them with
// profiles.
class KioskAppUpdateServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the KioskAppUpdateService for `profile`, creating it if it is not
  // yet created.
  static KioskAppUpdateService* GetForProfile(Profile* profile);

  // Returns the KioskAppUpdateServiceFactory instance.
  static KioskAppUpdateServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<KioskAppUpdateServiceFactory>;

  KioskAppUpdateServiceFactory();
  ~KioskAppUpdateServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_UPDATE_SERVICE_H_
