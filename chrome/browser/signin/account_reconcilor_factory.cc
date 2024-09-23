// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_reconcilor_factory.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/ash/account_manager/account_manager_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/signin/core/browser/mirror_landing_account_reconcilor_delegate.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ChromeOSLimitedAccessAccountReconcilorDelegate
    : public signin::MirrorAccountReconcilorDelegate {
 public:
  enum class ReconcilorBehavior {
    kChild,
    kEnterprise,
  };

  ChromeOSLimitedAccessAccountReconcilorDelegate(
      ReconcilorBehavior reconcilor_behavior,
      signin::IdentityManager* identity_manager)
      : signin::MirrorAccountReconcilorDelegate(identity_manager),
        reconcilor_behavior_(reconcilor_behavior) {}

  ChromeOSLimitedAccessAccountReconcilorDelegate(
      const ChromeOSLimitedAccessAccountReconcilorDelegate&) = delete;
  ChromeOSLimitedAccessAccountReconcilorDelegate& operator=(
      const ChromeOSLimitedAccessAccountReconcilorDelegate&) = delete;

  base::TimeDelta GetReconcileTimeout() const override {
    switch (reconcilor_behavior_) {
      case ReconcilorBehavior::kChild:
        return base::Seconds(10);
      case ReconcilorBehavior::kEnterprise:
        // 60 seconds is enough to cover about 99% of all reconcile cases.
        return base::Seconds(60);
      default:
        NOTREACHED_IN_MIGRATION();
        return MirrorAccountReconcilorDelegate::GetReconcileTimeout();
    }
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

    if (reconcilor_behavior_ == ReconcilorBehavior::kChild) {
      UMA_HISTOGRAM_BOOLEAN(
          "ChildAccountReconcilor.ForcedUserExitOnReconcileError", true);
    }
    // Force a logout.
    chrome::AttemptUserExit();
  }

 private:
  const ReconcilorBehavior reconcilor_behavior_;
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

AccountReconcilorFactory::AccountReconcilorFactory()
    : ProfileKeyedServiceFactory(
          "AccountReconcilor",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
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

KeyedService* AccountReconcilorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  SigninClient* signin_client =
      ChromeSigninClientFactory::GetForProfile(profile);
#if BUILDFLAG(IS_CHROMEOS)
  AccountReconcilor* reconcilor = new AccountReconcilor(
      identity_manager, signin_client,
      ::GetAccountManagerFacade(profile->GetPath().value()),
      CreateAccountReconcilorDelegate(profile));
#else
  AccountReconcilor* reconcilor =
      new AccountReconcilor(identity_manager, signin_client,
                            CreateAccountReconcilorDelegate(profile));
#endif  // BUILDFLAG(IS_CHROMEOS)
  reconcilor->Initialize(true /* start_reconcile_if_tokens_available */);
  return reconcilor;
}

void AccountReconcilorFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(prefs::kForceLogoutUnauthenticatedUserEnabled,
                                false);
#endif
  AccountReconcilor::RegisterProfilePrefs(registry);
}

// static
std::unique_ptr<signin::AccountReconcilorDelegate>
AccountReconcilorFactory::CreateAccountReconcilorDelegate(Profile* profile) {
  signin::AccountConsistencyMethod account_consistency =
      AccountConsistencyModeManager::GetMethodForProfile(profile);
  switch (account_consistency) {
    case signin::AccountConsistencyMethod::kMirror:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      // Only for child accounts on Chrome OS, use the specialized Mirror
      // delegate.
      if (profile->IsChild()) {
        return std::make_unique<ChromeOSLimitedAccessAccountReconcilorDelegate>(
            ChromeOSLimitedAccessAccountReconcilorDelegate::ReconcilorBehavior::
                kChild,
            IdentityManagerFactory::GetForProfile(profile));
      }

      if (profile->GetPrefs()->GetBoolean(
              prefs::kForceLogoutUnauthenticatedUserEnabled)) {
        return std::make_unique<ChromeOSLimitedAccessAccountReconcilorDelegate>(
            ChromeOSLimitedAccessAccountReconcilorDelegate::ReconcilorBehavior::
                kEnterprise,
            IdentityManagerFactory::GetForProfile(profile));
      }

      return std::make_unique<signin::MirrorAccountReconcilorDelegate>(
          IdentityManagerFactory::GetForProfile(profile));
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
      return std::make_unique<signin::MirrorLandingAccountReconcilorDelegate>(
          IdentityManagerFactory::GetForProfile(profile),
          ChromeSigninClientFactory::GetForProfile(profile)
              ->GetInitialPrimaryAccount()
              .has_value());
#else
      return std::make_unique<signin::MirrorAccountReconcilorDelegate>(
          IdentityManagerFactory::GetForProfile(profile));
#endif

    case signin::AccountConsistencyMethod::kDisabled:
      return std::make_unique<signin::AccountReconcilorDelegate>();

    case signin::AccountConsistencyMethod::kDice:
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      return std::make_unique<signin::DiceAccountReconcilorDelegate>(
          IdentityManagerFactory::GetForProfile(profile),
          ChromeSigninClientFactory::GetForProfile(profile));
#else
      NOTREACHED_IN_MIGRATION();
      return nullptr;
#endif
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}
