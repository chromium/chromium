// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_ARC_ARC_KIOSK_APP_LAUNCHER_H_
#define CHROME_BROWSER_ASH_APP_MODE_ARC_ARC_KIOSK_APP_LAUNCHER_H_

#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace ash {

// Starts Android app in kiosk mode.
// Keeps track of start progress and pins app window
// when it's finally opened.
class ArcKioskAppLauncher : public ArcAppListPrefs::Observer,
                            public aura::EnvObserver,
                            public aura::WindowObserver {
 public:
  class Delegate {
   public:
    Delegate() = default;
    virtual void OnAppWindowLaunched() = 0;

   protected:
    virtual ~Delegate() = default;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  ArcKioskAppLauncher(content::BrowserContext* context,
                      ArcAppListPrefs* prefs,
                      const std::string& app_id,
                      Delegate* delegate);

  ~ArcKioskAppLauncher() override;

  // ArcAppListPrefs::Observer overrides.
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent,
                     int32_t session_id) override;

  // aura::EnvObserver overrides.
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver overrides.
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

 private:
  // Check whether it's the app's window and pins it.
  bool CheckAndPinWindow(aura::Window* const window);
  void StopObserving();

  const std::string app_id_;
  ArcAppListPrefs* const prefs_;
  int task_id_ = -1;
  std::set<aura::Window*> windows_;
  // Not owning the delegate, delegate owns this class.
  Delegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(ArcKioskAppLauncher);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_ARC_ARC_KIOSK_APP_LAUNCHER_H_
