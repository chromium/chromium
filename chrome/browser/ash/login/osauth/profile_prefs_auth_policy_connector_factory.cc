// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/osauth/profile_prefs_auth_policy_connector_factory.h"

#include "base/check_is_test.h"
#include "chrome/browser/ash/login/osauth/profile_prefs_auth_policy_connector.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/osauth/impl/login_screen_auth_policy_connector.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"

namespace ash {

// static
ProfilePrefsAuthPolicyConnectorFactory*
ProfilePrefsAuthPolicyConnectorFactory::GetInstance() {
  return base::Singleton<ProfilePrefsAuthPolicyConnectorFactory>::get();
}

// static
ProfilePrefsAuthPolicyConnector*
ProfilePrefsAuthPolicyConnectorFactory::GetForProfile(Profile* profile) {
  return static_cast<ProfilePrefsAuthPolicyConnector*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ProfilePrefsAuthPolicyConnectorFactory::ProfilePrefsAuthPolicyConnectorFactory()
    : BrowserContextKeyedServiceFactory(
          "ProfilePrefsAuthPolicyConnector",
          BrowserContextDependencyManager::GetInstance()) {}

ProfilePrefsAuthPolicyConnectorFactory::
    ~ProfilePrefsAuthPolicyConnectorFactory() = default;

KeyedService* ProfilePrefsAuthPolicyConnectorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* connector = new ProfilePrefsAuthPolicyConnector();
  AuthParts::Get()->SetProfilePrefsAuthPolicyConnector(connector);
  return connector;
}

content::BrowserContext*
ProfilePrefsAuthPolicyConnectorFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Instance from the primary/main profile apply to all profiles.

  if (!context || context->IsOffTheRecord()) {
    return nullptr;
  }

  auto* profile = Profile::FromBrowserContext(context);
  if (profile->AsTestingProfile() || profile->IsGuestSession() ||
      !ProfileHelper::IsUserProfile(profile)) {
    return nullptr;
  }

  AuthParts::Get()->ReleaseEarlyLoginAuthPolicyConnector();
  auto* user_manager = user_manager::UserManager::Get();
  if (auto* primary_user = user_manager->GetPrimaryUser()) {
    if (auto* primary_browser_context =
            ash::BrowserContextHelper::Get()->GetBrowserContextByUser(
                primary_user)) {
      return primary_browser_context;
    }
  }
  return context;
}

bool ProfilePrefsAuthPolicyConnectorFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace ash
