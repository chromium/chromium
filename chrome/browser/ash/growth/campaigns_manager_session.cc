// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/campaigns_manager_session.h"

#include "base/check.h"
#include "base/logging.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "components/session_manager/session_manager_types.h"

CampaignsManagerSession::CampaignsManagerSession() {
  // SessionManager may be unset in unit tests.
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager) {
    session_manager_observation_.Observe(session_manager);
    OnSessionStateChanged();
  }
}

CampaignsManagerSession::~CampaignsManagerSession() = default;

void CampaignsManagerSession::OnSessionStateChanged() {
  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::ACTIVE) {
    // Loads campaigns at session active only.
    return;
  }

  if (!IsEligible()) {
    return;
  }

  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);
  campaigns_manager->LoadCampaigns(
      base::BindOnce(&CampaignsManagerSession::MaybeTriggerProactiveCampaigns,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CampaignsManagerSession::SetProfileForTesting(Profile* profile) {
  profile_for_testing_ = profile;
}

Profile* CampaignsManagerSession::GetProfile() {
  if (profile_for_testing_) {
    return profile_for_testing_;
  }

  return ProfileManager::GetActiveUserProfile();
}

bool CampaignsManagerSession::IsEligible() {
  Profile* profile = GetProfile();
  CHECK(profile);
  // TODO(b/320789239): Enable for unicorn users.
  if (profile->GetProfilePolicyConnector()->IsManaged()) {
    // Only enabled for consumer session for now.
    // Demo Mode session is handled separately at `DemoSession`.
    return false;
  }

  return true;
}

void CampaignsManagerSession::MaybeTriggerProactiveCampaigns() {
  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);

  // TODO(b/318885858): Trigger nudge if nudge campaigns is matched.
}
