// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_reuse_manager_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_reuse_detector_impl.h"
#include "components/password_manager/core/browser/password_reuse_manager_impl.h"
#include "components/password_manager/core/browser/password_reuse_manager_signin_notifier_impl.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/shared_preferences_delegate.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/password_manager/android/shared_preferences_delegate_android.h"
#endif

namespace {

using password_manager::metrics_util::SignInState;

SignInState GetSignInStateForMetrics(Profile* profile) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(profile);
  if (!identity_manager) {
    return SignInState::kSignedOut;
  }

  // TODO(crbug.com/40066949): Simplify once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  const std::string sync_username =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
          .email;
  if (!sync_username.empty()) {
    return SignInState::kSyncing;
  }

  const bool is_signed_in =
      !identity_manager->GetAccountsWithRefreshTokens().empty();
  return is_signed_in ? SignInState::kSignedInSyncDisabled
                      : SignInState::kSignedOut;
}

}  // namespace

PasswordReuseManagerFactory::PasswordReuseManagerFactory()
    : ProfileKeyedServiceFactory(
          "PasswordReuseManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ProfilePasswordStoreFactory::GetInstance());
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

std::unique_ptr<KeyedService>
PasswordReuseManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK(base::FeatureList::IsEnabled(
      password_manager::features::kPasswordReuseDetectionEnabled));

  Profile* profile = Profile::FromBrowserContext(context);

  password_manager::PasswordStoreInterface* store =
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)
          .get();
  // Incognito, guest, or system profiles doesn't have PasswordStore so
  // PasswordReuseManager shouldn't be created as well.
  if (!store) {
    return nullptr;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  std::unique_ptr<password_manager::SharedPreferencesDelegate>
      shared_pref_delegate;

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          password_manager::features::kFetchGaiaHashOnSignIn)) {
    shared_pref_delegate = std::make_unique<SharedPreferencesDelegateAndroid>();
  }
#endif
  auto reuse_manager =
      std::make_unique<password_manager::PasswordReuseManagerImpl>();
  reuse_manager->Init(
      profile->GetPrefs(), g_browser_process->local_state(),
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)
          .get(),
      AccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)
          .get(),
      std::make_unique<password_manager::PasswordReuseDetectorImpl>(),
      identity_manager, std::move(shared_pref_delegate));

  // Prepare password hash data for reuse detection.
  reuse_manager->PreparePasswordHashData(GetSignInStateForMetrics(profile));

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<password_manager::PasswordReuseManagerSigninNotifier>
      notifier = std::make_unique<
          password_manager::PasswordReuseManagerSigninNotifierImpl>(
          IdentityManagerFactory::GetForProfile(profile));
  reuse_manager->SetPasswordReuseManagerSigninNotifier(std::move(notifier));
#endif

  return reuse_manager;
}
