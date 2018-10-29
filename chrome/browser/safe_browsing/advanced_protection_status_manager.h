// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_H_

#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "services/identity/public/cpp/access_token_info.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace identity {
class PrimaryAccountAccessTokenFetcher;
}

class Profile;

namespace safe_browsing {

// Responsible for keeping track of advanced protection status of the sign-in
// profile.
// Note that for profile that is not signed-in, we consider it NOT under
// advanced protection.
// For incognito profile Chrome returns users' advanced protection status
// of its original profile.
class AdvancedProtectionStatusManager
    : public KeyedService,
      public AccountTrackerService::Observer,
      public identity::IdentityManager::Observer {
 public:
  explicit AdvancedProtectionStatusManager(Profile* profile);
  ~AdvancedProtectionStatusManager() override;

  // If the primary account of |profile| is under advanced protection.
  static bool IsUnderAdvancedProtection(Profile* profile);

  bool is_under_advanced_protection() const {
    return is_under_advanced_protection_;
  }

  Profile* profile() const { return profile_; }

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
                           AlreadySignedInAndUnderAPIncognito);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerTest,
                           AlreadySignedInAndNotUnderAPIncognito);

  void Initialize();

  // Called after |Initialize()|. May trigger advanced protection status
  // refresh.
  void MaybeRefreshOnStartUp();

  // Subscribes to sign-in events.
  void SubscribeToSigninEvents();

  // Subscribes from sign-in events.
  void UnsubscribeFromSigninEvents();

  // AccountTrackerService::Observer implementations.
  void OnAccountUpdated(const AccountInfo& info) override;
  void OnAccountRemoved(const AccountInfo& info) override;

  // IdentityManager::Observer implementations.
  void OnPrimaryAccountSet(const AccountInfo& account_info) override;
  void OnPrimaryAccountCleared(const AccountInfo& account_info) override;

  void OnAdvancedProtectionEnabled();

  void OnAdvancedProtectionDisabled();

  void OnAccessTokenFetchComplete(std::string account_id,
                                  GoogleServiceAuthError error,
                                  identity::AccessTokenInfo token_info);

  // Requests Gaia refresh token to obtain advanced protection status.
  void RefreshAdvancedProtectionStatus();

  // Starts a timer to schedule next refresh.
  void ScheduleNextRefresh();

  // Cancels any status refresh in the future.
  void CancelFutureRefresh();

  // Sets |last_refresh_| to now and persists it.
  void UpdateLastRefreshTime();

  bool IsPrimaryAccount(const AccountInfo& account_info);

  // Decodes |id_token| to get advanced protection status.
  void OnGetIDToken(const std::string& account_id, const std::string& id_token);

  // Only called in tests.
  void SetMinimumRefreshDelay(const base::TimeDelta& delay);

  // Gets the account ID of the primary account of |profile_|.
  // Returns an empty string if user is not signed in.
  std::string GetPrimaryAccountId() const;

  // Only called in tests to set a customized minimum delay.
  AdvancedProtectionStatusManager(Profile* profile,
                                  const base::TimeDelta& min_delay);

  Profile* const profile_;

  identity::IdentityManager* identity_manager_;
  std::unique_ptr<identity::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  AccountTrackerService* account_tracker_service_;

  // Is the profile account under advanced protection.
  bool is_under_advanced_protection_;

  base::OneShotTimer timer_;
  base::Time last_refreshed_;
  base::TimeDelta minimum_delay_;

  DISALLOW_COPY_AND_ASSIGN(AdvancedProtectionStatusManager);
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_H_
