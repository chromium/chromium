// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_ARCVM_APP_KIOSK_ARCVM_APP_LAUNCHER_H_
#define CHROME_BROWSER_ASH_APP_MODE_ARCVM_APP_KIOSK_ARCVM_APP_LAUNCHER_H_

#include <optional>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

// TODO(crbug.com/418946082): Use forward decl to move applicable headers to
// .cc.
namespace ash {

// Starts Android app in kiosk mode. Keeps track of app start progress, pins app
// window when it's finally opened and notifies the Owner of app window launch.
class KioskArcvmAppLauncher : public ArcAppListPrefs::Observer,
                              public exo::WMHelper::ExoWindowObserver,
                              public aura::WindowObserver {
 public:
  // Owner class that initializes the KioskArcvmAppLauncher class and implements
  // the OnAppWindowLaunched functionality.
  // TODO(crbug.com/418946082): Refactor to callback and pass to LaunchApp.
  class Owner {
   public:
    Owner() = default;
    Owner(const Owner&) = delete;
    Owner& operator=(const Owner&) = delete;
    virtual void OnAppWindowLaunched() = 0;

   protected:
    virtual ~Owner() = default;
  };

  KioskArcvmAppLauncher(ArcAppListPrefs* prefs,
                        const std::string& app_id,
                        Owner* owner);
  KioskArcvmAppLauncher(const KioskArcvmAppLauncher&) = delete;
  KioskArcvmAppLauncher& operator=(const KioskArcvmAppLauncher&) = delete;
  ~KioskArcvmAppLauncher() override;

  // Launches the app associated with this launcher instance in landscape mode
  // and in non-touch mode.
  void LaunchApp(content::BrowserContext* context);

  // ArcAppListPrefs::Observer overrides.
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent,
                     int32_t session_id) override;

  // exo::WMHelper::ExoWindowObserver
  void OnExoWindowCreated(aura::Window* window) override;

  // aura::WindowObserver overrides.
  void OnWindowDestroying(aura::Window* window) override;

 private:
  // Check whether it's the app's window and pins it.
  bool CheckAndPinWindow(aura::Window* const window);
  void StopObserving();

  const std::string app_id_;
  const raw_ptr<ArcAppListPrefs> prefs_;
  std::optional<int> task_id_ = std::nullopt;
  std::set<raw_ptr<aura::Window, SetExperimental>> windows_;
  // Pointer to the owner of this class.
  const raw_ptr<Owner> owner_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_ARCVM_APP_KIOSK_ARCVM_APP_LAUNCHER_H_
