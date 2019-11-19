// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_status_metrics_provider_delegate.h"

#include <string>
#include <vector>

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/core/browser/signin_status_metrics_provider.h"
#include "components/signin/public/identity_manager/identity_manager.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#endif

ChromeSigninStatusMetricsProviderDelegate::
    ChromeSigninStatusMetricsProviderDelegate() {}

ChromeSigninStatusMetricsProviderDelegate::
    ~ChromeSigninStatusMetricsProviderDelegate() {
#if !defined(OS_ANDROID)
  BrowserList::RemoveObserver(this);
#endif

  auto* factory = IdentityManagerFactory::GetInstance();
  if (factory)
    factory->RemoveObserver(this);
}

void ChromeSigninStatusMetricsProviderDelegate::Initialize() {
#if !defined(OS_ANDROID)
  // On Android, there is always only one profile in any situation, opening new
  // windows (which is possible with only some Android devices) will not change
  // the opened profiles signin status.
  BrowserList::AddObserver(this);
#endif

  auto* factory = IdentityManagerFactory::GetInstance();
  if (factory)
    factory->AddObserver(this);
}

AccountsStatus
ChromeSigninStatusMetricsProviderDelegate::GetStatusOfAllAccounts() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::vector<Profile*> profile_list = profile_manager->GetLoadedProfiles();

  AccountsStatus accounts_status;
  accounts_status.num_accounts = profile_list.size();
  for (Profile* profile : profile_list) {
#if !defined(OS_ANDROID)
    if (chrome::GetBrowserCount(profile) == 0) {
      // The profile is loaded, but there's no opened browser for this profile.
      continue;
    }
#endif
    accounts_status.num_opened_accounts++;

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile->GetOriginalProfile());
    if (identity_manager && identity_manager->HasPrimaryAccount())
      accounts_status.num_signed_in_accounts++;
  }

  return accounts_status;
}

std::vector<signin::IdentityManager*>
ChromeSigninStatusMetricsProviderDelegate::GetIdentityManagersForAllAccounts() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::vector<Profile*> profiles = profile_manager->GetLoadedProfiles();

  std::vector<signin::IdentityManager*> managers;
  for (Profile* profile : profiles) {
    auto* identity_manager =
        IdentityManagerFactory::GetForProfileIfExists(profile);
    if (identity_manager)
      managers.push_back(identity_manager);
  }

  return managers;
}

#if !defined(OS_ANDROID)
void ChromeSigninStatusMetricsProviderDelegate::OnBrowserAdded(
    Browser* browser) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser->profile());

  // Nothing will change if the opened browser is in incognito mode.
  if (!identity_manager)
    return;

  const bool signed_in = identity_manager->HasPrimaryAccount();
  UpdateStatusWhenBrowserAdded(signed_in);
}
#endif

void ChromeSigninStatusMetricsProviderDelegate::IdentityManagerCreated(
    signin::IdentityManager* identity_manager) {
  owner()->OnIdentityManagerCreated(identity_manager);
}

void ChromeSigninStatusMetricsProviderDelegate::IdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  owner()->OnIdentityManagerShutdown(identity_manager);
}

void ChromeSigninStatusMetricsProviderDelegate::UpdateStatusWhenBrowserAdded(
    bool signed_in) {
#if !defined(OS_ANDROID)
  SigninStatusMetricsProviderBase::SigninStatus status =
      owner()->signin_status();

  // NOTE: If |status| is MIXED_SIGNIN_STATUS, this method
  // intentionally does not update it.
  if ((status == SigninStatusMetricsProviderBase::ALL_PROFILES_NOT_SIGNED_IN &&
       signed_in) ||
      (status == SigninStatusMetricsProviderBase::ALL_PROFILES_SIGNED_IN &&
       !signed_in)) {
    owner()->UpdateSigninStatus(
        SigninStatusMetricsProviderBase::MIXED_SIGNIN_STATUS);
  } else if (status == SigninStatusMetricsProviderBase::UNKNOWN_SIGNIN_STATUS) {
    // If when function ProvideCurrentSessionData() is called, Chrome is
    // running in the background with no browser window opened, |signin_status_|
    // will be reset to |UNKNOWN_SIGNIN_STATUS|. Then this newly added browser
    // is the only opened browser/profile and its signin status represents
    // the whole status.
    owner()->UpdateSigninStatus(
        signed_in
            ? SigninStatusMetricsProviderBase::ALL_PROFILES_SIGNED_IN
            : SigninStatusMetricsProviderBase::ALL_PROFILES_NOT_SIGNED_IN);
  }
#endif
}
