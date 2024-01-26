// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_REVOCATION_REQUEST_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_REVOCATION_REQUEST_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/permissions/crowd_deny_preload_data.h"
#include "chrome/browser/permissions/crowd_deny_safe_browsing_request.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "url/gurl.h"

class Profile;

// Revokes the notifications permission if an origin marked as abusive or
// disruptive. This is the case when:
//  1) The notifications permission revocation experiment is enabled.
//  2) The origin exists on ABUSIVE_PROMPTS, ABUSIVE_CONTENT or
//     DISRUPTIVE_BEHAVIOR blocking lists.
//  3) The origin exists on SafeBrowsing.
//  4) If a user granted notification permission via quiet permission prompt UI,
//  revocation will not applied.
class PermissionRevocationRequest {
 public:
  // The permission revocation verdict for a given origin.
  enum class Outcome {
    PERMISSION_NOT_REVOKED,
    PERMISSION_REVOKED_DUE_TO_ABUSE,
    PERMISSION_REVOKED_DUE_TO_DISRUPTIVE_BEHAVIOR,
  };

  using OutcomeCallback = base::OnceCallback<void(Outcome)>;

  // Asynchronously starts origin verification and revokes notifications
  // permission.
  PermissionRevocationRequest(Profile* profile,
                              const GURL& origin,
                              OutcomeCallback callback);
  ~PermissionRevocationRequest();

  PermissionRevocationRequest(const PermissionRevocationRequest&) = delete;
  PermissionRevocationRequest& operator=(const PermissionRevocationRequest&) =
      delete;

  static void ExemptOriginFromFutureRevocations(Profile* profile,
                                                const GURL& origin);

  static bool IsOriginExemptedFromFutureRevocations(Profile* profile,
                                                    const GURL& origin);

  static bool HasPreviouslyRevokedPermission(Profile* profile,
                                             const GURL& origin);

 private:
  // Verifies if |origin_| is on ABUSIVE_PROMPTS, ABUSIVE_CONTENT or
  // DISRUPTIVE_BEHAVIOR lists. If yes, the notifications permission will be
  // revoked. |callback_| will be synchronously called with the result.
  void CheckAndRevokeIfBlocklisted();
  void OnSiteReputationReady(
      const CrowdDenyPreloadData::SiteReputation* reputation);
  void OnSafeBrowsingVerdictReceived(
      const CrowdDenyPreloadData::SiteReputation* reputation,
      CrowdDenySafeBrowsingRequest::Verdict verdict);
  void NotifyCallback(Outcome outcome);

  std::optional<CrowdDenySafeBrowsingRequest> safe_browsing_request_;
  raw_ptr<Profile> profile_;
  const GURL origin_;
  OutcomeCallback callback_;
  // The time when the Crowd Deny request starts.
  std::optional<base::TimeTicks> crowd_deny_request_start_time_;
  // The Crowd Deny component load duration.
  std::optional<base::TimeDelta> crowd_deny_request_duration_;
  base::WeakPtrFactory<PermissionRevocationRequest> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_REVOCATION_REQUEST_H_
