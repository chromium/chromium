// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GROWTH_CAMPAIGNS_MANAGER_SESSION_H_
#define CHROME_BROWSER_ASH_GROWTH_CAMPAIGNS_MANAGER_SESSION_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

class Profile;

// Campaigns Manager session to store camapigns manager specific state, and to
// observe related components changes to conditionally trigger proactive growth
// slots.
class CampaignsManagerSession : public session_manager::SessionManagerObserver {
 public:
  CampaignsManagerSession();
  CampaignsManagerSession(const CampaignsManagerSession&) = delete;
  CampaignsManagerSession& operator=(const CampaignsManagerSession&) = delete;
  ~CampaignsManagerSession() override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  void SetProfileForTesting(Profile* profile);

 private:
  Profile* GetProfile();
  bool IsEligible();
  void MaybeTriggerProactiveCampaigns();

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};

  raw_ptr<Profile, DanglingUntriaged> profile_for_testing_ = nullptr;

  base::WeakPtrFactory<CampaignsManagerSession> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_GROWTH_CAMPAIGNS_MANAGER_SESSION_H_
