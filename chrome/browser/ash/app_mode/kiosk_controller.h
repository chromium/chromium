// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_CONTROLLER_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_CONTROLLER_H_

#include <optional>
#include <vector>

#include "ash/public/cpp/login_accelerators.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "content/public/browser/web_contents.h"

class Profile;

namespace ash {

class AppLaunchSplashScreen;
class KioskProfileLoadFailedObserver;
class KioskSystemSession;
class LoginDisplayHost;

// Public interface for Kiosk.
class KioskController {
 public:
  static KioskController& Get();

  KioskController();
  virtual ~KioskController();

  virtual std::vector<KioskApp> GetApps() const = 0;
  virtual std::optional<KioskApp> GetAppById(
      const KioskAppId& app_id) const = 0;
  virtual std::optional<KioskApp> GetAutoLaunchApp() const = 0;

  // Launches a kiosk session running the given app.
  virtual void StartSession(const KioskAppId& app,
                            bool is_auto_launch,
                            LoginDisplayHost* host,
                            AppLaunchSplashScreen* splash_screen) = 0;
  // Launches a kiosk session after a browser crash, which is a faster launch
  // without any UI.
  virtual void StartSessionAfterCrash(const KioskAppId& app,
                                      Profile* profile) = 0;

  // Returns true if a kiosk launch is in progress. Will return false at any
  // other time, including when the kiosk launch is finished.
  virtual bool IsSessionStarting() const = 0;

  // Cancels the kiosk session launch, if any is in progress.
  virtual void CancelSessionStart() = 0;

  // Adds/Removes an observer that will be informed if the profile fails to
  // load during launch. Can only be called while a kiosk launch is in progress.
  virtual void AddProfileLoadFailedObserver(
      KioskProfileLoadFailedObserver* observer) = 0;
  virtual void RemoveProfileLoadFailedObserver(
      KioskProfileLoadFailedObserver* observer) = 0;

  virtual bool HandleAccelerator(LoginAcceleratorAction action) = 0;

  // Notify Kiosk that a new guest web content was added.
  virtual void OnGuestAdded(content::WebContents* guest_web_contents) = 0;

  // Returns the `KioskSystemSession`. Can be `nullptr` if called outside a
  // Kiosk session, or before `InitializeSystemSession`.
  virtual KioskSystemSession* GetKioskSystemSession() = 0;

  // Removes the cryptohomes of kiosks that were removed from policy.
  virtual void RemoveObsoleteCryptohomes() = 0;

  // Registers local state prefs relevant for Kiosk.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Registers profile prefs relevant for Kiosk.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_CONTROLLER_H_
