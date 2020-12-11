// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_ABUSIVE_ORIGIN_PERMISSION_REVOCATION_REQUEST_H_
#define CHROME_BROWSER_PERMISSIONS_ABUSIVE_ORIGIN_PERMISSION_REVOCATION_REQUEST_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/permissions/crowd_deny_safe_browsing_request.h"
#include "url/gurl.h"

class Profile;
enum class ContentSettingsType;

// Revokes the notifications permission if an origin marked as abusive.
// This is the case when:
//  1) The notifications permission revocation experiment is enabled.
//  2) The origin exists on ABUSIVE_PROMPTS or ABUSIVE_CONTENT blocking lists.
//  3) The origin exists on SafeBrowsing.
//  4) If a user granted notification permission via quiet permission prompt UI,
//  revocation will not applied.
class AbusiveOriginPermissionRevocationRequest {
 public:
  // The abusive permission revocation verdict for a given origin.
  enum class Outcome {
    PERMISSION_NOT_REVOKED,
    PERMISSION_REVOKED_DUE_TO_ABUSE,
  };

  using OutcomeCallback = base::OnceCallback<void(Outcome)>;

  // Asynchronously starts origin verification and revokes notifications
  // permission.
  AbusiveOriginPermissionRevocationRequest(Profile* profile,
                                           const GURL& origin,
                                           OutcomeCallback callback);
  ~AbusiveOriginPermissionRevocationRequest();

  AbusiveOriginPermissionRevocationRequest(
      const AbusiveOriginPermissionRevocationRequest&) = delete;
  AbusiveOriginPermissionRevocationRequest& operator=(
      const AbusiveOriginPermissionRevocationRequest&) = delete;

  static void ExemptOriginFromFutureRevocations(Profile* profile,
                                                const GURL& origin);

  static bool IsOriginExemptedFromFutureRevocations(Profile* profile,
                                                    const GURL& origin);

  static bool HasPreviouslyRevokedPermission(Profile* profile,
                                             const GURL& origin);

 private:
  // Verifies if |origin_| is on ABUSIVE_PROMPTS and ABUSIVE_CONTENT lists. If
  // yes, the notifications permission will be revoked. |callback_| will be
  // synchronously called with the result.
  void CheckAndRevokeIfAbusive();
  void OnSafeBrowsingVerdictReceived(
      CrowdDenySafeBrowsingRequest::Verdict verdict);

  base::Optional<CrowdDenySafeBrowsingRequest> safe_browsing_request_;
  Profile* profile_;
  const GURL origin_;
  OutcomeCallback callback_;
  base::WeakPtrFactory<AbusiveOriginPermissionRevocationRequest> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_PERMISSIONS_ABUSIVE_ORIGIN_PERMISSION_REVOCATION_REQUEST_H_
