// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_H_

#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace signin {
class PrimaryAccountAccessTokenFetcher;
}

class PrefService;

namespace safe_browsing {

// Responsible for keeping track of advanced protection status of the sign-in
// profile.
// Note that for profile that is not signed-in, we consider it NOT under
// advanced protection.
// For incognito profile Chrome returns users' advanced protection status
// of its original profile.
class AdvancedProtectionStatusManager
    : public KeyedService,
      public signin::IdentityManager::Observer {
 public:
  AdvancedProtectionStatusManager(PrefService* pref_service,
                                  signin::IdentityManager* identity_manager);
  ~AdvancedProtectionStatusManager() override;

  // If the primary account of associated profile is requesting advanced
  // protection verdicts.
  bool RequestsAdvancedProtectionVerdicts();

  bool is_under_advanced_protection() const {
    return is_under_advanced_protection_;
  }

  // KeyedService:
  void Shutdown() override;

  bool IsRefreshScheduled();

 private:
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerTest,
                           NotSignedInOnStartUp);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerTest,
                           SignedInLongTimeAgoRefreshFailTransientError);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerTest,
                           SignedInLongTimeAgoRefreshFailNonTransientError);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerTest,
                           SignedInLongTimeAgoNotUnderAP);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerTest,
                           SignedInLongTimeAgoUnderAP);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerTest,
                           AlreadySignedInAndUnderAP);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerTest,
                           SignInAndSignOutEvent);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerTest, AccountRemoval);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerTest,
                           StayInAdvancedProtection);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerTest,
                           AlreadySignedInAndNotUnderAP);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerTest,
                           AdvancedProtectionDisabledAfterSignin);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerTest,
                           StartupAfterLongWaitRefreshesImmediately);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerTest,
                           TracksUnconsentedPrimaryAccount);

  void Initialize();

  // Called after |Initialize()|. May trigger advanced protection status
  // refresh.
  void MaybeRefreshOnStartUp();

  // Subscribes to sign-in events.
  void SubscribeToSigninEvents();

  // Subscribes from sign-in events.
  void UnsubscribeFromSigninEvents();

  // IdentityManager::Observer implementations.
  void OnUnconsentedPrimaryAccountChanged(
      const CoreAccountInfo& account_info) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnExtendedAccountInfoRemoved(const AccountInfo& info) override;

  void OnAdvancedProtectionEnabled();

  void OnAdvancedProtectionDisabled();

  void OnAccessTokenFetchComplete(CoreAccountId account_id,
                                  GoogleServiceAuthError error,
                                  signin::AccessTokenInfo token_info);

  // Requests Gaia refresh token to obtain advanced protection status.
  void RefreshAdvancedProtectionStatus();

  // Starts a timer to schedule next refresh.
  void ScheduleNextRefresh();

  // Cancels any status refresh in the future.
  void CancelFutureRefresh();

  // Sets |last_refresh_| to now and persists it.
  void UpdateLastRefreshTime();

  bool IsUnconsentedPrimaryAccount(const CoreAccountInfo& account_info);

  // Decodes |id_token| to get advanced protection status.
  void OnGetIDToken(const CoreAccountId& account_id,
                    const std::string& id_token);

  // Only called in tests.
  void SetMinimumRefreshDelay(const base::TimeDelta& delay);

  // Gets the account ID of the unconsented primary account of |profile_|.
  // Returns an empty string if user is not signed in.
  CoreAccountId GetUnconsentedPrimaryAccountId() const;

  // Only called in tests to set a customized minimum delay.
  AdvancedProtectionStatusManager(PrefService* pref_service,
                                  signin::IdentityManager* identity_manager,
                                  const base::TimeDelta& min_delay);

  SEQUENCE_CHECKER(sequence_checker_);

  PrefService* pref_service_ = nullptr;
  signin::IdentityManager* identity_manager_ = nullptr;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // Is the profile account under advanced protection.
  bool is_under_advanced_protection_ = false;

  base::OneShotTimer timer_;
  base::Time last_refreshed_;
  base::TimeDelta minimum_delay_;

  DISALLOW_COPY_AND_ASSIGN(AdvancedProtectionStatusManager);
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_H_
