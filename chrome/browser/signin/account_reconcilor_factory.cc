// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_reconcilor_factory.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/account_reconcilor_delegate.h"
#include "components/signin/core/browser/mirror_account_reconcilor_delegate.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_client.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/ash/account_manager/account_manager_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS)
class ChromeOSChildAccountReconcilorDelegate
    : public signin::MirrorAccountReconcilorDelegate {
 public:
  ChromeOSChildAccountReconcilorDelegate(
      signin::IdentityManager* identity_manager)
      : signin::MirrorAccountReconcilorDelegate(identity_manager) {}

  ChromeOSChildAccountReconcilorDelegate(
      const ChromeOSChildAccountReconcilorDelegate&) = delete;
  ChromeOSChildAccountReconcilorDelegate& operator=(
      const ChromeOSChildAccountReconcilorDelegate&) = delete;

  base::TimeDelta GetReconcileTimeout() const override {
    return base::Seconds(10);
  }

  void OnReconcileError(const GoogleServiceAuthError& error) override {
    // If |error| is |GoogleServiceAuthError::State::NONE| or a transient error.
    if (!error.IsPersistentError()) {
      return;
    }

    if (!GetIdentityManager()->HasAccountWithRefreshTokenInPersistentErrorState(
            GetIdentityManager()->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin))) {
      return;
    }

    // Mark the account to require an online sign in.
    const user_manager::User* primary_user =
        user_manager::UserManager::Get()->GetPrimaryUser();
    DCHECK(primary_user);
    user_manager::UserManager::Get()->SaveForceOnlineSignin(
        primary_user->GetAccountId(), true /* force_online_signin */);

    UMA_HISTOGRAM_BOOLEAN(
        "ChildAccountReconcilor.ForcedUserExitOnReconcileError", true);

    // Force a logout.
    chrome::AttemptUserExit();
  }
};
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

AccountReconcilorFactory::AccountReconcilorFactory()
    : ProfileKeyedServiceFactory(
          "AccountReconcilor",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .Build()) {
  DependsOn(ChromeSigninClientFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

AccountReconcilorFactory::~AccountReconcilorFactory() = default;

// static
AccountReconcilor* AccountReconcilorFactory::GetForProfile(Profile* profile) {
  return static_cast<AccountReconcilor*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AccountReconcilorFactory* AccountReconcilorFactory::GetInstance() {
  static base::NoDestructor<AccountReconcilorFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
AccountReconcilorFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  SigninClient* signin_client =
      ChromeSigninClientFactory::GetForProfile(profile);
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<AccountReconcilor> reconcilor =
      std::make_unique<AccountReconcilor>(
          identity_manager, signin_client,
          ash::AccountManagerFactory::Get()->GetAccountManagerFacade(
              profile->GetPath().value()),
          CreateAccountReconcilorDelegate(profile));
#else
  std::unique_ptr<AccountReconcilor> reconcilor =
      std::make_unique<AccountReconcilor>(
          identity_manager, signin_client,
          CreateAccountReconcilorDelegate(profile));
#endif  // BUILDFLAG(IS_CHROMEOS)
  reconcilor->Initialize(true /* start_reconcile_if_tokens_available */);
  return reconcilor;
}

void AccountReconcilorFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  AccountReconcilor::RegisterProfilePrefs(registry);
}

// static
std::unique_ptr<signin::AccountReconcilorDelegate>
AccountReconcilorFactory::CreateAccountReconcilorDelegate(Profile* profile) {
  signin::AccountConsistencyMethod account_consistency =
      AccountConsistencyModeManager::GetMethodForProfile(profile);
  switch (account_consistency) {
    case signin::AccountConsistencyMethod::kMirror:
#if BUILDFLAG(IS_CHROMEOS)
      // Only for child accounts on Chrome OS, use the specialized Mirror
      // delegate.
      if (profile->IsChild()) {
        return std::make_unique<ChromeOSChildAccountReconcilorDelegate>(
            IdentityManagerFactory::GetForProfile(profile));
      }
#endif
      return std::make_unique<signin::MirrorAccountReconcilorDelegate>(
          IdentityManagerFactory::GetForProfile(profile));

    case signin::AccountConsistencyMethod::kDisabled:
      return std::make_unique<signin::AccountReconcilorDelegate>();

    case signin::AccountConsistencyMethod::kDice:
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      return std::make_unique<signin::DiceAccountReconcilorDelegate>(
          IdentityManagerFactory::GetForProfile(profile),
          ChromeSigninClientFactory::GetForProfile(profile));
#else
      NOTREACHED();
#endif
  }

  NOTREACHED();
}
