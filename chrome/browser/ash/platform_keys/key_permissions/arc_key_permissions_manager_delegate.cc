// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/platform_keys/key_permissions/arc_key_permissions_manager_delegate.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/platform_keys/extension_key_permissions_service.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"

namespace ash {
namespace platform_keys {

namespace {

// Owned by `ChromeBrowserMainPartsAsh`.
SystemTokenArcKpmDelegate* g_system_token_arc_usage_manager_delegate = nullptr;

}  // namespace

ArcKpmDelegate::Observer::Observer() = default;
ArcKpmDelegate::Observer::~Observer() = default;

ArcKpmDelegate::ArcKpmDelegate() = default;
ArcKpmDelegate::~ArcKpmDelegate() = default;

void ArcKpmDelegate::Shutdown() {}

void ArcKpmDelegate::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ArcKpmDelegate::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ArcKpmDelegate::NotifyArcUsageAllowanceForCorporateKeysChanged(
    bool allowed) {
  for (auto& observer : observer_list_) {
    observer.OnArcUsageAllowanceForCorporateKeysChanged(allowed);
  }
}

UserPrivateTokenArcKpmDelegate::UserPrivateTokenArcKpmDelegate(Profile* profile)
    : profile_(profile),
      is_primary_profile_(ProfileHelper::IsPrimaryProfile(profile)),
      policy_service_(profile->GetProfilePolicyConnector()->policy_service()) {
  DCHECK(profile_);
  DCHECK(policy_service_);

  if (is_primary_profile_) {
    SystemTokenArcKpmDelegate::Get()->SetPrimaryUserArcKpmDelegate(this);
  }

  policy_change_registrar_ = std::make_unique<policy::PolicyChangeRegistrar>(
      policy_service_, policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                               /*component_id=*/std::string()));

  policy_change_registrar_->Observe(
      policy::key::kKeyPermissions,
      base::BindRepeating(
          &UserPrivateTokenArcKpmDelegate::OnKeyPermissionsPolicyChanged,
          base::Unretained(this)));

  auto* arc_app_list_prefs = ArcAppListPrefs::Get(profile_);
  if (arc_app_list_prefs) {
    arc_app_list_prefs->AddObserver(this);
  }

  auto* arc_session_manager = arc::ArcSessionManager::Get();
  if (arc_session_manager) {
    arc_session_manager->AddObserver(this);
  }

  CheckArcKeyAvailibility();
}

UserPrivateTokenArcKpmDelegate::~UserPrivateTokenArcKpmDelegate() {
  if (is_primary_profile_) {
    SetArcUsageAllowance(false);
    SystemTokenArcKpmDelegate::Get()->SetPrimaryUserArcKpmDelegate(nullptr);
  }
}

bool UserPrivateTokenArcKpmDelegate::AreCorporateKeysAllowedForArcUsage()
    const {
  return corporate_keys_allowed_for_arc_usage_;
}

void UserPrivateTokenArcKpmDelegate::Shutdown() {
  if (is_shutdown_) {
    return;
  }

  is_shutdown_ = true;

  auto* arc_session_manager = arc::ArcSessionManager::Get();
  if (arc_session_manager) {
    arc_session_manager->RemoveObserver(this);
  }

  DCHECK(profile_);
  auto* arc_app_list_prefs = ArcAppListPrefs::Get(profile_);
  if (arc_app_list_prefs) {
    arc_app_list_prefs->RemoveObserver(this);
  }

  policy_change_registrar_.reset();

  profile_ = nullptr;
  policy_service_ = nullptr;
}

void UserPrivateTokenArcKpmDelegate::CheckArcKeyAvailibility() {
  if (!arc::IsArcAllowedForProfile(profile_)) {
    SetArcUsageAllowance(false);
    return;
  }

  std::vector<std::string> corporate_key_usage_allowed_app_ids =
      chromeos::platform_keys::ExtensionKeyPermissionsService::
          GetCorporateKeyUsageAllowedAppIds(policy_service_);

  for (const auto& package_name : corporate_key_usage_allowed_app_ids) {
    auto* arc_app_list_prefs = ArcAppListPrefs::Get(profile_);
    DCHECK(arc_app_list_prefs);

    if (arc_app_list_prefs->ArcAppListPrefs::IsPackageInstalled(package_name)) {
      SetArcUsageAllowance(true);
      return;
    }
  }

  SetArcUsageAllowance(false);
}

void UserPrivateTokenArcKpmDelegate::SetArcUsageAllowance(bool allowed) {
  if (corporate_keys_allowed_for_arc_usage_ != allowed) {
    corporate_keys_allowed_for_arc_usage_ = allowed;
    NotifyArcUsageAllowanceForCorporateKeysChanged(allowed);
  }
}

void UserPrivateTokenArcKpmDelegate::OnArcPlayStoreEnabledChanged(
    bool enabled) {
  CheckArcKeyAvailibility();
}

void UserPrivateTokenArcKpmDelegate::OnKeyPermissionsPolicyChanged(
    const base::Value* old_value,
    const base::Value* new_value) {
  CheckArcKeyAvailibility();
}

void UserPrivateTokenArcKpmDelegate::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  CheckArcKeyAvailibility();
}

void UserPrivateTokenArcKpmDelegate::OnPackageRemoved(
    const std::string& package_name,
    bool uninstalled) {
  CheckArcKeyAvailibility();
}

// static
SystemTokenArcKpmDelegate* SystemTokenArcKpmDelegate::Get() {
  return g_system_token_arc_usage_manager_delegate;
}

// static
void SystemTokenArcKpmDelegate::SetSystemTokenArcKpmDelegateForTesting(
    SystemTokenArcKpmDelegate* system_token_arc_kpm_delegate) {
  g_system_token_arc_usage_manager_delegate = system_token_arc_kpm_delegate;
}

SystemTokenArcKpmDelegate::SystemTokenArcKpmDelegate() {
  DCHECK(!g_system_token_arc_usage_manager_delegate);
  g_system_token_arc_usage_manager_delegate = this;

  DCHECK(!primary_user_arc_usage_manager_);

  // Notification for the initial state of the usage allowance. The state may
  // change after SetPrimaryUserArcKpmDelegate is called.
  NotifyArcUsageAllowanceForCorporateKeysChanged(false);
}

SystemTokenArcKpmDelegate::~SystemTokenArcKpmDelegate() {
  DCHECK(g_system_token_arc_usage_manager_delegate);
  g_system_token_arc_usage_manager_delegate = nullptr;

  ClearPrimaryUserArcKpmDelegate();
}

bool SystemTokenArcKpmDelegate::AreCorporateKeysAllowedForArcUsage() const {
  if (!primary_user_arc_usage_manager_) {
    return false;
  }

  return primary_user_arc_usage_manager_->AreCorporateKeysAllowedForArcUsage();
}

// TODO(crbug.com/1144820): Make setting the primary user's ARC KPM delegate in
// SystemTokenArcKpmDelegate more robust.
void SystemTokenArcKpmDelegate::SetPrimaryUserArcKpmDelegate(
    UserPrivateTokenArcKpmDelegate* primary_user_arc_usage_manager) {
  ClearPrimaryUserArcKpmDelegate();

  if (primary_user_arc_usage_manager == nullptr) {
    return;
  }

  primary_user_arc_usage_manager_ = primary_user_arc_usage_manager;
  primary_user_arc_usage_manager_delegate_observation_.Observe(
      primary_user_arc_usage_manager_.get());
  OnArcUsageAllowanceForCorporateKeysChanged(
      primary_user_arc_usage_manager_->AreCorporateKeysAllowedForArcUsage());
}

void SystemTokenArcKpmDelegate::ClearPrimaryUserArcKpmDelegate() {
  if (!primary_user_arc_usage_manager_) {
    return;
  }

  DCHECK(primary_user_arc_usage_manager_delegate_observation_.IsObservingSource(
      primary_user_arc_usage_manager_.get()));
  primary_user_arc_usage_manager_delegate_observation_.Reset();
  primary_user_arc_usage_manager_ = nullptr;
  OnArcUsageAllowanceForCorporateKeysChanged(false);
}

void SystemTokenArcKpmDelegate::OnArcUsageAllowanceForCorporateKeysChanged(
    bool allowed) {
  if (corporate_keys_allowed_for_arc_usage_ == allowed) {
    return;
  }

  corporate_keys_allowed_for_arc_usage_ = allowed;
  NotifyArcUsageAllowanceForCorporateKeysChanged(allowed);
}

}  // namespace platform_keys
}  // namespace ash
