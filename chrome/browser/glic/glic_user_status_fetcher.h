// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_USER_STATUS_FETCHER_H_
#define CHROME_BROWSER_GLIC_GLIC_USER_STATUS_FETCHER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/glic/glic_user_status_code.h"
#include "chrome/browser/profiles/profile.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace glic {

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
class GlicUserStatusFetcher {
 public:
  explicit GlicUserStatusFetcher(Profile* profile,
                                 base::RepeatingClosure callback);
  ~GlicUserStatusFetcher();

  static std::optional<CachedUserStatus> GetCachedUserStatus(Profile* profile);
  static bool IsDisabled(Profile* profile);

  bool IsEnterpriseAccount();
  void InvalidateCachedStatus();
  void UpdateUserStatus();
  void UpdateUserStatusIfNeeded();
  void ScheduleUserStatusUpdate(base::TimeDelta time_to_next_update);
  void CancelUserStatusUpdateIfNeeded();

  void SetGlicUserStatusUrlForTest(GURL test_url) { endpoint_ = test_url; }

 private:
  static std::optional<signin::GaiaIdHash> GetGaiaIdHashForPrimaryAccount(
      Profile* profile);

  void CreateRequestSender();

  void FetchNow();

  void ProcessResponse(const std::string& account_id_hash,
                       UserStatusCode result_code);

  bool is_user_status_waiting_for_refresh_token_ = false;
  raw_ptr<Profile> profile_;
  const base::RepeatingClosure callback_;
  GURL endpoint_;
  std::string oauth2_scope_;
  base::WallClockTimer refresh_status_timer_;
  std::unique_ptr<google_apis::RequestSender> request_sender_;
  base::OnceClosure cancel_closure_;

  base::WeakPtrFactory<GlicUserStatusFetcher> weak_ptr_factory_{this};
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_GLIC_USER_STATUS_FETCHER_H_
