// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_DESKTOP_H_
#define CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_DESKTOP_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
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

// Desktop implementation of `AdvancedProtectionStatusManager`.
// Note that for profile that is not signed-in, we consider it NOT under
// advanced protection.
class AdvancedProtectionStatusManagerDesktop
    : public signin::IdentityManager::Observer,
      public AdvancedProtectionStatusManager {
 public:
  AdvancedProtectionStatusManagerDesktop(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager);

  AdvancedProtectionStatusManagerDesktop(
      const AdvancedProtectionStatusManagerDesktop&) = delete;
  AdvancedProtectionStatusManagerDesktop& operator=(
      const AdvancedProtectionStatusManagerDesktop&) = delete;

  ~AdvancedProtectionStatusManagerDesktop() override;

  // AdvancedProtectionManager:
  bool IsUnderAdvancedProtection() const override;
  void SetAdvancedProtectionStatusForTesting(bool enrolled) override;

  // KeyedService:
  void Shutdown() override;

  bool IsRefreshScheduled();

 private:
  friend class AdvancedProtectionStatusManagerTest;

  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerDesktopTest,
                           NotSignedInOnStartUp);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerDesktopTest,
                           SignedInLongTimeAgoRefreshFailTransientError);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerDesktopTest,
                           SignedInLongTimeAgoRefreshFailNonTransientError);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerDesktopTest,
                           SignedInLongTimeAgoNotUnderAP);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerDesktopTest,
                           SignedInLongTimeAgoUnderAP);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerDesktopTest,
                           AlreadySignedInAndUnderAP);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerDesktopTest,
                           SignInAndSignOutEvent);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerDesktopTest,
                           AccountRemoval);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerDesktopTest,
                           StayInAdvancedProtection);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerDesktopTest,
                           AlreadySignedInAndNotUnderAP);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerDesktopTest,
                           AdvancedProtectionDisabledAfterSignin);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerDesktopTest,
                           StartupAfterLongWaitRefreshesImmediately);
  FRIEND_TEST_ALL_PREFIXES(AdvancedProtectionStatusManagerDesktopTest,
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

  // Only called in tests to set a customized minimum delay.
  AdvancedProtectionStatusManagerDesktop(
      PrefService* pref_service,
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
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_DESKTOP_H_
