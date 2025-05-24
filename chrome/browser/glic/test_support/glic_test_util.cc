// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/glic_test_util.h"

#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

BrowserActivator::BrowserActivator() {
  BrowserList::AddObserver(this);
}

BrowserActivator::~BrowserActivator() {
  BrowserList::RemoveObserver(this);
}

void BrowserActivator::SetMode(Mode mode) {
  mode_ = mode;
}

void BrowserActivator::OnBrowserAdded(Browser* browser) {
  switch (mode_) {
    case Mode::kSingleBrowser:
      CHECK(!active_browser_) << "BrowserActivator::kSingleBrowser found "
                                 "second active browser.";
      break;
    case Mode::kFirst:
      if (active_browser_) {
        return;
      }
      break;
    case Mode::kManual:
      return;
  }

  SetActivePrivate(browser);
}

void BrowserActivator::OnBrowserRemoved(Browser* browser) {
  if (active_browser_.get() == browser || active_browser_.WasInvalidated()) {
    active_lock_.reset();
    if (mode_ == Mode::kFirst) {
      if (!BrowserList::GetInstance()->empty()) {
        SetActivePrivate(*BrowserList::GetInstance()->begin());
      }
    }
  }
}
void BrowserActivator::SetActive(Browser* browser) {
  mode_ = Mode::kManual;
  if (!browser) {
    active_lock_.reset();
    active_browser_ = nullptr;
  } else {
    SetActivePrivate(browser);
  }
}

void BrowserActivator::SetActivePrivate(Browser* browser) {
  CHECK(browser);
  active_lock_ = browser->GetBrowserView().GetWidget()->LockPaintAsActive();
  active_browser_ = browser->AsWeakPtr();
}

void ForceSigninAndModelExecutionCapability(Profile* profile) {
  SetFRECompletion(profile, prefs::FreStatus::kCompleted);
  SigninWithPrimaryAccount(profile);
  SetModelExecutionCapability(profile, true);
}

void SigninWithPrimaryAccount(Profile* profile) {
  // Sign-in and enable account capability.
  auto* const identity_manager = IdentityManagerFactory::GetForProfile(profile);
  auto account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, "glic-test@example.com", signin::ConsentLevel::kSignin);
  ASSERT_FALSE(account_info.IsEmpty());

  account_info.full_name = "Glic Testing";
  account_info.given_name = "Glic";
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);
}

void SetModelExecutionCapability(Profile* profile, bool enabled) {
  auto* const identity_manager = IdentityManagerFactory::GetForProfile(profile);
  AccountInfo primary_account =
      identity_manager->FindExtendedAccountInfoByAccountId(
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(primary_account.IsEmpty());

  AccountCapabilitiesTestMutator mutator(&primary_account.capabilities);
  mutator.set_can_use_model_execution_features(enabled);
  signin::UpdateAccountInfoForAccount(identity_manager, primary_account);
}

void SetFRECompletion(Profile* profile, prefs::FreStatus fre_status) {
  profile->GetPrefs()->SetInteger(prefs::kGlicCompletedFre,
                                  static_cast<int>(fre_status));
}

void InvalidateAccount(Profile* profile) {
  auto* const identity_manager = IdentityManagerFactory::GetForProfile(profile);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager,
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  ASSERT_TRUE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          identity_manager->GetPrimaryAccountId(
              signin::ConsentLevel::kSignin)));
  ASSERT_FALSE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  ASSERT_TRUE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}

void ReauthAccount(Profile* profile) {
  auto* const identity_manager = IdentityManagerFactory::GetForProfile(profile);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager,
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      GoogleServiceAuthError::AuthErrorNone());
}

}  // namespace glic
