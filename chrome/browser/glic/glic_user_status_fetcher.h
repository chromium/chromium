// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_USER_STATUS_FETCHER_H_
#define CHROME_BROWSER_GLIC_GLIC_USER_STATUS_FETCHER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/glic/glic_user_status_code.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "url/gurl.h"

namespace glic {

namespace prefs {
enum class SettingsPolicyState;
}

// This class, GlicUserStatusFetcher, is responsible for asynchronously fetching
// the Glic user status from a Google-owned API. The response is processed in a
// callback and determines if the current signed-in primary account user is able
// to see Glic UI surfaces(e.g. tab strip button).
//
// Ownership:
// This class is instantiate as a member of the `GlicEnabling` object if the
// feature is enabled.
//
// Request Schedule:
// The fetching of the user status is scheduled as follows:
// - Upon construction: An initial request is made if enough time has passed
//   since the last successful update (based on `kGlicUserStatusRequestDelay`
//   feature flag).
// - Upon primary account sign-in: When a primary account is signed in (or when
//   Chrome launches with a signed-in primary account), a request is initiated.
// - When refresh token becomes available: If a request is attempted
//   but the refresh token is not yet available, the fetch is retried when the
//   refresh token is updated.
// - Periodically: Subsequent requests are scheduled to occur every
//   `kGlicUserStatusRequestDelay` (currently 23 hours) after a successful
//   response.
// See comments on the timer members for more info.
//
// Error Handling:
// If the server responds with an HTTP error (non-200 status code) or fails to
// provide a valid JSON response, the `ProcessResponse` method will interpret
// this as `UserStatusCode::SERVER_UNAVAILABLE`. In this scenario, the locally
// cached user status (if any) is *not* overwritten, ensuring that the Glic
// enabling state remains consistent based on the last known good status. The
// callback will still be executed to notify the observer of the fetch attempt
// outcome.
//
// Clock Changes:
// The `GlicUserStatusFetcher` use the `base::WallClockTimer` for
// scheduling the periodic fetching. Unlike, `base::TimeTicks`, `base::Time`
// which `base::WallClockTimer` uses does not freeze during suspend. Significant
// system clock errors could lead to unexpected timing of the status updates and
// potentially affect the QPS too. However, we expect it is only minor system
// clock adjustment if any.
class GlicUserStatusFetcher : public signin::IdentityManager::Observer {
 public:
  using FetchOverrideCallback = base::RepeatingCallback<void(
      base::OnceCallback<void(const CachedUserStatus&)>)>;

  explicit GlicUserStatusFetcher(Profile* profile,
                                 base::RepeatingClosure callback);
  ~GlicUserStatusFetcher() override;

  static std::optional<CachedUserStatus> GetCachedUserStatus(Profile* profile);

  void InvalidateCachedStatus();
  void UpdateUserStatus();
  void UpdateUserStatusIfNeeded();
  void ScheduleUserStatusUpdate(base::TimeDelta time_to_next_update);
  void CancelUserStatusUpdateIfNeeded();

  // Updates the user status when information suggests that it might have
  // changed recently. This is internally throttled to avoid excessive
  // requests, for signals that might be received multiple times.
  void UpdateUserStatusWithThrottling();

  void SetGlicUserStatusUrlForTest(GURL test_url) { endpoint_ = test_url; }
  void SetFetchOverrideForTest(FetchOverrideCallback fetch_override) {
    fetch_override_for_test_ = std::move(fetch_override);
  }

 private:
  static std::optional<signin::GaiaIdHash> GetGaiaIdHashForPrimaryAccount(
      Profile* profile);

  // Updates the user status and schedules another update in the future.
  void UpdateUserStatusAndScheduleNextRefresh();

  // Called when the account managed status is found, if it was not available
  // when `UpdateUserStatus` was called.
  void OnAccountManagedStatusFound();

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  // Called when the Gemini settings pref changes.
  void OnGeminiSettingsChanged();

  void ProcessResponse(const std::string& account_id_hash,
                       const CachedUserStatus& user_status);

  // If true, the user status update could not proceed because the refresh token
  // was not yet available, and it should be retried when it becomes available.
  bool is_user_status_waiting_for_refresh_token_ = false;

  // Stores the previous value of `prefs::kGeminiSettings` to detect
  // transitions.
  glic::prefs::SettingsPolicyState cached_gemini_settings_value_;

  // Used to find the account managed status of the primary account.
  // A finder will exist only if the status is pending.
  std::unique_ptr<signin::AccountManagedStatusFinder>
      account_managed_status_finder_;
  signin::AccountManagedStatusFinderOutcome account_managed_status_ =
      signin::AccountManagedStatusFinderOutcome::kPending;

  raw_ptr<Profile> profile_;
  const base::RepeatingClosure callback_;
  GURL endpoint_;
  std::string oauth2_scope_;

  // Ensures we run a request at least as often as
  // `features::kGlicUserStatusRequestDelay`. Reset on browser start, when a
  // periodic refresh is running, and when any request (periodic or otherwise)
  // completes successfully.
  base::WallClockTimer refresh_status_timer_;

  // Ensures certain events which might trigger sooner refreshes don't occur
  // more often than every `features::kGlicUserStatusThrottleInterval`.
  base::OneShotTimer throttle_timer_;

  // If true, a throttled update was requested too soon after the last one.
  // Always false when `throttle_timer_` is not running.
  bool update_was_throttled_ = false;

  std::unique_ptr<google_apis::RequestSender> request_sender_;
  base::OnceClosure cancel_closure_;

  // When set, replaces the actual fetch with a callback.
  FetchOverrideCallback fetch_override_for_test_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<GlicUserStatusFetcher> weak_ptr_factory_{this};
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_GLIC_USER_STATUS_FETCHER_H_
