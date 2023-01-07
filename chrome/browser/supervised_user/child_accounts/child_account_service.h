// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_SERVICE_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_SERVICE_H_

#include "chrome/browser/supervised_user/child_accounts/family_info_fetcher.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "chrome/common/pref_names.h"

class ChildAccountService : public KeyedService {
 public:
  enum class AuthState { AUTHENTICATED, NOT_AUTHENTICATED, PENDING };

  static constexpr bool IsChildAccountDetectionEnabled() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
    // Child account detection is always enabled on Android and ChromeOS, and
    // disabled in other platforms.
    return true;
#else
    return false;
#endif
  }

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
    registry->RegisterBooleanPref(prefs::kChildAccountStatusKnown, false);
  }

  virtual void Init() = 0;

  // Responds whether at least one request for child status was successful.
  // And we got answer whether the profile belongs to a child account or not.
  virtual bool IsChildAccountStatusKnown() = 0;

  virtual void AddChildStatusReceivedCallback(base::OnceClosure callback) = 0;

  // Returns whether or not the user is authenticated on Google web properties
  // based on the state of the cookie jar. Returns AuthState::PENDING if
  // authentication state can't be determined at the moment.
  virtual AuthState GetGoogleAuthState() = 0;

  // Subscribes to changes to the Google authentication state
  // (see IsGoogleAuthenticated()). Can send a notification even if the
  // authentication state has not changed.
  virtual base::CallbackListSubscription ObserveGoogleAuthState(
      const base::RepeatingCallback<void()>& callback) = 0;

  virtual void OnGetFamilyMembersSuccess(
      const std::vector<FamilyInfoFetcher::FamilyMember>& members) = 0;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_SERVICE_H_
