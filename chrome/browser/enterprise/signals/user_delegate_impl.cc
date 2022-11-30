// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/user_delegate_impl.h"

#include "base/check.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace enterprise_signals {

UserDelegateImpl::UserDelegateImpl(Profile* profile,
                                   signin::IdentityManager* identity_manager)
    : profile_(profile), identity_manager_(identity_manager) {
  DCHECK(profile_);
  DCHECK(identity_manager_);
}

UserDelegateImpl::~UserDelegateImpl() = default;

bool UserDelegateImpl::IsAffiliated() const {
  return chrome::enterprise_util::IsProfileAffiliated(profile_);
}

bool UserDelegateImpl::IsManaged() const {
  const auto* profile_policy_connector = profile_->GetProfilePolicyConnector();

  if (!profile_policy_connector) {
    return false;
  }

  return profile_policy_connector->IsManaged();
}

bool UserDelegateImpl::IsSameUser(const std::string& gaia_id) const {
  return identity_manager_->GetPrimaryAccountId(
             signin::ConsentLevel::kSignin) ==
         identity_manager_->FindExtendedAccountInfoByGaiaId(gaia_id).account_id;
}

}  // namespace enterprise_signals
