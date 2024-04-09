// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GROWTH_CAMPAIGNS_MANAGER_SESSION_H_
#define CHROME_BROWSER_ASH_GROWTH_CAMPAIGNS_MANAGER_SESSION_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "ui/aura/window.h"

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

  void SetProfileForTesting(Profile* profile);

  aura::Window* GetOpenedWindow() { return opened_window_; }

 private:
  Profile* GetProfile();
  bool IsEligible();
  void SetupWindowObserver();
  void OnLoadCampaignsCompleted();

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};

  raw_ptr<Profile, DanglingUntriaged> profile_for_testing_ = nullptr;
  GURL active_url_;

  base::ScopedObservation<apps::InstanceRegistry,
                          apps::InstanceRegistry::Observer>
      scoped_observation_{this};

  // Dangling when executing
  // AudioSettingsInteractiveUiTest.LaunchAudioSettingDisabledOnLockScreen:
  raw_ptr<aura::Window, DanglingUntriaged> opened_window_ = nullptr;

  base::WeakPtrFactory<CampaignsManagerSession> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_GROWTH_CAMPAIGNS_MANAGER_SESSION_H_
