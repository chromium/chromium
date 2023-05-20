// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNALS_USER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNALS_USER_DELEGATE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "components/device_signals/core/browser/user_delegate.h"

class Profile;

namespace enterprise_connectors {
class DeviceTrustConnectorService;
}  // namespace enterprise_connectors

namespace signin {
class IdentityManager;
}  // namespace signin

namespace enterprise_signals {

class UserDelegateImpl : public device_signals::UserDelegate {
 public:
  UserDelegateImpl(Profile* profile,
                   signin::IdentityManager* identity_manager,
                   enterprise_connectors::DeviceTrustConnectorService*
                       device_trust_connector_service);
  ~UserDelegateImpl() override;

  UserDelegateImpl(const UserDelegateImpl&) = delete;
  UserDelegateImpl& operator=(const UserDelegateImpl&) = delete;

  // UserDelegate:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool IsSigninContext() const override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  bool IsAffiliated() const override;
  bool IsManagedUser() const override;
  bool IsSameUser(const std::string& gaia_id) const override;
  std::set<policy::PolicyScope> GetPolicyScopesNeedingSignals() const override;

 private:
  const raw_ptr<Profile> profile_;
  const raw_ptr<signin::IdentityManager> identity_manager_;

  // The connector service in charge of giving information about whether the
  // Device Trust connector is enabled or not. Can be nullptr if the
  // browser/profile is in an unsupported context (e.g. incognito).
  const raw_ptr<enterprise_connectors::DeviceTrustConnectorService>
      device_trust_connector_service_;
};

}  // namespace enterprise_signals

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNALS_USER_DELEGATE_IMPL_H_
