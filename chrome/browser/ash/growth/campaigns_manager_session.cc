// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/campaigns_manager_session.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/growth/action_performer.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "chromeos/ash/components/growth/campaigns_model.h"
#include "components/app_constants/constants.h"
#include "components/session_manager/session_manager_types.h"

namespace {

CampaignsManagerSession* g_instance = nullptr;

std::optional<growth::ActionType> GetActionTypeBySlot(growth::Slot slot) {
  if (slot == growth::Slot::kNotification) {
    return growth::ActionType::kShowNotification;
  }

  if (slot == growth::Slot::kNudge) {
    return growth::ActionType::kShowNudge;
  }

  return std::nullopt;
}

void MaybeTriggerSlot(growth::Slot slot) {
  const auto action_type = GetActionTypeBySlot(slot);
  if (!action_type) {
    LOG(ERROR) << "Invalid: no supported action type for slot "
               << static_cast<int>(action_type.value());
    return;
  }

  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);

  auto* campaign = campaigns_manager->GetCampaignBySlot(slot);
  if (!campaign) {
    // No campaign matched.
    return;
  }

  auto campaign_id = growth::GetCampaignId(campaign);
  if (!campaign_id) {
    LOG(ERROR) << "Invalid: Missing campaign id.";
    return;
  }

  const auto* payload = growth::GetPayloadBySlot(campaign, slot);
  if (!payload) {
    LOG(ERROR) << "Invalid: Missing payload.";
    return;
  }

  campaigns_manager->PerformAction(campaign_id.value(), action_type.value(),
                                   payload);
}

void MaybeTriggerCampaignsWhenAppOpened() {
  if (!ash::features::IsGrowthCampaignsTriggerByAppOpenEnabled()) {
    return;
  }

  MaybeTriggerSlot(growth::Slot::kNudge);
  MaybeTriggerSlot(growth::Slot::kNotification);
}

}  // namespace

// static
CampaignsManagerSession* CampaignsManagerSession::Get() {
  return g_instance;
}

CampaignsManagerSession::CampaignsManagerSession() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;

  // SessionManager may be unset in unit tests.
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager) {
    session_manager_observation_.Observe(session_manager);
    OnSessionStateChanged();
  }
}

CampaignsManagerSession::~CampaignsManagerSession() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void CampaignsManagerSession::OnSessionStateChanged() {
  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::ACTIVE) {
    // Loads campaigns at session active only.
    return;
  }

  if (!IsEligible()) {
    return;
  }
  SetupWindowObserver();

  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);
  campaigns_manager->LoadCampaigns(
      base::BindOnce(&CampaignsManagerSession::MaybeTriggerProactiveCampaigns,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CampaignsManagerSession::OnInstanceUpdate(
    const apps::InstanceUpdate& update) {
  // No state changes. Ignore the update.
  if (!update.StateChanged()) {
    return;
  }

  auto app_id = update.AppId();

  if (app_id == app_constants::kChromeAppId ||
      app_id == app_constants::kAshDebugBrowserAppId ||
      app_id == app_constants::kLacrosAppId) {
    // TODO: b/331975665 - handle browser app with URL targeting.
    return;
  }

  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);

  if (update.IsCreation()) {
    campaigns_manager->SetOpenedApp(app_id);
    opened_window_ = update.Window();

    MaybeTriggerCampaignsWhenAppOpened();
  } else if (update.IsDestruction()) {
    // TODO: b/330409492 - Maybe trigger a campaign when app is about to be
    // destroyed.
    if (app_id == campaigns_manager->GetOpenedAppId()) {
      campaigns_manager->SetOpenedApp(std::string());
      opened_window_ = nullptr;
    }
  }
}

void CampaignsManagerSession::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* cache) {
  if (scoped_observation_.GetSource() == cache) {
    scoped_observation_.Reset();
  }
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

void CampaignsManagerSession::SetupWindowObserver() {
  auto* profile = GetProfile();
  // Some test profiles will not have AppServiceProxy.
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return;
  }
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  CHECK(proxy);
  scoped_observation_.Observe(&proxy->InstanceRegistry());
}

void CampaignsManagerSession::MaybeTriggerProactiveCampaigns() {
  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);

  // TODO(b/318885858): Trigger campaigns if matched.
}
