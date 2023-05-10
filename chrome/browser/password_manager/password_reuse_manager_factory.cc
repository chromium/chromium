// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_reuse_manager_factory.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/password_manager/core/browser/password_reuse_manager_impl.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/password_store_signin_notifier_impl.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace {

std::string GetSyncUsername(Profile* profile) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(profile);
  return identity_manager
             ? identity_manager
                   ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
                   .email
             : std::string();
}

bool IsSignedIn(Profile* profile) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(profile);
  return identity_manager
             ? !identity_manager->GetAccountsWithRefreshTokens().empty()
             : false;
}

}  // namespace

PasswordReuseManagerFactory::PasswordReuseManagerFactory()
    : ProfileKeyedServiceFactory(
          "PasswordReuseManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(PasswordStoreFactory::GetInstance());
  DependsOn(AccountPasswordStoreFactory::GetInstance());
}

PasswordReuseManagerFactory::~PasswordReuseManagerFactory() = default;

PasswordReuseManagerFactory* PasswordReuseManagerFactory::GetInstance() {
  static base::NoDestructor<PasswordReuseManagerFactory> instance;
  return instance.get();
}

password_manager::PasswordReuseManager*
PasswordReuseManagerFactory::GetForProfile(Profile* profile) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kPasswordReuseDetectionEnabled)) {
    return nullptr;
  }

  return static_cast<password_manager::PasswordReuseManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

KeyedService* PasswordReuseManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(base::FeatureList::IsEnabled(
      password_manager::features::kPasswordReuseDetectionEnabled));

  Profile* profile = Profile::FromBrowserContext(context);

  password_manager::PasswordStoreInterface* store =
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::EXPLICIT_ACCESS)
          .get();
  // Incognito, guest, or system profiles doesn't have PasswordStore so
  // PasswordReuseManager shouldn't be created as well.
  if (!store)
    return nullptr;

  password_manager::PasswordReuseManager* reuse_manager =
      new password_manager::PasswordReuseManagerImpl();
  reuse_manager->Init(profile->GetPrefs(),
                      PasswordStoreFactory::GetForProfile(
                          profile, ServiceAccessType::EXPLICIT_ACCESS)
                          .get(),
                      AccountPasswordStoreFactory::GetForProfile(
                          profile, ServiceAccessType::EXPLICIT_ACCESS)
                          .get());

  // Prepare password hash data for reuse detection.
  reuse_manager->PreparePasswordHashData(GetSyncUsername(profile),
                                         IsSignedIn(profile));

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<password_manager::PasswordStoreSigninNotifier> notifier =
      std::make_unique<password_manager::PasswordStoreSigninNotifierImpl>(
          IdentityManagerFactory::GetForProfile(profile));
  reuse_manager->SetPasswordStoreSigninNotifier(std::move(notifier));
#endif

  return reuse_manager;
}
