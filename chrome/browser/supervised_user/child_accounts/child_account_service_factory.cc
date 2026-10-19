// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/family_link_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/child_account_service.h"
#include "components/supervised_user/core/browser/family_link_settings_service.h"
#include "components/supervised_user/core/common/features.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"
#endif

namespace {
// Asserts that `is_child` matches the child status of the primary user.
// Terminates user session in case of status mismatch in order to prevent
// supervision incidents. Relevant on Chrome OS platform that has the concept
// of the user.
void AssertChildStatusOfTheUser(Profile* profile, bool is_child) {
#if BUILDFLAG(IS_CHROMEOS)
  if (profile->AsTestingProfile() != nullptr) {
    // Bail out during unit tests using TestingProfile. They have hard time
    // simulating ChromeOS user types.
    // TODO(crbug.com/561937726): Consider creating a drop-in test utility that
    // would seamlessly assert expected ChromeOS user type, or better: preset
    // such user type alongside with the TestingProfile.
    return;
  }
  user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (user && is_child != (user->GetType() == user_manager::UserType::kChild)) {
    LOG(FATAL) << "User child flag has changed: " << is_child;
  }
  if (!user && ash::ProfileHelper::IsUserProfile(profile)) {
    LOG(FATAL) << "User instance not found while setting child account flag.";
  }
#endif
}

}  // namespace

// static
supervised_user::ChildAccountService* ChildAccountServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<supervised_user::ChildAccountService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ChildAccountServiceFactory* ChildAccountServiceFactory::GetInstance() {
  static base::NoDestructor<ChildAccountServiceFactory> instance;
  return instance.get();
}

ChildAccountServiceFactory::ChildAccountServiceFactory()
    : ProfileKeyedServiceFactory(
          "ChildAccountService",
          supervised_user::BuildProfileSelectionsForRegularAndGuest()) {
  // Source of truth for the supervision status.
  DependsOn(IdentityManagerFactory::GetInstance());
  // Consumer that shall be activated according to supervision status.
  DependsOn(supervised_user::FamilyLinkSettingsServiceFactory::GetInstance());
}

ChildAccountServiceFactory::~ChildAccountServiceFactory() = default;

std::unique_ptr<KeyedService>
ChildAccountServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    // Match lifecycle of the identity manager. No identity makes impossible to
    // determine supervision status.
    return nullptr;
  }

  CHECK(profile->GetPrefs());
  return std::make_unique<supervised_user::ChildAccountService>(
      *profile->GetPrefs(), identity_manager,
      CHECK_DEREF(supervised_user::FamilyLinkSettingsServiceFactory::GetForKey(
          profile->GetProfileKey())),
      base::BindOnce(&AssertChildStatusOfTheUser, profile));
}

bool ChildAccountServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
