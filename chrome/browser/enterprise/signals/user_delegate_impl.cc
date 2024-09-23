// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/user_delegate_impl.h"

#include <set>

#include "base/check.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/common_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/policy_types.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace enterprise_signals {

using DTCPolicyLevel = enterprise_connectors::DTCPolicyLevel;

namespace {

policy::PolicyScope ToPolicyScope(DTCPolicyLevel policy_level) {
  switch (policy_level) {
    case DTCPolicyLevel::kUser:
      return policy::POLICY_SCOPE_USER;
    case DTCPolicyLevel::kBrowser:
      return policy::POLICY_SCOPE_MACHINE;
  }
}

}  // namespace

UserDelegateImpl::UserDelegateImpl(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    enterprise_connectors::DeviceTrustConnectorService*
        device_trust_connector_service)
    : profile_(profile),
      identity_manager_(identity_manager),
      device_trust_connector_service_(device_trust_connector_service) {
  CHECK(profile_);
}

UserDelegateImpl::~UserDelegateImpl() = default;

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool UserDelegateImpl::IsSigninContext() const {
  return ash::ProfileHelper::IsSigninProfile(profile_);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

bool UserDelegateImpl::IsAffiliated() const {
  return enterprise_util::IsProfileAffiliated(profile_);
}

bool UserDelegateImpl::IsManagedUser() const {
  const auto* profile_policy_connector = profile_->GetProfilePolicyConnector();

  if (!profile_policy_connector) {
    return false;
  }

  return profile_policy_connector->IsManaged();
}

bool UserDelegateImpl::IsSameUser(const std::string& gaia_id) const {
  return identity_manager_ &&
         identity_manager_->GetPrimaryAccountId(
             signin::ConsentLevel::kSignin) ==
             identity_manager_->FindExtendedAccountInfoByGaiaId(gaia_id)
                 .account_id;
}

std::set<policy::PolicyScope> UserDelegateImpl::GetPolicyScopesNeedingSignals()
    const {
  std::set<policy::PolicyScope> policy_scopes;
  if (device_trust_connector_service_) {
    for (const auto policy_level :
         device_trust_connector_service_->GetEnabledInlinePolicyLevels()) {
      policy_scopes.insert(ToPolicyScope(policy_level));
    }
  }
  return policy_scopes;
}

}  // namespace enterprise_signals
