// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace signin {
class PrimaryAccountAccessTokenFetcher;
}

class PrefService;
FORWARD_DECLARE_TEST(GeneratedHttpsFirstModePrefTest,
                     AdvancedProtectionStatusChange_InitiallySignedIn);
FORWARD_DECLARE_TEST(GeneratedHttpsFirstModePrefTest,
                     AdvancedProtectionStatusChange_InitiallyNotSignedIn);

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
  // Tracks Advanced Protection status. Might be recorded multiple times for
  // the same user. Recorded in histograms, do not reorder or delete items.
  enum class UmaEvent {
    kNone = 0,
    // Advanced Protection is disabled. Either the user hasn't signed in or
    // the signed-in user isn't under AP.
    kDisabled = 1,
    // Advanced Protection is enabled for the signed-in user.
    kEnabled = 2,
    // Advanced Protection was enabled for the user, but is now disabled.
    // This is only recorded within a browser session and is only a rough count.
    // If the user unenrolls from AP and closes Chrome before the next refresh,
    // we'll record kDisabled on startup instead of kDisabledAfterEnabled.
    kDisabledAfterEnabled = 3,
    // Advanced Protection was disabled for the user, but is now enabled.
    // Also a rough count. If the user enrolls to AP and closes Chrome before
    // the next refresh, we'll record kEnabled on startup instead of
    // kEnabledAfterDisabled.
    kEnabledAfterDisabled = 4,
    kMaxValue = kEnabledAfterDisabled,
  };

  // Observer to track changes in the enabled/disabled status of Advanced
  // Protection. Observers must use IsUnderAdvancedProtection() to check the
  // status.
  class StatusChangedObserver : public base::CheckedObserver {
   public:
    virtual void OnAdvancedProtectionStatusChanged(bool enabled) {}
  };

  AdvancedProtectionStatusManager(PrefService* pref_service,
                                  signin::IdentityManager* identity_manager);

  AdvancedProtectionStatusManager(const AdvancedProtectionStatusManager&) =
      delete;
  AdvancedProtectionStatusManager& operator=(
      const AdvancedProtectionStatusManager&) = delete;

  ~AdvancedProtectionStatusManager() override;

  // Returns whether the unconsented primary account of the associated profile
  // is under Advanced Protection.
  bool IsUnderAdvancedProtection() const;

  // KeyedService:
  void Shutdown() override;

  bool IsRefreshScheduled();

  void SetAdvancedProtectionStatusForTesting(bool enrolled);

  // Adds and removes observers to observe enabled/disabled status changes.
  void AddObserver(StatusChangedObserver* observer);
  void RemoveObserver(StatusChangedObserver* observer);

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
  FRIEND_TEST_ALL_PREFIXES(::GeneratedHttpsFirstModePrefTest,
                           AdvancedProtectionStatusChange_InitiallySignedIn);
  FRIEND_TEST_ALL_PREFIXES(::GeneratedHttpsFirstModePrefTest,
                           AdvancedProtectionStatusChange_InitiallyNotSignedIn);

  void Initialize();

  // Called after |Initialize()|. May trigger advanced protection status
  // refresh.
  void MaybeRefreshOnStartUp();

  // Subscribes to sign-in events.
  void SubscribeToSigninEvents();

  // Subscribes from sign-in events.
  void UnsubscribeFromSigninEvents();

  // IdentityManager::Observer implementations.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
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

  // Gets the account ID of the unconsented primary account.
  // Returns an empty string if user is not signed in.
  CoreAccountId GetUnconsentedPrimaryAccountId() const;

  void NotifyStatusChanged();

  // Only called in tests to set a customized minimum delay.
  AdvancedProtectionStatusManager(PrefService* pref_service,
                                  signin::IdentityManager* identity_manager,
                                  const base::TimeDelta& min_delay);

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<PrefService> pref_service_ = nullptr;
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // Is the primary account under advanced protection.
  bool is_under_advanced_protection_ = false;

  base::OneShotTimer timer_;
  base::Time last_refreshed_;
  base::TimeDelta minimum_delay_;
  base::ObserverList<StatusChangedObserver> observers_;
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_H_
