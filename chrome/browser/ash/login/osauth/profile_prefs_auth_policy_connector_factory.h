// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OSAUTH_PROFILE_PREFS_AUTH_POLICY_CONNECTOR_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_OSAUTH_PROFILE_PREFS_AUTH_POLICY_CONNECTOR_FACTORY_H_

#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/login/osauth/profile_prefs_auth_policy_connector.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace ash {

class ProfilePrefsAuthPolicyConnectorFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ProfilePrefsAuthPolicyConnector* GetForProfile(Profile* profile);
  static ProfilePrefsAuthPolicyConnectorFactory* GetInstance();

  ProfilePrefsAuthPolicyConnectorFactory(
      const ProfilePrefsAuthPolicyConnectorFactory&) = delete;
  ProfilePrefsAuthPolicyConnectorFactory& operator=(
      const ProfilePrefsAuthPolicyConnectorFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<
      ProfilePrefsAuthPolicyConnectorFactory>;

  ProfilePrefsAuthPolicyConnectorFactory();
  ~ProfilePrefsAuthPolicyConnectorFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_OSAUTH_PROFILE_PREFS_AUTH_POLICY_CONNECTOR_FACTORY_H_
