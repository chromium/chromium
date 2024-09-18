// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GROWTH_CAMPAIGNS_MANAGER_SESSION_H_
#define CHROME_BROWSER_ASH_GROWTH_CAMPAIGNS_MANAGER_SESSION_H_

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "ui/aura/window.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

class Profile;

// Campaigns Manager session to store camapigns manager specific state, and to
// observe related components changes to conditionally trigger proactive growth
// slots.
class CampaignsManagerSession : public session_manager::SessionManagerObserver,
                                public apps::InstanceRegistry::Observer {
 public:
  CampaignsManagerSession();
  CampaignsManagerSession(const CampaignsManagerSession&) = delete;
  CampaignsManagerSession& operator=(const CampaignsManagerSession&) = delete;
  ~CampaignsManagerSession() override;

  static CampaignsManagerSession* Get();

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // apps::InstanceRegistry::Observer:
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override;
  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* cache) override;

  void PrimaryPageChanged(const content::WebContents* web_contents);

  void MaybeTriggerCampaignsOnEvent(std::string_view event);

  aura::Window* GetOpenedWindow() { return opened_window_; }

  void SetProfileForTesting(Profile* profile);

 private:
  void SetupWindowObserver();
  void OnOwnershipDetermined(bool is_user_owner);
  void OnLoadCampaignsCompleted();
  void StartDelayedTimer();

  void CacheAppOpenContext(const apps::InstanceUpdate& update, const GURL& url);
  void ClearAppOpenContext();

  // Handles instance update of app other than web browser/pwa/swa and Arc app.
  void HandleAppInstanceUpdate(const apps::InstanceUpdate& update);

  // Handles Arc instance update.
  void HandleArcInstanceUpdate(const apps::InstanceUpdate& update);

  // Handles Chrome browser and Lacros browser instance update. It caches
  // current web browser context but defers campaign trigger to
  // PrimaryPageChanged when page navigations happens.
  void HandleWebBrowserInstanceUpdate(const apps::InstanceUpdate& update);

  // Handles Pwa or Swa instance update.
  void HandlePwaInstanceUpdate(const apps::InstanceUpdate& update);

  // Handles app destruction update.
  void HandleAppInstanceDestruction(const apps::InstanceUpdate& update);

  void MaybeTriggerCampaignsWhenAppOpened();

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};

  base::ScopedObservation<apps::InstanceRegistry,
                          apps::InstanceRegistry::Observer>
      scoped_observation_{this};

  // Dangling when executing
  // AudioSettingsInteractiveUiTest.LaunchAudioSettingDisabledOnLockScreen:
  raw_ptr<aura::Window, DanglingUntriaged> opened_window_ = nullptr;

  // A timer to trigger campaigns after the campaigns loaded.
  base::OneShotTimer delayed_timer_;

  base::WeakPtrFactory<CampaignsManagerSession> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_GROWTH_CAMPAIGNS_MANAGER_SESSION_H_
